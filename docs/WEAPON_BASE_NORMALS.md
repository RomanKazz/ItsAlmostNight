# Round weapon pedestal shading

The authored Cannon, Catapult and Crossbow carriages shared 12 bottom-rim
vertices between their cylinder walls and undersides. Their smoothed wall
normals had Y ≈ -0.653 at the lower rim, instead of Y = 0. Interpolation toward
the upper horizontal wall normals created an artificial downward-facing band,
reducing direct and sky lighting over the lower part of the pedestal.

`tools/fix_weapon_base_normals.py` splits those 12 cap vertices per model:

- Side normals are radial and horizontal at both heights, preserving smooth
  shading around the cylinder.
- The underside has flat downward normals and a sharp 90° cap/wall seam.
- UVs, geometry, triangle counts, collisions, sockets, materials and upgrades
  remain intact. Only vertex duplication and normals/index data change.
- Buffer views are repacked in place; rebuilding upgrade overlays remains safe.

The tool is idempotent. `tests/WeaponBaseNormalChecks.hpp` checks all three
imported runtime models, including correct flat underside normals.

Before/after world-shader captures confirm that unlit renders are byte-identical:
the fix changes lighting, preserving the original texture and silhouette.
