# Отчёт об аудите и безопасном рефакторинге

Дата аудита: 2026-08-02.

Основной тематический коммит: `7becb51` (`fix: harden simulation and resource
lifetime`).

Второй тематический коммит: `576f2c9` (`refactor: isolate building command
orchestration`).

Третий тематический коммит: `d46a78a` (`fix: reject non-finite simulation
inputs`).

## Исходное состояние

- Репозиторий перед началом работ был чистым (`git status --short` не вывел
  изменений), поэтому пользовательские изменения не затрагивались.
- Объём просмотренной C++ кодовой базы: 42 010 строк вместе с тестами,
  36 759 строк в `src`.
- Базовая Debug-сборка прошла, `ian_tests` прошёл за 0,22 с.
- Базовая ASan+UBSan-сборка прошла, `ian_tests` прошёл за 1,19 с.
- В коде проекта компилятор не сообщил предупреждений. Сборка повторяла 54
  предупреждения `-Wmissing-field-initializers` из `raymath.h` для каждой
  затронутой единицы трансляции. CMake также сообщает upstream-предупреждение
  raylib о deprecated OpenGL на macOS.

## Карта подсистем и зависимостей

Основной поток уже соответствует документации:

```text
raylib input -> App -> PlayerCommand -> Simulation (fixed 60 Hz)
                                      -> snapshot/events
                                      -> graphics/UI/audio/presentation
```

- `app`: frame loop, ввод, debug-инструменты и orchestration render passes;
- `game`: состояние забега, команды, события, snapshot и координация систем;
- `buildings`: обычные здания, grid occupancy, модульные конструкции,
  structural support и каскадное разрушение;
- `enemies`, `navigation`, `world`: AI, flow field, collision world, terrain и
  spatial hash;
- `combat`: оружие, турели, пушки, ловушки и бомбы;
- `resources`, `economy`, `waves`: добыча, производство и планирование волн;
- `graphics`: RAII-обёртки raylib, модели, terrain и renderer;
- `ui`, `audio`, `presentation`: клиентское представление без изменения
  simulation state;
- `assets` и JSON loaders: collision metadata и валидируемая конфигурация.

Прямых циклических зависимостей библиотечных CMake-целей не обнаружено.
Долгоживущие игровые ссылки представлены `EntityId {index, generation}`, а
raylib-ресурсы в основном уже закрыты некопируемыми RAII-обёртками.

## Найденные проблемы

### Высокая серьёзность

1. **Утечка на неуспешной загрузке модели.** Raylib `LoadModel()` создаёт
   default material даже при отсутствии mesh. `ModelResource` вызывал
   `UnloadModel()` только для валидной модели, поэтому ошибочный asset мог
   оставить RAM allocations.
2. **Утечки на других частичных ошибках загрузки.** Неуспешный shader может
   владеть массивом locations, а невалидный render texture — частично
   созданными GPU handles. Они не освобождались, если итоговый `valid()` был
   false.

### Средняя серьёзность

3. **Undefined behavior на экстремальных координатах.** Spatial hash и две
   collision grids преобразовывали `floor(NaN/Inf/слишком большого double)` в
   `int`. Такое преобразование не определено стандартом C++.
4. **Необратимый NaN в fixed-step accumulator.** Один нечисловой frame time
   делал accumulator нечисловым; последующие кадры больше не выполняли тики.
5. **Лишние перестроения spatial hash при взрыве.** `damageInRadius()` вызывал
   обычный `damage()` для каждой цели, а каждое убийство полностью
   перестраивало индекс. При массовом убийстве одного взрыва индекс
   перестраивался до числа убитых целей раз.
6. **Нечисловые значения в публичных spatial/simulation запросах.** Flow field
   преобразовывал `NaN/Inf` через `lround`, terrain — через `floor`, а
   `Simulation::tick()` распространял невалидный delta time по состоянию.

### Низкая серьёзность / технический долг

7. `Simulation.cpp` (3007 строк до работ), `App.cpp` (1910) и несколько
   renderer/UI файлов остаются крупными. Это реальные orchestration-монолиты,
   но их нельзя безопасно делить только по размеру.
8. Значительная часть поиска сущности по стабильному ID использует линейный
   `find_if`. При текущем лимите 160 активных врагов это ограничено, но
   `densestEnemy()` остаётся квадратичным внутри spatially filtered набора.
9. **Signed overflow в бесконечном режиме.** Счётчик волны, начисления ресурсов
   и произведение награды использовали обычную арифметику `int`. После
   практически недостижимого числа волн это приводило бы к UB.
10. Автоматизированный тест не создаёт графический контекст и потому не может
   напрямую проверить реальные GPU failure paths и порядок shutdown окна.

## Исправления

- `FixedStep::advance()` игнорирует нечисловые frame times, не повреждая
  accumulator. Нормальные, отрицательные и длинные кадры сохраняют прежнюю
  семантику clamp.
- Координаты spatial hash и collision grids проверяются до преобразования в
  `int`; внешние и нечисловые значения получают безопасный sentinel/clamp.
- `ShaderResource`, `ModelResource` и `RenderTextureResource` теперь очищают
  частично созданные ресурсы сразу после неуспешной загрузки. Для текстуры
  также закрывается редкий случай невалидного объекта с ненулевым handle.
- `EnemySystem::damageInRadius()` применяет урон к собранному стабильному
  набору ID и перестраивает spatial hash один раз после всего batch, если были
  убийства. Результаты, hit animation, knockback и порядок целей сохранены.
- Заголовки raylib/raymath помечены как `SYSTEM` для собственных целей.
  Предупреждения зависимости больше не маскируют предупреждения проекта;
  warnings не подавлялись глобально и строгие флаги проекта сохранены.
- Начисления ресурсов, возвраты, награда и переход к следующему номеру волны
  используют saturating arithmetic. Форматы и обычные значения не изменены;
  экстремальные значения фиксируются на границе `int` вместо UB.
- Flow field и terrain отклоняют нечисловые координаты до integer conversion;
  Simulation игнорирует отрицательный и нечисловой delta time.

## Архитектурный рефакторинг

- Lifecycle волн (`prepareWave`, `beginPreparedWave`, дозированный spawn и
  `completeWave`) перенесён из общего `Simulation.cpp` в
  `SimulationWaves.cpp`.
- Весь lifecycle building commands — preview, placement с automatic
  foundation, upgrade, repair, sell, modular removal и gate toggle — перенесён
  в `SimulationBuildingCommands.cpp`. `Simulation.cpp` уменьшен с 3007 строк
  исходного состояния до 2422 строк.
- Публичный интерфейс не менялся. Перемещение соответствует обязанности
  систем и не дробит связанные транзакции между разными файлами.

## Чистка кода

Подтверждённо неиспользуемых production-файлов или функций в просмотренных
подсистемах не найдено. Debug-команды из README имеют реальные вызовы, поэтому
не удалялись. Устаревшие альтернативные реализации также не удалялись без
доказательства. Единственное объединение поведения — batch-обработка area
damage вместо повторения полного rebuild для каждого убийства.

## Производительность

- В горячем пути area damage число rebuild spatial hash снижено с `K` до
  максимум одного, где `K` — число убитых одним взрывом врагов.
- Spawn buffers, event buffers, projectile pools, enemy storage и основные
  spatial structures уже переиспользуют память или имеют `reserve()`/фиксированный
  размер. Слепые изменения этих контейнеров не выполнялись.
- Точный frame-time benchmark не проводился: в репозитории нет headless
  gameplay benchmark, а сравнение интерактивного renderer без одинакового
  replay было бы недостоверным.

## Тесты и проверки

Добавлены проверки:

- `FixedStep` восстанавливается после `NaN` и `Inf` frame times;
- spatial hash отклоняет нечисловые позиции;
- запрос с бесконечным радиусом остаётся определённым и не вызывает UB;
- area damage по полному лимиту из 160 врагов атомарно очищает spatial index и
  сохраняет generation-safe повторное использование слота;
- saturating add/multiply сохраняют обычные результаты и закрывают обе границы
  `int`.

Фактически выполненные команды и результаты:

- baseline `cmake --preset debug && cmake --build --preset debug && ctest
  --preset debug`: успешно, 1/1 CTest, 0,22 с;
- baseline sanitizer preset: успешно, 1/1 CTest, 1,19 с;
- промежуточная Debug-проверка после исправлений: успешно, 1/1 CTest;
- промежуточная ASan+UBSan-проверка после memory/container изменений:
  успешно, 1/1 CTest, 0,98 с;
- ASan+UBSan-сборка после второго этапа: успешно, 1/1 CTest, 1,13 с;
- Release сначала не смог загрузить отдельный FetchContent-кэш из-за DNS.
  После явного использования уже загруженных закреплённых исходников raylib и
  raygui финальная Release-сборка и тесты завершились успешно: 1/1 CTest,
  0,32 с;
- финальный `git diff --check`: успешно.

## Оставшиеся риски и следующий этап

1. Расширить существующий headless stress test сценариями cascade collapse и
   многократного restart run; измерять tick budgets.
2. Отделять resource transaction от placement side effects только после
   добавления command-level regression tests.
3. Добавить индекс `EntityId -> slot` только после benchmark линейных lookup в
   enemy/tower/cannon paths; generation должен проверяться при каждом lookup.
4. Добавить integration smoke test с невидимым raylib-контекстом для missing
   shaders/models и многократных initialize/shutdown.
5. Профилировать renderer draw calls, animated model updates, shadows, grass и
   particles на фиксированном replay; без измерения оптимизации не вносить.

## Основные изменённые файлы

- `CMakeLists.txt` — новый simulation source и системные include paths raylib;
- `src/core/FixedStep.hpp` — защита accumulator;
- `src/core/SaturatingArithmetic.hpp` — определённая арифметика счётчиков;
- `src/world/SpatialHash.cpp` — безопасная конверсия координат;
- `src/enemies/EnemyCollision.cpp` — безопасная collision-grid конверсия;
- `src/enemies/EnemySystem.cpp` — безопасная query grid и один rebuild на
  area-damage batch;
- `src/graphics/GraphicsResources.cpp` — очистка partial/failed loads;
- `src/game/Simulation.cpp`, `src/game/SimulationWaves.cpp`,
  `src/game/SimulationBuildingCommands.cpp` — выделение lifecycle волн и
  building commands;
- `tests/FixedStepTests.cpp`, `tests/SpatialHashTests.cpp` — граничные тесты.
- `tests/EnemySystemTests.cpp`, `tests/SaturatingArithmeticTests.cpp` — stress
  area damage и арифметические границы.
