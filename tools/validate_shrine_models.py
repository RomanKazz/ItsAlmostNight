"""Validate embedded palette, clean imported mesh geometry and glyph clearance."""
from pathlib import Path
import json,struct,hashlib,math
import bpy,bmesh
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/shrines'
results={}
for kind in ('player','building'):
    data=(ROOT/'assets/models/world_landmarks'/f'{kind}_shrine.glb').read_bytes()
    length=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+length]);binary=data[28+length:]
    assert len(doc['meshes'])==2 and len(doc['materials'])==1
    assert not doc.get('extensionsRequired') and all('uri' not in b for b in doc['buffers'])
    view=doc['bufferViews'][doc['images'][0]['bufferView']]
    palette=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
    assert palette==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
    scene=bpy.data.scenes.new(kind+'_SHRINE_CLEAN_VALIDATION');bpy.context.window.scene=scene
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/world_landmarks'/f'{kind}_shrine.glb'))
    meshes=[o for o in scene.objects if o.type=='MESH'];assert len(meshes)==2
    stats=[];trees={}
    for obj in meshes:
        m=obj.data;m.calc_loop_triangles()
        assert all(math.isfinite(x) for v in m.vertices for x in v.co)
        assert m.uv_layers.active and all(0<=x<=1 for uv in m.uv_layers.active.data for x in uv.uv)
        bm=bmesh.new();bm.from_mesh(m);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
        assert all(e.is_manifold and e.is_contiguous for e in bm.edges),obj.name
        assert bm.calc_volume(signed=True)>0,obj.name
        bm.free()
        verts=[obj.matrix_world@v.co for v in m.vertices]
        trees['body' if 'body' in obj.name else 'glyph']=BVHTree.FromPolygons(verts,[tuple(t.vertices) for t in m.loop_triangles],all_triangles=True)
        stats.append({'name':obj.name,'triangles':len(m.loop_triangles),'manifold':True,'outward_winding':True})
    assert not trees['body'].overlap(trees['glyph']),kind+' emblem must clear altar'
    for mesh in doc['meshes']:
        a=doc['accessors'][mesh['primitives'][0]['attributes']['NORMAL']];v=doc['bufferViews'][a['bufferView']]
        begin=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',12)
        normals=[struct.unpack_from('<3f',binary,begin+i*stride) for i in range(a['count'])]
        assert all(all(math.isfinite(x) for x in n) and abs(sum(x*x for x in n)-1)<1e-4 for n in normals)
    coords=[obj.matrix_world@v.co for obj in meshes for v in obj.data.vertices]
    assert abs(min(v.z for v in coords))<1e-6
    results[kind]={'meshes':stats,'palette_byte_identical':True,'unit_normals':True,'glyph_clearance':True,'floor_zero':True,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results))
