# Этап 3. DLL, загрузчик и протокол

## 1. Что требуется

По заданию на этапе 3 нужно:

1. собрать «пустую» DLL;
2. разработать программу, которая загружает DLL в целевой процесс;
3. продумать протокол передачи режима и аргументов в DLL и сообщений
   мониторинга обратно.

На этом этапе DLL ещё **не перехватывает** функции. `HookPatch`
переносится в DLL на этапе 4.

## 2. Структура

```
stage3/dll/dllmain.cpp          — пустая по перехвату DLL + pipe-поток
stage3/dll/stage3_dll.vcxproj
stage3/loader/loader.cpp        — инжектор + клиент pipe
stage3/loader/stage3_loader.vcxproj
stage3/victim/victim.cpp        — целевой процесс
stage3/victim/stage3_victim.vcxproj
```

Выходные файлы:

```text
bin\x64\Debug\stage3_plugin.dll
bin\x64\Debug\stage3_loader.exe
bin\x64\Debug\stage3_victim.exe
```

## 3. Пустая DLL

`DllMain` только создаёт поток:

```cpp
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
```

`PipeThread` подключается к каналу, отправляет `HELLO <pid>` и отвечает
на команды. Никаких `VirtualProtect`, `HookPatch` и `CreateFileW` здесь
пока нет.

## 4. Загрузчик

Запуск:

```text
stage3_loader.exe <pid> <path_to_dll>
```

`InjectDll`:

1. `OpenProcess` с минимальными правами:
   ```cpp
   PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
   PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ
   ```
2. `GetFullPathNameW` и проверка существования DLL.
3. `VirtualAllocEx` в целевом процессе.
4. `WriteProcessMemory` — записать полный путь к DLL.
5. `CreateRemoteThread` с `LoadLibraryW`.
6. `WaitForSingleObject` до 10 секунд.
7. `GetExitCodeThread`: `0` — ошибка, не `0` — HMODULE, DLL загружена.
8. `VirtualFreeEx`, `CloseHandle`.

`PROCESS_ALL_ACCESS` не используется: он избыточен и может не сработать
на защищённых процессах.

Целевой процесс `stage3_victim.exe` печатает PID и спит 30 секунд.
Его удобно запускать из PowerShell:

```powershell
$victim = Start-Process bin\x64\Debug\stage3_victim.exe -PassThru
Start-Sleep -Seconds 1
bin\x64\Debug\stage3_loader.exe $victim.Id (Resolve-Path bin\x64\Debug\stage3_plugin.dll)
```

## 5. Протокол

Используется именованный канал в message-режиме:

- имя: `\\.\pipe\lab1_stage3`;
- `PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE`;
- сообщение — null-terminated ASCII-строка до 1024 байт;
- каждая `WriteFile` — одно сообщение, `ReadFile` возвращает его целиком;
- без ручного поиска `\n`.

### Реализовано на этапе 3

| Направление | Сообщение | Смысл |
|---|---|---|
| DLL → loader | `HELLO <pid>` | DLL загрузилась и подключилась |
| loader → DLL | `-ping` | проверка протокола |
| DLL → loader | `OK pong` | протокол работает |
| loader → DLL | `-stop` | завершить поток DLL |
| DLL → loader | `OK bye` | подтверждение |
| DLL → loader | `ERROR <text>` | неизвестная команда |

### Зарезервировано на этап 4

| Направление | Сообщение | Смысл |
|---|---|---|
| loader → DLL | `-func <name>` | мониторинг функции |
| loader → DLL | `-hide <path>` | скрытие файла |
| DLL → loader | `OK <text>` | команда принята |
| DLL → loader | `LOG <time> <function>` | сообщение мониторинга |

Формат выбран потому, что он минимальный: не нужно придумывать
экранирование и разбирать поток байтов. Для лабораторной достаточно
одной команды на соединение.

## 6. Тест

Сборка x64 Debug/Release, затем:

```text
[loader] target pid=12432
[loader] dll=C:\...\bin\x64\Debug\stage3_plugin.dll
[loader] LoadLibraryW -> 0xC29F0000 (loaded)
[loader] pipe connected
[loader] <- HELLO 12432
[loader] -> -ping
[loader] <- OK pong
[loader] -> -stop
[loader] <- OK bye
[loader] done
```

Проверено:

- victim остаётся жив после инъекции и протокола;
- DLL загружается (HMODULE не 0);
- `HELLO` содержит PID именно целевого процесса;
- `-ping`/`-stop` получают ожидаемые ответы;
- loader завершается с кодом 0;
- после `-stop` поток DLL закрывает канал и выходит.

## 7. Результат

Проверено 2026-11-04 на Windows 11 Pro 10.0.28000 x64,
Visual Studio 2026 18.0.

- [x] Пустая DLL собирается x64.
- [x] Loader загружает DLL в процесс по PID.
- [x] Проверяется результат `LoadLibraryW`.
- [x] `OpenProcess` с минимальными правами.
- [x] DLL подключается к pipe и отправляет `HELLO`.
- [x] `-ping` → `OK pong`.
- [x] `-stop` → `OK bye`.
- [x] Victim не падает.
- [x] Протокол описан, `-func`/`-hide`/`LOG` зарезервированы.
- [x] Debug и Release x64 проходят тест.

## 8. Ограничения

- Только x64.
- Loader и target должны быть в одной сессии и на одном уровне
  целостности.
- Имя pipe фиксировано, поэтому одновременно запускать только один
  тест.
- `ConnectNamedPipe` в loader синхронный: если DLL загрузилась, но
  не подключилась, loader будет ждать. В тесте DLL подключается сразу;
  таймаут можно добавить на этапе 4.

## 9. Что дальше

Этап 4:

- CLI `-pid`/`-name`/`-func`/`-hide`;
- перенос `HookPatch` в DLL;
- реализация `-func` и `-hide` по зарезервированному протоколу;
- сообщения `LOG`;
- тестирование и отчёт.
