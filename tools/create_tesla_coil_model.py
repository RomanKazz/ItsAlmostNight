"""Tesla concept variant with a readable tall coil, terminal toroid and large orb.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_tesla_coil_model.py
Creates new tesla_coil files; preserves existing tesla and manual flamethrower.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

assert bpy.app.background, 'Run in a separate background Blender process'
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'tesla_coil_views';VIEWS.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.name='TESLA_COIL';scene['generated_model']='tesla_coil'
for c in list(scene.collection.children):scene.collection.children.unlink(c)
model=bpy.data.collections.new('TESLA_COIL_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('TESLA_COIL_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('TESLA_COIL_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='tesla_coil_colormap';image.pack()
palette=bpy.data.materials.new('tesla_coil_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
energy=palette.copy();energy.name='tesla_coil_energy'
shader=next(n for n in energy.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
tex=next(n for n in energy.node_tree.nodes if n.type=='TEX_IMAGE')
energy.node_tree.links.new(tex.outputs['Color'],shader.inputs['Emission Color'])
shader.inputs['Emission Strength'].default_value=.65
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CYAN=(.03125,.105,.185)

def assign_uv(obj,swatch,material=palette):
    obj.data.materials.clear();obj.data.materials.append(material)
    uv=obj.data.uv_layers.active or obj.data.uv_layers.new(name='UVMap')
    lo=min(v.co.z for v in obj.data.vertices);hi=max(v.co.z for v in obj.data.vertices)
    for poly in obj.data.polygons:
        for li in poly.loop_indices:
            z=obj.data.vertices[obj.data.loops[li].vertex_index].co.z
            t=(z-lo)/max(hi-lo,1e-6)
            uv.data[li].uv=(swatch[0],swatch[1]+t*(swatch[2]-swatch[1]))
def mesh(name,verts,faces,swatch,smooth=()):
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);model.objects.link(obj);assign_uv(obj,swatch)
    for poly in data.polygons:poly.use_smooth=poly.index in smooth
    data.set_sharp_from_angle(angle=math.radians(45))
    return obj
def lathe(name,profile,swatch,segments=12,axis='Z',center=(0,0,0)):
    verts=[];faces=[];smooth=[]
    for r,z in profile:
        for i in range(segments):
            a=math.tau*i/segments;co=Vector((r*math.cos(a),r*math.sin(a),z))
            if axis=='X':co=Vector((co.z,co.y,-co.x))
            verts.append(tuple(co+Vector(center)))
    for ring in range(len(profile)-1):
        for i in range(segments):
            a=ring*segments+i;b=ring*segments+(i+1)%segments
            if abs(profile[ring+1][1]-profile[ring][1])>1e-6:smooth.append(len(faces))
            faces.append((a,b,b+segments,a+segments))
    faces.extend((tuple(reversed(range(segments))),tuple((len(profile)-1)*segments+i for i in range(segments))))
    return mesh(name,verts,faces,swatch,smooth)
def prism(name,outline,xlo,xhi,swatch):
    n=len(outline);verts=[(x,y,z) for x in (xlo,xhi) for y,z in outline]
    faces=[tuple(reversed(range(n))),tuple(n+i for i in range(n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    return mesh(name,verts,faces,swatch)
def empty(name,loc,parent=None):
    o=bpy.data.objects.new(name,None);model.objects.link(o);o.location=loc;o.parent=parent
    o.empty_display_type='PLAIN_AXES';o.empty_display_size=.045
    return o
root=empty('asset_root',(0,0,0))
root['asset']='tesla_coil';root['forward_axis']='Blender +Y / glTF -Z'
root['variant']='Tesla with tall three-rib coil, toroidal terminal and large energy sphere'
body=[]
body.append(lathe('tesla_foot',[(.39294,0),(.39294,.10),(.60,.10),(.60,.30)],WOOD))
body.append(lathe('tesla_pedestal',[(.30,.295),(.30,.325),(.23,.365),(.23,.505),(.22,.535)],ORANGE))
for i in range(4):
    a=i*math.pi/2
    brace=prism('tesla_brace_%d'%i,[(-.085,.295),(.085,.295),(.064,.550),(.042,.595),(-.042,.595),(-.064,.550)],.25,.39,STEEL)
    for v in brace.data.vertices:v.co.x+=(.60-v.co.z)*.22
    brace.rotation_euler.z=a;body.append(brace)
    bolt=lathe('tesla_brace_bolt_%d'%i,[(.044,.420),(.044,.455)],DARK_STEEL,8,'X',(0,0,.445))
    bolt.rotation_euler.z=a;body.append(bolt)
body.append(lathe('tesla_lower_collar',[(.232,.510),(.267,.531),(.267,.615),(.232,.635)],ORANGE))
body.append(lathe('tesla_insulator_stem',[(.108,.626),(.108,1.045)],DARK_STEEL))
for i,z in enumerate((.656,.780,.904)):
    body.append(lathe('tesla_insulator_rib_%d'%i,[(.106,z),(.167,z+.009),(.205,z+.031),(.205,z+.051),(.167,z+.075),(.106,z+.084)],STEEL))
body.append(lathe('tesla_upper_collar',[(.228,1.006),(.25,1.021),(.25,1.070),(.233,1.086)],ORANGE))
body.append(lathe('tesla_emitter_seat',[(.105,1.075),(.123,1.086),(.123,1.140),(.110,1.150)],ORANGE))
# A broad conducting terminal ring gives the head a recognizable Tesla silhouette.
bpy.ops.mesh.primitive_torus_add(major_segments=16,minor_segments=6,major_radius=.225,minor_radius=.035,location=(0,0,1.102))
terminal=bpy.context.object
for c in list(terminal.users_collection):c.objects.unlink(terminal)
model.objects.link(terminal);terminal.name='tesla_terminal_toroid';terminal.data.name=terminal.name
assign_uv(terminal,STEEL)
for p in terminal.data.polygons:p.use_smooth=True
terminal.data.set_sharp_from_angle(angle=math.radians(45));body.append(terminal)

bpy.context.view_layer.update()
# Two visible primitives: fixed structural body and faceted energy sphere.
verts=[];faces=[];uvs=[];smooth=[];parts=[]
for o in body:
    offset=len(verts);verts.extend(tuple(o.matrix_world@v.co) for v in o.data.vertices)
    for p in o.data.polygons:
        faces.append(tuple(offset+i for i in p.vertices));smooth.append(p.use_smooth)
        uvs.extend(tuple(o.data.uv_layers.active.data[li].uv) for li in p.loop_indices)
    parts.append((o.name,offset,len(o.data.vertices)))
base=mesh('base_mesh',verts,faces,WOOD)
for i,co in enumerate(uvs):base.data.uv_layers.active.data[i].uv=co
for p,flag in zip(base.data.polygons,smooth):p.use_smooth=flag
for name,start,count in parts:base.vertex_groups.new(name=name).add(list(range(start,start+count)),1,'REPLACE')
for o in body:
    data=o.data;bpy.data.objects.remove(o,do_unlink=True)
    if data.users==0:bpy.data.meshes.remove(data)
base.parent=root
bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=.190,location=(0,0,1.310))
orb=bpy.context.object
for c in list(orb.users_collection):c.objects.unlink(orb)
model.objects.link(orb);orb.name='energy_mesh';orb.data.name='energy_mesh'
assign_uv(orb,CYAN,energy)
for p in orb.data.polygons:p.use_smooth=False
orb.parent=root
empty('MUZZLE_01',(0,.210,1.310),root)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.600),root)
empty('socket_effect_energy',(0,0,1.310),root)

verts=[(x,y,z) for z in (0,1.50) for y in (-.6,.6) for x in (-.6,.6)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col)
col.parent=root;col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('tesla_coil_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0
shader.inputs['Roughness'].default_value=1;data.materials.append(diagnostic)
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/tesla_coil.glb'),
    use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,
    export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
    export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Tesla_coil_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('tesla_coil_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new(data.name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.55))
data=bpy.data.cameras.new('tesla_coil_camera');cam=bpy.data.objects.new(data.name,data)
studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.5,4,2.7),(0,0,.70),2.10),('front',(0,4,.75),(0,0,.75),1.90),('side',(-4,0,.75),(0,0,.75),1.90),('back',(0,-4,.75),(0,0,.75),1.90),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'tesla_coil_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'tesla_coil.blend'),check_existing=False)

# Comparison studio lives only in this authoring process, not in the saved file.
comparison=bpy.data.scenes.new('Tesla_coil_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','FLAMETHROWER','TESLA')):
    dx=(i-1)*1.65
    if label=='TESLA':
        for src in (base,orb):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('art/references/kenney_tower_defense/weapon-cannon.glb' if label=='CANNON' else 'assets/models/buildings/flamethrower.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before
        transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
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
data=bpy.data.cameras.new('comparison_camera');cam=bpy.data.objects.new(data.name,data)
comparison.collection.objects.link(cam);comparison.camera=cam
cam.location=(2,8.5,4.2);aim(cam,(0,0,.45));data.lens=50
comparison.render.resolution_x=1800;comparison.render.resolution_y=950;comparison.render.resolution_percentage=100
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'tesla_coil_comparison.png')
bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('TESLA_COIL_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,orb) for p in o.data.polygons),'base_diameter':1.2,'height':1.50,'visible_meshes':2}))
