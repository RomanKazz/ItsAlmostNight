"""Generate rigid axe/pickaxe L2-L8 overlays in their authored mesh space.

L1, embedded tool atlas and swing animation remain byte-identical. Overlays
use the unchanged defense colormap; one extra draw for the chosen level only.
"""
from pathlib import Path
import copy
import hashlib
import json
import struct
from upgrade_mesh import GlbAsset, Geometry

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/models/tool_levels'
GOLD, ORANGE, RED, STEEL, DARK = [(i, 0, 0) for i in range(10, 15)]
SWATCHES = {GOLD: (.65625, .285, .465), ORANGE: (.84375, .275, .475),
            RED: (.96875, .275, .475), STEEL: (.78125, .04, .20),
            DARK: (.59375, .025, .225)}

class PaletteGeometry(Geometry):
    def append(self, asset, name):
        super().append(asset, name)
        primitive = asset.doc['meshes'][-1]['primitives'][0]
        del primitive['attributes']['COLOR_0']
        low, high = self.bounds()
        values = []
        for i, height in enumerate(self.positions[1::3]):
            u, bottom, top = SWATCHES[tuple(self.colors[i*4:i*4+3])]
            fraction = (height-low[1])/max(1e-6, high[1]-low[1])
            values.extend((u, 1-(bottom+(top-bottom)*fraction)))
        asset.binary.extend(b'\0' * (-len(asset.binary)%4))
        offset = len(asset.binary)
        asset.binary.extend(struct.pack('<'+'f'*len(values), *values))
        view = len(asset.doc['bufferViews'])
        asset.doc['bufferViews'].append({'buffer': 0, 'byteOffset': offset,
            'byteLength': len(asset.binary)-offset, 'target': 34962})
        accessor = len(asset.doc['accessors'])
        asset.doc['accessors'].append({'bufferView': view, 'componentType': 5126,
            'count': len(values)//2, 'type': 'VEC2'})
        primitive['attributes']['TEXCOORD_0'] = accessor


def cheeks(g, outline, inset, z, color):
    for side in (-1, 1):
        lo, hi = sorted((side*z, side*(z+.008)))
        g.prism(outline, 2, lo, hi, GOLD)
        lo, hi = sorted((side*(z+.0082), side*(z+.014)))
        g.prism(inset, 2, lo, hi, color)


def axe(g, level):
    # Shaft above the original hand wrap. Nothing added inside the grip.
    g.ring(1, (0,.146,0), .038,.025,.037,GOLD,8)
    g.ring(1, (0,.383,0), .044,.012,.027,GOLD,8)
    if level >= 3:
        cheeks(g, [(-.112,.217),(.078,.215),(.090,.252),(.074,.355),
                   (-.098,.357),(-.147,.308)],
               [(-.104,.232),(.062,.231),(.073,.254),(.059,.338),
                (-.089,.339),(-.128,.303)], .0575, RED if level>=7 else ORANGE)
    if level >= 4:
        # Sharp wedge: steel cutting surface, not an ornamental extra spike.
        extent = .247 if level < 6 else (.263 if level < 8 else .280)
        g.prism([(-extent,.163),(-.195,.197),(-.178,.337),
                 (-extent+.028,.388),(-extent-.002,.341)], 2,-.023,.023,STEEL)
        for side in (-1,1):
            g.ring(2,(.035,.28,side*.078),.026,.004,.011,GOLD,8)
    if level >= 5:
        g.ring(1,(0,.104,0),.037,.025,.024,GOLD,8)
        g.box((.075,.286,0),(.067,.17,.13),ORANGE,.23)
        g.box((.109,.285,0),(.014,.108,.137),GOLD,.18)
    if level >= 6:
        cheeks(g,[(-.155,.200),(-.040,.203),(-.040,.370),(-.157,.380),(-.204,.322)],
               [(-.151,.217),(-.055,.218),(-.055,.355),(-.149,.362),(-.184,.318)],
               .075, RED if level>=7 else ORANGE)
        g.ring(1,(0,.178,0),.047,.025,.026,GOLD,8)
    if level >= 7:
        for side in (-1,1):
            for y in (.249,.283,.317):
                g.box((-.074,y,side*.095),(.059,.012,.009),GOLD,.15)
        g.box((.075,.360,0),(.072,.044,.145),RED,.2)
    if level >= 8:
        g.box((-.037,.395,0),(.195,.031,.105),GOLD,.22)
        g.box((.106,.289,0),(.030,.154,.156),GOLD,.2)
        g.box((-.037,.414,0),(.109,.016,.080),RED,.23)
        for side in (-1,1):
            g.ring(2,(-.12,.28,side*.106),.027,.003,.015,GOLD,8)


def pickaxe(g, level):
    g.ring(1,(0,.383,0),.041,.025,.036,GOLD,8)
    g.ring(1,(0,.574,0),.043,.011,.023,GOLD,8)
    if level >= 3:
        cheeks(g,[(-.082,.461),(.082,.461),(.100,.485),(.073,.555),
                   (-.073,.555),(-.100,.485)],
               [(-.064,.474),(.064,.474),(.081,.488),(.059,.540),
                (-.059,.540),(-.081,.488)], .0528, RED if level>=7 else ORANGE)
    if level >= 4:
        for side in (-1,1):
            # Work points continue the original downward curvature.
            end = .398 if level<6 else (.416 if level<8 else .438)
            poly=[(.232,.462),(.319,.429),(end,.366),(end-.015,.424),(.287,.486),(.228,.488)]
            if side<0:poly=[(-x,y) for x,y in reversed(poly)]
            g.prism(poly,2,-.021,.021,STEEL)
        for side in (-1,1):
            g.ring(2,(0,.506,side*.073),.034,.006,.014,GOLD,8)
    if level >= 5:
        g.ring(1,(0,.317,0),.037,.025,.030,GOLD,8)
        g.box((0,.421,0),(.082,.069,.066),ORANGE,.21)
        g.ring(1,(0,.448,0),.053,.025,.019,GOLD,8)
    if level >= 6:
        for side in (-1,1):
            poly=[(.082,.481),(.259,.435),(.289,.472),(.144,.535),(.078,.552)]
            inset=[(.101,.489),(.244,.452),(.263,.471),(.139,.516),(.098,.532)]
            if side<0:
                poly=[(-x,y) for x,y in reversed(poly)]
                inset=[(-x,y) for x,y in reversed(inset)]
            cheeks(g,poly,inset,.047,RED if level>=7 else ORANGE)
    if level >= 7:
        for side in (-1,1):
            for x in (-.170,0,.170):
                g.box((x,.504-abs(x)*.16,side*.071),(.015,.036,.011),GOLD,.15)
        g.box((0,.559,0),(.129,.034,.112),RED,.23)
    if level >= 8:
        g.box((0,.586,0),(.162,.025,.119),GOLD,.22)
        g.box((0,.604,0),(.094,.015,.086),RED,.23)
        for side in (-1,1):
            # Broad angular crown, visibly stronger while preserving tool silhouette.
            poly=[(.076,.549),(.226,.509),(.244,.529),(.129,.578),(.074,.589)]
            if side<0:poly=[(-x,y) for x,y in reversed(poly)]
            g.prism(poly,2,-.045,.045,GOLD)


def generate():
    ART.mkdir(parents=True,exist_ok=True)
    reference=GlbAsset(ROOT/'assets/models/buildings/cannon.glb')
    image=reference.doc['images'][0]
    view=reference.doc['bufferViews'][image['bufferView']]
    png=reference.binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
    manifest={'palette_sha256':hashlib.sha256(png).hexdigest(),'tools':{}}
    for name, make in [('axe',axe),('pickaxe',pickaxe)]:
        source=ROOT/f'assets/models/tools/{name}.glb'
        asset=GlbAsset(source)
        asset.path=ROOT/f'assets/models/tools/{name}_levels.glb'
        asset.binary.extend(b'\0'*(-len(asset.binary)%4));offset=len(asset.binary)
        asset.binary.extend(png);view_index=len(asset.doc['bufferViews'])
        asset.doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(png)})
        image_index=len(asset.doc['images'])
        asset.doc['images'].append({'name':'original_defense_colormap','mimeType':'image/png','bufferView':view_index})
        texture=len(asset.doc['textures']);asset.doc['textures'].append({'source':image_index,'sampler':asset.doc['textures'][0].get('sampler',0)})
        asset.doc['materials'][asset.upgrade_material]={'name':'tool_upgrade_colormap','pbrMetallicRoughness':{
            'baseColorTexture':{'index':texture},'metallicFactor':0,'roughnessFactor':.7}}
        original=asset.doc['accessors'][asset.doc['meshes'][0]['primitives'][0]['attributes']['POSITION']]
        base_bounds=[original['min'],original['max']]
        levels=[{'level':1,'mesh':0,'triangles':508,'local_bounds':base_bounds,
                 'visible_size':[base_bounds[1][i]-base_bounds[0][i] for i in range(3)]}]
        for level in range(2,9):
            g=PaletteGeometry();make(g,level);g.append(asset,f'upgrade_{name}_L{level}')
            node=len(asset.doc['nodes'])-1
            # Same full hierarchy as L1. raylib bakes these transforms into vertices.
            if name=='pickaxe':
                asset.doc['nodes'][node]['scale']=copy.deepcopy(asset.doc['nodes'][0]['scale'])
                asset.doc['nodes'][1]['children'].append(node)
                asset.doc['scenes'][asset.doc.get('scene',0)]['nodes'].remove(node)
            bounds=g.bounds()
            overall=[[min(bounds[0][i],base_bounds[0][i]) for i in range(3)],
                     [max(bounds[1][i],base_bounds[1][i]) for i in range(3)]]
            levels.append({'level':level,'mesh':level-1,'triangles':len(g.positions)//9,
                'local_bounds':bounds,'visible_size':[overall[1][i]-overall[0][i] for i in range(3)]})
        asset.write()
        manifest['tools'][name]={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'asset':str(asset.path.relative_to(ROOT)), 'levels':levels,
            'draws_per_frame':2,'collider':'unchanged gameplay reach; no model collider needed',
            'hierarchy':asset.doc['nodes'], 'geometry_coordinates':'original rigid tool mesh local space, Y up',
            'grip_and_animation':'original; L1 buffers and authored hierarchy retained'}
    (ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

if __name__=='__main__':generate()
