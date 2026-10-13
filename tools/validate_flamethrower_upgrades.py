"""Validate the authored overlay GLB and render its family comparison in Blender."""
from pathlib import Path
import hashlib,json,math,struct
import bpy,bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/flamethrower_upgrades'
manifest=json.loads((ART/'manifest.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ART/'flamethrower_upgrades.blend'))
scene=bpy.context.scene
parts=[o for o in scene.objects if o.type=='MESH' and o.name.startswith('upgrade_l')]
rig={'base':bpy.data.objects.get('asset_root'),'yaw':bpy.data.objects.get('yaw_pivot'),'pitch':bpy.data.objects.get('pitch_pivot')}
for obj in parts:
    previous=obj.matrix_world.copy();obj.parent=rig[obj['rig_group']];obj.matrix_world=previous
bpy.context.view_layer.update()
# The editable source follows the same real pivots as runtime rendering.
for pivot in (rig['yaw'],rig['pitch']):pivot.rotation_mode='XYZ'
source_base=next(o for o in scene.objects if o.name=='base_mesh')
source_meshes=[o for o in scene.objects if o.type=='MESH' and o.name in ('base_mesh','pitch_mesh','yaw_mesh')]

def tree(objects):
    vertices=[];triangles=[]
    for obj in objects:
        obj.data.calc_loop_triangles();offset=len(vertices)
        vertices.extend(obj.matrix_world@v.co for v in obj.data.vertices)
        triangles.extend(tuple(offset+i for i in tri.vertices) for tri in obj.data.loop_triangles)
    return BVHTree.FromPolygons(vertices,triangles,all_triangles=True)

motion=[]
for level in range(2,9):
    current=[o for o in parts if o.get('upgrade_level')==level]
    fixed=tree([source_base]+[o for o in current if o['rig_group']=='base'])
    for yaw in range(0,360):
        for pitch in (-10,0,21):
            rig['yaw'].rotation_euler.z=math.radians(yaw)
            rig['pitch'].rotation_euler.x=math.radians(pitch)
            bpy.context.view_layer.update()
            for role in ('yaw','pitch'):
                collisions=fixed.overlap(tree([o for o in current if o['rig_group']==role]))
                if collisions:motion.append({'level':level,'yaw':yaw,'pitch':pitch,'role':role,'triangle_pairs':len(collisions)})
rig['yaw'].rotation_euler.z=0;rig['pitch'].rotation_euler.x=0;bpy.context.view_layer.update()

raw=(ROOT/manifest['component_glb']).read_bytes();n=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+n]);binary=raw[28+n:]
palette=[]
for image in doc['images']:
    view=doc['bufferViews'][image['bufferView']];offset=view.get('byteOffset',0)
    palette.append(hashlib.sha256(binary[offset:offset+view['byteLength']]).hexdigest())
assert palette==[manifest['palette_sha256']]
assert hashlib.sha256((ROOT/'assets/models/buildings/flamethrower.glb').read_bytes()).hexdigest()==manifest['base_sha256']

# Clean round-trip import: verify finite unit normals, UVs and welded topology.
check=bpy.data.scenes.new('Round-trip verification');bpy.context.window.scene=check
bpy.ops.import_scene.gltf(filepath=str(ROOT/manifest['component_glb']))
checks=[]
for obj in check.objects:
    if obj.type!='MESH':continue
    mesh=obj.data
    finite=all(math.isfinite(value) for v in mesh.vertices for value in v.co)
    normals=all(abs(v.normal.length-1)<1e-4 for v in mesh.vertices)
    uv=all(0<=value<=1 for loop in mesh.uv_layers.active.data for value in loop.uv)
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
    closed=all(edge.is_manifold for edge in bm.edges)
    volume=bm.calc_volume(signed=True);bm.free()
    checks.append({'mesh':obj.name,'finite':finite,'unit_normals':normals,'valid_uv':uv,'manifold':closed,'positive_volume':volume>0,
        'smooth_faces':sum(p.use_smooth for p in mesh.polygons)})
    assert finite and normals and uv and closed and volume>0,checks[-1]
bpy.context.window.scene=scene

# Same camera/light: base flamethrower, master flamethrower and source cannon.
existing=set(scene.objects)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'assets/models/buildings/cannon.glb'))
reference=[o for o in scene.objects if o not in existing]
cannon=[o for o in reference if o.type=='MESH' and not o.name.startswith(('upgrade_','COL_'))]
for obj in scene.objects:
    if obj.type=='MESH':obj.hide_render=True
clones=[]
for sources,x in ((source_meshes,-1.65),(source_meshes+[o for o in parts if o.get('upgrade_level')==8],0),(cannon,1.65)):
    for original in sources:
        obj=original.copy();obj.data=original.data;scene.collection.objects.link(obj)
        obj.parent=None;obj.matrix_world=original.matrix_world.copy();obj.location.x+=x
        obj.hide_render=False;clones.append(obj)
camera=scene.camera;camera.location=(3.8,7.5,3.7)
camera.rotation_euler=(Vector((0,0,.62))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=5.4
scene.render.resolution_x=1320;scene.render.resolution_y=660;scene.render.filepath=str(ART/'family_comparison.png')
bpy.ops.render.render(write_still=True)
for obj in clones+reference:bpy.data.objects.remove(obj,do_unlink=True)
for obj in source_meshes:obj.hide_render=False
for obj in parts:obj.hide_render=obj.get('upgrade_level')!=8
camera.location=(2.6,4.6,2.7);camera.rotation_euler=(Vector((0,0,.67))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=2
scene.render.resolution_x=440;scene.render.resolution_y=440
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'flamethrower_upgrades.blend'))
report={'meshes':checks,'palette_matches':True,'base_preserved':True,'tested_poses':7*360*3,
    'rotating_overlaps_fixed_geometry':motion,'loader_verified':False,'native_playtest':False}
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('Upgrade validation:',len(checks),'meshes,',len(motion),'motion contacts')
