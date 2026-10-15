"""Validate turret GLB preservation, colormap, editable rig and complete yaw."""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/gun_turret_levels'

def read(path):
    data=path.read_bytes();n=struct.unpack_from('<I',data,12)[0]
    assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data))
    return json.loads(data[20:20+n]),data[28+n:]
doc,binary=read(ROOT/'assets/models/buildings/gun_turret.glb');base,base_binary=read(ART/'base_reference.glb')
manifest=json.loads((ART/'manifest.json').read_text())
assert hashlib.sha256((ART/'base_reference.glb').read_bytes()).hexdigest()==manifest['base_reference_sha256']
assert len(doc['meshes'])==17 and len(doc['materials'])==1
assert doc['meshes'][:3]==base['meshes'] and doc['nodes'][:len(base['nodes'])]==base['nodes']
assert doc['materials']==base['materials']
assert binary[:base['buffers'][0]['byteLength']]==base_binary[:base['buffers'][0]['byteLength']]
assert doc['accessors'][:len(base['accessors'])]==base['accessors']
assert doc['bufferViews'][:len(base['bufferViews'])]==base['bufferViews']
assert [n['mesh'] for n in doc['nodes'] if 'mesh' in n]==list(range(17))
assert not doc.get('extensionsRequired') and not doc.get('animations') and not doc.get('skins')
assert all('uri' not in entry for entry in doc['buffers']+doc['images'])
view=doc['bufferViews'][doc['images'][0]['bufferView']];start=view.get('byteOffset',0)
assert binary[start:start+view['byteLength']]==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
scene=bpy.data.scenes.new('TURRET_ROUND_TRIP');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings/gun_turret.glb'));bpy.context.view_layer.update()
objects={o.name:o for o in scene.objects if o.type=='MESH'}
checks=[]
for name,obj in objects.items():
    if not name.startswith('upgrade_'):continue
    mesh=obj.data;mesh.calc_loop_triangles()
    assert all(math.isfinite(c) for v in mesh.vertices for c in v.co),name
    assert mesh.uv_layers.active and all(0<=c<=1 for loop in mesh.uv_layers.active.data for c in loop.uv),name
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(edge.is_manifold and edge.is_contiguous for edge in bm.edges),name
    assert bm.calc_volume(signed=True)>0,name;bm.free()
    raw=next(m for m in doc['meshes'] if m.get('name')==name)['primitives'][0]
    a=doc['accessors'][raw['attributes']['NORMAL']];v=doc['bufferViews'][a['bufferView']]
    offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',12)
    for i in range(a['count']):
        normal=struct.unpack_from('<3f',binary,offset+i*stride)
        assert all(math.isfinite(c) for c in normal) and abs(sum(c*c for c in normal)-1)<1e-4,name
    checks.append({'mesh':name,'triangles':len(mesh.loop_triangles),'finite_normals_uv_manifold':True})

def geometry(items,pose=Matrix.Identity(4)):
    vertices=[];triangles=[]
    for obj in items:
        obj.data.calc_loop_triangles();start=len(vertices)
        vertices.extend(pose@obj.matrix_world@v.co for v in obj.data.vertices)
        triangles.extend(tuple(start+i for i in t.vertices) for t in obj.data.loop_triangles)
    return vertices,triangles

def tree(items,pose=Matrix.Identity(4)):
    vertices,triangles=geometry(items,pose)
    return BVHTree.FromPolygons(vertices,triangles,all_triangles=True)

poses=0
for level in range(2,9):
    fixed=[objects['weapon-turret'],objects[f'upgrade_l{level}_base']]
    moving=[objects[f'upgrade_l{level}_head']]
    fixed_tree=tree(fixed)
    assert not tree([objects[f'upgrade_l{level}_base']]).overlap(tree([objects['barrel']])),level
    for angle in range(360):
        yaw=Matrix.Rotation(math.radians(angle),4,'Z')
        for recoil in (0,.04,.075):
            pose=yaw@Matrix.Translation((0,-recoil,0));poses+=1
            assert not fixed_tree.overlap(tree(moving,pose)),('fixed/moving collision',level,angle,recoil)
            verts,_=geometry(moving,pose)
            assert all(v.z>.48 and math.hypot(v.x,v.y)<.90 for v in verts),('motion envelope',level)
    # Both barrels keep an unobstructed exit, including inside the added collars.
    for x in (-.140388,.140388):
        hit=tree(moving).ray_cast(Vector((x,.614021,.685445)),Vector((0,1,0)),.20)
        assert hit[0] is None,('muzzle blocked',level,x)

bpy.ops.wm.open_mainfile(filepath=str(ART/'gun_turret_levels.blend'))
assert bpy.context.scene.name=='GUN_TURRET_LEVELS'
for obj in bpy.context.scene.objects:
    if not obj.name.startswith('upgrade_'):continue
    assert obj.vertex_groups
    assert (obj.parent is not None)==(obj['rig_group']=='head')
    image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
    assert image.packed_file and hashlib.sha256(bytes(image.packed_file.data)).hexdigest()==manifest['palette_sha256']
report={'original_base_housing_collider_sockets_preserved':True,'exact_palette':True,
    'render_meshes':16,'additional_draws':2,'clean_import':checks,'yaw_recoil_poses':poses,
    'moving_fixed_collisions':0,'open_muzzle_channels':True,'editable_packed_source':True,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('TURRET_LEVELS_VALIDATED',poses,'poses')
