"""Validate colormap, round trip and full motion of mortar upgrade overlays."""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/mortar_levels'
manifest=json.loads((ART/'manifest.json').read_text());blob=(ROOT/manifest['component_glb']).read_bytes()
assert struct.unpack_from('<III',blob)==(0x46546c67,2,len(blob))
n=struct.unpack_from('<I',blob,12)[0];d=json.loads(blob[20:20+n]);binary=blob[28+n:]
assert len(d['meshes'])==21 and len(d['materials'])==1 and len(d['images'])==1
assert not d.get('extensionsRequired') and not d.get('animations') and not d.get('skins')
v=d['bufferViews'][d['images'][0]['bufferView']];start=v.get('byteOffset',0)
assert binary[start:start+v['byteLength']]==(ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()
assert hashlib.sha256((ROOT/'assets/models/buildings/mortar.glb').read_bytes()).hexdigest()==manifest['base_sha256']
scene=bpy.data.scenes.new('MORTAR_UPGRADE_ROUND_TRIP');bpy.context.window.scene=scene
bpy.ops.import_scene.gltf(filepath=str(ROOT/manifest['component_glb']));bpy.context.view_layer.update()
checks=[]
for obj in scene.objects:
    if obj.type!='MESH':continue
    mesh=obj.data
    assert len(mesh.materials)==1
    assert all(math.isfinite(value) for v in mesh.vertices for value in v.co)
    assert all(abs(v.normal.length-1)<1e-4 for v in mesh.vertices)
    assert all(0<=value<=1 for loop in mesh.uv_layers.active.data for value in loop.uv)
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    assert all(edge.is_manifold for edge in bm.edges),obj.name
    assert bm.calc_volume(signed=True)>0,obj.name
    bm.free();checks.append({'mesh':obj.name,'finite_unit_normals_uv_manifold':True})
# Open the isolated ordinary source, which also proves it opens to the intended scene.
bpy.ops.wm.open_mainfile(filepath=str(ART/'mortar_levels.blend'))
scene=bpy.context.scene;assert scene.name=='MORTAR_LEVELS'
assert scene.unit_settings.scale_length==1
parts=[obj for obj in scene.objects if obj.name.startswith('mortar_upgrade_l')]
source={name:scene.objects[name] for name in ['base_mesh','pitch_mesh','yaw_mesh']}
rig={name:scene.objects[name+'_pivot'] for name in ['yaw','pitch']}
for pivot in rig.values():pivot.rotation_mode='XYZ'
for obj in parts:
    assert len(obj.vertex_groups)>0
    assert obj.parent==scene.objects[{'base':'asset_root','yaw':'yaw_pivot','pitch':'pitch_pivot'}[obj['rig_group']]]
    image=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
    assert image.packed_file and hashlib.sha256(bytes(image.packed_file.data)).hexdigest()==manifest['palette_sha256']

def data(objects):
    vertices=[];triangles=[]
    for obj in objects:
        obj.data.calc_loop_triangles();offset=len(vertices)
        vertices.extend(obj.matrix_world@v.co for v in obj.data.vertices)
        triangles.extend(tuple(offset+i for i in tri.vertices) for tri in obj.data.loop_triangles)
    return vertices,triangles

def tree(objects):
    vertices,triangles=data(objects);return BVHTree.FromPolygons(vertices,triangles,all_triangles=True)

motion=[];poses=0
for level in range(2,9):
    current=[o for o in parts if o['level']==level]
    fixed=[source['base_mesh']]+[o for o in current if o['rig_group']=='base']
    new_yaw=[o for o in current if o['rig_group']=='yaw'];new_pitch=[o for o in current if o['rig_group']=='pitch']
    rig['yaw'].rotation_euler.z=0;rig['pitch'].rotation_euler.x=0;bpy.context.view_layer.update()
    fixed_tree=tree(fixed)
    # Added moving pieces versus fixed base through a full turn at three elevations.
    for yaw in range(360):
        rig['yaw'].rotation_euler.z=math.radians(yaw)
        for pitch in (-10,0,30):
            rig['pitch'].rotation_euler.x=math.radians(pitch);bpy.context.view_layer.update();poses+=1
            for role,objects in [('yaw',new_yaw),('pitch',new_pitch)]:
                overlaps=fixed_tree.overlap(tree(objects))
                assert not overlaps,('fixed/moving overlap',level,yaw,pitch,role,len(overlaps))
                vertices,_=data(objects)
                assert min(v.z for v in vertices)>.45,('floor/fixed envelope',level,role)
                assert max(v.z for v in vertices)<1.45,('collider height envelope',level,role)
                assert max(math.hypot(v.x,v.y) for v in vertices)<.601,('moving collider radius',level,role)
    # Barrel/yoke sweep. Source/source bearing contacts are intentional and unchanged.
    rig['yaw'].rotation_euler.z=0
    for pitch in range(-10,31):
        rig['pitch'].rotation_euler.x=math.radians(pitch);bpy.context.view_layer.update();poses+=1
        assert not tree(new_pitch).overlap(tree([source['yaw_mesh']]+new_yaw)),('new barrel/yoke collision',level,pitch)
        assert not tree([source['pitch_mesh']]).overlap(tree(new_yaw)),('source barrel/new yoke collision',level,pitch)
    motion.append({'level':level,'full_yaw_at_35_45_75':True,'pitch_sweep_step_degrees':1,'no_moving_fixed_collisions':True})
rig['yaw'].rotation_euler.z=0;rig['pitch'].rotation_euler.x=0;bpy.context.view_layer.update()
report={'base_unchanged':True,'exact_embedded_palette':True,'packed_editable_source':True,'round_trip':checks,
    'motion':motion,'poses_checked':poses,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('MORTAR_LEVELS_VALIDATED',poses,'poses')
