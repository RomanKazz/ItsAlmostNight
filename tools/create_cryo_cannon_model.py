"""Create a Kenney-style cryo_cannon turret from the supplied concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_cryo_cannon_model.py
Final metre units, +Y forward in Blender, one cryogenic muzzle; yaw-only tank and hose; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'cryo_cannon_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('CRYO_CANNON'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('CRYO_CANNON');scene['generated_model']='cryo_cannon'
bpy.context.window.scene=scene
model=bpy.data.collections.new('CRYO_CANNON_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('CRYO_CANNON_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('CRYO_CANNON_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='cryo_cannon_colormap';image.pack()
palette=bpy.data.materials.new('cryo_cannon_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);CYAN=(.03125,.155,.235);ICE_BLUE=(.09375,.025,.225)

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
def hose(name,points,radius=.025,segments=8):
    vertices=[];faces=[]
    normal=None
    for i,point in enumerate(points):
        tangent=(Vector(points[min(i+1,len(points)-1)])-Vector(points[max(i-1,0)])).normalized()
        if normal is None:
            normal=tangent.cross(Vector((0,0,1)))
        else:
            normal=normal-tangent*normal.dot(tangent)
        if normal.length<.01:normal=tangent.cross(Vector((0,1,0)))
        normal.normalize();second=tangent.cross(normal).normalized()
        for j in range(segments):
            angle=math.tau*j/segments
            vertices.append(tuple(Vector(point)+radius*(math.cos(angle)*normal+math.sin(angle)*second)))
    for i in range(len(points)-1):
        for j in range(segments):
            a=i*segments+j;b=i*segments+(j+1)%segments
            faces.append((a,b,b+segments,a+segments))
    faces.extend((tuple(reversed(range(segments))),tuple((len(points)-1)*segments+j for j in range(segments))))
    return mesh(name,vertices,faces,CHARCOAL)
root=empty('asset_root',(0,0,0))
root['asset']='cryo_cannon';root['forward_axis']='Blender +Y / glTF -Z'
root['palette']='Original Kenney colormap';root['variant']='Cryogenic projector with wide reservoir and six clamp lugs'
yaw=empty('yaw_pivot',(0,0,.500),root)
pitch=empty('pitch_pivot',(0,.120,.360),yaw);pitch['elevation_degrees']=[-12,25]
base_parts=[];yaw_parts=[];pitch_parts=[]
base_parts.append(lathe('cryo_foot',[(.39294,0),(.39294,.075),(.60,.075),(.60,.230)],WOOD))
base_parts.append(lathe('cryo_pedestal',[(.415,.225),(.415,.257),(.310,.430),(.282,.452)],ORANGE))
for i in range(4):
    angle=math.pi/4+i*math.pi/2
    brace=prism('cryo_base_brace_%d'%i,[(-.077,.230),(.077,.230),(.058,.410),(.039,.460),(-.039,.460),(-.058,.410)],.285,.417,STEEL)
    for v in brace.data.vertices:v.co.x+=(.45-v.co.z)*.36
    brace.rotation_euler.z=angle;base_parts.append(brace)
    bolt=lathe('cryo_base_bolt_%d'%i,[(.043,.458),(.043,.510)],DARK_STEEL,8,'X',(0,0,.310),False)
    bolt.rotation_euler.z=angle;base_parts.append(bolt)
yaw_parts.append(lathe('cryo_turntable',[(.285,.448),(.353,.464),(.353,.512),(.329,.528)],DARK_STEEL))
yaw_parts.append(lathe('cryo_mount_platform',[(.320,.520),(.347,.534),(.347,.557),(.320,.576)],ORANGE))
# A low cradle supports the reservoir. The tank never pitches with the nozzle.
yaw_parts.append(lathe('cryo_tank_cradle',[(.128,.546),(.155,.567),(.155,.731),(.137,.780)],DARK_STEEL,12,center=(0,-.240,0)))
reservoir=lathe('cryo_reservoir',[(.095,-.330),(.205,-.290),(.270,-.200),(.285,-.100),(.285,.100),(.270,.200),(.205,.290),(.095,.330)],ICE_BLUE,12,'X',smooth=False)
for v in reservoir.data.vertices:v.co.y=v.co.y*.90-.240;v.co.z+=1.045
yaw_parts.append(reservoir)
for sign in (-1,1):
    # Belt wall section is tailored to the tank profile, rather than floating around it.
    lo,hi=sorted((sign*.165,sign*.215))
    rlo=.287 if sign>0 else .270;rhi=.270 if sign>0 else .287
    band=lathe('cryo_tank_strap_%s'%('left' if sign<0 else 'right'),
        [(rlo,lo),(rhi,hi),(rhi-.019,hi),(rlo-.019,lo)],ORANGE,12,'X',smooth=False,closed=True)
    for v in band.data.vertices:v.co.y=v.co.y*.90-.240;v.co.z+=1.045
    yaw_parts.append(band)
yaw_parts.append(lathe('cryo_tank_cap_seat',[(.125,1.310),(.143,1.324),(.143,1.344)],ORANGE,12,center=(0,-.240,0)))
yaw_parts.append(lathe('cryo_tank_cap',[(.108,1.337),(.119,1.350),(.119,1.379),(.103,1.390)],STEEL,10,center=(0,-.240,0)))
for sign in (-1,1):
    lo,hi=sorted((sign*.180,sign*.245))
    yaw_parts.append(prism('cryo_yoke_%s'%('left' if sign<0 else 'right'),[(.045,.553),(.200,.553),(.190,.862),(.153,.918),(.089,.918),(.050,.865)],lo,hi,ORANGE))
    lo,hi=sorted((sign*.240,sign*.278))
    yaw_parts.append(lathe('cryo_pivot_bolt_%s'%('left' if sign<0 else 'right'),[(.052,lo),(.052,hi)],DARK_STEEL,8,'X',(0,.120,.860),False))
pitch_parts.append(lathe('cryo_trunnion_axle',[(.034,-.238),(.034,.238)],DARK_STEEL,8,'X',(0,.120,.860),False))
pitch_parts.append(box_profile('cryo_receiver',[(.108,.111,.025,.085),(.139,.139,.032,.111),(.139,.139,.032,.297),(.119,.119,.025,.322)],ORANGE,.860))
pitch_parts.append(lathe('cryo_nozzle_neck',[(.117,.301),(.117,.350)],CHARCOAL,12,'Y',(0,0,.860)))
pitch_parts.append(lathe('cryo_nozzle_collar',[(.141,.329),(.173,.346),(.173,.383),(.159,.397)],STEEL,12,'Y',(0,0,.860)))
pitch_parts.append(lathe('cryo_nozzle_rim',[(.157,.378),(.184,.402),(.184,.496),(.162,.522),(.106,.522),(.106,.452)],DARK_STEEL,12,'Y',(0,0,.860),closed=True))
pitch_parts.append(lathe('cryo_cold_core',[(.100,.469),(.106,.493),(.092,.505)],CYAN,12,'Y',(0,0,.860),False))
for i in range(6):
    angle=math.tau*i/6
    lug=box_profile('cryo_nozzle_clamp_%d'%i,[(.029,.023,.010,.357),(.034,.027,.011,.371),(.034,.027,.011,.408),(.029,.023,.010,.422)],STEEL)
    # The blocks follow the nozzle circumference, giving a chunky six-clamp silhouette.
    for v in lug.data.vertices:
        x,z=v.co.x,v.co.z+.174
        v.co.x=x*math.cos(angle)-z*math.sin(angle)
        v.co.z=.860+x*math.sin(angle)+z*math.cos(angle)
    pitch_parts.append(lug)
# Route the hose outside the reservoir and above the stationary base. End on the pitch axis.
yaw_parts.append(hose('cryo_coolant_hose',[(0,-.495,1.000),(-.120,-.540,.920),(-.290,-.470,.770),(-.385,-.360,.690),(-.370,-.150,.680),(-.300,.030,.770),(-.245,.120,.860)]))
yaw_parts.append(lathe('cryo_rear_hose_port',[(.041,-.516),(.041,-.478)],DARK_STEEL,8,'Y',(0,0,1.000),False))

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
empty('MUZZLE_01',(0,.422,0),pitch)
empty('socket_effect_coolant',(0,-.240,.545),yaw)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.500),root)
verts=[(x,y,z) for z in (0,1.420) for y in (-.6,.6) for x in (-.6,.6)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('cryo_cannon_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/cryo_cannon.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Cryo cannon_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('cryo_cannon_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('cryo_cannon_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('cryo_cannon_camera');cam=bpy.data.objects.new('cryo_cannon_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'cryo_cannon_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'cryo_cannon.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['CRYO_CANNON']
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
subprocess.run([bpy.app.binary_path,'--background',str(ART/'cryo_cannon.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Cryo cannon_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','FLAMETHROWER','CRYO_CANNON')):
    dx=(i-1)*1.65
    if label=='CRYO_CANNON':
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'cryo_cannon_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('CRYO_CANNON_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,mount,head) for p in o.data.polygons),'visible_meshes':3,'base_diameter':1.2,'height_neutral':1.390,'pitch_range':[-12,25]}))

