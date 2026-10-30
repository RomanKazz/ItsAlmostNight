"""Create a five-lantern Kenney-style decoy beacon from the supplied concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_decoy_beacon_model.py
Final metre units, +Y forward in Blender, light/effect sockets, no weapon muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'decoy_beacon_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('DECOY_BEACON'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('DECOY_BEACON');scene['generated_model']='decoy_beacon'
bpy.context.window.scene=scene
model=bpy.data.collections.new('DECOY_BEACON_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('DECOY_BEACON_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('DECOY_BEACON_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='decoy_beacon_colormap';image.pack()
palette=bpy.data.materials.new('decoy_beacon_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
energy=palette.copy();energy.name='decoy_beacon_energy'
node=next(n for n in energy.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
energy_tex=next(n for n in energy.node_tree.nodes if n.type=='TEX_IMAGE')
energy.node_tree.links.new(energy_tex.outputs['Color'],node.inputs['Emission Color'])
node.inputs['Emission Strength'].default_value=.65
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);AMBER=(.65625,.295,.455)

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
def plate(name,outline,ylo,yhi,swatch,material=palette):
    area=sum(outline[i][0]*outline[(i+1)%len(outline)][1]-outline[(i+1)%len(outline)][0]*outline[i][1] for i in range(len(outline)))
    if area<0:outline=list(reversed(outline))
    n=len(outline);vertices=[(x,y,z) for y in (ylo,yhi) for x,z in outline]
    faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
    faces.extend((i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n))
    return mesh(name,vertices,faces,swatch,material)
root=empty('asset_root',(0,0,0))
root['asset']='decoy_beacon';root['forward_axis']='Blender +Y / glTF -Z'
root['palette']='Original Kenney colormap';root['variant']='Five-lantern attraction beacon with cross arms and lifting eye'
body=[];accents=[]
# Low steel plinth with four wide sloped orange armoured feet.
body.append(lathe('decoy_base',[(.355,0),(.395,.028),(.395,.179),(.364,.218)],DARK_STEEL))
body.append(lathe('decoy_pedestal',[(.290,.205),(.315,.234),(.278,.311),(.245,.336)],ORANGE))
for i in range(4):
    angle=math.pi/4+i*math.pi/2
    brace=prism('decoy_foot_%02d'%i,[(-.110,0),(.110,0),(.110,.100),(.075,.329),(-.075,.329),(-.110,.100)],.275,.450,DARK_STEEL)
    armor=prism('decoy_armor_%02d'%i,[(-.099,.018),(.099,.018),(.099,.092),(.064,.325),(-.064,.325),(-.099,.092)],.445,.487,ORANGE)
    for part in (brace,armor):
        for v in part.data.vertices:v.co.x+=(.330-v.co.z)*.45
        part.rotation_euler.z=angle;body.append(part)
    bolt=lathe('decoy_bolt_%02d'%i,[(.044,.563),(.044,.609)],DARK_STEEL,8,'X',(0,0,.150),False)
    bolt.rotation_euler.z=angle;body.append(bolt)
body.append(lathe('decoy_lower_socket',[(.134,.322),(.154,.345),(.154,.395),(.134,.411)],STEEL))
# The centre lantern is a shell around the load-bearing mast, not a floating ornament.
body.append(lathe('decoy_mast',[(.060,.384),(.060,1.110)],DARK_STEEL,8))
body.append(lathe('decoy_center_lower_cap',[(.112,.404),(.139,.428),(.139,.467),(.123,.484)],ORANGE))
body.append(lathe('decoy_center_upper_cap',[(.123,.746),(.148,.767),(.148,.815),(.126,.837)],ORANGE))
accents.append(lathe('decoy_center_glass',[(.113,.466),(.126,.488),(.126,.737),(.113,.759)],AMBER,12,material=energy))
for i in range(4):
    a=math.pi/4+i*math.pi/2
    bar=box_profile('decoy_center_guard_%02d'%i,[(.014,.136,.003,.114),(.014,.136,.003,.135)],ORANGE,.612)
    bar.rotation_euler.z=a;body.append(bar)
body.append(lathe('decoy_mast_collar',[(.071,.836),(.079,.850),(.079,.890),(.071,.904)],STEEL,8))
# Four solid arms and hanging stems, each reaching its lamp cap.
body.append(lathe('decoy_head_lower',[(.157,1.014),(.188,1.032),(.188,1.084)],DARK_STEEL))
body.append(lathe('decoy_head',[(.191,1.068),(.225,1.092),(.225,1.188),(.193,1.212)],ORANGE))
for i in range(4):
    angle=i*math.pi/2
    arm=box_profile('decoy_arm_%02d'%i,[(.031,.032,.008,.147),(.031,.032,.008,.505)],DARK_STEEL,1.056)
    arm.rotation_euler.z=angle;body.append(arm)
    joint=box_profile('decoy_arm_joint_%02d'%i,[(.047,.047,.009,.467),(.052,.052,.009,.485),(.052,.052,.009,.533),(.047,.047,.009,.548)],STEEL,1.056)
    joint.rotation_euler.z=angle;body.append(joint)
    center=Vector((-math.sin(angle)*.505,math.cos(angle)*.505,0))
    body.append(lathe('decoy_hanger_%02d'%i,[(.023,.787),(.023,1.038)],DARK_STEEL,8,center=center))
    body.append(lathe('decoy_hanger_seat_%02d'%i,[(.036,.779),(.043,.791),(.043,.811),(.035,.819)],STEEL,8,center=center))
    body.append(lathe('decoy_lamp_top_%02d'%i,[(.090,.730),(.111,.751),(.111,.786),(.080,.810)],ORANGE,12,center=center))
    body.append(lathe('decoy_lamp_bottom_%02d'%i,[(.069,.512),(.100,.532),(.105,.571),(.092,.588)],ORANGE,12,center=center))
    accents.append(lathe('decoy_lamp_glass_%02d'%i,[(.088,.568),(.098,.584),(.098,.725),(.088,.743)],AMBER,12,center=center,material=energy))
# Octagonal lifting eye with a real through hole, on its own broad seat.
body.append(lathe('decoy_eye_seat',[(.100,1.205),(.113,1.219),(.113,1.239),(.100,1.249)],DARK_STEEL))
body.append(lathe('decoy_lifting_eye',[(.055,-.034),(.105,-.034),(.119,-.020),(.119,.020),(.105,.034),(.055,.034)],STEEL,8,'Y',(0,0,1.342),False,phase=math.pi/8,closed=True))

def focused_vertex(name,co):
    """Emphasise the central lantern while preserving mounting points."""
    co=co.copy()
    if name.startswith('decoy_center_'):
        co.x*=1.20;co.y*=1.20
        co.z=.404+(co.z-.404)*1.20
    elif name.startswith(('decoy_lamp_','decoy_hanger_seat_')):
        angle=int(name.rsplit('_',1)[1])*math.pi/2
        anchor=Vector((-math.sin(angle)*.505,math.cos(angle)*.505,.810))
        co=anchor+(co-anchor)*.90
    elif name=='decoy_mast_collar':
        co.z+=.0866
    return co

# Apply the focus proportions before combining; UVs keep the original palette.
for part in body+accents:
    for v in part.data.vertices:v.co=focused_vertex(part.name,v.co)
    part.data.update()
scene['lantern_focus_revision']=True

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
base=combine('base_mesh',body,root,palette)
service=combine('light_mesh',accents,root,energy)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.560),root)
empty('socket_effect_attract',(0,0,.6536),root)
for i in range(4):
    a=i*math.pi/2
    empty('socket_light_%02d'%(i+1),(-math.sin(a)*.505,math.cos(a)*.505,.6705),root)
verts=[(x,y,z) for z in (0,1.470) for y in (-.640,.640) for x in (-.640,.640)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('decoy_beacon_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/decoy_beacon.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Decoy beacon_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('decoy_beacon_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('decoy_beacon_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('decoy_beacon_camera');cam=bpy.data.objects.new('decoy_beacon_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.69),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'decoy_beacon_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'decoy_beacon.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['DECOY_BEACON']
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
subprocess.run([bpy.app.binary_path,'--background',str(ART/'decoy_beacon.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Decoy beacon_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','REPAIR_STATION','DECOY_BEACON')):
    dx=(i-1)*1.65
    if label=='DECOY_BEACON':
        for src in (base,service):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('art/references/kenney_tower_defense/weapon-cannon.glb' if label=='CANNON' else 'assets/models/buildings/repair_station.glb')
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'decoy_beacon_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('DECOY_BEACON_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,service) for p in o.data.polygons),'visible_meshes':2,'footprint_span':1.2098,'height':1.451944,'static':True}))

