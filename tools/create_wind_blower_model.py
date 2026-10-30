"""Create a Kenney-style wind blower based on the generated project concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_wind_blower_model.py
Final metre units, +Y forward in Blender, yaw turntable, separate four-blade rotor, one wind muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'wind_blower_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('WIND_BLOWER'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('WIND_BLOWER');scene['generated_model']='wind_blower'
bpy.context.window.scene=scene
model=bpy.data.collections.new('WIND_BLOWER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('WIND_BLOWER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('WIND_BLOWER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='wind_blower_colormap';image.pack()
palette=bpy.data.materials.new('wind_blower_palette');palette.use_nodes=True
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
def plate(name,outline,ylo,yhi,swatch,material=palette):
    area=sum(outline[i][0]*outline[(i+1)%len(outline)][1]-outline[(i+1)%len(outline)][0]*outline[i][1] for i in range(len(outline)))
    if area<0:outline=list(reversed(outline))
    n=len(outline);vertices=[(x,y,z) for y in (ylo,yhi) for x,z in outline]
    faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
    faces.extend((i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n))
    return mesh(name,vertices,faces,swatch,material)
root=empty('asset_root',(0,0,0));root['asset']='wind_blower'
root['forward_axis']='Blender +Y / glTF -Z';root['palette']='Original Kenney colormap'
root['variant']='Four-blade axial fan with protective ring, orange U-yoke and rear grille'
yaw=empty('yaw_pivot',(0,0,.350),root)
spin=empty('rotor_pivot',(0,.300,.610),yaw)
base_parts=[];yaw_parts=[];rotor_parts=[]
base_parts.append(lathe('wind_foot',[(.380,0),(.380,.055),(.600,.055),(.600,.170)],WOOD))
base_parts.append(lathe('wind_pedestal',[(.432,.162),(.432,.196),(.337,.333),(.313,.353)],ORANGE))
for i in range(4):
    a=math.pi/4+i*math.pi/2
    brace=prism('wind_base_brace_%02d'%i,[(-.083,.150),(.083,.150),(.064,.318),(.043,.358),(-.043,.358),(-.064,.318)],.325,.447,STEEL)
    for v in brace.data.vertices:v.co.x+=(.35-v.co.z)*.38
    brace.rotation_euler.z=a;base_parts.append(brace)
    bolt=lathe('wind_base_bolt_%02d'%i,[(.041,.475),(.041,.528)],DARK_STEEL,8,'X',(0,0,.230),False)
    bolt.rotation_euler.z=a;base_parts.append(bolt)
yaw_parts.append(lathe('wind_turntable',[(.308,.350),(.360,.365),(.360,.412),(.336,.429)],DARK_STEEL))
yaw_parts.append(lathe('wind_yoke_seat',[(.249,.422),(.287,.435),(.287,.463)],ORANGE))
outline=[(-.220,.444),(.220,.444),(.427,.580),(.427,.980),(.397,1.030),(.362,1.030),
    (.337,.995),(.337,.620),(.200,.520),(-.200,.520),(-.337,.620),(-.337,.995),
    (-.362,1.030),(-.397,1.030),(-.427,.980),(-.427,.580)]
yaw_parts.append(plate('wind_u_yoke',outline,-.080,.120,ORANGE))
# Short shafts physically join the head to the side yoke, without crossing the fan opening.
for sign in (-1,1):
    p=[(.037,.312),(.037,.450)]
    if sign<0:p=[(rr,-xx) for rr,xx in reversed(p)]
    yaw_parts.append(lathe('wind_side_axle_'+('left' if sign<0 else 'right'),p,DARK_STEEL,8,'X',(0,.020,.960),False))
    p=[(.056,.420),(.066,.435),(.066,.460),(.054,.472)]
    if sign<0:p=[(rr,-xx) for rr,xx in reversed(p)]
    yaw_parts.append(lathe('wind_side_bolt_'+('left' if sign<0 else 'right'),p,DARK_STEEL,8,'X',(0,.020,.960),False))
# Hollow short barrel, whose entire inner bore clears the blade swept volume.
yaw_parts.append(lathe('wind_duct',[(.300,-.200),(.325,-.164),(.325,.280),(.305,.325),(.266,.325),(.266,-.164),(.280,-.200)],ORANGE,12,'Y',(0,0,.960),closed=True))
yaw_parts.append(lathe('wind_duct_liner',[(.270,-.166),(.270,.326),(.262,.326),(.262,-.166)],CHARCOAL,12,'Y',(0,0,.960),closed=True))
yaw_parts.append(lathe('wind_front_rim',[(.302,.309),(.355,.328),(.355,.418),(.333,.440),(.266,.440),(.266,.309)],STEEL,12,'Y',(0,0,.960),closed=True))
yaw_parts.append(lathe('wind_duct_band',[(.324,.105),(.339,.119),(.339,.152),(.324,.166),(.315,.152),(.315,.119)],ORANGE,12,'Y',(0,0,.960),closed=True))
for i in range(4):
    a=math.pi/4+i*math.pi/2
    rib=box_profile('wind_barrel_rib_%02d'%i,[(.018,.014,.004,-.177),(.018,.014,.004,.285)],ORANGE)
    for v in rib.data.vertices:
        radial=v.co.z+.329;xx=v.co.x
        v.co.x=xx*math.cos(a)+radial*math.sin(a)
        v.co.z=.960-xx*math.sin(a)+radial*math.cos(a)
    yaw_parts.append(rib)
    x=.312*math.sin(a);z=.960+.312*math.cos(a)
    yaw_parts.append(lathe('wind_rim_bolt_%02d'%i,[(.022,.436),(.022,.459)],DARK_STEEL,8,'Y',(x,0,z),False))
# Recessed rear motor cap with a thick frame and three real grille bars.
yaw_parts.append(lathe('wind_motor_backplate',[(.250,-.254),(.288,-.236),(.288,-.205)],DARK_STEEL,12,'Y',(0,0,.960)))
yaw_parts.append(lathe('wind_rear_grille_frame',[(.200,-.296),(.245,-.283),(.245,-.246),(.185,-.246),(.185,-.296)],DARK_STEEL,12,'Y',(0,0,.960),closed=True))
yaw_parts.append(lathe('wind_rear_grille_dark',[(.194,-.264),(.194,-.252)],CHARCOAL,12,'Y',(0,0,.960),False))
for i,dz in enumerate((-.090,0,.090)):
    width=.215 if i==1 else .196
    yaw_parts.append(box_profile('wind_rear_grille_bar_%02d'%i,[(width,.016,.006,-.304),(width,.016,.006,-.280)],DARK_STEEL,.960+dz))
# Four broad swept paddles, with gentle pitch baked into their geometry.
blade_outline=[(-.028,.055),(.022,.055),(.075,.105),(.150,.170),(.120,.220),(.045,.245),(-.038,.235),(-.045,.180)]
for i in range(4):
    a=i*math.pi/2
    blade=plate('wind_blade_%02d'%i,blade_outline,-.012,.012,STEEL)
    for v in blade.data.vertices:
        x,z=v.co.x,v.co.z
        v.co.y+=.300+.30*x
        v.co.x=x*math.cos(a)+z*math.sin(a)
        v.co.z=.960-x*math.sin(a)+z*math.cos(a)
    rotor_parts.append(blade)
rotor_parts.append(lathe('wind_rotor_shaft',[(.028,-.246),(.028,.316)],DARK_STEEL,8,'Y',(0,0,.960),False))
rotor_parts.append(lathe('wind_rotor_hub',[(.058,.276),(.077,.293),(.077,.342),(.062,.360)],ORANGE,12,'Y',(0,0,.960),False))
rotor_parts.append(lathe('wind_rotor_bolt',[(.032,.357),(.032,.379)],DARK_STEEL,8,'Y',(0,0,.960),False))

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
rotor=combine('rotor_mesh',rotor_parts,spin,palette)
empty('socket_base',(0,0,0),root)
empty('socket_ui_top',(0,0,1.430),root)
empty('MUZZLE_01',(0,.479,.610),yaw)
verts=[(x,y,z) for z in (0,1.340) for y in (-.610,.610) for x in (-.610,.610)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('wind_blower_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/wind_blower.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Wind blower_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('wind_blower_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('wind_blower_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('wind_blower_camera');cam=bpy.data.objects.new('wind_blower_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'wind_blower_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'wind_blower.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['WIND_BLOWER']
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
subprocess.run([bpy.app.binary_path,'--background',str(ART/'wind_blower.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Wind blower_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('CANNON','REPAIR_STATION','WIND_BLOWER')):
    dx=(i-1)*1.65
    if label=='WIND_BLOWER':
        for src in (base,mount,rotor):
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'wind_blower_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('WIND_BLOWER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,mount,rotor) for p in o.data.polygons),'visible_meshes':3,'base_diameter':1.2,'height':1.315,'yaw':360,'rotor':360}))

