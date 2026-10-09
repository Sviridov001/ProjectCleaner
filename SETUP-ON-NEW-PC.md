# Открыть проект ProjectCleaner на другом компьютере в OpenCode

Пошаговая инструкция. Время: ~30–60 минут (плюс скачивание ArchiCAD/DevKit).

---

## 1. Что должно стоять на новом ПК

| Софт | Зачем | Где брать / версия как здесь |
|---|---|---|
| Git | клонировать репозиторий | git 2.52, `git --version` |
| GitHub CLI (`gh`) | пуш и релизы | gh 2.102, потом `gh auth login` |
| Node.js LTS | MCP-серверы, `npm` | nodejs в `C:\Program Files\nodejs` |
| Python 3.x | `uvx`, компиляция ресурсов ArchiCAD | python.org, при установке поставить галку **Add to PATH** |
| `uv` (даёт `uvx`) | запуск MCP-серверов Blender/ArchiCAD | `powershell -c "irm https://astral.sh/uv/install.ps1 \| iex"`, проверка: `uvx --version` |
| Visual Studio 2022/18 Community | компилятор C++ | workload **Desktop development with C++** |
| CMake 4.x | сборка `.apx` | идёт в составе VS (`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) |
| ArchiCAD 26 | запуск и проверка аддона | та же версия 26 |
| APIDevKit 26 | заголовки и инструменты API | положить в `C:\Dev\APIDevKit-26` (если путь другой — см. п. 7) |
| OpenCode | сам агент | см. п. 2 |

## 2. Установить и открыть OpenCode

Вариант А (как здесь): десктопное приложение **OpenCode AI Desktop** с opencode.ai —
открыть в нём папку проекта.

Вариант Б (терминал): `npm i -g opencode-ai`, затем в папке проекта `opencode`.

## 3. Склонировать репозиторий

```powershell
git clone https://github.com/Sviridov001/ProjectCleaner.git C:\Work\vibecod\proekt01
```

## 4. Авторизация модели

В OpenCode выполнить вход — та же учётка/провайдер, что на старом ПК
(здесь модель `agentrouter-openai/gpt-5.5` через AgentRouter):

```powershell
opencode auth login
```

API-ключ провайдера вводится при логине. Файл `auth.json` со старого ПК **не копировать**
(там секреты) — залогиниться заново.

## 5. Скопировать то, чего НЕТ в git (важно!)

В `.gitignore` специально исключены `opencode.json`, `.opencode/`, `*.apx`, `*.svg`
(кроме иконок), `SlabFromHatch/` — их git не перенесёт. Скопировать со старого ПК вручную:

| Что | Откуда (старый ПК) | Куда (новый ПК) |
|---|---|---|
| `opencode.json` | корень репо | корень репо |
| `.opencode/skills/archicad-tapir/` | корень репо | корень репо |
| Глобальные скиллы (`archicad-gsm-from-photo`, `excel-formula-links`) | `%USERPROFILE%\.config\opencode\skills\` | туда же |
| Кастомные команды (`tokenscope.md` и др.) | `%USERPROFILE%\.config\opencode\command\` | туда же |
| `C:\Dev\APIDevKit-26` | целиком | `C:\Dev\APIDevKit-26` (или другой путь — см. п. 7) |
| Собранный `ProjectCleaner.apx` | `C:\Dev\build\ProjectCleaner\Release\` | можно не копировать — собрать заново (п. 7) |

`node_modules` (в `.opencode/` и в конфиге) копировать **не нужно** — восстановятся сами.

## 6. MCP-серверы (проверка)

`opencode.json` уже содержит три сервера, ничего ставить вручную не надо:

- **blender**: `uvx mcp-for-blender` — подтянется сам при первом запуске; в Blender включить add-on «Blender MCP»;
- **archicad**: `uvx --from tapir-archicad-mcp archicad-server` — подтянется сам; в ArchiCAD 26
  установить **TapirAddOn_AC26_Win.apx** с релизов `ENZYME-APD/tapir-archicad-automation`
  (Файл → Менеджер дополнений → Добавить, нужен запуск от админа/доверенная папка);
- **revit**: пути к `node.exe`/`index.js` Revit-плагина захардкожены под пользователя `User` —
  **на новом ПК поправить пути в `opencode.json`** под локального пользователя (или отключить сервер).

Проверка в чате OpenCode: попросить выполнить `blender_get_addon_status` /
`archicad_list_commands` — серверы должны ответить.

## 7. Сборка ProjectCleaner.apx

Патч APIDevKit (один раз, как здесь): в
`C:\Dev\APIDevKit-26\Support\Modules\GSRoot\Definitions.hpp` около строки 185
снят верхний лимит `_MSC_VER` для тулчейна v145. Без него сборка новым компилятором не стартует.

```powershell
# Конфигурация (путь DevKit захардкожен в вызове; если APIDevKit лежит в другом месте —
# сначала найти, где CMakeLists проекта берёт AC_API_DEVKIT_DIR, и подставить свой путь)
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S ProjectCleaner -B C:\Dev\build\ProjectCleaner -G "Visual Studio 18 2026" -A x64

# Сборка (ArchiCAD должен быть ЗАКРЫТ, иначе LNK1104 — .apx заблокирован)
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Dev\build\ProjectCleaner" --config Release --target AddOn
```

Выход: `C:\Dev\build\ProjectCleaner\Release\ProjectCleaner.apx` →
Файл → Менеджер дополнений → Добавить → перезапустить ArchiCAD.

## 8. GitHub-доступ для релизов

```powershell
gh auth login
```

Дальше как обычно: коммит → `git push origin master` → `gh release create vX.Y ...`.

## 9. Финальная проверка

1. `git status` в папке проекта — чисто.
2. В OpenCode: сборка проходит, MCP `archicad` отвечает.
3. ArchiCAD: меню **Project Cleaner → Очистка проекта**, панель **Окно → Панели → Project Cleaner**.

---

## Что НЕ переезжает само (чек-лист)

- [ ] `opencode auth login` выполнен заново
- [ ] `opencode.json`, `.opencode/skills`, глобальные скиллы скопированы вручную
- [ ] Пути Revit MCP в `opencode.json` исправлены под нового пользователя
- [ ] APIDevKit на месте + патч `Definitions.hpp`
- [ ] `gh auth login` выполнен
- [ ] Tapir-аддон стоит в ArchiCAD (для MCP-команд строительства)
