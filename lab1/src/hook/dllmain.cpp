// Финальная DLL: HookPatch, режимы -func и -hide
// Режимы:
//   -func <name>  — хук на функцию kernelbase, каждый вызов пишет LOG;
//   -hide <path>  — хуки на FindFirstFileW/FindNextFileW/CreateFileW
//
// Протокол: HELLO, -func <name>, -hide <path>, OK/ERROR и LOG

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "HookPatch.h"

namespace {

constexpr const wchar_t* kPipeName = L"\\\\.\\pipe\\lab1_monitor";
constexpr DWORD kBufferSize = 1024;
constexpr size_t kMaxFunctionName = 128;

using FindFirstFileW_t = HANDLE(WINAPI*)(LPCWSTR, LPWIN32_FIND_DATAW);
using FindNextFileW_t = BOOL(WINAPI*)(HANDLE, LPWIN32_FIND_DATAW);
using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD,
                                     LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

struct HookTarget {
    const char* name;
    size_t patchSize;
};

// Размеры прологов измерены для этой сборки Windows
// CreateFileW — 16, чтобы подошёл и абсолютный вариант FF 25,
// если DLL окажется далеко от kernelbase
const HookTarget kTargets[] = {
    {"CreateFileW", 16},
    {"FindFirstFileW", 9},
    {"FindNextFileW", 11},
    {"CloseHandle", 6},
};

HANDLE g_pipe = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_logLock;
char g_function[kMaxFunctionName] = {};
wchar_t g_hiddenPath[MAX_PATH] = {};
wchar_t g_hiddenName[MAX_PATH] = {};

HookPatch* g_functionPatch = nullptr;
HookPatch* g_findFirstPatch = nullptr;
HookPatch* g_findNextPatch = nullptr;
HookPatch* g_createFilePatch = nullptr;

void* g_realFindFirst = nullptr;
void* g_realFindNext = nullptr;
void* g_realCreateFile = nullptr;

extern "C" void hook_handler();
extern "C" void* g_trampoline = nullptr;

// Отправляет одну строку в канал
bool SendMessage(const char* message) {
    if (g_pipe == INVALID_HANDLE_VALUE || message == nullptr) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>(strlen(message) + 1);
    DWORD written = 0;
    return WriteFile(g_pipe, message, bytes, &written, nullptr) && written == bytes;
}

// Читает одно сообщение из канала
bool ReceiveMessage(HANDLE pipe, char* buffer, DWORD size) {
    DWORD read = 0;
    ZeroMemory(buffer, size);
    return ReadFile(pipe, buffer, size - 1, &read, nullptr) && read > 0;
}

// Подключается к monitor и включает message-режим
HANDLE ConnectToLoader() {
    for (int attempt = 0; attempt < 50; ++attempt) {
        HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE,
                                  0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            for (int retry = 0; retry < 20; ++retry) {
                if (SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
                    return pipe;
                }
                Sleep(50);
            }
            CloseHandle(pipe);
            return INVALID_HANDLE_VALUE;
        }
        Sleep(100);
    }
    return INVALID_HANDLE_VALUE;
}

// Вызывается из hook_handler.asm при каждом вызове функции -func
extern "C" void Status() {
    if (g_pipe == INVALID_HANDLE_VALUE || g_function[0] == '\0') {
        return;
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);

    char message[kBufferSize];
    sprintf_s(message, "LOG %02d:%02d:%02d %s",
              time.wHour, time.wMinute, time.wSecond, g_function);

    EnterCriticalSection(&g_logLock);
    SendMessage(message);
    LeaveCriticalSection(&g_logLock);
}

// Проверяет, совпадает ли путь со скрытым файлом
bool IsHiddenPath(const wchar_t* path) {
    if (path == nullptr || g_hiddenPath[0] == L'\0') {
        return false;
    }
    if (_wcsicmp(path, g_hiddenPath) == 0) {
        return true;
    }

    wchar_t fullPath[MAX_PATH];
    const DWORD length = GetFullPathNameW(path, MAX_PATH, fullPath, nullptr);
    return length > 0 && length < MAX_PATH && _wcsicmp(fullPath, g_hiddenPath) == 0;
}

// Проверяет имя файла в результатах поиска
bool IsHiddenName(const wchar_t* name) {
    return name != nullptr && g_hiddenName[0] != L'\0' &&
           _wcsicmp(name, g_hiddenName) == 0;
}

// Обёртка FindFirstFileW: скрывает прямой путь и первый скрытый результат
HANDLE WINAPI MyFindFirstFileW(LPCWSTR fileName, LPWIN32_FIND_DATAW data) {
    if (IsHiddenPath(fileName)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    auto real = reinterpret_cast<FindFirstFileW_t>(g_realFindFirst);
    HANDLE handle = real(fileName, data);
    if (handle == INVALID_HANDLE_VALUE) {
        return handle;
    }

    // Если первый результат — скрытый файл, листаем дальше
    while (IsHiddenName(data->cFileName)) {
        auto realNext = reinterpret_cast<FindNextFileW_t>(g_realFindNext);
        if (!realNext(handle, data)) {
            const DWORD error = GetLastError();
            FindClose(handle);
            SetLastError(error == ERROR_NO_MORE_FILES ? ERROR_FILE_NOT_FOUND : error);
            return INVALID_HANDLE_VALUE;
        }
    }
    return handle;
}

// Обёртка FindNextFileW: пропускает скрытое имя
BOOL WINAPI MyFindNextFileW(HANDLE handle, LPWIN32_FIND_DATAW data) {
    auto real = reinterpret_cast<FindNextFileW_t>(g_realFindNext);
    for (;;) {
        if (!real(handle, data)) {
            return FALSE;
        }
        if (!IsHiddenName(data->cFileName)) {
            return TRUE;
        }
    }
}

// Обёртка CreateFileW: запрещает открытие скрытого файла
HANDLE WINAPI MyCreateFileW(LPCWSTR fileName, DWORD desiredAccess,
                            DWORD shareMode, LPSECURITY_ATTRIBUTES security,
                            DWORD creationDisposition, DWORD flags,
                            HANDLE templateFile) {
    if (IsHiddenPath(fileName)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    auto real = reinterpret_cast<CreateFileW_t>(g_realCreateFile);
    return real(fileName, desiredAccess, shareMode, security,
                creationDisposition, flags, templateFile);
}

// Ищет функцию в таблице поддерживаемых
const HookTarget* FindTarget(const char* name) {
    for (const HookTarget& target : kTargets) {
        if (strcmp(name, target.name) == 0) {
            return &target;
        }
    }
    return nullptr;
}

// Ставит хук на функцию из таблицы и запоминает её имя
bool InstallFunctionHook(const char* name) {
    const HookTarget* info = FindTarget(name);
    if (info == nullptr) {
        return false;
    }

    HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
    void* target = reinterpret_cast<void*>(GetProcAddress(kernelbase, info->name));
    if (target == nullptr) {
        return false;
    }

    strncpy_s(g_function, sizeof(g_function), name, _TRUNCATE);
    g_functionPatch = new HookPatch(target,
                                    reinterpret_cast<void*>(&hook_handler),
                                    info->patchSize);
    if (!g_functionPatch->Install(&g_trampoline)) {
        delete g_functionPatch;
        g_functionPatch = nullptr;
        g_function[0] = '\0';
        return false;
    }
    return true;
}

// Преобразует UTF-8 в широкую строку
bool ConvertUtf8ToWide(const char* text, wchar_t* buffer, size_t count) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0 || static_cast<size_t>(length) > count) {
        return false;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, buffer, static_cast<int>(count));
    return true;
}

// Сохраняет полный путь и имя скрытого файла
bool PrepareHiddenPath(const char* utf8Path) {
    if (utf8Path == nullptr || *utf8Path == '\0') {
        return false;
    }
    if (!ConvertUtf8ToWide(utf8Path, g_hiddenPath, MAX_PATH)) {
        return false;
    }

    const wchar_t* name = wcsrchr(g_hiddenPath, L'\\');
    name = (name != nullptr) ? name + 1 : g_hiddenPath;
    wcscpy_s(g_hiddenName, MAX_PATH, name);
    return true;
}

// Снимает и удаляет один хук
void RemovePatch(HookPatch*& patch) {
    if (patch != nullptr) {
        patch->Remove();
        delete patch;
        patch = nullptr;
    }
}

// Снимает все хуки скрытия
void RemoveHidePatches() {
    RemovePatch(g_findFirstPatch);
    RemovePatch(g_createFilePatch);
    RemovePatch(g_findNextPatch);
}

// Ставит три хука для скрытия файла
bool InstallHidePatches(const char* utf8Path) {
    if (!PrepareHiddenPath(utf8Path)) {
        return false;
    }

    HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
    if (kernelbase == nullptr) {
        return false;
    }

    void* findFirst = reinterpret_cast<void*>(GetProcAddress(kernelbase, "FindFirstFileW"));
    void* findNext = reinterpret_cast<void*>(GetProcAddress(kernelbase, "FindNextFileW"));
    void* createFile = reinterpret_cast<void*>(GetProcAddress(kernelbase, "CreateFileW"));
    if (findFirst == nullptr || findNext == nullptr || createFile == nullptr) {
        return false;
    }

    // FindNext ставим первым: MyFindFirstFileW использует его трамплин
    g_findNextPatch = new HookPatch(findNext,
                                    reinterpret_cast<void*>(&MyFindNextFileW), 11);
    if (!g_findNextPatch->Install(&g_realFindNext)) {
        RemoveHidePatches();
        return false;
    }

    g_createFilePatch = new HookPatch(createFile,
                                      reinterpret_cast<void*>(&MyCreateFileW), 16);
    if (!g_createFilePatch->Install(&g_realCreateFile)) {
        RemoveHidePatches();
        return false;
    }

    g_findFirstPatch = new HookPatch(findFirst,
                                     reinterpret_cast<void*>(&MyFindFirstFileW), 9);
    if (!g_findFirstPatch->Install(&g_realFindFirst)) {
        RemoveHidePatches();
        return false;
    }
    return true;
}

// Ждёт команду от monitor и запускает нужный режим
DWORD WINAPI PipeThread(LPVOID) {
    g_pipe = ConnectToLoader();
    if (g_pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }

    char hello[kBufferSize];
    sprintf_s(hello, "HELLO %lu", GetCurrentProcessId());
    SendMessage(hello);

    char command[kBufferSize];
    if (!ReceiveMessage(g_pipe, command, sizeof(command))) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
        return 0;
    }

    if (strncmp(command, "-func ", 6) == 0) {
        const bool ok = InstallFunctionHook(command + 6);
        SendMessage(ok ? "OK hooked" : "ERROR unsupported function");
    } else if (strncmp(command, "-hide ", 6) == 0) {
        const bool ok = InstallHidePatches(command + 6);
        SendMessage(ok ? "OK hidden" : "ERROR hide failed");
    } else {
        SendMessage("ERROR unknown command");
    }
    return 0;
}

}  // namespace

// Точка входа DLL: создаёт поток протокола
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        InitializeCriticalSection(&g_logLock);
        HANDLE thread = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
