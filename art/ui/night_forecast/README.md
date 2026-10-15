# Enemy forecast portraits

Eight final transparent 256×256 HUD icons are in `assets/ui/enemies/`:
basic, fast, heavy, boss, ranged, sapper, flying, splitter.

Generated with the built-in ImageGen tool, one request per enemy. Exact prompt
set: `prompts.json`. Each request used the matching model render from
`references/` as the identity reference and
`assets/ui/building_flamethrower.png` as the existing icon style reference.
The sapper portrait uses the in-game warm orange role color.

Only a lossless PNG resize was applied after generation; genuine alpha is
preserved. Splitling intentionally reuses its parent's portrait in code.
