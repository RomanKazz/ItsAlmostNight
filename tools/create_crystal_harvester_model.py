"""Create a compact crystal harvester and reusable resource-family pedestal from the generated concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_crystal_harvester_model.py
Final metre units, +Y forward in Blender, resource/effect sockets, no weapon muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'crystal_harvester_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('CRYSTAL_HARVESTER'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('CRYSTAL_HARVESTER');scene['generated_model']='crystal_harvester'
bpy.context.window.scene=scene
model=bpy.data.collections.new('CRYSTAL_HARVESTER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('CRYSTAL_HARVESTER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('CRYSTAL_HARVESTER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='crystal_harvester_colormap';image.pack()
palette=bpy.data.materials.new('crystal_harvester_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.275,.475);CRYSTAL=(.46875,.525,.725);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
STEEL=(.84375,.025,.225);DARK_STEEL=(.59375,.025,.225)
CHARCOAL=(.46875,.025,.225);ROCK=(.53125,.025,.225)

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
import sys
sys.path.insert(0,str(ROOT/'tools'))
from harvester_family_base import build_base,FAMILY_ID,MOUNT_HEIGHT
root=empty('asset_root',(0,0,0));root['asset']='crystal_harvester'
root['forward_axis']='Blender +Y / glTF -Z';root['family']=FAMILY_ID
root['variant']='Three faceted purple crystals, four extraction prongs and a front shard tray'
base_parts=build_base(lathe,ORANGE,STEEL,DARK_STEEL)
module_parts=[];resource_parts=[]
# A supported extraction cup surrounds the roots of the three crystals.
module_parts.append(lathe('crystal_socket_drum',[(.218,.204),(.239,.224),(.239,.276),(.217,.294)],ORANGE,8,smooth=False,phase=math.pi/8))
module_parts.append(lathe('crystal_socket_rim',[(.222,.271),(.250,.283),(.250,.307),(.213,.307),(.213,.275)],STEEL,8,phase=math.pi/8,closed=True))
module_parts.append(lathe('crystal_extraction_bed',[(.216,.270),(.216,.298)],DARK_STEEL,8,smooth=False,phase=math.pi/8))
# Four broad prongs form a static extraction cradle. Their feet are inside the deck.
for i in range(4):
    a=i*math.pi/2
    foot=plate('crystal_prong_foot_%02d'%i,[(.246,.204),(.344,.204),(.353,.219),(.340,.260),(.246,.260)],-.055,.055,ORANGE)
    foot.rotation_euler.z=a;module_parts.append(foot)
    arm=plate('crystal_extraction_prong_%02d'%i,[(.286,.249),(.339,.249),(.365,.318),(.335,.510),(.309,.540),(.280,.526),(.303,.321)],-.042,.042,STEEL)
    arm.rotation_euler.z=a;module_parts.append(arm)
    bolt=lathe('crystal_prong_bolt_%02d'%i,[(.021,.310),(.021,.380)],DARK_STEEL,8,'X',(0,0,.260),False)
    bolt.rotation_euler.z=a;module_parts.append(bolt)
# A small supported front tray shows already extracted fragments.
tray=box_profile('crystal_output_tray',[(.075,.012,.008,.210),(.075,.012,.008,.350)],STEEL,.243)
for v in tray.data.vertices:v.co.x-=.200
module_parts.append(tray)
for i,y in enumerate((.230,.305)):
    foot=box_profile('crystal_output_foot_%02d'%i,[(.055,.018,.005,y-.014),(.055,.018,.005,y+.014)],DARK_STEEL,.222)
    for v in foot.data.vertices:v.co.x-=.200
    module_parts.append(foot)
# Closed hexagonal prisms with broad irregular facets and pointed tips.
# Tilt is baked into vertices; the root and render meshes retain identity TRS.
def crystal(name,center,radius,height,lean=(0,0),phase=0):
    n=6;verts=[];faces=[]
    for rr,h in ((.70,0),(1.0,.12),(.94,.70),(.61,.86)):
        for j in range(n):
            a=phase+j*math.tau/n
            uneven=1+.045*math.sin(j*2.3)
            verts.append((center[0]+math.cos(a)*radius*rr*uneven+lean[0]*h,
                          center[1]+math.sin(a)*radius*rr*uneven+lean[1]*h,
                          center[2]+height*h))
    tip=len(verts);verts.append((center[0]+lean[0]+radius*.07,center[1]+lean[1]-radius*.05,center[2]+height))
    faces.append(tuple(reversed(range(n))))
    for ring in range(3):
        for j in range(n):faces.append((ring*n+j,ring*n+(j+1)%n,(ring+1)*n+(j+1)%n,(ring+1)*n+j))
    for j in range(n):faces.append((3*n+j,3*n+(j+1)%n,tip))
    return mesh(name,verts,faces,CRYSTAL)
resource_parts.append(crystal('crystal_main',(0,-.035,.281),.137,.569,(.010,-.020),math.pi/6))
resource_parts.append(crystal('crystal_side_left',(-.145,.065,.281),.100,.319,(-.025,.015),.13))
resource_parts.append(crystal('crystal_side_right',(.145,.065,.281),.101,.374,(.026,.009),.43))
resource_parts.append(crystal('crystal_output_shard_00',(-.238,.273,.249),.031,.105,(-.009,.005),.20))
resource_parts.append(crystal('crystal_output_shard_01',(-.165,.283,.249),.027,.082,(.009,-.006),.60))

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
module=combine('module_mesh',module_parts,root,palette)
resource=combine('resource_mesh',resource_parts,root,palette)
empty('socket_base',(0,0,0),root)
empty('socket_resource_mount',(0,0,MOUNT_HEIGHT),root)
empty('socket_ui_top',(0,0,.970),root)
empty('socket_output',(-.200,.370,.285),root)
empty('socket_effect_extraction',(0,0,.460),root)
verts=[(x,y,z) for z in (0,.900) for y in (-.480,.480) for x in (-.480,.480)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('crystal_harvester_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/crystal_harvester.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Crystal harvester_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('crystal_harvester_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('crystal_harvester_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('crystal_harvester_camera');cam=bpy.data.objects.new('crystal_harvester_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.425),1.42),('front',(0,4,.44),(0,0,.44),1.20),('side',(-4,0,.44),(0,0,.44),1.20),('back',(0,-4,.44),(0,0,.44),1.20),('top',(0,.001,4),(0,0,0),1.20)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'crystal_harvester_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'crystal_harvester.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['CRYSTAL_HARVESTER']
bpy.context.window.scene=s
s.objects['COL_BOX_BODY'].hide_set(True)
for other in list(bpy.data.scenes):
    if other!=s:bpy.data.scenes.remove(other)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.overlay.show_extras=False
        area.spaces.active.overlay.show_relationship_lines=False
        area.spaces.active.region_3d.view_perspective='CAMERA'
        area.spaces.active.shading.type='MATERIAL'
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,check_existing=False)
"""
subprocess.run([bpy.app.binary_path,'--background',str(ART/'crystal_harvester.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Crystal harvester_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('WOOD_HARVESTER','STONE_HARVESTER','CRYSTAL_HARVESTER')):
    dx=(i-1)*1.65
    if label=='CRYSTAL_HARVESTER':
        for src in (base,module,resource):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('assets/models/buildings/stone_harvester.glb' if label=='STONE_HARVESTER' else 'assets/models/buildings/wood_harvester.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before
        for o in imported:
            if o.type=='MESH':
                if o.name.startswith('COL_'):o.hide_render=True;continue
                matrix=o.matrix_world.copy();o.parent=None
                o.matrix_world=Matrix.Translation((dx,0,0))@matrix
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'crystal_harvester_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('CRYSTAL_HARVESTER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,module,resource) for p in o.data.polygons),'visible_meshes':3,'width':.918,'height':.850,'family':FAMILY_ID}))

