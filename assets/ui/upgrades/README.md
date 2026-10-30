# Upgrade symbols

Fifteen icons generated with the built-in imagegen tool, then exported as transparent
256x256 RGBA PNGs. All exports preserve aspect ratio and center the silhouette
inside a 216x216 content area. See `prompts.json` for the exact shared style prompt,
subject prompts and original generation paths.

- hammer: attack, attack speed and defense power/rate upgrades
- shrapnel: kill shards, cluster blasts and cluster cannon shells
- luck: upgrade luck
- daylight: extended build phase
- choices: additional choices, lock and reroll effects
- production: faster production

The nine original upgrade assets are replaced at their existing paths in
`assets/ui/upgrade_*.png`, so every existing UI consumer uses the new artwork:

- armor: blue steel shield for recoverable armor
- extra: three brass bullets for extra projectiles
- heart: red heart for maximum health and regeneration
- lifesteal: heart with vampire fangs for blood harvest
- magnet: horseshoe magnet and coin for salvage
- move: leather boot for movement speed
- pierce: bullet through a steel plate for penetration
- ricochet: bullet and bent trajectory for shot bounce
- xp_gain: blue experience book (retained legacy asset)

All icons use the same target black outline weight, broad cel-shaded forms and
minimal detail. No rarity backgrounds or glows are baked into the artwork.

GameUi resolves every offer to a real icon, reusing existing weapon, building,
health, armor and projectile assets where applicable. Offers, the unlock catalog,
the upgrade inventory and the hammer HUD share that resolver and asset lifetime.
Rarity is conveyed by the offer stripe and label, not painted into these icons.
