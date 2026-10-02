# Main menu layout

The menu uses a 1920×1080 reference canvas with uniform scaling and centered
letterboxing. Quest summaries and the silver counter attach to the top edge;
the main actions and bottom controls use the centered canvas.

- Four recommended quests occupy the upper left. A summary leads with its
  unlock name or goal, then shows progress and any available reward.
- The Figma frame `695:2` in file `Cf7YU7vDqFoeyjEQdUt3og` supplies the logo
  and gray stone button artwork. These are local, unmodified transparent PNGs
  in `assets/ui/main_menu/`, loaded and released with the other GameUi textures.
- The logo occupies (618, 24, 684, 235). PLAY, Unlocks, Quests and Shop use
  equal 356.48×101.76 buttons, starting at (781.56, 414.38), with 12px gaps.
  Continue is a smaller secondary action above PLAY when a saved run exists.
- Settings, Credits and Exit form the lower left group at (39, 785), using
  285.184×81.408 buttons and 9.6px gaps. Language selection remains in Settings.
- Run records occupy a compact panel at the lower right. Labels fit the space
  left by the right-aligned value, including localized labels and large counts.
- CIKO display typography matches the Figma button labels: 56px for main
  actions and 44.8px for utilities, with cream fill and dark outline/shadow.
  Body / BodyBold handle quest details, record rows and keyboard hints.
- Hover adds a subtle warm highlight, pressing darkens the original artwork
  and offsets it by 2px. Existing release-to-activate behavior is retained.

Background selection is available from **Settings → Preset → Choose background**
in the main menu. The existing background editor remains sharp. Normal menu
rendering uses the existing depth-of-field pass plus a translucent dark overlay;
blur requires the Post Processing graphics option to be enabled. Gameplay
rendering and graphics preferences are unchanged.

GameUi exposes two texture accessors for the main menu artwork; save formats
are unchanged. `drawMainMenuButton` is an internal presentation helper, while
`drawMenuButton` continues to serve existing subpages. Quest summaries keep
the existing click/open contract and quest IDs. Visual review covers English
and Russian at 1920×1080, 1280×720 and 1280×800.
