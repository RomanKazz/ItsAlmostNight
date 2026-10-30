"""Create a Kenney-style mortar from the supplied concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_mortar_model.py
Final metre units, +Y forward in Blender, one elevated launch socket; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'mortar_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('MORTAR'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('MORTAR');scene['generated_model']='mortar';scene['barrel_revision_20261015']='Short tapered barrel and rounded breech'
bpy.context.window.scene=scene
model=bpy.data.collections.new('MORTAR_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('MORTAR_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('MORTAR_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='mortar_colormap';image.pack()
palette=bpy.data.materials.new('mortar_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);CYAN=(.03125,.105,.205)

def mesh(name,verts,faces,swatch,material=palette,smooth=()):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);model.objects.link(obj);data.materials.append(material)
    uv=data.uv_layers.new(name='UVMap')
    lo=min(v.co.z for v in data.vertices);hi=max(v.co.z for v in data.vertices)
    for p in data.polygons:
        p.use_smooth=p.index in smooth
        for li in p.loop_indices:
            z=data.vertices[data.loops[li].vertex_index].co.z
            uv.data[li].uv=(swatch[0],swatch[1]+(z-lo)/max(hi-lo,1e-6)*(swatch[2]-swatch[1]))
    data.set_sharp_from_angle(angle=math.radians(45))
    return obj
def lathe(name,profile,swatch,segments=12,axis='Z',center=(0,0,0),smooth=True,phase=0,material=palette,closed=False):
    verts=[];faces=[];smooth_faces=[]
    for r,z in profile:
        for i in range(segments):
            a=phase+math.tau*i/segments;co=Vector((r*math.cos(a),r*math.sin(a),z))
            if axis=='Y':co=Vector((co.x,co.z,-co.y))
            elif axis=='X':co=Vector((co.z,co.y,-co.x))
            verts.append(tuple(co+Vector(center)))
    links=len(profile) if closed else len(profile)-1
    for ring in range(links):
        next_ring=(ring+1)%len(profile)
        for i in range(segments):
            a=ring*segments+i;b=ring*segments+(i+1)%segments
            c=next_ring*segments+(i+1)%segments;d=next_ring*segments+i
            if smooth and abs(profile[next_ring][1]-profile[ring][1])>1e-6:smooth_faces.append(len(faces))
            faces.append((a,b,c,d))
    if not closed:
        faces.extend((tuple(reversed(range(segments))),tuple((len(profile)-1)*segments+i for i in range(segments))))
    return mesh(name,verts,faces,swatch,material,smooth_faces)
def prism(name,outline,xlo,xhi,swatch):
    area=sum(outline[i][0]*outline[(i+1)%len(outline)][1]-outline[(i+1)%len(outline)][0]*outline[i][1] for i in range(len(outline)))
    if area<0:outline=list(reversed(outline))
    n=len(outline);verts=[(x,y,z) for x in (xlo,xhi) for y,z in outline]
    faces=[tuple(reversed(range(n))),tuple(n+i for i in range(n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    return mesh(name,verts,faces,swatch)
def empty(name,loc,parent=None):
    o=bpy.data.objects.new(name,None);model.objects.link(o);o.location=loc;o.parent=parent
    o.empty_display_type='PLAIN_AXES';o.empty_display_size=.04
    return o

# Beveled rectangular shells retain the broad flat faces of the supplied concept.
def rectangle(hw,hh,b):
    b=min(b,hw*.75,hh*.75)
    return [(-hw+b,-hh),(hw-b,-hh),(hw,-hh+b),(hw,hh-b),
            (hw-b,hh),(-hw+b,hh),(-hw,hh-b),(-hw,-hh+b)]
def box_profile(name,rings,swatch,zcenter=0,close_profile=False):
    vertices=[]
    for hw,hh,b,y in rings:
        vertices.extend((x,y,zcenter+z) for x,z in rectangle(hw,hh,b))
    faces=[]
    for ring in range(len(rings) if close_profile else len(rings)-1):
        nxt=(ring+1)%len(rings)
        for i in range(8):
            faces.append((ring*8+i,nxt*8+i,nxt*8+(i+1)%8,ring*8+(i+1)%8))
    if not close_profile:
        faces.extend((tuple(range(8)),tuple(reversed(range((len(rings)-1)*8,len(rings)*8)))))
    return mesh(name,vertices,faces,swatch)
root=empty('asset_root',(0,0,0))
root['asset']='mortar';root['forward_axis']='Blender +Y / glTF -Z'
root['palette']='Original Kenney colormap';root['variant']='Short heavy mortar with deep open bore'
yaw=empty('yaw_pivot',(0,0,.470),root)
pitch=empty('pitch_pivot',(0,0,.400),yaw)
pitch['neutral_elevation_degrees']=45;pitch['absolute_elevation_degrees']=[35,75]
pitch['rotation_delta_degrees']=[-10,30]
base_parts=[];yaw_parts=[];pitch_parts=[]
base_parts.append(lathe('mortar_foot',[(.39294,0),(.39294,.075),(.60,.075),(.60,.230)],WOOD))
base_parts.append(lathe('mortar_pedestal',[(.425,.225),(.425,.255),(.345,.420),(.305,.444)],ORANGE))
for i in range(4):
    angle=math.pi/4+i*math.pi/2
    lug=prism('mortar_foot_lug_%d'%i,[(-.090,.080),(.090,.080),(.107,.112),(.107,.223),(.083,.250),(-.083,.250),(-.107,.223),(-.107,.112)],.427,.596,WOOD)
    lug.rotation_euler.z=angle;base_parts.append(lug)
    brace=prism('mortar_base_brace_%d'%i,[(-.073,.230),(.073,.230),(.058,.395),(.041,.438),(-.041,.438),(-.058,.395)],.310,.436,STEEL)
    for v in brace.data.vertices:v.co.x+=(.44-v.co.z)*.40
    brace.rotation_euler.z=angle;base_parts.append(brace)
    bolt=lathe('mortar_base_bolt_%d'%i,[(.043,.469),(.043,.517)],DARK_STEEL,8,'X',(0,0,.305),False)
    bolt.rotation_euler.z=angle;base_parts.append(bolt)
yaw_parts.append(lathe('mortar_turntable',[(.295,.440),(.395,.454),(.395,.493),(.370,.511)],DARK_STEEL))
yaw_parts.append(lathe('mortar_yoke_platform',[(.345,.501),(.375,.517),(.375,.548),(.355,.561)],ORANGE))
for sign in (-1,1):
    xlo,xhi=sorted((sign*.241,sign*.320))
    yaw_parts.append(prism('mortar_yoke_%s'%('left' if sign<0 else 'right'),[
        (-.112,.533),(.112,.533),(.124,.737),(.079,.870),(.034,.907),
        (-.053,.907),(-.125,.825),(-.147,.675)
    ],xlo,xhi,ORANGE))
    xlo,xhi=sorted((sign*.315,sign*.357))
    yaw_parts.append(lathe('mortar_pivot_bolt_%s'%('left' if sign<0 else 'right'),
        [(.071,xlo),(.071,xhi)],DARK_STEEL,8,'X',(0,0,.870),False))
    xlo,xhi=sorted((sign*.203,sign*.300))
    pitch_parts.append(lathe('mortar_trunnion_%s'%('left' if sign<0 else 'right'),
        [(.052,xlo),(.052,xhi)],DARK_STEEL,8,'X',(0,0,.870),False))
# Build the tube around +Y and bake its neutral 45-degree elevation into the geometry.
# Closed wall profile: smooth outer tube, hard base, deep dark inner wall and real bore floor.
barrel=lathe('mortar_barrel',[
    (.095,-.190),(.170,-.165),(.215,-.105),(.230,.015),(.218,.220),(.208,.394),
    (.148,.394),(.135,-.090)
],DARK_STEEL,12,'Y',smooth=True,closed=True)
# Closing plate is behind the bore. Side trunnions deliberately never cross the channel.
breech=lathe('mortar_breech',[(.133,-.102),(.133,-.092)],CHARCOAL,12,'Y',smooth=False)
rear_cap=lathe('mortar_breech_cap',[(.097,-.195),(.097,-.185)],DARK_STEEL,12,'Y',smooth=False)
mouth=lathe('mortar_muzzle_rim',[
    (.208,.385),(.230,.402),(.230,.440),(.219,.460),
    (.145,.460),(.141,.444),(.141,.399),(.148,.385)
],STEEL,12,'Y',smooth=True,closed=True)
# Darken the real bore walls with the charcoal region of the original palette.
for obj,rings in ((barrel,{6}),(mouth,{4,5,6})):
    for face in obj.data.polygons:
        if face.index//12 in rings:
            for li in face.loop_indices:obj.data.uv_layers.active.data[li].uv=(CHARCOAL[0],.105)

# Further thickness revision: enlarge only outside rings, keeping the bore diameter.
for obj,rings in ((barrel,6),(mouth,4),(rear_cap,2)):
    for v in list(obj.data.vertices)[:rings*12]:v.co.x*=1.25;v.co.z*=1.25
for obj in yaw_parts:
    if obj.name.startswith('mortar_yoke_') and obj.name!='mortar_yoke_platform':
        for v in obj.data.vertices:
            v.co.x+=(-.065 if 'left' in obj.name else .065)
            if v.co.z>.570:v.co.z+=.050
    elif obj.name.startswith('mortar_pivot_bolt_'):
        for v in obj.data.vertices:v.co.x+=(-.065 if 'left' in obj.name else .065)
    elif obj.name in ('mortar_turntable','mortar_yoke_platform'):
        factor=1.20 if obj.name=='mortar_turntable' else 1.22
        for v in obj.data.vertices:v.co.x*=factor;v.co.y*=factor
for obj in pitch_parts:
    if obj.name.startswith('mortar_trunnion_'):
        for v in obj.data.vertices:v.co.x+=(-.065 if 'left' in obj.name else .065)
scene['thick_barrel_20261015']='Outer diameter +25%; bore preserved; matching supports widened'
neutral=Matrix.Rotation(math.radians(45),4,'X')
for o in (barrel,breech,rear_cap,mouth):
    for v in o.data.vertices:v.co=neutral@v.co+Vector((0,0,.870))
    pitch_parts.append(o)

def combine(name,parts,parent,material):
    bpy.context.view_layer.update();verts=[];faces=[];uvs=[];flags=[];ranges=[]
    for o in parts:
        offset=len(verts);matrix=parent.matrix_world.inverted()@o.matrix_world
        verts.extend(tuple(matrix@v.co) for v in o.data.vertices)
        for p in o.data.polygons:
            faces.append(tuple(offset+i for i in p.vertices));flags.append(p.use_smooth)
            uvs.extend(tuple(o.data.uv_layers.active.data[li].uv) for li in p.loop_indices)
        ranges.append((o.name,offset,len(o.data.vertices)))
    o=mesh(name,verts,faces,WOOD,material);o.parent=parent
    for i,co in enumerate(uvs):o.data.uv_layers.active.data[i].uv=co
    for p,flag in zip(o.data.polygons,flags):p.use_smooth=flag
    for label,start,count in ranges:o.vertex_groups.new(name=label).add(list(range(start,start+count)),1,'REPLACE')
    for part in parts:
        data=part.data;bpy.data.objects.remove(part,do_unlink=True)
        if data.users==0:bpy.data.meshes.remove(data)
    return o
base=combine('base_mesh',base_parts,root,palette)
mount=combine('yaw_mesh',yaw_parts,yaw,palette)
head=combine('pitch_mesh',pitch_parts,pitch,palette)
muzzle=empty('MUZZLE_01',(0,.480/math.sqrt(2),.480/math.sqrt(2)),pitch)
muzzle.rotation_euler.x=math.radians(45)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.550),root)
verts=[(x,y,z) for z in (0,1.450) for y in (-.6,.6) for x in (-.6,.6)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('mortar_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/mortar.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Mortar_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('mortar_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('mortar_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('mortar_camera');cam=bpy.data.objects.new('mortar_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'mortar_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'mortar.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['MORTAR']
bpy.context.window.scene=s
for other in list(bpy.data.scenes):
    if other!=s:bpy.data.scenes.remove(other)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,check_existing=False)
"""
subprocess.run([bpy.app.binary_path,'--background',str(ART/'mortar.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Mortar_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','FLAMETHROWER','MORTAR')):
    dx=(i-1)*1.65
    if label=='MORTAR':
        for src in (base,mount,head):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('art/references/kenney_tower_defense/weapon-cannon.glb' if label=='CANNON' else 'assets/models/buildings/flamethrower.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before;transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
        for o,matrix in transforms.items():
            o.parent=None
            extra=Matrix.Rotation(math.pi,4,'Z')@Matrix.Scale(2,4) if label=='CANNON' else Matrix.Identity(4)
            o.matrix_world=Matrix.Translation((dx,0,0))@extra@matrix
            if o.name.startswith('COL_'):o.hide_render=True
    text=bpy.data.curves.new('label_'+label,'FONT');text.body=label;text.align_x='CENTER';text.size=.10
    o=bpy.data.objects.new(text.name,text);comparison.collection.objects.link(o);o.location=(dx,.83,-.05)
    o.rotation_euler=(Vector((2,8.5,4.2))-o.location).to_track_quat('Z','Y').to_euler()
for src in studio.objects:
    if src.type=='LIGHT':
        o=bpy.data.objects.new('comparison_'+src.name,src.data.copy());comparison.collection.objects.link(o)
        o.matrix_world=src.matrix_world.copy();o.data.size*=1.5;o.data.energy*=1.3
data=bpy.data.cameras.new('comparison_camera');cam=bpy.data.objects.new(data.name,data);comparison.collection.objects.link(cam);comparison.camera=cam
cam.location=(2,8.5,4.2);aim(cam,(0,0,.55));data.lens=50
comparison.render.resolution_x=1800;comparison.render.resolution_y=950;comparison.render.resolution_percentage=100
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'mortar_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('MORTAR_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,mount,head) for p in o.data.polygons),'visible_meshes':3,'base_diameter':1.2,'neutral_elevation_degrees':45,'pitch_absolute_degrees':[35,75]}))

