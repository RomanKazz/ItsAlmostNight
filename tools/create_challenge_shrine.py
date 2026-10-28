"""Build the symmetric challenge shrine in Blender, without touching other shrines.

Uses the same authoring geometry and packed Kenney palette as the woodland
shrine family. The skull has actual through-cut eye/nose sockets, not decals.
"""
from pathlib import Path
import ast
import hashlib
import json
import math

import bpy
import bmesh
from mathutils import Euler, Matrix, Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models/challenge_shrine';ART.mkdir(parents=True,exist_ok=True)
OUT=ROOT/'assets/models/challenges/skull_column.glb'
SCENE='IAN_CHALLENGE_SHRINE_20261028'
old=bpy.data.scenes.get(SCENE)
if old:
    assert old.get('ian_challenge_shrine')==1
    for obj in list(old.objects):
        mesh=obj.data if obj.type=='MESH' else None
        bpy.data.objects.remove(obj,do_unlink=True)
        if mesh and mesh.users==0:bpy.data.meshes.remove(mesh)
    bpy.data.scenes.remove(old)
scene=bpy.data.scenes.new(SCENE);scene['ian_challenge_shrine']=1
bpy.context.window.scene=scene
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
models=bpy.data.collections.new('CHALLENGE_SHRINE_MODEL');scene.collection.children.link(models)
studio=bpy.data.collections.new('CHALLENGE_SHRINE_STUDIO');scene.collection.children.link(studio)
palette=ROOT/'assets/models/buildings/Textures/colormap.png'
image=bpy.data.images.load(str(palette),check_existing=False);image.pack()
material=bpy.data.materials.new('Challenge_Shrine_Kenney_Colormap');material.use_nodes=True
shader=next(n for n in material.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Roughness'].default_value=1;shader.inputs['Metallic'].default_value=0
tex=material.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
material.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.28,.47);OAK=(.21875,.28,.47)
STONE=(.59375,.035,.19);LIGHT_STONE=(.84375,.035,.19)
IRON=(.46875,.04,.18);RED=(.96875,.28,.46)
BRONZE=(.65625,.28,.46);BONE=(.46875,.28,.46)

# Load only the shared authoring definitions. Importing/executing the original
# generator would overwrite the already accepted Player/Building shrines.
source=ast.parse((ROOT/'tools/refine_shrine_models.py').read_text())
definitions=[n for n in source.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ('Geo','bolt')]
exec(compile(ast.Module(body=definitions,type_ignores=[]),'shrine_geometry','exec'))

g=Geo()
g.lathe('stepped_stone_foundation',[(.65,0),(.77,.075),(.77,.19),(.68,.29),(.66,.33)],STONE,sides=8,smooth=False)
g.lathe('upper_stone_platform',[(.62,.29),(.68,.35),(.68,.42),(.61,.47)],LIGHT_STONE,sides=8,smooth=False)
for side in (-1,1):
    g.box('approach_step_'+str(side),(0,side*.58,.23),(.65,.32,.24),STONE,.035)
    g.box('approach_tread_'+str(side),(0,side*.60,.38),(.57,.28,.08),LIGHT_STONE,.02)
g.lathe('altar_oak',[(.35,.44),(.41,.52),(.35,.65),(.33,1.23),(.41,1.29)],WOOD,sides=8,smooth=False)
g.lathe('altar_iron_foot',[(.38,.50),(.42,.55),(.42,.66),(.37,.69)],IRON,sides=8,smooth=False)
g.lathe('altar_slab',[(.40,1.23),(.48,1.29),(.49,1.40),(.42,1.47)],LIGHT_STONE,sides=10,smooth=False)
g.lathe('offering_iron_recess',[(.32,1.471),(.32,1.49)],IRON,sides=10,smooth=False)
g.lathe('red_offering_pool',[(.27,1.491),(.27,1.51)],RED,sides=10,smooth=False)
for sign in (-1,1):
    g.box('post_stone_boot_'+str(sign),(sign*.51,0,.71),(.31,.39,.57),STONE,.045)
    g.box('oak_upright_'+str(sign),(sign*.51,0,1.95),(.25,.29,2.30),WOOD,.035)
    for height in (1.06,1.80,2.93):
        g.box('iron_wrap_'+str(sign)+'_'+str(height),(sign*.51,0,height),(.30,.35,.13),IRON,.018)
        for side in (-1,1):bolt(g,'wrap_rivet_'+str(sign)+'_'+str(height)+'_'+str(side),sign*.51,side*.177,height,.044)
    g.box('stone_cap_'+str(sign),(sign*.51,0,3.08),(.34,.38,.16),LIGHT_STONE,.03)
g.box('stone_lintel',(0,0,3.09),(1.44,.37,.18),STONE,.035)
g.box('lintel_iron_clasp',(0,0,3.09),(.19,.40,.20),IRON,.02)
g.gem('crown_ruby',(0,0,3.16),.10,.09,RED)
for side in (-1,1):
    bolt(g,'lintel_rivet_'+str(side),0,side*.202,3.09,.043)
    diamond=[(0,.16),(.13,0),(0,-.16),(-.13,0)]
    g.prism('red_votive_setting_'+str(side),diamond,(0,side*.354,.96),.055,BRONZE)
    g.prism('red_votive_gem_'+str(side),[(x*.73,z*.73) for x,z in diamond],(0,side*.395,.96),.07,RED)
    for sign in (-1,1):
        g.gem('base_ruby_'+str(sign)+'_'+str(side),(sign*.52,side*.35,.43),.10,.31,RED)
        tab=[(-.065,.18),(.065,.18),(.065,-.14),(0,-.10),(-.065,-.14)]
        g.prism('red_post_tab_'+str(sign)+'_'+str(side),tab,(sign*.51,side*.169,2.03),.035,RED)
body=g.object('challenge_shrine_body')

g=Geo()
for sign in (-1,1):
    start=len(g.vertices)
    blade=[(-.065,-.12),(.065,-.12),(.065,.51),(0,.70),(-.065,.51)]
    g.prism('trial_blade_'+str(sign),blade,(0,0,0),.085,LIGHT_STONE,.07)
    g.box('blade_guard_'+str(sign),(0,0,-.18),(.28,.13,.07),BRONZE,.015)
    g.box('blade_grip_'+str(sign),(0,0,-.36),(.09,.10,.28),OAK,.012)
    g.box('blade_pommel_'+str(sign),(0,0,-.53),(.13,.13,.08),IRON,.015)
    rotation=Matrix.Rotation(sign*.40,3,'Y')
    for i in range(start,len(g.vertices)):
        g.vertices[i]=tuple(rotation@Vector(g.vertices[i])+Vector((0,0,2.28)))
for x in (-.12,0,.12):g.box('skull_tooth_'+str(x),(x,0,2.10),(.087,.26,.15),BONE,.015)
for side in (-1,1):
    mark=[(0,.085),(.065,0),(0,-.085),(-.065,0)]
    g.prism('forehead_ruby_'+str(side),mark,(0,side*.169,2.70),.035,RED)
glyph=g.object('challenge_shrine_blessing')

g=Geo()
outline=[(-.22,-.29),(-.22,-.16),(-.33,-.10),(-.35,.12),(-.28,.31),(-.14,.37),
         (.14,.37),(.28,.31),(.35,.12),(.33,-.10),(.22,-.16),(.22,-.29)]
g.prism('carved_skull_shell',outline,(0,0,2.40),.32,BONE,.14)
skull=g.object('challenge_skull_shell')
eye=[(-.115,-.055),(-.075,-.115),(.075,-.115),(.115,-.055),(.115,.065),(.055,.115),(-.055,.115),(-.115,.065)]
nose=[(-.060,-.045),(.060,-.045),(0,.080)]
for label,shape,x,z in [('left_eye',eye,-.155,2.50),('right_eye',eye,.155,2.50),('nose',nose,0,2.29)]:
    cutter_geo=Geo();cutter_geo.prism(label,shape,(x,0,z),.80,IRON,.01)
    cutter=cutter_geo.object('challenge_socket_cutter')
    modifier=skull.modifiers.new('Carved_'+label,'BOOLEAN');modifier.operation='DIFFERENCE';modifier.object=cutter
    bpy.context.view_layer.objects.active=skull
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    mesh=cutter.data;bpy.data.objects.remove(cutter,do_unlink=True);bpy.data.meshes.remove(mesh)
    # Dark socket bottoms sit inside the actual cut, equally recessed on both faces.
    for side in (-1,1):
        inside=Geo();inside.prism(label+'_recess',[(u*1.03,v*1.03) for u,v in shape],(x,side*.085,z),.025,IRON,.01)
        backing=inside.object('challenge_'+label+'_recess_'+str(side))
        bpy.ops.object.select_all(action='DESELECT');glyph.select_set(True);backing.select_set(True)
        bpy.context.view_layer.objects.active=glyph;bpy.ops.object.join()
# Booleans may triangulate their two matching sockets differently. Bake an
# exact quarter mirror on the shell before assembly, preserving both symmetry
# planes without welding together the separately built swords/socket bottoms.
bpy.context.view_layer.objects.active=skull
mirror=skull.modifiers.new('Exact_Skull_Symmetry','MIRROR')
mirror.use_axis=(True,True,False);mirror.use_bisect_axis=(True,True,False)
mirror.use_mirror_merge=True;mirror.merge_threshold=1e-6
bpy.ops.object.modifier_apply(modifier=mirror.name)
bpy.ops.object.select_all(action='DESELECT');glyph.select_set(True);skull.select_set(True)
bpy.context.view_layer.objects.active=glyph;bpy.ops.object.join()
# Recalculate normals after the socket booleans, retaining hard low-poly edges.
bm=bmesh.new();bm.from_mesh(glyph.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(glyph.data);bm.free()
root=bpy.data.objects.new('challenge_shrine_root',None);models.objects.link(root)
body.parent=root;glyph.parent=root
bpy.ops.object.select_all(action='DESELECT')
for obj in (root,body,glyph):obj.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.export_scene.gltf(filepath=str(OUT),export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True)
counts={}
for obj in (body,glyph):
    obj.data.calc_loop_triangles();counts[obj.name]={'triangles':len(obj.data.loop_triangles),'vertex_groups':len(obj.vertex_groups)}
coords=[v.co for obj in (body,glyph) for v in obj.data.vertices]
manifest={'runtime_glb':'assets/models/challenges/skull_column.glb','height_m':3.25,
          'bounds_glb':{'min':[min(v[i] for v in coords) for i in (0,2,1)],'max':[max(v[i] for v in coords) for i in (0,2,1)]},
          'pivot_glb':[0,0,0],'symmetry_planes_glb':['X = 0','Z = 0'],'meshes':counts,'materials':1,
          'palette_sha256':hashlib.sha256(palette.read_bytes()).hexdigest(),
          'collision':'Existing challenge arena and interaction sphere unchanged; within selection bounds +/-0.75m and 3.35m high',
          'integration':'Existing challengeColumnModel path replaced; height remains 3.25m; active tint and completion shrink use existing renderer',
          'license':'Original geometry with existing Kenney CC0 palette; no old skull geometry reused'}
(ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
world=bpy.data.worlds.new('Challenge_Shrine_Studio');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs[0].default_value=(.045,.06,.065,1);bg.inputs[1].default_value=.6
for label,loc,power in [('Key',(-3,-5,7),850),('Fill',(4,-1,5),450),('Rim',(1,4,6),650)]:
    data=bpy.data.lights.new('Challenge_Shrine_'+label,'AREA');data.energy=power;data.size=4
    obj=bpy.data.objects.new(data.name,data);studio.objects.link(obj);obj.location=loc
    obj.rotation_euler=(Vector((0,0,1.5))-obj.location).to_track_quat('-Z','Y').to_euler()
camera=bpy.data.objects.new('Challenge_Shrine_Camera',bpy.data.cameras.new('Challenge_Shrine_Camera'))
studio.objects.link(camera);scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=4.15
camera.location=(3,-8,4.0);camera.rotation_euler=(Vector((0,0,1.6))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=1100;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard';scene.render.image_settings.file_format='PNG'
try:scene.render.engine='BLENDER_EEVEE'
except TypeError:scene.render.engine='BLENDER_EEVEE_NEXT'
bpy.context.view_layer.update();bpy.data.libraries.write(str(ART/'challenge_shrine.blend'),{scene},fake_user=True)
print(json.dumps(manifest,indent=2))
