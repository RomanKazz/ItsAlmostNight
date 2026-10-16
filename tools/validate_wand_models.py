"""Reimport both wands and validate their packed source and original atlas.

Run in a fresh background Blender. Writes art/models/wands/validation.json.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import bpy
import bmesh

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/wands'
palette=(ROOT/'assets/models/weapons/ice_wand/weapons_bits_texture.png').read_bytes()
report={'palette_sha256':hashlib.sha256(palette).hexdigest(),'models':{}}

def read(path):
    data=path.read_bytes()
    assert data[:4]==b'glTF' and struct.unpack_from('<I',data,8)[0]==len(data)
    size=struct.unpack_from('<I',data,12)[0]
    return json.loads(data[20:20+size]),data[28+size:]

def values(doc,binary,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    n={'VEC2':2,'VEC3':3}[a['type']]
    assert a['componentType']==5126
    start=v.get('byteOffset',0)+a.get('byteOffset',0)
    stride=v.get('byteStride',n*4)
    return [struct.unpack_from('<'+'f'*n,binary,start+i*stride) for i in range(a['count'])]

for element in ('ice','fire'):
    path=ROOT/f'assets/models/weapons/elemental_wands/{element}_wand.glb'
    doc,binary=read(path)
    assert not doc.get('extensionsRequired')
    assert all('uri' not in x for x in doc['buffers']+doc['images'])
    assert len(doc['meshes'])==1 and len(doc['meshes'][0]['primitives'])==1
    assert len(doc['materials'])==1
    image=doc['images'][0];view=doc['bufferViews'][image['bufferView']]
    start=view.get('byteOffset',0)
    assert binary[start:start+view['byteLength']]==palette
    primitive=doc['meshes'][0]['primitives'][0]
    positions=values(doc,binary,primitive['attributes']['POSITION'])
    for attribute in ('POSITION','NORMAL','TEXCOORD_0'):
        data=values(doc,binary,primitive['attributes'][attribute])
        assert all(math.isfinite(x) for v in data for x in v)
        if attribute=='NORMAL':assert all(abs(sum(x*x for x in v)-1)<1e-4 for v in data)
        if attribute=='TEXCOORD_0':assert all(0<x<1 for v in data for x in v)
    lo=[min(v[k] for v in positions) for k in range(3)]
    hi=[max(v[k] for v in positions) for k in range(3)]
    assert abs(lo[1]+.1613469)<1e-5 and .44<hi[1]<.47
    assert hi[0]-lo[0]<.19 and hi[2]-lo[2]<.19
    assert doc['accessors'][primitive['indices']]['count']//3<1000
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(path))
    obj=next(o for o in bpy.context.scene.objects if o.type=='MESH')
    assert abs(obj.matrix_world.determinant()-1)<1e-5
    report['models'][element]={'bounds_game':{'min':lo,'max':hi},'primitives':1,
        'triangles':doc['accessors'][primitive['indices']]['count']//3,'clean_import':'passed'}

bpy.ops.wm.open_mainfile(filepath=str(ART/'elemental_wands.blend'))
assert bpy.context.scene.name=='IAN_ELEMENTAL_WANDS'
for name in ('ICE_WAND','FIRE_WAND'):
    collection=bpy.data.collections[name]
    for obj in collection.objects:
        assert obj.type=='MESH' and obj.location.length<1e-6
        if obj.name.endswith('_grip'):continue
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
        assert all(e.is_manifold for e in bm.edges),obj.name
        assert bm.calc_volume(signed=True)>0,obj.name
        bm.free()
assert all(i.packed_file for i in bpy.data.images if i.source=='FILE')
report['editable_source']='packed, original grip, manifold new parts'
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('WAND_VALIDATION',json.dumps(report,indent=2))
