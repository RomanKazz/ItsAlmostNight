# Goleling: летающий танк

`EnemyType::AirTank` (ID 10) использует предоставленный `Goleling_Evolved.gltf`.
Модель скопирована в `assets/models/enemies/ultimate/goleling_evolved.gltf`
без изменения геометрии, палитры или родных анимаций. Буферы и текстура
встроены в glTF; зависимости от Downloads нет.

## Поведение

- Базовые HP 36, скорость 1.8, урон 36 до общих множителей волны/элиты.
  В текущих данных обычный летающий враг имеет HP 5 и скорость 3.2.
- Основная цель — ядро. Близкий игрок не уводит его от базы.
- Приманка перенаправляет танка по обычным правилам маяка; он действительно
  подлетает к ней и атакует. Уже подготовленный удар завершается до
  смены цели. В испытании целью становится игрок.
- Удар головой начинается только в контакте с целью: расстояние считается
  от радиуса тела и радиуса постройки, с допуском 0.25 м.
- Подготовка 0.9 с, восстановление 2 с. При потере цели или выходе из
  контакта удар отменяется. На арене игрок может уйти от зафиксированной
  точки удара. Заморозка приостанавливает подготовку.
- Сопротивление отталкиванию: 25% входного импульса. Ветродуй действует.
  Для заморозки используются правила тяжёлого врага; полной иммунности нет.
- Полёт игнорирует наземные стены и препятствия. Тело отделяется от других
  врагов общим цилиндрическим решателем. Крылья проходные.
- Катапульта, мортира и горизонтальная турель его не выбирают. Остальные
  системы ПВО используют общую классификацию `enemyIsFlying`.

Нативные клипы `Fast_Flying`, `Flying_Idle`, `Headbutt`, `HitReact` подключены
через существующий игровой рендер. При подготовке удара тело пульсирует
тёплым оттенком; отдельных мешей/частиц на кадр не создаётся. Корона уже
есть в модели: элитные аффиксы не добавляют поверх неё вторую.

Масштаб 0.90, смещение корня 0.50 м поверх общего импортного масштаба 0.82.
Цилиндр тела: радиус 1.02 м, высота 1.90 м, низ 1.40 м над поверхностью.
Центр прицеливания — 2.35 м. Отдельный крупный зелёный маркер на миникарте.

## Волны и награды

Танк открывается с пятой ночи. В базовых составах ночей 5 и 6 заменяет
одного тяжёлого и трёх базовых врагов за те же 8 очков угрозы. Общий бюджет
не растёт. Сценарии AirRaid/Siege/RoyalCourt дают ему больший вес;
конкретная доля зависит от состава и акта. После настроенных ночей его
базовое количество растёт на одного за каждые шесть дополнительных ночей.

Меню B: `GOLELING`, с обычным выбором элитных аффиксов. Каталог размещает
десять типов в четыре ряда и сохраняет доступность последнего ряда.
XP 12 до общих множителей, монеты — как у тяжёлого врага.
Старые массивы наград из 9 или 10 ролей читаются без потери значений;
недостающие роли берут награды по умолчанию. Формат сохранения не менялся.

## Иконка

`assets/ui/enemies/goleling_icon.png`, прозрачный PNG 256×256. Создан
встроенным `image_gen` с навыком imagegen по рендеру модели и существующей
иконке Flying. Исходник:
`/Users/roman/.codex/generated_images/01a0fed9-7555-7993-9b69-3f98f89b675f/exec-fbe44abe-b1b5-41d2-92e9-475ad8cdc697.png`.

Промпт:

> Use case: stylized-concept. Generate one small enemy portrait icon for ItsAlmostNight. Image 1 is identity reference: Goleling Evolved, a green horned bat monster with two tall pointed green horns, a small gold crown between them, thick purple brow and purple strip down the face, large white half-lidded sleepy eyes, wide open purple mouth with chunky ivory teeth, short green bat wings with purple membranes. Image 2 is the style reference from the game's existing flying-enemy icon. Match Image 2's centered front-facing compact composition, crisp dark outline, vivid saturated palette, chunky faceted cartoon shading. Preserve the distinctive green face, horn shapes, small crown, purple markings, eyes and toothy mouth of Image 1. This is a sturdy flying tank enemy, goofy but imposing. Head and face large and legible at 36px, wings compact toward the sides. Entire silhouette within canvas with small transparent margin. One icon, no scene, no floor, no text, no frame, no extra crown, no props. Actual transparent background.

## Публичные интерфейсы и проверки

- `EnemyType::AirTank`, расширение `enemyIsFlying`.
- `WaveDefinition::airTank`, `GameBalance::EnemyTypeCount = 11`.
- `EnemyInstance::airTankWindupRemaining/airTankImpactPosition` и общие
  константы `AirTankWindupSeconds/AirTankRecoverySeconds`.
- `EnemyModelVisual::AirTank`, загрузчики модели/анимаций `GraphicsResources`.
- Массивы опыта и UI-иконок расширены до 11 ролей.
- `elite_details::draw` принимает необязательный флаг рисования короны.

`ian_air_tank_tests` проверяет давление на ядро через стены, подготовку и
восстановление, приманку, заморозку, ветродуй, ПВО, аренные атаки/уклонение,
состав волн и старый конфиг XP. `ian_air_tank_graphics_tests` проверяет
реальную модель, взмахи крыльев, удар, прозрачность/размер иконки и каталог
в широком/вертикальном формате. `IAN_CAPTURE_AIR_TANK=<directory>` сохраняет
рендеры модели, удара и каталога; обычный тест не пишет скриншоты в проект.

Числа — стартовый баланс: окончательная сложность проверяется забегами.

Результаты 2026-10-18: полный Debug и полный ASan/UBSan — по 42/42,
без пропущенных графических тестов. Release собран; танк/рендер/пчёлы/
контакты/сценарии ночей — 5/5. Финальное уточнение завершения удара при
импульсе приманки дополнительно прошло боевой тест в Debug, Release и
ASan/UBSan. `git diff --check` и проверка staged diff прошли.
