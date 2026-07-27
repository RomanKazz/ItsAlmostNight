# Architecture

## Границы слоёв

- `app` владеет окном, frame loop, вводом и переходами экранов.
- `game` содержит чистую симуляцию и события.
- `rendering`, `audio`, `ui` позже читают снимок мира и события.

Симуляция не вызывает raylib, не читает устройства ввода, не загружает assets,
не проигрывает звук. Значимые действия поступают командами. Долгоживущие ссылки
на сущности используют `EntityId { index, generation }`.

`App` преобразует события симуляции в ограниченный пул краткоживущих визуальных
эффектов и тряску камеры. Эти эффекты не входят в snapshot и не меняют симуляцию.

## Время

Симуляция работает с фиксированным шагом `1/60` секунды. Frame time
накапливается. За кадр выполняется нужное число simulation ticks; rendering
получает коэффициент интерполяции.

Frame time ограничивается 250 мс, чтобы после breakpoint или подвисания не
возникала бесконтрольная очередь ticks.

## События и команды

Планируемые команды:

- `PlaceBuildingCommand`
- `UpgradeBuildingCommand`
- `RepairBuildingCommand`
- `SellBuildingCommand`
- `StartRunCommand`
- `StartWaveEarlyCommand`
- `FireWeaponCommand`
- `UseConsumableCommand`

Планируемые события:

- `BuildingDestroyedEvent`
- `EnemyKilledEvent`
- `WaveStartedEvent`
- `WaveCompletedEvent`
- `ProjectileHitEvent`
- `ResourceCollectedEvent`
- `CoreDamagedEvent`
- `RunEndedEvent`

## Данные

Баланс хранится в `assets/data/*.json`. Поведение остаётся в C++. Новая внешняя
зависимость требует ADR.

`app` загружает и валидирует `GameBalance`, затем передаёт его в `Simulation`.
Симуляция и игровые системы не выполняют файловый ввод-вывод.
`MapDefinition` отдельно описывает graybox-карту и также загружается слоем `app`.
Одинаковые препятствия карты передаются в коллизию игрока и `FlowField`;
навигация не содержит собственной копии graybox-геометрии.

## Решения

- C++20, raylib 6.0, CMake, Ninja.
- Без ECS и физического движка на старте.
- Flow field для орды; spatial hash для локальных запросов.
- Сначала одиночный Core Loop. Сеть только после Gate A.
