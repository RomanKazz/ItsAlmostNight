"""Create a compact stone harvester and reusable resource-family pedestal from the generated concept.

Blender --factory-startup --background --python-exit-code 1 --python tools/create_stone_harvester_model.py
Final metre units, +Y forward in Blender, drill pivot and resource sockets, no weapon muzzle; existing scenes preserved.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Matrix, Vector

import subprocess
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/models';VIEWS=ART/'stone_harvester_views';VIEWS.mkdir(parents=True,exist_ok=True)
assert not bpy.data.scenes.get('STONE_HARVESTER'), 'Use a fresh scene or save the existing manual model first'
scene=bpy.data.scenes.new('STONE_HARVESTER');scene['generated_model']='stone_harvester'
bpy.context.window.scene=scene
model=bpy.data.collections.new('STONE_HARVESTER_MODEL');scene.collection.children.link(model)
collision=bpy.data.collections.new('STONE_HARVESTER_COLLISION');scene.collection.children.link(collision)
studio=bpy.data.collections.new('STONE_HARVESTER_STUDIO');scene.collection.children.link(studio)
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
image=bpy.data.images.load(str(ROOT/'assets/models/buildings/Textures/colormap.png'),check_existing=False)
image.name='stone_harvester_colormap';image.pack()
palette=bpy.data.materials.new('stone_harvester_palette');palette.use_nodes=True
shader=next(n for n in palette.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
tex=palette.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
palette.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
WOOD=(.09375,.275,.475);ORANGE=(.84375,.275,.475);LIGHT_WOOD=(.34375,.275,.475)
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
root=empty('asset_root',(0,0,0));root['asset']='stone_harvester'
root['forward_axis']='Blender +Y / glTF -Z';root['family']=FAMILY_ID
root['variant']='Vertical helical stone auger, supported overhead drive, rock cradle and output stones'
drill=empty('drill_pivot',(0,-.030,.610),root)
base_parts=build_base(lathe,ORANGE,STEEL,DARK_STEEL)
module_parts=[];drill_parts=[];resource_parts=[]
# Rear frame is physically seated on the family deck and supports the drive head.
for side,x in (('left',-.222),('right',.222)):
    foot=box_profile('stone_frame_foot_'+side,[(.062,.028,.008,-.204),(.062,.028,.008,-.077)],DARK_STEEL,.233)
    for v in foot.data.vertices:v.co.x+=x
    module_parts.append(foot)
    mast=box_profile('stone_frame_mast_'+side,[(.032,.259,.008,-.174),(.032,.259,.008,-.117)],DARK_STEEL,.502)
    for v in mast.data.vertices:v.co.x+=x
    module_parts.append(mast)
    bolt=lathe('stone_frame_bolt_'+side,[(.022,-.125),(.022,-.094)],STEEL,8,'Y',(x,0,.300),False)
    module_parts.append(bolt)
module_parts.append(box_profile('stone_crossbeam',[(.255,.031,.010,-.181),(.255,.031,.010,-.050)],STEEL,.755))
module_parts.append(lathe('stone_drive_head',[(.110,.727),(.132,.746),(.132,.821),(.108,.846)],ORANGE,8,center=(0,-.030,0),smooth=False))
module_parts.append(lathe('stone_drive_cap',[(.095,.840),(.111,.854),(.111,.876),(.090,.890)],DARK_STEEL,8,center=(0,-.030,0),smooth=False))
module_parts.append(lathe('stone_spindle_bearing',[(.055,.695),(.071,.711),(.071,.741)],DARK_STEEL,12,center=(0,-.030,0)))
# An attached side motor gives the vertical tool a clear mechanical drive.
module_parts.append(lathe('stone_motor',[(.052,.104),(.064,.122),(.064,.247),(.050,.264)],ORANGE,8,'X',(0,-.030,.785),False))
module_parts.append(lathe('stone_motor_end',[(.047,.259),(.047,.274)],DARK_STEEL,8,'X',(0,-.030,.785),False))
for i,z in enumerate((.766,.785,.804)):
    vent=box_profile('stone_motor_vent_%02d'%i,[(.003,.004,.002,-.057),(.003,.004,.002,-.003)],CHARCOAL,z)
    for v in vent.data.vertices:v.co.x+=.276
    module_parts.append(vent)
# Low bed plus three separated clamp jaws, all below the auger sweep.
module_parts.append(lathe('stone_rock_bed',[(.186,.204),(.207,.223),(.207,.244)],DARK_STEEL,8,center=(0,.075,0),smooth=False))
for i,a in enumerate((0,2.15,4.13)):
    jaw=prism('stone_rock_jaw_%02d'%i,[(-.041,.229),(.041,.229),(.041,.306),(.025,.363),(-.025,.363),(-.041,.306)],.150,.207,STEEL)
    for v in jaw.data.vertices:
        x,y=v.co.x,v.co.y
        v.co.x=x*math.cos(a)-y*math.sin(a)
        v.co.y=.075+x*math.sin(a)+y*math.cos(a)
    module_parts.append(jaw)
# Faceted rocks use the original palette, with no new textures or vertex colours.
def rock(name,center,size,phase=0):
    import bmesh
    bm=bmesh.new()
    for ring,z in enumerate((-.50,-.10,.38)):
        rr=(.65,1.0,.60)[ring]
        for i in range(7):
            a=i*math.tau/7+phase+(ring%2)*.19
            stretch=1+.10*math.sin(i*2.4+ring)
            bm.verts.new((center[0]+math.cos(a)*size[0]*.5*rr*stretch,
                          center[1]+math.sin(a)*size[1]*.5*rr*stretch,
                          center[2]+(z+.06*math.sin(i*1.7+ring))*size[2]))
    result=bmesh.ops.convex_hull(bm,input=list(bm.verts),use_existing_faces=False)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.verts.ensure_lookup_table();bm.verts.index_update()
    verts=[tuple(v.co) for v in bm.verts];faces=[tuple(v.index for v in f.verts) for f in bm.faces]
    bm.free();return mesh(name,verts,faces,ROCK)
resource_parts.append(rock('stone_work_rock',(0,.075,.307),(.300,.270,.150),.3))
# A small side cradle of finished stones keeps the resource identity visible.
tray=box_profile('stone_output_tray',[(.073,.011,.006,.108),(.073,.011,.006,.338)],STEEL,.245)
for v in tray.data.vertices:v.co.x-=.285
module_parts.append(tray)
for i,y in enumerate((.137,.269)):
    leg=box_profile('stone_output_foot_%02d'%i,[(.043,.020,.005,y-.014),(.043,.020,.005,y+.014)],DARK_STEEL,.224)
    for v in leg.data.vertices:v.co.x-=.285
    module_parts.append(leg)
resource_parts.append(rock('stone_output_rock_00',(-.287,.179,.292),(.113,.114,.079),.1))
resource_parts.append(rock('stone_output_rock_01',(-.280,.286,.286),(.107,.090,.065),.7))
# Tapered helical flight: a real closed volume around a tapered central core.
drill_parts.append(lathe('stone_auger_core',[(.032,.438),(.037,.455),(.043,.685),(.043,.719)],DARK_STEEL,12,center=(0,-.030,0)))
drill_parts.append(lathe('stone_drill_tip',[(.012,.395),(.033,.426),(.044,.450)],STEEL,8,center=(0,-.030,0),smooth=False))
verts=[];faces=[];steps=80
for i in range(steps+1):
    t=i/steps;a=t*math.tau*2.05;z=.435+t*.243
    outer=.047+.077*t;inner=.037+.003*t
    for rr,dz in ((inner,-.010),(outer,-.010),(outer,.010),(inner,.010)):
        verts.append((math.cos(a)*rr,-.030+math.sin(a)*rr,z+dz))
for i in range(steps):
    for j in range(4):faces.append((i*4+j,i*4+(j+1)%4,(i+1)*4+(j+1)%4,(i+1)*4+j))
faces.extend((tuple(reversed(range(4))),tuple(steps*4+j for j in range(4))))
# Ring order makes the flight inward; reverse once, consistently.
faces=[tuple(reversed(f)) for f in faces]
drill_parts.append(mesh('stone_auger_flight',verts,faces,STEEL))

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
drill_mesh=combine('drill_mesh',drill_parts,drill,palette)
empty('socket_base',(0,0,0),root)
empty('socket_resource_mount',(0,0,MOUNT_HEIGHT),root)
empty('socket_ui_top',(0,0,.995),root)
empty('socket_output',(-.285,.355,.280),root)
empty('socket_effect_dust',(0,.075,.390),root)
verts=[(x,y,z) for z in (0,.920) for y in (-.480,.480) for x in (-.480,.480)]
faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]
data=bpy.data.meshes.new('COL_BOX_BODY');data.from_pydata(verts,[],faces)
col=bpy.data.objects.new('COL_BOX_BODY',data);collision.objects.link(col);col.parent=root
col.display_type='WIRE';col.hide_render=True
diagnostic=bpy.data.materials.new('stone_harvester_collision');diagnostic.use_nodes=True
shader=next(n for n in diagnostic.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
shader.inputs['Base Color'].default_value=(1,.1,.7,1);shader.inputs['Metallic'].default_value=0;shader.inputs['Roughness'].default_value=1
data.materials.append(diagnostic)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in list(model.objects)+list(collision.objects):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/stone_harvester.glb'),use_selection=True,
    use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,
    export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=1200
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='Standard';scene.view_settings.exposure=-.4
scene.world=bpy.data.worlds.new('Stone harvester_world');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND')
bg.inputs['Color'].default_value=(.18,.21,.25,1);bg.inputs['Strength'].default_value=.6
for name,pos,power,size in [('key',(2,4,6),400,6),('fill',(-4,2,4),260,6),('rim',(0,-4,5),400,4)]:
    data=bpy.data.lights.new('stone_harvester_'+name,'AREA');data.energy=power;data.size=size
    o=bpy.data.objects.new('stone_harvester_'+name,data);studio.objects.link(o);o.location=pos;aim(o,(0,0,.65))
data=bpy.data.cameras.new('stone_harvester_camera');cam=bpy.data.objects.new('stone_harvester_camera',data);studio.objects.link(cam);scene.camera=cam;data.type='ORTHO'
views=[('beauty',(-2.8,4,2.7),(0,0,.425),1.42),('front',(0,4,.44),(0,0,.44),1.20),('side',(-4,0,.44),(0,0,.44),1.20),('back',(0,-4,.44),(0,0,.44),1.20),('top',(0,.001,4),(0,0,0),1.20)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'stone_harvester_preview.png');bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.data.libraries.write(str(ART/'stone_harvester.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize = """import bpy
s=bpy.data.scenes['STONE_HARVESTER']
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
subprocess.run([bpy.app.binary_path,'--background',str(ART/'stone_harvester.blend'),
                '--python-exit-code','1','--python-expr',finalize],check=True)

comparison=bpy.data.scenes.new('Stone harvester_comparison');bpy.context.window.scene=comparison
comparison.world=scene.world;comparison.render.engine=scene.render.engine
comparison.view_settings.view_transform=scene.view_settings.view_transform;comparison.view_settings.exposure=scene.view_settings.exposure
for i,label in enumerate(('WOOD_HARVESTER','CRYSTAL_MINE','STONE_HARVESTER')):
    dx=(i-1)*1.65
    if label=='STONE_HARVESTER':
        for src in (base,module,resource,drill_mesh):
            o=bpy.data.objects.new('comparison_'+src.name,src.data);comparison.collection.objects.link(o)
            o.matrix_world=src.matrix_world.copy();o.location.x+=dx
    else:
        before=set(comparison.objects)
        path=ROOT/('assets/models/buildings/mine.glb' if label=='CRYSTAL_MINE' else 'assets/models/buildings/wood_harvester.glb')
        bpy.ops.import_scene.gltf(filepath=str(path));bpy.context.view_layer.update()
        imported=set(comparison.objects)-before;transforms={o:o.matrix_world.copy() for o in imported if o.type=='MESH'}
        ordered=sorted(transforms,key=lambda o:o.name)
        # Runtime draws the L1 replacement housing and original shards, not all baked levels.
        if label=='CRYSTAL_MINE':
            chosen=[o for o in ordered if o.name.startswith('upgrade_crystal_mine_l1')]
            shard=next(o for o in ordered if o.name.startswith('detail-crystal-large'))
            # The original mine contains body and crystals as two primitives; isolate its shard material.
            data=shard.data.copy();copy=bpy.data.objects.new('comparison_crystal_shards',data);comparison.collection.objects.link(copy)
            copy.matrix_world=transforms[shard];chosen.append(copy)
            import bmesh
            bm=bmesh.new();bm.from_mesh(data)
            # Purple crystal faces use U=.46875; the old green housing uses U=.59375.
            uv=bm.loops.layers.uv.active
            discard=[f for f in bm.faces if all(loop[uv].uv.x>.5 for loop in f.loops)]
            bmesh.ops.delete(bm,geom=discard,context='FACES');bm.to_mesh(data);bm.free()
            transforms[copy]=copy.matrix_world.copy()
        else:chosen=[o for o in ordered if not o.name.startswith('COL_')]
        for o in list(transforms):
            if o not in chosen:o.hide_render=True
        factor=1.05 if label=='CRYSTAL_MINE' else 1.0
        for o in chosen:
            matrix=transforms[o];o.parent=None
            o.matrix_world=Matrix.Translation((dx,0,0))@Matrix.Scale(factor,4)@matrix
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
comparison.render.image_settings.file_format='PNG';comparison.render.filepath=str(ART/'stone_harvester_comparison.png');bpy.ops.render.render(write_still=True)
bpy.context.window.scene=scene
print('STONE_HARVESTER_CREATED',json.dumps({'visible_triangles':sum(len(p.vertices)-2 for o in (base,module,resource,drill_mesh) for p in o.data.polygons),'visible_meshes':4,'width':.918,'height':.890,'family':FAMILY_ID}))

