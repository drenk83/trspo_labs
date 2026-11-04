# Этап 4. CLI, HookPatch в DLL и тестирование

## 1. Что сделано

- `stage4_plugin.dll`:
  - протокол `HELLO` / `-func` / `-hide`;
  - `HookPatch` из этапа 2;
  - ассемблерный `hook_handler` для `-func`;
  - три обёртки для `-hide`.
- `monitor.exe`:
  - CLI `-pid` / `-name` / `-func` / `-hide`;
  - инъекция DLL;
  - pipe-клиент и печать `LOG`.
- `stage4_victim.exe`:
  - перечисляет каталог и открывает файл, чтобы проверять оба режима.

## 2. Структура

```
stage4/dll/dllmain.cpp          — протокол, хуки, Status
stage4/dll/hook_handler.asm     — обработчик для -func
stage4/dll/stage4_dll.vcxproj
stage4/monitor/monitor.cpp      — CLI, инъекция, pipe-клиент
stage4/monitor/stage4_monitor.vcxproj
stage4/victim/victim.cpp        — перечисление + открытие файла
stage4/victim/stage4_victim.vcxproj
```

`HookPatch.h/.cpp` не копируются: проект DLL компилирует их из
`stage2/testapp`.

Выходные файлы:

```text
bin\x64\Debug\stage4_plugin.dll
bin\x64\Debug\monitor.exe
bin\x64\Debug\stage4_victim.exe
```

## 3. CLI

```text
monitor.exe -pid <PID> -func <functionName>
monitor.exe -name <processName> -func <functionName>
monitor.exe -pid <PID> -hide <filePath>
monitor.exe -name <processName> -hide <filePath>
```

Поддерживаются имена из таблицы этапа 1:

| Имя | Размер патча |
|---|---:|
| `CreateFileW` | 16 |
| `FindFirstFileW` | 9 |
| `FindNextFileW` | 11 |
| `CloseHandle` | 6 |

`CreateFile` автоматически превращается в `CreateFileW`,
`FindFirstFile` → `FindFirstFileW`, `FindNextFile` → `FindNextFileW`.
Неизвестная функция возвращает `ERROR unsupported function`.

`-name` ищет PID через `CreateToolhelp32Snapshot` и ждёт появления
процесса. `-hide` получает полный путь через `GetFullPathNameW` и
проверяет существование файла до инъекции.

## 4. Протокол

| Направление | Сообщение | Смысл |
|---|---|---|
| DLL → monitor | `HELLO <pid>` | DLL загружена и подключилась |
| monitor → DLL | `-func CreateFileW` | включить мониторинг |
| DLL → monitor | `OK hooked` | хук установлен |
| DLL → monitor | `LOG 19:45:12 CreateFileW` | каждый вызов функции |
| monitor → DLL | `-hide C:\...\secret.txt` | включить скрытие |
| DLL → monitor | `OK hidden` | три хука установлены |
| DLL → monitor | `ERROR ...` | ошибка |

Формат: именованный канал `\\.\pipe\lab1_stage4`, message-режим,
null-terminated строки до 1024 байт. Путь в `-hide` передаётся в UTF-8.

## 5. Режим `-func`

`monitor.exe` загружает DLL, получает `HELLO`, отправляет
`-func <name>` и ждёт `OK hooked`. Затем читает `LOG` до завершения
целевого процесса.

В DLL:

1. `GetProcAddress(kernelbase, name)`.
2. `new HookPatch(target, hook_handler, size)`.
3. `Install(&g_trampoline)`.
4. `hook_handler.asm` сохраняет регистры, вызывает `Status()`, затем
   прыгает в трамплин конкретного хука.
5. `Status()` пишет `LOG <время> <функция>` при **каждом** вызове,
   без старого подавления по секундам.

`Install(&g_trampoline)` заполняет трамплин до записи патча, поэтому
`hook_handler` не может перейти по нулевому адресу.

## 6. Режим `-hide`

Скрываются три функции:

| Функция | Обёртка |
|---|---|
| `FindFirstFileW` | `MyFindFirstFileW` |
| `FindNextFileW` | `MyFindNextFileW` |
| `CreateFileW` | `MyCreateFileW` |

Логика:

- сохраняются полный путь и имя файла;
- `CreateFileW` проверяет путь через `_wcsicmp` и `GetFullPathNameW`;
  если файл скрытый — `SetLastError(ERROR_FILE_NOT_FOUND)` и
  `INVALID_HANDLE_VALUE` без вызова оригинала;
- `FindNextFileW` пропускает скрытое имя в цикле и возвращает
  следующий файл; соседние файлы не теряются;
- `FindFirstFileW` скрывает прямой путь к файлу и, если скрытый файл
  оказался первым в маске, листает дальше через оригинальный
  `FindNextFileW`.

Порядок установки: `FindNextFileW`, `CreateFileW`, `FindFirstFileW` —
`MyFindFirstFileW` уже имеет готовый трамплин `FindNextFileW`.
При ошибке установки снимаются все уже поставленные хуки.

## 7. Тестовая программа

`stage4_victim.exe [dir] [file]`:

- печатает PID;
- ждёт 3 секунды, чтобы monitor успел поставить хуки;
- 20 раз перечисляет `dir\*` и вызывает `CreateFileW(dir\file)`;
- печатает `secret_listed`, `OPEN OK` / `OPEN FAILED`.

Данные:

```text
stage4/victim/data/secret.txt
stage4/victim/data/aaa.txt
stage4/victim/data/zzz.txt
```

## 8. Проверка

### `-func CreateFileW`

```powershell
$victim = Start-Process bin\x64\Debug\stage4_victim.exe -PassThru `
    -WorkingDirectory stage4\victim
Start-Sleep -Milliseconds 400
bin\x64\Debug\monitor.exe -pid $victim.Id -func CreateFileW
```

Фактический результат:

```text
[monitor] target pid=4328
[monitor] dll=C:\...\bin\x64\Debug\stage4_plugin.dll
[monitor] mode=func
[monitor] LoadLibraryW -> 0xC16C0000 (loaded)
[monitor] pipe connected
[monitor] <- HELLO 4328
[monitor] <- OK hooked
[monitor] LOG 19:43:28 CreateFileW
...
[monitor] LOG 19:43:37 CreateFileW
[monitor] target disconnected
[monitor] done
```

В victim — 20 `OPEN OK`, в monitor — **20** строк `LOG`. Логирование
каждого вызова подтверждено.

### `-hide secret.txt`

```powershell
$victim = Start-Process bin\x64\Debug\stage4_victim.exe -PassThru `
    -WorkingDirectory stage4\victim
Start-Sleep -Milliseconds 400
bin\x64\Debug\monitor.exe -pid $victim.Id -hide (Resolve-Path stage4\victim\data\secret.txt).Path
Start-Sleep -Seconds 4
Stop-Process -Id $victim.Id
```

`stage4_victim` ждёт 3 секунды перед первым перечислением, поэтому
даём ему время показать результат.

Результат victim:

```text
[00] count=4 secret_listed=0
[00] OPEN FAILED err=2
...
```

В перечислении остаются `.`, `..`, `aaa.txt`, `zzz.txt`; `secret.txt`
исчез, `CreateFileW` возвращает `ERROR_FILE_NOT_FOUND`. Соседние файлы
не пропали.

### Остальные проверки

- `monitor.exe -name stage4_victim.exe -func CreateFile` работает:
  алиас `CreateFile` → `CreateFileW`, `-name` находит PID.
- `monitor.exe -pid <pid> -func FindFirstFileW`,
  `-func FindNextFileW`, `-func CloseHandle` работают.
- `monitor.exe -pid <pid> -func MessageBoxW` возвращает
  `ERROR unsupported function`; target остаётся жив.
- Debug и Release x64 собираются; `-func` и `-hide` проверены в обоих.

## 9. Ограничения

- `-func` поддерживает только измеренные функции из таблицы выше.
  Для произвольной функции нужен дизассемблер длины; в KISS-версии
  таблица.
- `FindFirstFileW`/`FindNextFileW` имеют патчи 9/11 байт, поэтому
  используют относительный `E9`. DLL должна быть в пределах ±2 ГБ
  от `kernelbase`; при загрузке рядом с системными DLL это так.
- Скрытие сравнивает полный путь и имя; `\\?\`, короткие 8.3-имена и
  нестандартные формы пути не покрыты.
- Имя pipe фиксировано: одновременно один запуск.
- `-func` работает, пока жив target; для остановки — завершить target
  или прервать monitor.
- Только x64; monitor и target — одна сессия/уровень целостности.
- Debug DLL зависит от debug CRT; на чистой машине использовать
  Release.

## 10. Что дальше

Лабораторная выполнена по этапам. Остаётся собрать итоговый отчёт
(цель, задачи, ход по этапам 1–4, выводы) из `docs/stage1.md` …
`docs/stage4.md` и проверить его по требованиям задания.
