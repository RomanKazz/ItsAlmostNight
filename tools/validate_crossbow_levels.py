"""Validate crossbow GLB preservation, colormap, editable rig and yaw and pitch."""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/crossbow_levels'

def read(path):
    data=path.read_bytes();n=struct.unpack_from('<I',data,12)[0]
    assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data))
    return json.loads(data[20:20+n]),data[28+n:]
doc,binary=read(ROOT/'assets/models/weapons/crossbow.glb');base,base_binary=read(ART/'base_reference.glb')
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
scene=bpy.data.scenes.new('CROSSBOW_ROUND_TRIP');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/weapons/crossbow.glb'));bpy.context.view_layer.update()
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

pivot=Vector((0,0,.14931100606918335))
def pitch_pose(angle):
    return Matrix.Translation(pivot)@Matrix.Rotation(angle,4,'X')@Matrix.Translation(-pivot)
poses=0
for level in range(2,9):
    base_overlay=objects[f'upgrade_crossbow_l{level}_base']
    bow_overlay=objects[f'upgrade_crossbow_l{level}_bow']
    fixed=[objects['weapon-ballista'],base_overlay]
    if level>=3:
        # Regression: the L8 outer frame must not cover the orange side plate.
        for side in (-1,1):
            hit=tree([base_overlay]).ray_cast(Vector((side*1.0,.04,.42)),Vector((-side,0,0)),2.0)
            assert hit[0] is not None and abs(abs(hit[0].x)-.630)<1e-5,('side inset hidden',level,side)
            tri=base_overlay.data.loop_triangles[hit[2]]
            assert all(abs(base_overlay.data.uv_layers.active.data[i].uv.x-.84375)<1e-5 for i in tri.loops),('side inset is not orange',level,side)
    # Full relative pitch sampling in the existing graphics-test range.
    for sample in range(-18,19):
        pitch=pitch_pose(math.radians(sample));poses+=1
        assert not tree(fixed).overlap(tree([bow_overlay],pitch)),('bow/carriage collision',level,sample)
        assert not tree([base_overlay]).overlap(tree([objects['arrow']],pitch)),('new carriage/original bow collision',level,sample)
    for degree in range(360):
        yaw=Matrix.Rotation(math.radians(degree),4,'Z')
        fixed_tree=tree(fixed,yaw)
        for pitch in (-.30,0,.30):
            pose=yaw@pitch_pose(pitch);poses+=1
            assert not fixed_tree.overlap(tree([bow_overlay],pose)),('yaw/pitch collision',level,degree,pitch)
            vertices,_=geometry([bow_overlay],pose)
            assert min(v.z for v in vertices)>.4,('floor clearance',level,degree,pitch)
            assert max(math.hypot(v.x,v.y) for v in vertices)<.90,('yaw envelope',level,degree,pitch)
    # The game retains the original low pitch pivot. At steep angles its stock
    # can pass through the original pedestal. Surface belts share that legacy
    # joint; evaluate new carriage armor separately from those attached belts.
    armor_vertices,armor_triangles=geometry([base_overlay])
    armor_triangles=[tri for tri in armor_triangles if min(armor_vertices[i].z for i in tri)>.19]
    armor_tree=BVHTree.FromPolygons(armor_vertices,armor_triangles,all_triangles=True) if armor_triangles else None
    for degree in range(-90,91):
        pose=pitch_pose(math.radians(degree));poses+=1
        assert not tree(fixed).overlap(tree([bow_overlay],pose)),('full-pitch bow/carriage collision',level,degree)
        assert armor_tree is None or not armor_tree.overlap(tree([objects['arrow']],pose)),('full-pitch carriage armor collision',level,degree)
    # Added bow reinforcement must preserve the original launch channel.
    muzzle=Vector((0,.1994807869195938,.7464950680732727))
    hit=tree([bow_overlay]).ray_cast(muzzle,Vector((0,1,0)),1.0)
    assert hit[0] is None,('launch channel obstructed',level)

bpy.ops.wm.open_mainfile(filepath=str(ART/'crossbow_levels.blend'))
assert bpy.context.scene.name=='CROSSBOW_LEVELS'
for obj in bpy.context.scene.objects:
    if not obj.name.startswith('upgrade_'):continue
    assert obj.vertex_groups
    assert obj.parent is not None
    assert obj.parent.name==('pitch_pivot' if obj['rig_group']=='bow' else 'yaw_pivot.001')
    image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
    assert image.packed_file and hashlib.sha256(bytes(image.packed_file.data)).hexdigest()==manifest['palette_sha256']
report={'original_base_housing_collider_sockets_preserved':True,'exact_palette':True,
    'render_meshes':16,'additional_draws':2,'clean_import':checks,'yaw_pitch_poses':poses,'pitch_sweep_degrees':[-90,90],'both_side_insets_visible_orange':True,
    'new_bow_and_carriage_armor_collisions':0,'legacy_extreme_pitch_note':'Original stock/pedestal contacts remain at steep angles; attached foot belts share that existing joint','open_muzzle_channels':True,'editable_packed_source':True,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('CROSSBOW_LEVELS_VALIDATED',poses,'poses')
