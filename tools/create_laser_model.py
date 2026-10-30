"""Create a Kenney-style laser turret from the supplied concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_laser_model.py
Final metre units, +Y forward in Blender, no baked beam/VFX; existing models preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

assert bpy.app.background, 'Use a separate background Blender process'
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'laser_views';VIEWS.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.name='LASER';scene['generated_model']='laser'
scene['support_revision_20261015']='Full support platform and reactor-centred trunnion'
for c in list(scene.collection.children):scene.collection.children.unlink(c)
model=bpy.data.collections.new('LASER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('LASER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('LASER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='laser_colormap';image.pack()
palette=bpy.data.materials.new('laser_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
energy=palette.copy();energy.name='laser_energy'
shader=next(n for n in energy.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
tex=next(n for n in energy.node_tree.nodes if n.type=='TEX_IMAGE')
energy.node_tree.links.new(tex.outputs['Color'],shader.inputs['Emission Color'])
shader.inputs['Emission Strength'].default_value=.65
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475)
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
root=empty('asset_root',(0,0,0))
root['asset']='laser';root['forward_axis']='Blender +Y / glTF -Z';root['palette']='Original Kenney colormap'
yaw=empty('yaw_pivot',(0,0,.640),root)
pitch=empty('pitch_pivot',(0,-.170,.380),yaw);pitch['elevation_degrees']=[-12,20]
base_parts=[];yaw_parts=[];pitch_parts=[];energy_parts=[]
base_parts.append(lathe('laser_foot',[(.39294,0),(.39294,.10),(.60,.10),(.60,.30)],WOOD))
base_parts.append(lathe('laser_pedestal',[(.31,.295),(.31,.34),(.265,.37),(.265,.520),(.23,.550),(.23,.620)],ORANGE))
for i in range(4):
    a=math.pi/4+i*math.pi/2
    brace=prism('laser_base_brace_%d'%i,[(-.08,.295),(.08,.295),(.063,.565),(.039,.628),(-.039,.628),(-.063,.565)],.235,.395,STEEL)
    for v in brace.data.vertices:v.co.x+=(.60-v.co.z)*.22
    brace.rotation_euler.z=a;base_parts.append(brace)
    bolt=lathe('laser_base_bolt_%d'%i,[(.043,.425),(.043,.460)],DARK_STEEL,8,'X',(0,0,.420),False)
    bolt.rotation_euler.z=a;base_parts.append(bolt)
yaw_parts.append(lathe('laser_turntable',[(.228,.615),(.228,.633),(.410,.633),(.455,.646),(.455,.667),(.430,.679)],DARK_STEEL))
yaw_parts.append(lathe('laser_mount_foot',[(.405,.671),(.435,.681),(.435,.706),(.405,.716)],ORANGE))
for sign in (-1,1):
    xlo,xhi=sorted((sign*.224,sign*.282))
    yaw_parts.append(prism('laser_yoke_%s'%('left' if sign<0 else 'right'),[(-.110,.690),(.110,.690),(.092,1.025),(.060,1.090),(-.060,1.090),(-.092,1.025)],xlo,xhi,ORANGE))
    yaw_parts.append(lathe('laser_pivot_bolt_%s'%('left' if sign<0 else 'right'),[(.051,-.012),(.051,.015)],DARK_STEEL,8,'X',(sign*.284,0,1.020),False))
for part in yaw_parts:
    if part.name.startswith(('laser_yoke_','laser_pivot_bolt_')):
        for v in part.data.vertices:v.co.y-=.170
# The axle seats inside the solid reactor, behind the exposed cooling neck.
pitch_parts.append(lathe('laser_trunnion_axle',[(.046,-.276),(.046,.276)],DARK_STEEL,8,'X',(0,-.170,1.020),False))
phase=math.pi/8
pitch_parts.append(lathe('laser_reactor_housing',[(.207,-.355),(.214,-.220),(.214,-.020),(.182,.010)],ORANGE,8,'Y',(0,0,1.020),False,phase))
pitch_parts.append(lathe('laser_reactor_frame',[(.242,-.413),(.248,-.401),(.248,-.360),(.232,-.348),(.200,-.348),(.200,-.413)],STEEL,8,'Y',(0,0,1.020),False,phase,closed=True))
energy_parts.append(lathe('laser_reactor_crystal',[(.150,-.430),(.198,-.413),(.198,-.363),(.160,-.344)],CYAN,8,'Y',(0,0,1.020),False,phase,energy))
pitch_parts.append(lathe('laser_neck',[(.102,.000),(.102,.228)],CHARCOAL,10,'Y',(0,0,1.020)))
for i,y in enumerate((.038,.093,.148)):
    pitch_parts.append(lathe('laser_cooling_ring_%d'%i,[(.105,y),(.133,y+.006),(.133,y+.029),(.105,y+.035)],DARK_STEEL,10,'Y',(0,0,1.020)))
pitch_parts.append(lathe('laser_emitter_housing',[(.151,.205),(.178,.218),(.178,.372),(.155,.394)],ORANGE,8,'Y',(0,0,1.020),False,phase))
pitch_parts.append(lathe('laser_emitter_rim',[(.153,.382),(.169,.398),(.169,.420),(.147,.431),(.091,.431),(.091,.392)],STEEL,8,'Y',(0,0,1.020),False,phase,closed=True))
energy_parts.append(lathe('laser_front_lens',[(.081,.410),(.090,.424),(.078,.439)],CYAN,12,'Y',(0,0,1.020),False,0,energy))
pitch_parts.append(prism('laser_carry_handle',[(-.270,1.182),(-.270,1.318),(-.120,1.318),(-.015,1.238),(-.015,1.196),(-.137,1.281),(-.235,1.281),(-.235,1.182)],-.038,.038,ORANGE))

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
core=combine('energy_mesh',energy_parts,pitch,energy)
empty('MUZZLE_01',(0,.629,0),pitch)
empty('socket_effect_energy',(0,-.215,0),pitch)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.480),root)
verts=[(x,y,z) for z in (0,1.40) for y in (-.6,.6) for x in (-.6,.6)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('laser_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/laser.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Laser_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('laser_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new(data.name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('laser_camera');cam=bpy.data.objects.new(data.name,data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'laser_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'laser.blend'),check_existing=False)

comparison=bpy.data.scenes.new('Laser_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','FLAMETHROWER','LASER')):
    dx=(i-1)*1.65
    if label=='LASER':
        for src in (base,mount,head,core):
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'laser_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('LASER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,mount,head,core) for p in o.data.polygons),'visible_meshes':4,'base_diameter':1.2,'height_neutral':1.318,'pitch_range':[-12,20]}))
