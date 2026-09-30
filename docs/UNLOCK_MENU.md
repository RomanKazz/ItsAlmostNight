# Unlock collection menu

The main menu's UNLOCKS button opens `drawUnlockMenu`, independently of the
class chooser used to start a run. Its layout uses the same dark panels,
bronze borders, cream labels and secondary font as the quest screen.

## Public interface

`ui/UnlockMenu.hpp` exposes `UnlockCategory`, `UnlockMenuState`,
`UnlockMenuInput`, `UnlockMenuResult`, `unlockMenuPanelBounds`, and
`drawUnlockMenu`. Inputs and the viewport are explicit, allowing the screen to
be rendered and exercised without `App`. `App` handles confirmation audio,
closing, immediate persistence after a successful purchase, and navigation to
`result.questId` in the quest screen.

Tabs show classes, buildings (including modular pieces), weapons, active run
upgrades, items and meta unlocks. Cards use existing UI textures, simple item
emblems, and dark silhouettes for quest-locked entries. Retired run upgrades
and legacy weapon/storage entries are not presented. The footer shows the
selected entry's description, class traits, acquisition route, or linked quest
progress. Arrow keys select cards; Tab/Shift+Tab changes categories; wheel and
the scrollbar handle overflowing grids; Escape or X closes the screen.

## Ownership and purchases

Class gates and meta purchases read `QuestSystem::statuses`. Progress,
eligibility, ownership and silver therefore match the quest screen and the
class chooser. The menu uses `QuestSystem::purchaseUnlock` rather than
modifying silver or ownership directly. It cannot bypass prerequisites or
charge for an already owned unlock. Prices come from the quest catalog.

Other existing content is displayed as available in the collection, with its
run acquisition route explained. This screen introduces no new gameplay gates
or changes to chest/card drop pools.

`meta.refresh` grants one free card reroll at the start of each new run and
unlocks additional reroll purchases in SHOP. `meta.skip` and `meta.banish`
currently persist ownership only; their card behavior has not been implemented.
The UI states this limitation next to these entries. Class portraits and some item
art are represented by emblems until dedicated collection art is available.

## Verification

`ian_unlock_menu_smoke` exercises all categories in English and Russian at
1920x1080, 1280x720 and 1280x800, locked-class quest links, stale selection,
keyboard/category input, bounded scrolling, requirement and wallet checks,
purchase ownership, and prevention of duplicate purchases. Passing a directory
to the executable also exports previews for visual inspection.
