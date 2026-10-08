"""Bake matching wooden fence upgrades for straight sections and all junctions.

Run from the repository root. Original level-one meshes/textures are preserved.
Seven vertex-colored overlays per asset; only the current level is drawn.
"""
import math
from pathlib import Path

from upgrade_mesh import Geometry, GlbAsset

WOOD = (190, 94, 37)
DARK_WOOD = (124, 60, 28)
STEEL = (130, 152, 164)
DARK_STEEL = (57, 73, 82)
BRASS = (213, 161, 58)
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

def brace(g, start, end, z, width, depth, color, reverse=False):
    low, high = .17,.303
    ya,yb = (high,low) if reverse else (low,high)
    dx,dy = end-start,yb-ya
    length = math.hypot(dx,dy)
    ox,oy = -dy/length*width/2,dx/length*width/2
    polygon = [(start-ox,ya-oy),(end-ox,yb-oy),(end+ox,yb+oy),(start+ox,ya+oy)]
    g.prism(polygon,2,z-depth/2,z+depth/2,color)

for name, directions in VARIANTS.items():
    path = Path(f'assets/models/construction/walls/{name}.glb')
    asset = GlbAsset(path)
    posts = [(dx*.436,dz*.436) for dx,dz in directions]
    if name != 'isolated':
        posts.append((0,0))
    sections = [(-.372,.372,(1,0))] if name == 'isolated' else [(.064,.372,d) for d in directions]
    for level in range(2,9):
        g = Geometry()
        for x,z in posts:
            # Narrow binding bands, then a short iron shoe: wood stays visible.
            for y in (.125,.444):
                g.box((x,y,z),(.144,.026,.144),STEEL,bevel=.08)
            if level >= 5:
                g.box((x,.057,z),(.15,.114,.15),DARK_STEEL,bevel=.08)
                for side in (-1,1):
                    g.box((x,.37,z+side*.074),(.06,.12,.018),STEEL)
                    g.box((x+side*.074,.37,z),(.018,.12,.06),STEEL)
                    g.box((x,.37,z+side*.085),(.027,.027,.009),DARK_STEEL)
                    g.box((x+side*.085,.37,z),(.009,.027,.027),DARK_STEEL)
            if level >= 7:
                g.box((x,.497,z),(.158,.025,.158),DARK_STEEL)
                # Low hipped cap instead of a bulky block or oversized spike.
                corners = [(x-.075,.509,z-.075),(x+.075,.509,z-.075),
                           (x+.075,.509,z+.075),(x-.075,.509,z+.075)]
                tip = (x,.553,z)
                for i in range(4):
                    g.face([corners[i],tip,corners[(i+1)%4]],STEEL)
            if level == 8:
                for side in (-1,1):
                    g.box((x,.444,z+side*.076),(.043,.030,.008),BRASS)
                    g.box((x+side*.076,.444,z),(.008,.030,.043),BRASS)
                g.box((x,.502,z),(.16,.012,.16),BRASS)
        for start,end,direction in sections:
            arm = Geometry()
            center,length = (start+end)/2,end-start
            if level >= 3:
                arm.box((center,.145,0),(length,.07,.085),WOOD)
            if level >= 4:
                for side in (-1,1):
                    brace(arm,start+.015,end-.015,side*.040,.045,.035,DARK_WOOD)
            if level >= 6:
                count = 4 if name == 'isolated' else 2
                for i in range(count):
                    x = start+(i+.5)*length/count
                    picket = [(x-.033,.064),(x+.033,.064),(x+.033,.291),
                              (x+.018,.312),(x-.018,.312),(x-.033,.291)]
                    arm.prism(picket,2,-.035,.035,WOOD)
            if level >= 7:
                for side in (-1,1):
                    arm.box((center,.374,side*.038),(length,.027,.016),DARK_STEEL)
            if level == 8:
                for side in (-1,1):
                    brace(arm,start+.012,end-.012,side*.064,.023,.016,STEEL,True)
            merge_rotated(g,arm,direction)
        # Keep wide binding bands at joint boundaries with 1 mm clearance
        # from the authored wood faces, avoiding coplanar depth flicker.
        for i in range(0,len(g.positions),3):
            g.positions[i] = max(-.501,min(.501,g.positions[i]))
            g.positions[i+2] = max(-.501,min(.501,g.positions[i+2]))
        g.append(asset,f'upgrade_wall_level_{level}')
    print(f'{name}: {asset.write()} bytes, one overlay draw per upgraded section')
