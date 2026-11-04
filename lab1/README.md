# ТРСПО, лабораторная работа №1

«Мониторинг событий в процессе»: DLL-инъекция, перехват вызовов WinAPI
и скрытие файла от целевого процесса.

## Проекты

| Проект | Результат | Назначение |
|---|---|---|
| `src/hook` | `hook.dll` | `HookPatch`, режимы `-func` и `-hide` |
| `src/monitor` | `monitor.exe` | CLI, инъекция `hook.dll`, pipe-клиент |
| `src/victim` | `victim.exe` | тестовый процесс: перечисление и открытие файла |

`victim` — только тестовая цель, в поставку лабораторной не входит.
Сданная лабораторная с Desktop (`C:\Users\winuser\Desktop\trspo\1`)
использовалась как ориентир и не изменялась.

## Сборка

Открыть `trspo-lab1.sln` в Visual Studio 2026, выбрать `Debug | x64`,
собрать (`Ctrl+Shift+B`). Результаты — в `bin\x64\Debug`.

## CLI

```text
monitor.exe -pid <PID> -func <functionName>
monitor.exe -name <processName> -func <functionName>
monitor.exe -pid <PID> -hide <filePath>
monitor.exe -name <processName> -hide <filePath>
```

Поддерживаются измеренные функции:

| Имя | Размер патча |
|---|---:|
| `CreateFileW` | 16 |
| `FindFirstFileW` | 9 |
| `FindNextFileW` | 11 |
| `CloseHandle` | 6 |

`CreateFile` автоматически превращается в `CreateFileW`,
`FindFirstFile` → `FindFirstFileW`, `FindNextFile` → `FindNextFileW`.

## Мониторинг вызовов

```powershell
$victim = Start-Process bin\x64\Debug\victim.exe -PassThru `
    -WorkingDirectory src\victim
Start-Sleep -Milliseconds 400
bin\x64\Debug\monitor.exe -pid $victim.Id -func CreateFileW
```

`victim` делает 20 вызовов `CreateFileW`, monitor печатает `LOG` на
каждый вызов. Вариант по имени:

```powershell
bin\x64\Debug\monitor.exe -name victim.exe -func CreateFile
```

## Скрытие файла

```powershell
$victim = Start-Process bin\x64\Debug\victim.exe -PassThru `
    -WorkingDirectory src\victim
Start-Sleep -Milliseconds 400
bin\x64\Debug\monitor.exe -pid $victim.Id -hide (Resolve-Path src\victim\data\secret.txt).Path
Start-Sleep -Seconds 4
Stop-Process -Id $victim.Id
```

Victim ждёт 3 секунды перед первым перечислением, поэтому после
`monitor` нужно дать ему время показать результат. Ожидаемо:
`secret.txt` исчезает из перечисления, `CreateFileW` возвращает
`ERROR_FILE_NOT_FOUND`, а `aaa.txt` и `zzz.txt` остаются.

## Как это работает

- `monitor.exe` открывает процесс, загружает `hook.dll` через
  `CreateRemoteThread` + `LoadLibraryW` и создаёт pipe.
- DLL отправляет `HELLO <pid>`, получает `-func <name>` или
  `-hide <path>` и отвечает `OK`.
- `-func`: `HookPatch` ставит хук на функцию `kernelbase`, а
  ассемблерный `hook_handler` вызывает `Status()` и пишет
  `LOG <время> <функция>` при каждом вызове.
- `-hide`: три хука (`FindFirstFileW`, `FindNextFileW`, `CreateFileW`)
  скрывают файл из перечисления и запрещают его открытие.

## Ограничения

- `-func` работает с измеренными функциями из таблицы; для
  произвольной функции нужен дизассемблер длины.
- `FindFirstFileW`/`FindNextFileW` используют относительный патч
  9/11 байт и требуют, чтобы `hook.dll` была в пределах ±2 ГБ от
  `kernelbase`.
- Скрытие сравнивает полный путь и имя; `\\?\`, короткие имена и
  нестандартные формы пути не покрыты.
- Имя pipe фиксировано: одновременно один запуск.
- Только x64; monitor и target — одна сессия и уровень целостности.
- Debug DLL зависит от debug CRT; для чистой машины — Release.

## Документация

- [ОТЧЁТ.md](ОТЧЁТ.md) — итоговый отчёт по лабораторной работе.
- [docs/КАК_РАБОТАЕТ.md](docs/КАК_РАБОТАЕТ.md) — подробная техническая
  документация: инъекция, хуки, протокол, разбор кода и примеры.
- [screenshots.md](screenshots.md) — какие скриншоты сделать и как
  вставить их в отчёт.
- [docs/history](docs/history) — исторические заметки по этапам разработки
  (как разрабатывалась лабораторная; пути внутри относятся к старым
  коммитам).
- `docs/history/images` — скриншоты отладки `CreateFileW`.
