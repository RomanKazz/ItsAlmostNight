"""Bake palette-textured cannon L2-L8 overlays, preserving the original rig.

Two draws per upgraded cannon: yaw carriage + pitched barrel. Embedded
Kenney PNG and the two base render meshes are retained byte for byte.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
from upgrade_mesh import Geometry, GlbAsset

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/cannon_levels'
ART.mkdir(parents=True, exist_ok=True)
asset = GlbAsset(ROOT / 'assets/models/buildings/cannon.glb')
# Correct the historical pivot below the barrel while retaining neutral geometry,
# collider and muzzle positions. The runtime uses this same barrel axis.
pitch_node = next(n for n in asset.doc['nodes'] if n.get('name','').startswith('pitch_pivot'))
barrel_node = next(n for n in asset.doc['nodes'] if n.get('name','').startswith('barrel') and 'mesh' in n)
old_height = pitch_node['translation'][1] + barrel_node['translation'][1]
pitch_node['translation'][1] = .7001184225082397
barrel_node['translation'][1] = old_height - pitch_node['translation'][1]
# The overlay uses the exact same palette texture, not approximate vertex colors.
asset.doc['materials'][asset.upgrade_material] = {
    'name': 'cannon_upgrade_colormap', 'doubleSided': True,
    'pbrMetallicRoughness': {'baseColorTexture': {'index': 0},
                           'metallicFactor': 0, 'roughnessFactor': 1}}
GOLD, ORANGE, DARK, RED = (10,0,0), (11,0,0), (12,0,0), (13,0,0)
SWATCHES = {GOLD: (.65625,.285,.465), ORANGE: (.84375,.275,.475),
            DARK: (.59375,.025,.225), RED: (.96875,.275,.475)}
BARREL_Y = .7001184225082397

class PaletteGeometry(Geometry):
    def append(self, asset, name):
        super().append(asset, name)
        primitive = asset.doc['meshes'][-1]['primitives'][0]
        del primitive['attributes']['COLOR_0']
        values = []
        low, high = self.bounds()
        for i in range(len(self.positions)//3):
            swatch = SWATCHES[tuple(self.colors[i*4:i*4+3])]
            height = self.positions[i*3+1]
            shade = max(0, min(1, (height-low[1])/max(1e-6,high[1]-low[1])))
            values.extend((swatch[0], 1-(swatch[1]+(swatch[2]-swatch[1])*shade)))
        binary = asset.binary
        binary.extend(b'\0' * (-len(binary)%4))
        offset = len(binary)
        binary.extend(struct.pack('<'+'f'*len(values), *values))
        view = len(asset.doc['bufferViews'])
        asset.doc['bufferViews'].append({'buffer':0,'byteOffset':offset,
            'byteLength':len(binary)-offset,'target':34962})
        accessor = len(asset.doc['accessors'])
        asset.doc['accessors'].append({'bufferView':view,'componentType':5126,
            'count':len(values)//2,'type':'VEC2'})
        primitive['attributes']['TEXCOORD_0'] = accessor

# Eight-sided structural plates echo the core buttresses and flamethrower shoulders.
def side_plate(part, level):
    outline = [(.225,-.355),(.225,.32),(.66,.32),(.835,.09),
               (.775,-.22),(.55,-.39)]
    inset = [(.27,-.303),(.27,.264),(.635,.264),(.774,.079),
             (.723,-.174),(.538,-.330)]
    for side in (-1,1):
        lo,hi = sorted((side*.474,side*.633))
        part.prism(outline,0,lo,hi,GOLD)
        lo,hi = sorted((side*.634,side*.655))
        part.prism(inset,0,lo,hi,RED if level>=7 else ORANGE)
        if level>=4:
            # Trunnion hub and thick nut; mounts stay safely outside the barrel.
            part.ring(0,(side*.668,.522,-.015),.115,.035,.028,DARK,8)
            part.ring(0,(side*.691,.522,-.015),.079,.015,.025,GOLD,8)
        if level>=7:
            # Three large rank marks, readable at normal game distance.
            for y,width in [(.335,.21),(.390,.16),(.445,.11)]:
                part.box((side*.676,y,.17),(.027,.025,width),GOLD,.1)

bounds = []
manifest_levels = []
for level in range(2,9):
    base,barrel = PaletteGeometry(),PaletteGeometry()
    # L2: golden foot belt + robust receiver collar.
    base.ring(1,(0,.105,0),.627,.572,.078,GOLD)
    barrel.ring(2,(0,BARREL_Y,.260),.318,.284,.070,GOLD)
    if level>=3:
        side_plate(base,level)
    if level>=4:
        # Collar remains behind the original muzzle plane; the shot socket is unchanged.
        radius = .333 if level<6 else (.368 if level<8 else .388)
        barrel.ring(2,(0,BARREL_Y,-.538),radius,.230,.076,GOLD)
    if level>=5:
        # Orange armored breech between gold bands, with a broad dark rear cap.
        barrel.ring(2,(0,BARREL_Y,.115),.329,.296,.212,ORANGE)
        barrel.ring(2,(0,BARREL_Y,-.004),.341,.293,.030,GOLD)
        base.ring(1,(0,.215,0),.645,.558,.045,GOLD)
        # Four chunky braces sit on the foot, outside the pitch envelope.
        for x,z in [(-.50,-.36),(.50,-.36),(-.50,.36),(.50,.36)]:
            base.box((x,.265,z),(.15,.085,.16),GOLD,.23)
    if level>=6:
        # A contrasting muzzle shoulder follows the barrel, not the carriage.
        barrel.ring(2,(0,BARREL_Y,-.458),.308,.239,.064,RED)
        for side in (-1,1):
            base.box((side*.692,.31,-.02),(.11,.10,.50),GOLD,.23)
    if level>=7:
        # Gold rails link the receiver bands and leave the dark tube readable.
        for angle in (math.pi/4,3*math.pi/4,5*math.pi/4,7*math.pi/4):
            x,y = .326*math.cos(angle),BARREL_Y+.326*math.sin(angle)
            barrel.box((x,y,.115),(.048,.048,.27),GOLD,.15)
        barrel.ring(2,(0,BARREL_Y,.310),.295,.280,.030,DARK)
    if level==8:
        # Final: massive flared muzzle + a stepped golden breech, no extra barrel.
        barrel.ring(2,(0,BARREL_Y,-.510),.357,.277,.030,RED)
        barrel.ring(2,(0,BARREL_Y,.275),.357,.296,.033,GOLD)
        base.ring(1,(0,.172,0),.659,.595,.033,RED)
        for side in (-1,1):
            # Shoulder plate perched on the side shield, away from moving metal.
            base.box((side*.586,.848,.08),(.22,.095,.28),GOLD,.25)
    for part,suffix in ((base,'base'),(barrel,'barrel')):
        bounds.append(part.bounds())
        part.append(asset,f'upgrade_cannon_l{level}_{suffix}')
    manifest_levels.append({'level':level,'overlay_triangles':(len(base.positions)+len(barrel.positions))//9,
        'yaw_bounds':base.bounds(),'pitch_bounds':barrel.bounds()})

total = asset.write()
def authored_bounds(mesh_index, offset=(0,0,0)):
    accessor = asset.doc['accessors'][asset.doc['meshes'][mesh_index]['primitives'][0]['attributes']['POSITION']]
    return [[accessor[end][i]+offset[i] for i in range(3)] for end in ('min','max')]
def union_bounds(parts):
    return [[min(part[0][i] for part in parts) for i in range(3)],
            [max(part[1][i] for part in parts) for i in range(3)]]
base_bounds = union_bounds([authored_bounds(1,(0,BARREL_Y,.005820222198963165)),authored_bounds(2)])
for entry in manifest_levels:
    entry['visible_bounds_glb'] = union_bounds([base_bounds,entry['yaw_bounds'],entry['pitch_bounds']])
    entry['total_visible_triangles'] = 400+entry['overlay_triangles']
manifest = {'levels':manifest_levels,'mode':'base L1 plus one complete overlay pair for L2-L8',
    'render_mesh_order':['original pitch barrel','original yaw carriage']+
        [f'L{l} {role}' for l in range(2,9) for role in ('yaw','pitch')],
    'runtime_meshes':16,'additional_draws':2,'footprint_m':[2,2],
    'level_1_visible_bounds_glb':base_bounds,
    'collision_bounds_glb':authored_bounds(0),
    'runtime_glb':'assets/models/buildings/cannon.glb',
    'editable_source':'art/models/cannon_levels/cannon_levels.blend',
    'overlay_nodes_glb':'root-level identity transforms; runtime selects one yaw/pitch pair',
    'pitch_pivot_glb':[0,.7001184225082397,0],
    'muzzle_glb':[0,.7002042531967163,-.5782914683222771],
    'muzzle_axes_glb':{'forward':[0,0,-1],'up':[0,1,0]},
    'palette_sha256':hashlib.sha256((ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()).hexdigest(),
    'gold_swatch_blender_uv':[.65625,.285,.465],
    'runtime_bytes':total,'native_playtest':False,'loader_verified':False,
    'license':'Kenney original asset and palette CC0; project-authored upgrade geometry'}
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
def vec(v):
    return '{' + ', '.join(f'{x:.6f}F' for x in v) + '}'

header = '''#pragma once

#include "buildings/CannonRig.hpp"
#include <raymath.h>

#include <algorithm>
#include <array>

namespace ian::cannon_model_detail {

// Generated by tools/generate_cannon_upgrades.py, after collider removal.
// Original pitched barrel/yaw-only carriage: 0/1; two overlays per level.
inline constexpr int MeshCount = 16;
inline constexpr std::array<BoundingBox, 14> UpgradeBounds{{
'''
header += ''.join('    {' + vec(a) + ', ' + vec(b) + '},\n' for a,b in bounds)
header += '''}};

[[nodiscard]] inline Matrix pitchTransform(float pitchRadians) {
    return MatrixMultiply(MatrixMultiply(
        MatrixTranslate(0, -static_cast<float>(CannonPitchPivotY), 0),
        MatrixRotateX(pitchRadians)),
        MatrixTranslate(0, static_cast<float>(CannonPitchPivotY), 0));
}

template <typename Emit>
void forEachUpgradePart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) return;
    const int base = 2 + (level - 2)*2;
    emit(base, false);
    emit(base + 1, true);
}

[[nodiscard]] inline BoundingBox partBounds(int meshIndex) {
    return UpgradeBounds[static_cast<std::size_t>(meshIndex - 2)];
}

} // namespace ian::cannon_model_detail
'''
(ROOT / 'src/graphics/CannonModel.hpp').write_text(header)
print(f'Baked seven cannon levels: {total} bytes, two extra draws per cannon.')
