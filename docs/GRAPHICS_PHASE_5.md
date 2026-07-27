# Graphics Phase 5 — Day and Night System

Date: 2026-07-26

## Implemented

- Data-driven `EnvironmentSystem`.
- Dawn, Day, Dusk, and Night profiles loaded from
  `assets/data/environment.json`.
- Cyclic smoothstep interpolation between adjacent profiles.
- Profile-driven sky, horizon, lower sky, fog, celestial direction,
  directional light, hemisphere ambient light, exposure, saturation, and fog
  range.
- Shared celestial direction for visible sun/moon, lighting, and shadows.
- Separate authored night palette and ambient lighting for readable gameplay.
- Debug controls for freezing, scrubbing, profile switching, and restoring
  automatic time.
- Unit coverage for profile anchors, cyclic interpolation, freezing, manual
  override, automatic-mode restoration, and asset loading.

## Visual-time contract

The environment consumes normalized presentation time. It does not advance
waves or own simulation timing.

- Gathering and Build Phase: Day (`0.25`).
- Sunset: Day through Dusk to Night (`0.25`–`0.75`).
- Wave: Night (`0.75`).
- Wave Complete: Night through Dawn to Day (`0.75`–`1.25`, cyclic).
- Paused, Victory, and Defeat: retain the last visual time.

This keeps the visual system downstream of game state while allowing debug
time to run independently.

## Profile data

Each profile controls:

- upper, horizon, lower-sky, and fog colors;
- sun or moon direction and visible-disc color;
- directional and hemisphere ambient colors/intensity;
- day/night material tint;
- fog start and end;
- exposure and saturation;
- night contribution used by ground tint and core glow.

Invalid or missing JSON falls back to compiled defaults.

## Runtime controls

- `Y`: freeze or unfreeze visual time.
- `[` / `]`: move visual time backward or forward.
- `'`: switch instantly to the next authored profile.
- `\`: clear debug overrides and restore automatic visual time.

Current profile, normalized time, freeze state, and automatic/manual mode are
shown in the debug HUD.

## Automated verification

- Debug build: passed.
- Release build: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build: passed.
- Unit tests: 1/1 passed in Debug, Release, and sanitizer configurations.
- Source whitespace check: passed.

The application was not launched and no screenshots were taken, per the
project owner's integration-testing instructions. Visual tuning and driver-side
GLSL verification remain part of manual integration testing.

## Manual integration checklist

1. Start sunset; confirm Day transitions through Dusk before Night.
2. Complete a wave; confirm Night transitions through Dawn before Day.
3. Confirm sky disc, lighting direction, and shadow direction stay aligned.
4. Confirm enemies and buildings remain readable at Night.
5. Press `Y`; confirm the visual transition freezes while gameplay continues.
6. Use `[` and `]`; confirm all environment parameters change together.
7. Press `'`; confirm immediate Dawn/Day/Dusk/Night switching.
8. Press `\`; confirm automatic phase-driven time resumes.
9. Toggle fog, sky, shadows, and world shader; confirm existing fallbacks remain
   functional.

## Known limitations

- Palette, exposure, and fog distances need owner-run visual tuning.
- Celestial paths are authored profile directions, not astronomical movement.
- Stars, clouds, weather, and volumetric atmosphere remain out of scope.
- Ground material variety belongs to Phase 6.
