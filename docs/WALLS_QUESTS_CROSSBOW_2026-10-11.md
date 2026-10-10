# Wall construction prices, quest notifications and crossbow cost

## Result

- Wall construction surcharge is now `floor(baseResourceTotal * existingWallCount / 100)`, added to the primary resource. For the default ten-wood wall, the next wall costs 10 / 11 / 12 / 14 / 20 wood after 0 / 10 / 20 / 40 / 100 existing walls; previously it cost 10 / 20 / 30 / 50 / 110. Destroyed walls stop contributing. Other building types retain the existing per-copy pricing model. Wall upgrade prices are unchanged.
- Crossbow (`BuildingType::Turret`) construction costs 25 wood, 15 stone and 5 crystals. Runtime JSON and fallback defaults agree. Normal placement rejects four crystals and charges five on success. Its existing upgrade formula also includes the new crystal construction component: crystal prices for transitions to levels 2–8 are 10, 15, 25, 35, 50, 65, 85.
- The main-menu Quests button and all nine quest-category tabs show a coral circular badge for completed, unclaimed silver rewards. The All badge counts every category. Claiming reduces counts on the next draw; zero-count badges disappear. Immediate unlock-only quests and removed catalog entries are excluded. Counts above 99 display `99+`. Badges use the game display font and dark outline.

## Public interfaces

`QuestCategoryProgress` adds `unclaimedRewards`. `QuestSystem::categoryProgress` accepts an optional category, defaulting to the entire catalog; existing category calls still work. `MainMenuHomeState` adds `unclaimedQuestRewards`. The shared `drawUiNotificationBadge` draws the count. Save formats and stable IDs are unchanged.

## Changed working-tree files

- `assets/data/buildings.json`, `src/game/GameBalance.cpp`: crossbow construction price.
- `src/buildings/BuildingSystem.cpp`: wall construction inflation.
- `src/progression/QuestSystem.hpp`, `src/progression/QuestSystem.cpp`: category and whole-catalog reward counts.
- `src/ui/UiNotificationBadge.hpp`, `src/ui/QuestMenu.cpp`: badges and category label spacing.
- `src/ui/MainMenuHome.hpp`, `src/ui/MainMenuHome.cpp`, `src/app/AppMainMenu.cpp`: home badge and live count integration.
- `tests/GameBalanceTests.cpp`, `tests/BuildingSystemTests.cpp`, `tests/TowerSystemTests.cpp`, `tests/QuestSystemTests.cpp`, `tests/MainMenuHomeSmokeTests.cpp`: balance regressions, insufficient-resource placement, forty-wall pricing/charges/destruction, reward count/claim rules, and home preview state.

## Verification

- Debug configure/build succeeded without new compiler warnings.
- Full Debug CTest: 16/17 passed. Existing unrelated `ian_run_menus_smoke` failure: `native window retains upgrade names after the SDF cache warms under scissor`. This same failure was recorded before this task.
- Release configure/build succeeded, including the game executable and copied crossbow-price asset.
- Release `ian_tests`, `ian_main_menu_home_smoke`, `ian_quest_menu_smoke`: 3/3 passed.
- Quest and home preview smoke tests exported English/Russian layouts; inspected the 1920×1080 Russian screenshots. Labels and badges do not overlap. Preview files and logs are under `/tmp/ian-walls-quests-2026-10-11/` and are not committed.
- Working-tree and staged `git diff --check` passed.

## Commit scope

The repository already contained a large unfinished UI/gameplay refactor. Only the relevant construction-cost function, category badge edits, fallback/data price, reward-count API, regression tests and new badge helper are staged. The existing `MainMenuHome.*` and its smoke-test source were already untracked; their badge additions and the integration in the already-modified `AppMainMenu.cpp` remain in that working tree rather than committing those unrelated menu implementations wholesale. The tested Release includes these additions. This scoped commit is not a standalone snapshot of the pre-existing refactor.
