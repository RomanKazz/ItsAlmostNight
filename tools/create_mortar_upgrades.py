"""Author eight mortar tiers using the original embedded Kenney colormap.

Run in a fresh background Blender. Preserves the base GLB and author's blend.
Exports seven complete additive sets (fixed base, yaw carriage, pitched tube).
"""
from pathlib import Path
import hashlib, importlib.util, json, math, struct
import bpy
from mathutils import Matrix, Vector

assert bpy.app.background, "Run this generator in an isolated background Blender"
bpy.ops.wm.read_factory_settings(use_empty=True)
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/mortar_levels';ART.mkdir(parents=True,exist_ok=True)
BASE=ROOT/'assets/models/buildings/mortar.glb'
PALETTE=ROOT/'assets/models/buildings/Textures/colormap.png'
BASE_HASH=hashlib.sha256(BASE.read_bytes()).hexdigest()
spec=importlib.util.spec_from_file_location('mortar_geometry',ROOT/'tools/create_tesla_upgrades.py')
family=importlib.util.module_from_spec(spec);spec.loader.exec_module(family)
Geometry=family.Geometry
GOLD=(.65625,.285,.465);ORANGE=(.84375,.285,.465);RED=(.96875,.285,.465)
DARK=(.59375,.035,.215);STEEL=(.84375,.035,.215)

def aim(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
def move_new(g,start,transform):
    for i in range(start,len(g.verts)):g.verts[i]=tuple(transform@Vector(g.verts[i]))
def tube_ring(g,name,lo,hi,inner,outer,swatch):
    start=len(g.verts);g.ring(name,lo,hi,inner,outer,swatch)
    # Primitive Z axis -> tube Y axis, then authored neutral elevation.
    pose=Matrix.Translation((0,0,.870))@Matrix.Rotation(math.pi/4,4,'X')@Matrix.Rotation(-math.pi/2,4,'X')
    move_new(g,start,pose)
def bolt(g,name,sign,profile,swatch):
    start=len(g.verts);g.lathe(name,profile,swatch,sides=8)
    pose=Matrix.Translation((0,0,.870))@Matrix.Rotation(sign*math.pi/2,4,'Y')
    move_new(g,start,pose)
def box(g,name,center,size,swatch):
    x,y,z=center;w,d,h=size
    g.prism(name,[(x-w/2,z-h/2),(x+w/2,z-h/2),(x+w/2,z+h/2),(x-w/2,z+h/2)],d/2,0,swatch)
    # prism tangential center starts at 0.
    start,count=g.groups[name]
    for i in range(start,start+count):a,b,c=g.verts[i];g.verts[i]=(a,b+y,c)

scene=bpy.data.scenes.new('MORTAR_LEVELS');bpy.context.window.scene=scene
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
model=bpy.data.collections.new('MORTAR_UPGRADE_MODEL');scene.collection.children.link(model)
reference=bpy.data.collections.new('MORTAR_BASE_REFERENCE');scene.collection.children.link(reference)
studio=bpy.data.collections.new('MORTAR_UPGRADE_STUDIO');scene.collection.children.link(studio)
before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(BASE));bpy.context.view_layer.update()
source=list(set(scene.objects)-before)
for obj in source:
    for collection in list(obj.users_collection):collection.objects.unlink(obj)
    reference.objects.link(obj)
    if obj.name.startswith('COL_'):
        obj.hide_render=True;obj.hide_set(True);obj.display_type='WIRE'
image=bpy.data.images.load(str(PALETTE),check_existing=False);image.pack()
material=bpy.data.materials.new('mortar_upgrade_colormap');material.use_nodes=True
shader=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
texture=material.node_tree.nodes.new('ShaderNodeTexImage');texture.image=image
material.node_tree.links.new(texture.outputs['Color'],shader.inputs['Base Color'])
levels={}
for level in range(2,9):
    base=Geometry();yaw=Geometry();pitch=Geometry()
    base.ring('gold_foot_belt',.155,.212,.591,.625,GOLD)
    yaw.ring('gold_turntable_trim',.506,.535,.448,.475,GOLD)
    tube_ring(pitch,'breech_reinforcement',.025,.083,.277,.302,GOLD)
    if level>=3:
        tube_ring(pitch,'forward_collar',.270,.315,.255,.291,GOLD)
        for sign in (-1,1):
            bolt(yaw,'bearing_washer_'+str(sign),sign,[(.082,.419),(.087,.426),(.087,.448),(.076,.455)],GOLD)
            bolt(yaw,'bearing_bolt_'+str(sign),sign,[(.050,.448),(.050,.472),(.041,.480)],DARK)
    if level>=4:
        for i in range(4):
            angle=math.pi/4+i*math.pi/2
            base.prism('gold_foot_brace_'+str(i),[(.408,.233),(.535,.233),(.478,.385),(.420,.442),(.371,.442),(.421,.380)],.082,angle,GOLD)
            base.prism('orange_brace_inlay_'+str(i),[(.509,.262),(.531,.262),(.476,.379),(.423,.422),(.408,.410),(.457,.367)],.054,angle,ORANGE)
        for sign in (-1,1):
            yaw.prism('armored_bearing_cheek_'+str(sign),[(.385,.585),(.420,.585),(.438,.778),(.425,.918),(.397,.955),(.385,.930)],.121,0 if sign>0 else math.pi,GOLD)
    if level>=5:
        tube_ring(pitch,'front_heat_guard',.325,.389,.254,.300,GOLD)
        tube_ring(pitch,'orange_heat_guard_inlay',.341,.373,.297,.314,ORANGE)
        # Brackets root into the rotating platform, not the fixed pedestal.
        for sign in (-1,1):
            box(yaw,'ammo_rack_mount_'+str(sign),(sign*.17,-.365,.580),(.105,.24,.068),DARK)
            yaw.lathe('rack_gold_seat_'+str(sign),[(.102,.608),(.111,.620),(.111,.653),(.095,.665)],GOLD,(sign*.17,-.455))
    if level>=6:
        for sign in (-1,1):
            center=(sign*.17,-.455)
            yaw.lathe('loaded_shell_case_'+str(sign),[(.077,.650),(.084,.667),(.084,.924),(.073,.946)],RED,center)
            yaw.ring('shell_case_gold_band_'+str(sign),.742,.775,.081,.095,GOLD,center)
            yaw.lathe('shell_gold_nose_'+str(sign),[(.081,.931),(.081,.967),(.058,1.012),(.020,1.053)],GOLD,center)
        base.ring('second_foot_trim',.083,.116,.591,.624,GOLD)
    if level>=7:
        for i,axial in enumerate((.185,.221,.257)):
            tube_ring(pitch,'cooling_fin_'+str(i),axial,axial+.019,.267,.313,STEEL if i==1 else GOLD)
        # A readable rear ammunition guard, below the barrel's swept volume.
        box(yaw,'ammo_guard_crossbar',(0,-.520,1.160),(.51,.043,.060),GOLD)
        for sign in (-1,1):
            box(yaw,'ammo_guard_post_'+str(sign),(sign*.24,-.515,.880),(.038,.046,.56),GOLD)
        base.ring('red_master_pedestal_band',.218,.252,.420,.450,RED)
    if level>=8:
        tube_ring(pitch,'master_muzzle_flange',.429,.476,.148,.328,GOLD)
        tube_ring(pitch,'master_muzzle_orange_belt',.433,.456,.325,.339,ORANGE)
        base.ring('master_lower_belt',.118,.151,.591,.638,GOLD)
        for sign in (-1,1):
            # Two small central reserve shells complete a four-round rack.
            center=(sign*.058,-.535)
            yaw.lathe('reserve_shell_case_'+str(sign),[(.046,.650),(.051,.670),(.051,.996),(.045,1.018)],ORANGE,center,8)
            yaw.lathe('reserve_shell_nose_'+str(sign),[(.051,1.003),(.052,1.029),(.034,1.078),(.012,1.103)],GOLD,center,8)
            base.prism('master_foot_pauldron_'+str(sign),[(.385,.323),(.474,.323),(.491,.382),(.423,.463),(.372,.463)],.102,0 if sign>0 else math.pi,GOLD)
    objects=[]
    for role,g in [('base',base),('yaw',yaw),('pitch',pitch)]:
        obj=g.object(f'mortar_upgrade_l{level}_{role}',model,material)
        obj['level']=level;obj['rig_group']=role;obj.hide_render=True;objects.append(obj)
    levels[level]=objects

bpy.ops.object.select_all(action='DESELECT')
for objects in levels.values():
    for obj in objects:obj.select_set(True)
bpy.context.view_layer.objects.active=levels[8][0]
output=ROOT/'assets/models/buildings/mortar_upgrades.glb'
bpy.ops.export_scene.gltf(filepath=str(output),use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,
    export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)
raw=output.read_bytes();length=struct.unpack_from('<I',raw,12)[0];document=json.loads(raw[20:20+length])
indices={mesh['name']:index for index,mesh in enumerate(document['meshes'])}
bounds=[];triangles=[]
for mesh in document['meshes']:
    primitive=mesh['primitives'][0];a=document['accessors'][primitive['attributes']['POSITION']]
    bounds.append([a['min'],a['max']]);triangles.append(document['accessors'][primitive['indices']]['count']//3)
rows=[[indices[f'mortar_upgrade_l{level}_{role}'] for role in ('base','yaw','pitch')] for level in range(2,9)]
header='#pragma once\n#include <array>\n#include <raylib.h>\nnamespace ian::mortar_model {\n'
header+='// Generated by tools/create_mortar_upgrades.py. One complete set per level.\n'
header+='inline constexpr std::array<std::array<int,3>,7> UpgradeMeshes{{\n'
header+=''.join('    {{'+','.join(map(str,row))+'}},\n' for row in rows)+'}};\n'
(ROOT/'src/graphics/MortarUpgradeData.hpp').write_text(header+'}\n')
manifest={'base_sha256':BASE_HASH,'palette_sha256':hashlib.sha256(PALETTE.read_bytes()).hexdigest(),
    'component_glb':str(output.relative_to(ROOT)),'mode':'L1 base, L2-L8 base + only current complete overlay set',
    'roles':['base','yaw','pitch'],'level_mesh_indices':rows,'mesh_count':len(indices),'materials':len(document['materials']),
    'bounds_glb':bounds,'triangles':triangles,'pitch_pivot_glb':[0,.870,0],'pitch_delta_degrees':[-10,30],
    'collision':'Unchanged base COL_BOX_BODY','muzzle':'Unchanged MUZZLE_01; front flange ends before socket',
    'footprint_m':[2,2],'additional_draws':3,'license':'Original project geometry; Kenney colormap CC0',
    'native_playtest':False}
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Editable rig parenting preserves world transforms; only the authored base has sockets/collider.
for objects in levels.values():
    for obj in objects:
        parent=next(o for o in source if o.name=={'base':'asset_root','yaw':'yaw_pivot','pitch':'pitch_pivot'}[obj['rig_group']])
        world=obj.matrix_world.copy();obj.parent=parent;obj.matrix_world=world
bpy.context.view_layer.update()
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=480;scene.render.resolution_y=480;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=-.25
world=bpy.data.worlds.new('Mortar neutral studio');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.10,.14,.17,1);bg.inputs[1].default_value=.65
for name,pos,power,size in [('key',(3,4,6),500,5),('fill',(-4,2,4),300,4),('rim',(0,-4,5),380,4)]:
    data=bpy.data.lights.new('mortar_'+name,'AREA');data.energy=power;data.size=size
    obj=bpy.data.objects.new(data.name,data);studio.objects.link(obj);obj.location=pos;aim(obj,(0,0,.7))
data=bpy.data.cameras.new('mortar_levels_camera');camera=bpy.data.objects.new(data.name,data);studio.objects.link(camera);scene.camera=camera;data.type='ORTHO'
def render(level,label='beauty'):
    for current,objects in levels.items():
        for obj in objects:obj.hide_render=current!=level;obj.hide_set(current!=level)
    views={'beauty':((2.5,4,2.7),(0,0,.72),2.0),'front':((0,5,.75),(0,0,.75),1.8),
        'side':((5,0,.75),(0,0,.75),1.8),'back':((0,-5,.75),(0,0,.75),1.8),'top':((0,.001,5),(0,0,0),1.65)}
    pos,target,scale=views[label];camera.location=pos;aim(camera,target);data.ortho_scale=scale
    scene.render.filepath=str(ART/f'level_{level}_{label}.png');bpy.ops.render.render(write_still=True)
for level in range(1,9):render(level)
for view in ('front','side','back','top'):render(8,view)
render(8)
# Save one ordinary project, not a linked library; no existing author's file is overwritten.
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'mortar_levels.blend'),check_existing=False)
assert hashlib.sha256(BASE.read_bytes()).hexdigest()==BASE_HASH
print('MORTAR_LEVELS_WRITTEN',json.dumps({'meshes':len(indices),'triangles':sum(triangles)}))

comparison=family.new_scene('MORTAR_LEVEL_FAMILY');family.studio(comparison)
family.import_visible(ROOT/'assets/models/buildings/flamethrower.glb',offset=-1.7)
family.import_visible(ROOT/'assets/models/buildings/flamethrower_upgrades.glb',lambda o:'_l8_' in o.name,offset=-1.7)
family.import_visible(BASE)
family.import_visible(output,lambda o:'_l8_' in o.name)
family.import_visible(ROOT/'assets/models/buildings/tesla_tower.glb',offset=1.7)
family.import_visible(ROOT/'assets/models/buildings/tesla_upgrades.glb',lambda o:'l8' in o.name,offset=1.7)
comparison.camera.location=(2,9,4.5);aim(comparison.camera,(0,0,.7));comparison.camera.data.ortho_scale=5.4
comparison.render.resolution_x=1500;comparison.render.resolution_y=700
comparison.render.filepath=str(ART/'family_comparison.png');bpy.ops.render.render(write_still=True)
