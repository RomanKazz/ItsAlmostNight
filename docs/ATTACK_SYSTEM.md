# Общая система атак защитных сооружений

## Архитектура и результаты аудита

Проект написан на C++20/raylib. Наследуемого базового класса башни нет: данные здания находятся в `BuildingInstance` / `BuildingSystem`, а поведение разделено между `TowerSystem`, `CannonSystem` и `TrapSystem`. `BuildingType::Turret` — арбалет; `GunTurret` — пулемётная турель. Также существуют Cannon, Catapult, SpikeTrap и SlowTrap. Ballista, Tesla и сетевой runtime сейчас отсутствуют; `NETWORK_PROTOCOL.md` описывает будущий протокол.

До миграции `TowerSystem.cpp` сразу наносил урон по выбранным целям и вычислял пробитие лучом. Дополнительные боковые попадания создавались там же. `SimulationTowerCombat.cpp` отдельно рассчитывал мгновенные рикошеты и осколки; `AppEventPresentation.cpp` создавал декоративные стрелы и задерживал представление уже нанесённого урона. Cannon имел отдельную баллистику и ограниченный пул. Карточки определяются в `RunUpgrade.hpp`, применяются в `SimulationProgression.cpp`; уровни зданий и улучшения наковальней хранятся в `BuildingInstance`.

Теперь поток данных выглядит так:

```text
BaseStats(type, level)
 + run/meta/class modifiers
 + building modifiers (anvil, high ground)
 -> resolveBuildingAttackStats -> resolveAttackStats
 -> FinalAttackStats
 -> AttackEvent { sourceId, stats snapshot }
 -> weapon-specific pattern(quantity)
 -> travelling projectile / area / trap instances
 -> collision -> applyAttackHit -> EnemySystem health/armor/knockback
```

`AttackStats.hpp` содержит все общие характеристики: Damage, Attack Speed, Quantity, Projectile Speed, Duration, Size, Bounces, Pierce, Crit Chance/Damage, Knockback, Range, Burst Interval, Spread, Bounce Range, Prediction, Homing и Fan Spread. `AttackEvent` не зависит от физической пули. `AttackProjectileSystem` обслуживает летящие пули и болты; `applyAttackArea` использует тот же hit flow для взрывов и ловушек. Глобального менеджера с логикой каждого оружия нет.

## Формулы и семантика

Для обычного числового стата:

```text
final = base * product(source.multiplier) + sum(source.flat)
interval = max(0.025 sec, base.attackInterval / final.attackSpeed)
```

Поэтому отдельные глобальные, классовые и локальные источники перемножаются. Повторные уровни одной карточки накапливают её power и формируют один процентный источник: например Damage power 3 даёт `1 + 0.12 * 3`. База при покупке карточки не изменяется; повторная загрузка не применяет бонус дважды.

Quantity округляется до целого и не бывает меньше 1. Bounces и Pierce неотрицательны и означают **дополнительные** цели после первого попадания. Bounces=3 позволяет до четырёх последовательных попаданий обычной пули. Для AoE один взрыв может задеть несколько врагов: Bounces управляет дополнительными перелётами снаряда, а не ограничивает число врагов во взрыве.

Pierce расходуется первым: снаряд продолжает полёт через врага. После исчерпания Pierce используется Bounce: выбирается новая допустимая цель в Bounce Range, меняется скорость и продолжается физический полёт. Каждая instance имеет собственные счётчики и историю всех поражённых EntityId. Уже поражённые враги повторно не получают урон от той же пули; поколения EntityId различают повторно использованные сущности.

Crit определяется отдельно при каждом попадании детерминированным roll. Это не miss chance: при реальном контакте урон поступает в существующую систему здоровья/брони. Knockback использует существующее сопротивление врага. Crit Damage имеет минимум 1x, Chance ограничен 0..1. Damage/Speed/Range неотрицательны, Duration/Size/Burst Interval имеют положительные минимумы.

## Поведение оружий и Base Stats

| Оружие | Где настраивается база | Как интерпретируется Quantity |
| --- | --- | --- |
| GunTurret | `TowerSystem::baseStats`, таблицы damage/range/fireInterval в `TowerSystem.cpp` | Количество пуль в очереди |
| Crossbow (`Turret`) | Там же | Одновременный веер болтов через `fanAngleDegrees` |
| Cannon | `CannonSystem::baseStats`, `CannonAttack.hpp` для выбранного стиля | Баллистические снаряды с разнесёнными точками попадания |
| Catapult | `CannonSystem::baseStats`, параметры анимации в `CannonSystem.cpp` | Баллистическая очередь снарядов в одном запуске после windup |
| SpikeTrap | `TrapSystem::baseStats`, level helpers в `TrapSystem.cpp` | Дополнительные импульсы урона в активации |
| SlowTrap | Там же | Quantity неприменим; покрытие меняет только Size |

Уровни 1..8, визуальные модели, классовые особенности, наковальня и артиллерийские специализации сохранены. Выбранный стиль Cannon сначала определяет базовый профиль, затем применяется общая система modifiers. Не применимые к зоне статы Projectile Speed/Bounces/Pierce не заставляют ловушку создавать пули.

Базовая GunTurret выпускает 3 пули с полным уроном 1 за событие каждые 1.2 секунды: raw DPS 2.5, как у прежних Quantity=1 и интервала 0.4 секунды. Интервал события на всех уровнях утраивается относительно прежнего одиночного выстрела, сохраняя raw DPS таблицы уровней. Projectile Speed=22, Duration=3 sec, Spread=2°, Burst Interval=0.08 sec. Crossbow: Speed=14, Spread=1°, Fan Step=5°, прежнее базовое пробитие уровней. Для артиллерии Duration=4 sec; скорость сохраняет прежнюю семантику горизонтальной скорости баллистики.

Cooldown отсчитывается **от начала Attack Event**. Пока gun burst не завершён, следующее событие не начинается. Динамический интервал уплотняет очередь до максимум 60% периода события; увеличение Quantity не ограничивает пользу Attack Speed. Конкурирующих sequences нет. Каждая очередь получает снимок статов и направления, поэтому даже пропажа исходной цели не отменяет уже запланированные пули. Catapult также сохраняет snapshot при начале windup. SpikeTrap учитывает минимальную длительность своей анимации.

Turret наводится на текущую визуальную позицию врага, без идеального предсказания. Направление при запуске фиксировано; индивидуальный небольшой spread изменяет реальную траекторию. Выстрел может промахнуться по исходной цели и попасть в другого допустимого врага. Swept capsule collision предотвращает туннелирование быстрых снарядов. Size масштабирует и визуал, и hit radius (базовые радиусы болта 0.09, пули 0.13). Для артиллерии Size также масштабирует взрыв. Lifetime расходуется непрерывно, включая рикошеты; баллистика интегрируется с гравитацией без телепортации к цели.

Трейл пуль и болтов растёт по накопленному расстоянию полёта с интерполяцией между physics ticks. После bounce расстояние не уменьшается при возвращении к дулу. Пул очищает также текущую и предыдущую длину полёта; Size масштабирует хвост вместе со снарядом.

## Карточки, сохранения и удалённые пути

- Боковые дополнительные instant-hit цели удалены. ID 20 (бывший Twin Batteries / Side Shot) теперь Projectile Count.
- Мгновенная цепочка Ricochet удалена. ID 14 теперь Bounces; каждый снаряд получает собственный счётчик.
- ID 23 (бывший Gun Piercing) теперь универсальный Pierce.
- Добавлены ID 50..55: Projectile Speed, Duration, Size, Crit Chance, Crit Damage, Knockback. Старые индексы не сдвинуты.
- Карточки Quantity/Bounces/Pierce имеют высокую границу прогрессии 1000 вместо прежнего малого ограничения; сама stat infrastructure допускает большие значения.
- Числовые defense bonuses пересчитываются из сохранённых power и исходных meta/class данных. Player Damage/Attack Speed сохраняют своё прежнее отдельное назначение.
- Gun Shrapnel остался особым эффектом убийства, но создаёт физические pooled attack instances со snapshot урона родителя ×0.45. Они используют общий hit flow и сохраняют источник награды.
- Декоративные стрелы, искусственная задержка попаданий и копии расчёта defense damage в presentation удалены из игрового пути. Renderer рисует реальные projectile из simulation snapshot; звук/отдача приходят при запуске, flash/цифры — при контакте.

Формат зданий не изменён. Старые JSON-массивы upgrades короче нового набора читаются с нулевыми значениями новых полей. Ключи заданий `ricochet`, `twin_batteries`, `gun_piercing` специально сохранены для совместимости прогресса; названия в UI обновлены. Runtime projectile, как и раньше у Cannon, не сериализуется при checkpoint. Очереди, windup и снаряды очищаются при завершении волны, reset и начале новой игры. Unity serialized fields/prefabs в проекте отсутствуют.

## Как добавить улучшение

Для отдельного игрового эффекта достаточно создать источник modifiers:

```cpp
AttackModifiers extra;
extra[AttackStat::Quantity].flat = 1.0; // +1 Projectile Count
extra[AttackStat::Bounces].flat = 1.0;  // +1 Bounce
const std::array sources{runModifiers, buildingModifiers, extra};
FinalAttackStats final = resolveAttackStats(baseStats, sources);
```

Обе карточки уже существуют. Их единственные общие bindings находятся в `src/progression/DefenseAttackModifiers.hpp`:

```cpp
AttackUpgradeBinding{RunUpgradeEffect::ProjectileCount, AttackStat::Quantity, 1.0, 0.0},
AttackUpgradeBinding{RunUpgradeEffect::Bounces, AttackStat::Bounces, 1.0, 0.0},
```

Новая stat-карточка добавляется **в конец** стабильного enum и таблицы definitions в `RunUpgrade.hpp`, затем получает binding `{effect, stat, flatPerPower, percentPerPower}`, название/локализацию и UI preview. Реализации оружий менять для уже поддерживаемого стата не нужно. Не хранить `damage *= bonus` в apply/remove upgrade.

## Как подключить новое оружие

1. Задать immutable `AttackStats baseStats(type, level)` и разрешать источники через `resolveBuildingAttackStats`.
2. При запуске создать `AttackEvent{building.id, finalStats(building)}` и выставить cooldown из `event.stats.attackInterval`.
3. В оружии определить pattern: sequence, fan, зоны или несколько целей. Использовать `event.stats.quantity`; не менять глобальный расчёт статов.
4. Для физических атак вызвать общий `AttackProjectileSystem::spawn(source, type, origin, direction, event.stats, surfaceHeight)`. Для тяжёлой баллистики можно использовать Cannon pattern; для зон — `applyAttackArea` / `applyAttackHit`.
5. Передавать фактические launch/hit события presentation и очищать runtime при reset. Для нового `BuildingType` добавить обычную регистрацию строительства/рендера/сохранения и правила допустимых целей.

Пример pattern будущей Ballista может применять малый sequence delay к нескольким heavy bolts и задавать большой базовый Pierce. Tesla может трактовать Quantity как число целей, сохраняя общие damage/crit/knockback и самостоятельно задавая hit presentation. Homing/Prediction предусмотрены внутри stats, но не вынесены в публичную прогрессию.

## Пулы и диагностика

Оба пула используют O(1) свободные слоты и сохраняют high-water allocation. При reuse очищаются config, источник, velocity, history, lifetime, bounces/pierce, цель, Size, crit parameters, muzzle/shrapnel flags; артиллерия также сбрасывает trails/fuse/AoE fields. Callbacks и attached effect objects не хранятся в projectile; presentation получает копии событий, поэтому старые эффекты не переезжают в новую instance.

Страховочная ёмкость каждого пула — **16384**, без малого ограничения Quantity. При заполнении новый spawn отклоняется; `dropped()` считает неудачные acquire attempts, а не все оставшиеся элементы прерванного веера. Reset обнуляет счётчик и возвращает слоты. Итерация пока проходит по high-water storage, а не по отдельному компактному active list.

Карточка выбранного здания показывает реальные resolved stats и сравнение уровней. `Shift+F8` при выбранном здании пишет полный `FinalAttackStats` и счётчик отказов пулов в лог. Форматирование находится в `AttackStatsDebug.hpp`. `SimulationSnapshot::aimedFinalAttackStats` доступен для дополнительных debug панелей.

## Проверка миграции

`ian_attack_tests` проверяет:

- базовую Turret, Damage, Attack Speed, Quantity +1 и повторные добавления;
- комбинации Quantity=3/5/10/30, Speed multiplier=1/2/100, Bounces=0/1/3 и отсутствие наложения burst;
- пять независимых снарядов с двумя bounce: пятнадцать контактов;
- Projectile Speed, медленного/быстрого движущегося врага и физические промахи;
- попадание в другого врага после ухода исходной цели, толпу и swept contact на большой скорости;
- Size как реальный радиус контакта, Pierce + Bounce, crit/knockback и snapshot damage;
- Duration, reuse/reset пула, историю/поколения и насыщение 16384+1;
- уровни здания, наковальню, артиллерийские volley/bounce/pierce, catapult windup snapshot;
- физические осколки с сохранением источника;
- старые стабильные IDs, сохранение/повторную загрузку modifiers и reset нового забега.

`ian_defense_tests` запускает существующие расширенные Tower/Cannon/Trap проверки. Полный `ian_tests` проверяет строительство, здоровье, targeting, прогрессию, save/resume, монеты и задания. Тестовые Simulation fixtures переведены на heap: единая большая функция на macOS переполняла стек до выполнения assert.

`ian_run_upgrade_ui_smoke` создаёт снимки английского/русского UI в семи соотношениях сторон и проверяет preview новых карточек; изображения сохранены вне репозитория в `/tmp/ian-attack-ui`. Ручная длительная оценка баланса и кадрового времени позднего забега остаётся отдельной задачей; автоматические проверки не подменяют playtest.

Итог проверки: `ian_tests`, `ian_attack_tests` и `ian_defense_tests` прошли в Debug, Release и ASan/UBSan (3/3 в каждом прогоне; ошибок санитайзеров нет). Графический upgrade UI smoke также прошёл. `git diff --check` чистый.

Release собран через `cmake --preset release` и `cmake --build --preset release -j 8`. Бинарник: `build/release/its_almost_night`, macOS ARM64; runtime assets обновлены в `build/release/assets`. Запуск из корня проекта: `./build/release/its_almost_night`.

## Баланс и производительность: что наблюдать

- Физические промахи снижают фактический gun DPS по быстрым целям. Projectile Speed, Spread и визуальный hit radius требуют playtest; текущие 22/2° — начальные настройки.
- Дополнительные instances наносят полный damage вместо старых ослабленных боковых попаданий. Bounce больше не использует старое отдельное затухание. Это намеренная смена баланса; редкость и cost новых карточек могут потребовать настройки.
- Burst занимает не более 60% периода события. Увеличение Duration повышает число одновременно живущих пуль; Speed × Duration повышает расстояние полёта; Size усиливает и точность, и AoE.
- Pierce расходуется перед Bounce. Quantity spikes создаёт повторные полные импульсы, а Quantity slow trap неприменим; эти интерпретации надо учитывать при оценке универсальной карточки.
- Пул устраняет постоянное создание/удаление instances, но не делает произвольный build бесплатным: обход high-water слотов, история контактов, spatial queries и AoE в плотной толпе растут вместе с нагрузкой. Существующая логика брони/аур EnemySystem также имеет собственную стоимость.
- Большие залпы создают много hit events, чисел урона, частиц и звуков. Нужен профиль simulation **и** presentation на реальном позднем забеге. Порог 16384 должен быть виден через debug counter.
- Sweep обрабатывает врагов и существующие ограничения допустимых целей. Универсальную коллизию bullets с каждым препятствием мира этот рефактор не добавляет.

## Все изменённые файлы

Список ниже относится к этому рефактору; в рабочем дереве также были сторонние незакоммиченные изменения до начала работы.

- `CMakeLists.txt`
- `assets/data/localization.json`
- `assets/data/meta_quests.json`
- `docs/ATTACK_SYSTEM.md`
- `src/app/App.cpp`
- `src/app/App.hpp`
- `src/app/AppEventPresentation.cpp`
- `src/app/AppInput.cpp`
- `src/app/AppProjectileDrawing.hpp`
- `src/app/AppRunLifecycle.cpp`
- `src/app/AppRunUpgradeOverlay.cpp`
- `src/app/AppUpdate.cpp`
- `src/app/AppWorldRender.cpp`
- `src/buildings/BuildingModifiers.hpp`
- `src/buildings/BuildingStats.hpp`
- `src/buildings/TowerRig.hpp`
- `src/combat/AttackArea.hpp`
- `src/combat/AttackCollision.hpp`
- `src/combat/AttackHit.hpp`
- `src/combat/AttackProjectile.cpp`
- `src/combat/AttackProjectile.hpp`
- `src/combat/AttackStats.hpp`
- `src/combat/AttackStatsDebug.hpp`
- `src/combat/CannonSystem.cpp`
- `src/combat/CannonSystem.hpp`
- `src/combat/DefenseRuntimeSupport.hpp`
- `src/combat/DefenseTargeting.hpp`
- `src/combat/ProjectilePool.hpp`
- `src/combat/TowerSystem.cpp`
- `src/combat/TowerSystem.hpp`
- `src/combat/TrapSystem.cpp`
- `src/combat/TrapSystem.hpp`
- `src/enemies/EnemyCombat.cpp`
- `src/enemies/EnemySystem.hpp`
- `src/game/GameEvent.hpp`
- `src/game/QuestGameplayTracker.cpp`
- `src/game/Simulation.cpp`
- `src/game/Simulation.hpp`
- `src/game/SimulationCombat.cpp`
- `src/game/SimulationDefenses.cpp`
- `src/game/SimulationProgression.cpp`
- `src/game/SimulationSnapshot.cpp`
- `src/game/SimulationTowerCombat.cpp`
- `src/game/SimulationWaves.cpp`
- `src/graphics/RendererBuildingModels.cpp`
- `src/progression/DefenseAttackModifiers.hpp`
- `src/progression/QuestCatalog.cpp`
- `src/progression/RunUpgrade.hpp`
- `src/ui/GameUi.cpp`
- `src/ui/HudBuildingContextCard.cpp`
- `src/ui/QuestMenu.cpp`
- `src/ui/ShopMenu.cpp`
- `tests/AttackSystemTests.cpp`
- `tests/CannonSystemTests.cpp`
- `tests/DefenseSystemTests.cpp`
- `tests/RunUpgradeOverlaySmokeTests.cpp`
- `tests/RunUpgradeTests.cpp`
- `tests/SimulationTests.cpp`
- `tests/TowerSystemTests.cpp`

## Второй gameplay/balance pass (2026-10-10)

`AttackStats`, `FinalAttackStats`, физические projectiles и общий hit flow сохранены.

- Attack Speed управляет частотой начала Attack Events.
- Quantity задаёт число Attack Instances внутри Event.
- Burst Interval задаёт базовое расстояние во времени между Instances. Только GunTurret использует динамический burst.

```cpp
// AttackPattern.hpp, по снимку FinalAttackStats текущего события
maxDuration = attackInterval * 0.6;
effectiveBurstInterval = min(
    burstInterval / sqrt(max(1.0, attackSpeed)),
    maxDuration / max(1, quantity - 1));
totalBurstDuration = effectiveBurstInterval * (quantity - 1);
```

Слабое sqrt scaling не меняет урон, Quantity или число событий. Один runtime хранит одну sequence и её интервал; исчерпание cooldown переносит дробную часть шага в следующий дедлайн. Цикл выпускает все Instances, срок которых наступил за physics step; не ограничивает плотные очереди одной пулей за tick. Spread остаётся независимым для каждой пули и не зависит от Quantity. Изменение статов во время очереди применяется к следующему Event.

### GunTurret уровня 1

| Параметр | Значение |
|---|---:|
| Damage per bullet | 1 внутренняя единица / 10 в HUD |
| Quantity | 3 |
| Attack Speed | 1x |
| Attack Interval | 1.2 s |
| Base / Effective Burst Interval | 0.08 / 0.08 s |
| Total Burst Duration | 0.16 s |
| Projectile Speed | 22 |
| Duration / Size | 3 s / 1x |
| Bounces / Pierce | 0 / 0 |
| Crit Chance / Crit Damage | 0% / 2x |
| Spread | 2° |
| Raw DPS: старый / новый | 2.5 / 2.5 |

Пули базовой очереди выходят на 0 / 0.08 / 0.16 s. Следующая очередь начинается на 1.2 s; пауза после последней пули — 1.04 s. Здесь нет учёта брони, физических промахов или отсутствия цели. Глобальные, классовые и локальные modifiers продолжают перемножаться.

### Timing

| Quantity | Attack Speed | Attack Interval, s | Effective Burst Interval, s | Burst Duration, s | Bullets/s |
|---:|---:|---:|---:|---:|---:|
| 3 | 1x | 1.2 | 0.080000 | 0.160 | 2.500 |
| 10 | 1x | 1.2 | 0.080000 | 0.720 | 8.333 |
| 30 | 1x | 1.2 | 0.024828 | 0.720 | 25.000 |
| 10 | 2x | 0.6 | 0.040000 | 0.360 | 16.667 |
| 30 | 5x | 0.24 | 0.004966 | 0.144 | 125.000 |

Physics ticks квантуют запуск до ближайшего шага. Автотесты измеряют фактическую частоту на 60/120 Hz за 30 секунд. Минимальный Attack Interval 0.025 s сохранён.

### Compatibility

`AttackStatCompatibility.hpp` содержит constexpr metadata всех шести defense attack types. `supportsAttackStat(type, stat)` и `defenseUpgradeAppliesToAny(effect, buildings)` позволяют UI проверять карточку без switch по оружиям. Выбор карточек пока не менялся: построить первое подходящее оружие можно позже. SlowTrap нормализует неприменимые modifiers в нейтральные значения; reactive shock остаётся отдельной классовой способностью с прежним scaling.

| Оружие | Damage | Attack Speed | Quantity | Projectile Speed | Duration | Size | Bounces | Pierce | Crit | Knockback |
|---|---|---|---|---|---|---|---|---|---|---|
| GunTurret | Да | Да | Да | Да | Да | Да | Да | Да | Да | Да |
| Crossbow | Да | Да | Да | Да | Да | Да | Да | Да | Да | Да |
| Cannon | Да | Да | Да | Да | Да | Да | Да | Да | Да | Да |
| Catapult | Да | Да | Да | Да | Да | Да | Да | Да | Да | Да |
| SpikeTrap | Да | Да | Да | Нет | Да | Да | Нет | Нет | Да | Да |
| SlowTrap | Нет | Да | Нет | Нет | Да | Да | Нет | Нет | Нет | Нет |

SlowTrap Attack Speed влияет на повторную активацию замедления; Duration — на время дебаффа. SpikeTrap Duration — время активации/анимации и минимальный cooldown. Поэтому Attack Speed шипов ограничивается длительностью анимации, как раньше. Шипы сохраняют полные area pulses на одной активации: при Quantity=3 три контакта по 100%, каждый с отдельным crit roll. Это такой же линейный множитель по Quantity, как у projectiles, хотя AoE усиливает его против толпы. Добавлен небольшой базовый Knockback=0.4, чтобы процентная карточка имела эффект; shared hit flow уже поддерживал импульс.

### Presentation и нагрузка

Gun sound имеет общий cooldown 0.045 s; несколько запусков за кадр не перезапускают одну громкую voice. Визуальные projectile и muzzle flashes не прореживаются. Pool capacity 16384, counters, lifetime, history, reuse, save IDs не менялись.

Новый stress test использует Quantity=30, Attack Speed=5x, Bounces=5, Duration=5x, большую долю физических промахов. Он проверяет число активных Instances, стабильный high-water storage, нулевые dropped instances, reuse после очистки, максимум 5 spawn за 60 Hz frame. Замер: 1878 активных, 1881 слот, максимум 4 spawn/frame. Отдельная Quantity=1000003 проверка доводит пул до 16384, получает один observable drop и отменяет остаток текущей sequence, не перебирая миллион отказов. Остальные тесты сохраняют реальные bounced/pierced hits, crit, скорость против движущихся целей, save/load/reset.

Playtest: проверить длинную базовую паузу 1.04 s, feel sqrt scaling, голоса нескольких турелей и силу полных SpikeTrap pulses против плотной толпы. Raw DPS сохранён, но распределение урона во времени меняется; избыточный урон по слабой цели может повысить потери. Автотесты не заменяют оценку feel.

Файлы второго pass:

- `src/combat/AttackPattern.hpp` (новый helper расчёта burst timing)
- `src/combat/AttackStatCompatibility.hpp` (новая таблица/API совместимости)
- `src/combat/AttackStats.hpp` (комментарии семантики)
- `src/combat/AttackStatsDebug.hpp`
- `src/combat/TowerSystem.cpp`, `src/combat/TowerSystem.hpp`
- `src/combat/TrapSystem.cpp`
- `src/progression/DefenseAttackModifiers.hpp`
- `src/audio/AudioSystem.cpp`, `src/audio/AudioSystem.hpp`
- `src/app/AppInput.cpp`
- `tests/AttackSystemTests.cpp`, `tests/TowerSystemTests.cpp`
- `docs/ATTACK_SYSTEM.md`

Проверка второго pass: полный Debug 16/16; полный ASan/UBSan 16/16. После последней saturation/spread проверки attack/defense цели пересобраны и повторно прошли в Debug и ASan/UBSan (2/2). Release пересобран, attack/defense/graphics/audio 4/4. Compiler warnings/errors отсутствуют; `git diff --check` чистый. Ручной gameplay playtest не выполнялся.

Коммиты этой работы: `c176e71` — аналитический tactical sector; `30726ba` — новые helpers burst timing/compatibility. Остальные интеграционные изменения оставлены поверх существующего незакоммиченного attack refactor; чужой рефактор и несвязанные WIP файлы не добавлялись в коммиты.
