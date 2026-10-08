"""Bake matching wooden fence upgrades for straight sections and all junctions.

Run from the repository root. Original level-one meshes/textures are preserved.
Seven vertex-colored overlays per asset; only the current level is drawn.
"""
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset

WOOD = (215, 108, 43)
STEEL = (151, 173, 183)
GOLD = (242, 188, 45)
VARIANTS = {
    'isolated': [(1,0),(-1,0)],
    'end': [(1,0)],
    'corner': [(-1,0),(0,-1)],
    't': [(1,0),(-1,0),(0,-1)],
    'cross': [(1,0),(-1,0),(0,-1),(0,1)],
}

def merge_rotated(destination, source, direction):
    dx, dz = direction
    for values, result in ((source.positions,destination.positions),
                           (source.normals,destination.normals)):
        for i in range(0,len(values),3):
            x,y,z = values[i:i+3]
            result.extend((x*dx-z*dz,y,x*dz+z*dx))
    destination.colors.extend(source.colors)

for name, directions in VARIANTS.items():
    path = Path(f'assets/models/construction/walls/{name}.glb')
    asset = GlbAsset(path)
    posts = [(dx*.436,dz*.436) for dx,dz in directions]
    if name != 'isolated':
        posts.append((0,0))
    sections = [(-.372,.372,(1,0))] if name == 'isolated' else [(.064,.372,d) for d in directions]
    for level in range(2,9):
        g = Geometry()
        # Few large shapes: gray feet, thicker wooden posts and gold bands.
        # They match the core's palette rather than industrial metal hardware.
        for x,z in posts:
            foot_top = .079 if level == 8 else .086
            g.box((x,foot_top/2,z),(.150,foot_top,.150),STEEL,bevel=.08)
            if level >= 4:
                # Binding bands replace the corresponding wood height rather
                # than intersect it: clipped joint faces must not be coplanar.
                wood_bottom = .113 if level == 8 else .098
                intervals = [(wood_bottom,.393),(.437,.452)] if level >= 5 else [(.098,.452)]
                for bottom,top in intervals:
                    g.box((x,(bottom+top)/2,z),(.148,top-bottom,.148),WOOD,bevel=.08)
            if level >= 5:
                g.box((x,.415,z),(.162,.044,.162),GOLD,bevel=.08)
            if level >= 7:
                cap_bottom = .5045 if level == 8 else .485
                g.box((x,(cap_bottom+.545)/2,z),(.170,.545-cap_bottom,.170),STEEL,bevel=.12)
            if level == 8:
                g.box((x,.482,z),(.178,.045,.178),GOLD,bevel=.12)
                g.box((x,.096,z),(.160,.034,.160),GOLD,bevel=.08)
        for start,end,direction in sections:
            arm = Geometry()
            center,length = (start+end)/2,end-start
            if 3 <= level < 6:
                arm.box((center,.163,0),(length,.100,.090),WOOD,bevel=.08)
            if level >= 6:
                # A single wide wooden panel replaces the many small pickets
                # and crossed braces, keeping the silhouette easy to read.
                arm.box((center,.200,0),(length,.185,.096),WOOD,bevel=.08)
            merge_rotated(g,arm,direction)
        # Keep wide binding bands at joint boundaries with 1 mm clearance
        # from the authored wood faces, avoiding coplanar depth flicker.
        for i in range(0,len(g.positions),3):
            g.positions[i] = max(-.501,min(.501,g.positions[i]))
            g.positions[i+2] = max(-.501,min(.501,g.positions[i+2]))
        g.append(asset,f'upgrade_wall_level_{level}')
    print(f'{name}: {asset.write()} bytes, one overlay draw per upgraded section')
