# Main menu layout

The menu uses a 1920×1080 reference canvas with uniform scaling and centered
letterboxing. Quest summaries and the silver counter attach to the top edge;
the main actions and bottom controls use the centered canvas.

- Four recommended quests occupy the upper left. A summary leads with its
  unlock name or goal, then shows progress and any available reward.
- PLAY is the largest action. Continue is a smaller secondary action above it;
  Unlocks, Quests and Shop follow below without an enclosing panel.
- Settings, language, Credits and Exit form the lower left group.
- Run records occupy a compact panel at the lower right. Labels fit the space
  left by the right-aligned value, including localized labels and large counts.
- Display typography is reserved for the logo and main actions. Body / BodyBold
  handle quest details, utilities, record rows and keyboard hints.

Background selection is available from **Settings → Preset → Choose background**
in the main menu. The existing background editor remains sharp. Normal menu
rendering uses the existing depth-of-field pass plus a translucent dark overlay;
blur requires the Post Processing graphics option to be enabled. Gameplay
rendering and graphics preferences are unchanged.

No public interface or save format changed. `drawMenuButton` is an internal
presentation helper; quest summaries keep the existing click/open contract and
quest IDs. Smoke tests cover English and Russian at 1920×1080, 1280×720 and
1280×800, including the revised summary bounds.
