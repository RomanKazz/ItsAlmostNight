# Лазер: уровни 1–8

Полные варианты `laser_lvl01.glb` … `laser_lvl08.glb` в
`assets/models/buildings/laser_levels/`. Рисовать только один выбранный вариант.
Исходные `laser.glb` и `art/models/laser.blend` сохранены.

## Вид уровней

| Уровень | Изменения |
| --- | --- |
| 1 | Исходный оранжевый лазер с голубой линзой и реактором |
| 2 | Золотая оправа линзы, рамка реактора и пояс подошвы |
| 3 | Золотые опоры, ручка и стойка; оранжевые накладки поверх стойки |
| 4 | Четыре направляющих излучателя и усиливающий обод |
| 5 | Парные конденсаторы с золотыми креплениями и голубыми клеммами |
| 6 | Удлинённые конденсаторы, дополнительные рёбра охлаждения и воротник пьедестала |
| 7 | Защитные направляющие реактора и второй пояс основания |
| 8 | Усиленные ободы реактора и излучателя, красный пояс и знаки ранга на стойке |

Линза, голубой сердечник, направление луча и положение дула одинаковы на всех
уровнях. Конденсаторы сидят в кронштейнах и наклоняются вместе с головой.
Золотые накладки не перекрывают оранжевые панели. Луч и эффекты в GLB не запечены.

## Материалы и геометрия

Неизменённая Kenney `assets/models/buildings/Textures/colormap.png` встроена
в каждый GLB и упакована в Blender. Metallic 0, roughness 1; энергетический
материал сохраняет emissive factor [0.65,0.65,0.65]. Палитра CC0,
геометрия создана для проекта. Исходные группы деталей сохранены; новые детали
тоже имеют именованные vertex groups. На каждом уровне четыре видимых меша,
четыре primitives и два материала. Коллайдер — отдельный пятый mesh.
Количество треугольников, bounds и SHA256 записаны в `manifest.json`.

## Контракт рига

Единицы — метры, footprint 2×2 клетки. Blender +Z вверх, +Y фронт;
GLB +Y вверх, −Z фронт. Root на полу, rotation identity, scale 1.
Координаты ниже — GLB X/Y/Z относительно root:

| Узел | Родитель | Позиция |
| --- | --- | --- |
| `asset_root` | — | (0,0,0) |
| `yaw_pivot` | `asset_root` | (0,0.640,0) |
| `pitch_pivot` | `yaw_pivot` | (0,1.020,0.170) |
| `MUZZLE_01` | `pitch_pivot` | (0,1.020,−0.459) |
| `socket_effect_energy` | `pitch_pivot` | (0,1.020,0.385) |
| `socket_base` | `asset_root` | (0,0,0) |
| `socket_ui_top` | `asset_root` | (0,1.550,0) |

Local pitch относительно yaw — (0,0.380,0.170). Local muzzle относительно
pitch — (0,0,−0.629), оси −Z вперёд и +Y вверх. Yaw 360°, pitch −12°…+20°,
без отдачи. Никаких новых Empty, skeleton или animation clips.

| Render index после удаления collider | Mesh | Движение |
| --- | --- | --- |
| 0 | `base_mesh` | Только установка здания |
| 1 | `energy_mesh` | Pitch, затем yaw |
| 2 | `pitch_mesh` | Pitch, затем yaw |
| 3 | `yaw_mesh` | Только yaw |

`COL_BOX_BODY`: root-level box, 8 уникальных вершин, 12 треугольников,
без `_WALK`. Уровень 1: X/Z ±0.600, Y 0…1.400.
Уровни 2–8: X/Z ±0.700, Y 0…1.500, включая диапазон наведения.
Коллайдер скрыт в студии, экспортируется и удаляется игровым загрузчиком.
Это объём выбора; footprint и блокировка движения задаются игрой отдельно.

## Интеграция

`GraphicsResources::laserTowerModel(int level=1)` выбирает один из восьми
заранее загруженных ресурсов. Уровень ограничивается диапазоном 1…8.
`Renderer::drawLaserTower(..., level)` использует выбранный вариант во всех
существующих вызовах: мир, тени, обводка, размещение, появление и удаление.
Raycast использует выбранный уровень и те же yaw/pitch transforms, что рендер.
UI использует высоту безопасного диапазона наведения. Характеристики и луч
продолжают работать по существующим правилам лазера.

## Проверки

- Raw GLB: имена, родители, TRS, прежние pivots/MUZZLE, порядок primitives,
  normals, UV, встроенная PNG, отсутствие вырожденных и дублированных граней.
- Чистый импорт всех восьми файлов: manifold после временного weld швов,
  положительный объём, пол и размеры, прежнее положение дула.
- Наведение: 1320 поз на уровень; yaw с шагом 1° при крайних/нулевом pitch,
  pitch с шагом 1° при восьми направлениях yaw. Проверяется голова и голубые
  детали против основания/стойки. Исключён только предназначенный контакт оси
  цапф с подшипниками. Декор остаётся внутри collision envelope.
- В исходнике концы осевых болтов различаются на 3 мм при отражении X;
  эта исходная особенность сохранена. Новые детали симметричны.
- `tools/validate_laser_levels_loader.cpp`: настоящие GlbCollisionAsset и
  ModelResource, один box и MUZZLE, четыре видимых меша и встроенные текстуры.
- `tests/LaserGraphicsChecks.hpp`: все восемь ресурсов и их различимость при
  реальном рендере, соответствие дула, наведение, луч и depth occlusion.

Отчёт загрузчика — `loader_report.json`, подробный статус — `manifest.json`.
Интерактивный игровой playtest отдельно не проводился.

## Файлы и воспроизведение

- `laser_levels.blend`: восемь сцен `LASER_L01` … `LASER_L08`.
- `levels_1-8.png`, `family_comparison.png`, `level_N_beauty.png`.
- `level_{1,8}_{front,side,back,top}.png`.
- Генератор — `tools/create_laser_levels.py`, validator — `tools/validate_laser_levels.py`.

```sh
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/create_laser_levels.py
/Applications/Blender.app/Contents/MacOS/Blender --factory-startup --background \
  --python-exit-code 1 --python tools/validate_laser_levels.py
/Applications/Blender.app/Contents/MacOS/Blender --background \
  art/models/laser_levels/laser_levels.blend --python-exit-code 1 \
  --python tools/create_laser_levels.py -- --render
```

Перед пересборкой сохранить отдельную копию ручных правок `.blend`.
Validator пересоздаёт manifest; после него повторить проверку загрузчика и
обновить статус runtime-проверок.
