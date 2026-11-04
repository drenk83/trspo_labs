// monitor.exe: CLI, инъекция hook.dll и протокол
//
// Примеры:
//   monitor.exe -pid 1234 -func CreateFile
//   monitor.exe -name victim.exe -func CreateFileW
//   monitor.exe -name victim.exe -hide C:\temp\secret.txt

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace {

// Разбирает командную строку в широкие аргументы
struct CommandLine {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    CommandLine() = default;

    ~CommandLine() {
        if (argv != nullptr) {
            LocalFree(argv);
        }
    }

    CommandLine(const CommandLine&) = delete;
    CommandLine& operator=(const CommandLine&) = delete;
};

constexpr const wchar_t* kPipeName = L"\\\\.\\pipe\\lab1_monitor";
constexpr DWORD kBufferSize = 1024;

struct Options {
    bool useName = false;
    DWORD pid = 0;
    std::wstring processName;
    bool hide = false;
    std::wstring filePath;
    std::string functionName;
};

// Разбирает PID из аргумента командной строки
bool ParsePid(const wchar_t* text, DWORD& pid) {
    wchar_t* end = nullptr;
    const unsigned long value = wcstoul(text, &end, 10);
    if (end == text || *end != L'\0' || value == 0) {
        return false;
    }
    pid = static_cast<DWORD>(value);
    return true;
}

// Преобразует широкую строку в UTF-8 для канала
std::string WideToUtf8(const std::wstring& text) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1,
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length,
                        nullptr, nullptr);
    return result;
}

// Приводит CreateFile/FindFirstFile/FindNextFile к W-версиям
std::wstring NormalizeFunction(const std::wstring& name) {
    if (_wcsicmp(name.c_str(), L"CreateFile") == 0) {
        return L"CreateFileW";
    }
    if (_wcsicmp(name.c_str(), L"FindFirstFile") == 0) {
        return L"FindFirstFileW";
    }
    if (_wcsicmp(name.c_str(), L"FindNextFile") == 0) {
        return L"FindNextFileW";
    }
    return name;
}

// Делает путь полным и проверяет, что файл существует
bool ResolveFile(const std::wstring& text, std::wstring& fullPath) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetFullPathNameW(text.c_str(), MAX_PATH, buffer, nullptr);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    if (GetFileAttributesW(buffer) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    fullPath = buffer;
    return true;
}

// Разбирает аргументы командной строки
bool ParseOptions(int argc, wchar_t** argv, Options& options, DWORD& pid) {
    bool havePid = false;
    bool haveName = false;
    bool haveFunc = false;
    bool haveHide = false;

    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"-pid") == 0 && i + 1 < argc) {
            if (!ParsePid(argv[++i], options.pid)) {
                return false;
            }
            havePid = true;
        } else if (wcscmp(argv[i], L"-name") == 0 && i + 1 < argc) {
            options.processName = argv[++i];
            options.useName = true;
            haveName = true;
        } else if (wcscmp(argv[i], L"-func") == 0 && i + 1 < argc) {
            options.functionName = WideToUtf8(NormalizeFunction(argv[++i]));
            options.hide = false;
            haveFunc = true;
        } else if (wcscmp(argv[i], L"-hide") == 0 && i + 1 < argc) {
            if (!ResolveFile(argv[++i], options.filePath)) {
                wprintf(L"[-] File does not exist: %ls\n", argv[i]);
                return false;
            }
            options.hide = true;
            haveHide = true;
        } else {
            return false;
        }
    }

    if (havePid == haveName) {
        return false;
    }
    if (haveFunc == haveHide) {
        return false;
    }
    if (options.functionName.size() + 1 > kBufferSize) {
        return false;
    }

    pid = options.pid;
    return true;
}

// Ищет PID процесса по имени
DWORD FindProcessId(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

// Ищет hook.dll рядом с monitor.exe
bool GetPluginPath(std::wstring& path) {
    wchar_t exePath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash == nullptr) {
        return false;
    }
    *(slash + 1) = L'\0';

    path = std::wstring(exePath) + L"hook.dll";
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Загружает hook.dll в процесс и проверяет результат загрузки
bool InjectDll(DWORD pid, const std::wstring& dllPath) {
    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (process == nullptr) {
        wprintf(L"[-] OpenProcess failed, error=%lu\n", GetLastError());
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryW"));
    if (loadLibrary == nullptr) {
        wprintf(L"[-] LoadLibraryW is not found\n");
        CloseHandle(process);
        return false;
    }

    const SIZE_T pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(process, nullptr, pathBytes,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remotePath == nullptr) {
        wprintf(L"[-] VirtualAllocEx failed, error=%lu\n", GetLastError());
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, remotePath, dllPath.c_str(), pathBytes, nullptr)) {
        wprintf(L"[-] WriteProcessMemory failed, error=%lu\n", GetLastError());
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary,
                                       remotePath, 0, nullptr);
    if (thread == nullptr) {
        wprintf(L"[-] CreateRemoteThread failed, error=%lu\n", GetLastError());
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 10000);
    if (wait != WAIT_OBJECT_0) {
        wprintf(L"[-] LoadLibraryW did not finish in 10 seconds\n");
        CloseHandle(thread);
        CloseHandle(process);
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    wprintf(L"[monitor] LoadLibraryW -> 0x%08lX (%ls)\n",
            static_cast<unsigned long>(exitCode),
            exitCode != 0 ? L"loaded" : L"failed");

    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(thread);
    CloseHandle(process);
    return exitCode != 0;
}

// Отправляет одну строку в канал
bool SendMessage(HANDLE pipe, const std::string& message) {
    const DWORD bytes = static_cast<DWORD>(message.size() + 1);
    DWORD written = 0;
    return WriteFile(pipe, message.c_str(), bytes, &written, nullptr) &&
           written == bytes;
}

// Читает одно сообщение из канала
bool ReceiveMessage(HANDLE pipe, std::string& message) {
    char buffer[kBufferSize];
    ZeroMemory(buffer, sizeof(buffer));
    DWORD read = 0;
    if (!ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, nullptr) || read == 0) {
        return false;
    }
    message.assign(buffer);
    return true;
}

// Создаёт канал, отправляет команду и читает ответы DLL
bool RunProtocol(DWORD pid, const Options& options) {
    HANDLE pipe = CreateNamedPipeW(
        kPipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
        1,
        kBufferSize,
        kBufferSize,
        0,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        wprintf(L"[-] CreateNamedPipe failed, error=%lu\n", GetLastError());
        return false;
    }

    if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
        wprintf(L"[-] ConnectNamedPipe failed, error=%lu\n", GetLastError());
        CloseHandle(pipe);
        return false;
    }
    wprintf(L"[monitor] pipe connected\n");

    std::string message;
    if (!ReceiveMessage(pipe, message)) {
        wprintf(L"[-] HELLO was not received, error=%lu\n", GetLastError());
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }
    wprintf(L"[monitor] <- %hs\n", message.c_str());

    char expectedHello[64];
    sprintf_s(expectedHello, sizeof(expectedHello), "HELLO %lu", pid);
    if (message != expectedHello) {
        wprintf(L"[-] Unexpected HELLO, expected %hs\n", expectedHello);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    std::string command;
    if (options.hide) {
        command = "-hide " + WideToUtf8(options.filePath);
    } else {
        command = "-func " + options.functionName;
    }
    if (command.size() + 1 > kBufferSize) {
        wprintf(L"[-] Command is too long\n");
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    if (!SendMessage(pipe, command)) {
        wprintf(L"[-] Command was not sent\n");
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    bool acknowledged = false;
    while (ReceiveMessage(pipe, message)) {
        wprintf(L"[monitor] <- %hs\n", message.c_str());
        if (message.rfind("OK", 0) == 0) {
            acknowledged = true;
            break;
        }
        if (message.rfind("ERROR", 0) == 0) {
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
            return false;
        }
    }

    if (!acknowledged) {
        wprintf(L"[-] Command was not acknowledged\n");
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    if (options.hide) {
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return true;
    }

    // Режим -func: печатаем LOG, пока целевой процесс жив
    while (ReceiveMessage(pipe, message)) {
        wprintf(L"[monitor] %hs\n", message.c_str());
    }
    wprintf(L"[monitor] target disconnected\n");

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return true;
}

}  // namespace

int main() {
    CommandLine commandLine;
    if (commandLine.argv == nullptr) {
        wprintf(L"[-] CommandLineToArgvW failed, error=%lu\n", GetLastError());
        return 1;
    }

    int argc = commandLine.argc;
    wchar_t** argv = commandLine.argv;
    Options options;
    DWORD pid = 0;

    if (argc < 3 || !ParseOptions(argc, argv, options, pid)) {
        wprintf(L"Usage:\n");
        wprintf(L"  monitor.exe -pid <PID> -func <functionName>\n");
        wprintf(L"  monitor.exe -name <processName> -func <functionName>\n");
        wprintf(L"  monitor.exe -pid <PID> -hide <filePath>\n");
        wprintf(L"  monitor.exe -name <processName> -hide <filePath>\n");
        return 1;
    }

    if (options.useName) {
        wprintf(L"[monitor] waiting for process \"%ls\"...\n", options.processName.c_str());
        while ((pid = FindProcessId(options.processName)) == 0) {
            Sleep(500);
        }
    }

    std::wstring pluginPath;
    if (!GetPluginPath(pluginPath)) {
        wprintf(L"[-] hook.dll was not found next to monitor.exe\n");
        return 1;
    }

    wprintf(L"[monitor] target pid=%lu\n", pid);
    wprintf(L"[monitor] dll=%ls\n", pluginPath.c_str());
    wprintf(L"[monitor] mode=%ls\n", options.hide ? L"hide" : L"func");

    if (!InjectDll(pid, pluginPath)) {
        wprintf(L"[-] DLL injection failed\n");
        return 1;
    }

    if (!RunProtocol(pid, options)) {
        wprintf(L"[-] Protocol failed\n");
        return 1;
    }

    wprintf(L"[monitor] done\n");
    return 0;
}
