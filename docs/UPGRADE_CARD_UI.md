# Upgrade card menu

Cards retain their current textures and size. Idle rarity glow is half its
previous strength, with stronger glow on hover. The level-up banner is 18%
narrower. Names use a common text size and wrap at word boundaries when needed.
Long unbroken words may shrink to fit. Icons are smaller to leave space for
wrapped titles. The level indicator is compact, without a separate panel;
fixed-power consumables and attack-style choices read ONE-TIME.

Hover highlights changed stats and compares the current value with the result
of the offered card. Numeric predictions include rarity, remaining power caps,
class factors and already accumulated upgrades. Cannon styles additionally show
shell damage and rate/count/cluster tradeoffs. Benefits are green; reductions
and increased enemy difficulty are orange. Contextual effects absent from the
main stats list appear at the bottom of the panel. The table stays in place
when moving between cards.

## UI interfaces

- `app_detail::previewRunUpgrade(snapshot, choiceIndex)` returns a value copy
  with projected HUD values. It does not run simulation commands or mutate the
  source snapshot/world. Non-HUD world views in the result remain unchanged.
- `drawRunUpgradeOverlay` accepts an optional pointer position for deterministic
  UI captures. Ordinary callers keep using the actual mouse position.

The projection is presentation code, not an upgrade command. Future changes to
upgrade formulas should update it alongside simulation rules. The UI smoke test
compares projected values against real upgrade application and renders ordinary
cards, luck hover and cannon hover in English/Russian at 720p and 1080p.
