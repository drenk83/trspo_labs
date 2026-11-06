// Тестовая цель: в текущем каталоге ищет secret.txt и пробует его открыть.
// Каталог не передаётся: victim работает там, откуда его запустили.

#include <windows.h>

#include <cstdio>
#include <cwchar>

constexpr int kIterations = 20;         // сколько раз проверить каталог
constexpr DWORD kStartDelayMs = 3000;   // время monitor на установку хука
constexpr DWORD kStepDelayMs = 500;     // пауза между проверками
constexpr const wchar_t* kTargetFile = L"secret.txt";

int main() {
    // Абсолютный путь печатаем, чтобы его же можно было передать в -hide
    wchar_t targetPath[MAX_PATH] = {};
    GetFullPathNameW(kTargetFile, MAX_PATH, targetPath, nullptr);

    wprintf(L"[victim] pid=%lu target=%ls\n", GetCurrentProcessId(), targetPath);
    wprintf(L"[victim] waiting %lu ms for the monitor\n", kStartDelayMs);
    fflush(stdout);
    Sleep(kStartDelayMs);

    for (int i = 0; i < kIterations; ++i) {
        WIN32_FIND_DATAW data{};
        int entries = 0;
        bool listed = false;

        wprintf(L"[%02d]", i);
        const HANDLE find = FindFirstFileW(L"*", &data);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                ++entries;
                if (_wcsicmp(data.cFileName, kTargetFile) == 0) {
                    listed = true;
                }
                wprintf(L" %ls", data.cFileName);
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }

        const HANDLE file = CreateFileW(kTargetFile, GENERIC_READ, FILE_SHARE_READ,
                                        nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            wprintf(L"\n[%02d] entries=%d secret=%ls open=fail err=%lu\n",
                    i, entries, listed ? L"yes" : L"no", GetLastError());
        } else {
            CloseHandle(file);
            wprintf(L"\n[%02d] entries=%d secret=%ls open=ok\n",
                    i, entries, listed ? L"yes" : L"no");
        }

        fflush(stdout);
        Sleep(kStepDelayMs);
    }
    return 0;
}
