// Тестовый процесс для проверки -func и -hide
// Перечисляет каталог и пытается открыть файл
//
// Использование:
//   victim.exe [dir] [file]
// По умолчанию: data\secret.txt

#include <windows.h>
#include <shellapi.h>

#include <cstdio>
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

}  // namespace

// Точка входа тестовой программы
int main() {
    CommandLine commandLine;
    if (commandLine.argv == nullptr) {
        wprintf(L"[-] CommandLineToArgvW failed, error=%lu\n", GetLastError());
        return 1;
    }

    int argc = commandLine.argc;
    wchar_t** argv = commandLine.argv;
    const std::wstring directory = (argc > 1) ? argv[1] : L"data";
    const std::wstring fileName = (argc > 2) ? argv[2] : L"secret.txt";
    const std::wstring mask = directory + L"\\*";
    const std::wstring fullPath = directory + L"\\" + fileName;

    wprintf(L"[victim] pid=%lu dir=%ls file=%ls\n",
            GetCurrentProcessId(), directory.c_str(), fileName.c_str());
    fflush(stdout);

    // Даём monitor время загрузить DLL и поставить хук
    Sleep(3000);

    for (int i = 0; i < 20; ++i) {
        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileW(mask.c_str(), &data);
        int count = 0;
        bool listed = false;

        if (find != INVALID_HANDLE_VALUE) {
            do {
                ++count;
                if (_wcsicmp(data.cFileName, fileName.c_str()) == 0) {
                    listed = true;
                }
                wprintf(L"  %ls\n", data.cFileName);
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }

        wprintf(L"[%02d] count=%d secret_listed=%d\n",
                i, count, listed ? 1 : 0);

        HANDLE file = CreateFileW(
            fullPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            wprintf(L"[%02d] OPEN FAILED err=%lu\n", i, error);
        } else {
            wprintf(L"[%02d] OPEN OK\n", i);
            CloseHandle(file);
        }

        fflush(stdout);
        Sleep(500);
    }
    return 0;
}
