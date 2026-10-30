"""Author full standalone levels 1..8 for all three compact harvesters.
Run in an isolated background Blender. Original models and gameplay are never written.
Style references: Tesla, flamethrower, mortar gold/red cumulative upgrades.
"""
from pathlib import Path
import bpy, importlib.util, hashlib, json, math, shutil, sys
from mathutils import Vector,Matrix
assert bpy.app.background,'Use an isolated background Blender'
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/harvester_levels';ART.mkdir(parents=True,exist_ok=True)
OUT=ROOT/'assets/models/buildings/harvester_levels';OUT.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('harvester_level_geometry',ROOT/'tools/create_tesla_upgrades.py')
family=importlib.util.module_from_spec(spec);spec.loader.exec_module(family);Geometry=family.Geometry
GOLD=(.65625,.285,.465);RED=(.96875,.285,.465);ORANGE=(.84375,.275,.475)
STEEL=(.84375,.025,.225);DARK=(.59375,.025,.225);BARK=(.15625,.275,.475)
END=(.34375,.275,.475);GRAIN=(.09375,.275,.475);PURPLE=(.46875,.525,.725)
ASSETS=['wood_harvester','stone_harvester','crystal_harvester']
ORIGINALS={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for a in ASSETS for p in (ROOT/f'art/models/{a}.blend',ROOT/f'assets/models/buildings/{a}.glb')}
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
def shift(g,start,pose):
    for i in range(start,len(g.verts)):g.verts[i]=tuple(pose@Vector(g.verts[i]))
def ring(g,name,lo,hi,inner,outer,swatch,center=(0,0),sides=8,phase=0,axis='Z',origin=(0,0,0)):
    start=len(g.verts);g.ring(name,lo,hi,inner,outer,swatch,center,sides)
    pose=Matrix.Rotation(phase,4,'Z')
    if axis=='Y':pose=Matrix.Rotation(-math.pi/2,4,'X')@pose
    elif axis=='X':pose=Matrix.Rotation(math.pi/2,4,'Y')@pose
    shift(g,start,Matrix.Translation(origin)@pose)
def lathe(g,name,profile,swatch,center=(0,0),sides=8,axis='Z',origin=(0,0,0)):
    start=len(g.verts);g.lathe(name,profile,swatch,center,sides)
    pose=Matrix.Identity(4)
    if axis=='Y':pose=Matrix.Rotation(-math.pi/2,4,'X')
    elif axis=='X':pose=Matrix.Rotation(math.pi/2,4,'Y')
    shift(g,start,Matrix.Translation(origin)@pose)
def box(g,name,center,size,swatch):
    x,y,z=center;w,d,h=size;start=len(g.verts)
    g.prism(name,[(x-w/2,z-h/2),(x+w/2,z-h/2),(x+w/2,z+h/2),(x-w/2,z+h/2)],d/2,0,swatch)
    for i in range(start,len(g.verts)):a,b,c=g.verts[i];g.verts[i]=(a,b+y,c)
def merge(g,obj,collection):
    if not g.verts:return
    mat=obj.data.materials[0];tmp=g.object('upgrade_temporary',collection,mat)
    matrix=obj.matrix_world.inverted();verts=[tuple(v.co) for v in obj.data.vertices]+[tuple(matrix@v.co) for v in tmp.data.vertices]
    offset=len(obj.data.vertices);faces=[tuple(p.vertices) for p in obj.data.polygons]+[tuple(offset+j for j in p.vertices) for p in tmp.data.polygons]
    uvs=[tuple(x.uv) for x in obj.data.uv_layers.active.data]+[tuple(x.uv) for x in tmp.data.uv_layers.active.data]
    smooth=[p.use_smooth for p in obj.data.polygons]+[p.use_smooth for p in tmp.data.polygons]
    old=obj.data
    prior_groups=[(g.name,[v.index for v in old.vertices if any(w.group==g.index for w in v.groups)]) for g in obj.vertex_groups]
    data=bpy.data.meshes.new(obj.name+'_data');data.from_pydata(verts,[],faces);data.update();data.materials.append(mat)
    uv=data.uv_layers.new(name='UVMap')
    for x,co in zip(uv.data,uvs):x.uv=co
    for p,flag in zip(data.polygons,smooth):p.use_smooth=flag
    # Existing vertex groups survive replacement; new ranges have unique descriptive names.
    obj.data=data
    obj.vertex_groups.clear()
    for name,members in prior_groups:
        group=obj.vertex_groups.new(name=name)
        if members:group.add(members,1,'REPLACE')
    for name,(start,count) in g.groups.items():obj.vertex_groups.new(name='upg_'+name).add(list(range(offset+start,offset+start+count)),1,'REPLACE')
    bpy.data.objects.remove(tmp,do_unlink=True)
    data.set_sharp_from_angle(angle=math.radians(45))
def recolor(obj,names,swatch):
    for name in names:
        group=obj.vertex_groups.get(name)
        if not group:continue
        ids={v.index for v in obj.data.vertices if any(w.group==group.index for w in v.groups)}
        zs=[obj.data.vertices[i].co.z for i in ids];lo=min(zs);hi=max(zs)
        for p in obj.data.polygons:
            if all(i in ids for i in p.vertices):
                for li in p.loop_indices:
                    z=obj.data.vertices[obj.data.loops[li].vertex_index].co.z
                    obj.data.uv_layers.active.data[li].uv=(swatch[0],swatch[1]+(swatch[2]-swatch[1])*(z-lo)/max(hi-lo,1e-6))
def common(g,level):
    if level>=2:ring(g,'gold_lower_belt',.069,.095,.451,.490,GOLD,phase=math.pi/8)
    if level>=4:ring(g,'gold_upper_rim',.202,.223,.410,.481,GOLD,phase=math.pi/8)
    if level>=6:ring(g,'second_foot_belt',.024,.047,.451,.492,GOLD,phase=math.pi/8)
    if level>=7:ring(g,'red_master_band',.143,.171,.443,.480,RED,phase=math.pi/8)
    if level>=8:
        for i in range(4):
            a=i*math.pi/2
            g.prism('master_base_armor_%02d'%i,[(.426,.049),(.468,.049),(.470,.137),(.448,.173),(.425,.173)],.062,a,GOLD)
            # Replacement washers/bolts remain outside the new armor face.
            start=len(g.verts);lathe(g,'master_washer_%02d'%i,[(.029,.462),(.029,.472)],GOLD,axis='X',origin=(0,0,.110))
            shift(g,start,Matrix.Rotation(a,4,'Z'))
            start=len(g.verts);lathe(g,'master_bolt_%02d'%i,[(.020,.470),(.020,.479)],DARK,axis='X',origin=(0,0,.110))
            shift(g,start,Matrix.Rotation(a,4,'Z'))
def wood(level,scene):
    fixed=Geometry();moving=Geometry();resource=Geometry();module=scene.objects['module_mesh']
    if level>=2:ring(moving,'hub_gold_washer',.016,.032,.027,.063,GOLD,axis='Y',origin=(-.100,0,.630))
    if level>=3:
        for i,(lo,hi) in enumerate(((-.298,-.279),(-.211,-.187))):ring(fixed,'motor_band_%02d'%i,lo,hi,.071,.091,GOLD,axis='Y',origin=(-.100,0,.630))
    if level>=4:
        recolor(module,['wood_saw_frame','wood_stock_cradle_00','wood_stock_cradle_01'],GOLD)
        ring(moving,'blade_inner_gold_band',-.049,-.038,.128,.154,GOLD,axis='Y',origin=(-.100,0,.630),sides=16)
    if level>=5:
        # Radial guard extension is outside the full tooth envelope.
        outline=[]
        for rad,angles in ((.292,[1.30+2.19*i/16 for i in range(17)]),(.277,[1.30+2.19*i/16 for i in reversed(range(17))])):
            outline.extend((-.100+rad*math.sin(a),.630+rad*math.cos(a)) for a in angles)
        # Geometry.prism radial helper at angle 0 is a plate in X/Z, tangential width Y.
        fixed.prism('gold_saw_guard',outline,.047,0,GOLD)
        start,count=fixed.groups['gold_saw_guard']
        for i in range(start,start+count):x,y,z=fixed.verts[i];fixed.verts[i]=(x,y-.056,z)
    if level>=6:
        lathe(fixed,'motor_red_extension',[(.072,-.383),(.080,-.373),(.080,-.328)],RED,axis='Y',origin=(-.100,0,.630))
        ring(fixed,'motor_rear_gold_cap',-.390,-.375,.025,.088,GOLD,axis='Y',origin=(-.100,0,.630))
        top=.844 if level>=8 else .720
        for i,y in enumerate((-.186,.143)):
            for j,x in enumerate((.165,.335)):box(fixed,'rack_post_%d_%d'%(i,j),(x,y,(.536+top)/2),(.025,.029,top-.536),GOLD)
        # Slight intended bark contact seats the stack without coincident tangent edges.
        for i,z in enumerate((.620,.750) if level>=8 else (.620,)):
            lathe(resource,'reserve_log_%d_bark'%i,[(.059,-.233),(.066,-.219),(.066,.199),(.060,.213)],BARK,axis='Y',origin=(.250,0,z))
            lathe(resource,'reserve_log_%d_end'%i,[(.060,.212),(.060,.216)],END,axis='Y',origin=(.250,0,z))
            ring(resource,'reserve_log_%d_ring'%i,.2165,.2175,.030,.033,GRAIN,axis='Y',origin=(.250,0,z))
    if level>=7:
        top=.844 if level>=8 else .720
        for i,y in enumerate((-.186,.143)):box(fixed,'rack_crossbar_%d'%i,(.250,y,top),(.196,.030,.027),GOLD)
    if level>=8:ring(moving,'master_rotor_hub',.005,.014,.070,.094,GOLD,axis='Y',origin=(-.100,0,.630))
    return fixed,moving,resource

def stone(level,scene):
    fixed=Geometry();moving=Geometry();resource=Geometry();module=scene.objects['module_mesh']
    if level>=2:ring(fixed,'drive_gold_collar',.736,.757,.119,.143,GOLD,center=(0,-.030))
    if level>=3:
        ring(fixed,'bearing_gold_collar',.704,.728,.058,.080,GOLD,center=(0,-.030),sides=12)
        ring(fixed,'motor_gold_collar',.239,.267,.046,.075,GOLD,axis='X',origin=(0,-.030,.785))
    if level>=4:
        recolor(module,['stone_frame_mast_left','stone_frame_mast_right','stone_frame_foot_left','stone_frame_foot_right'],GOLD)
        recolor(module,['stone_frame_bolt_left','stone_frame_bolt_right'],DARK)
    if level>=5:
        for i,axial in enumerate((.135,.183,.231)):ring(fixed,'motor_cooling_fin_%d'%i,axial,axial+.016,.058,.081,GOLD,axis='X',origin=(0,-.030,.785))
    if level>=6:
        for i,x in enumerate((-.222,.222)):
            c=(x,-.090)
            lathe(fixed,'hydraulic_seat_%d'%i,[(.042,.252),(.044,.268),(.044,.292)],GOLD,c)
            lathe(fixed,'hydraulic_red_cell_%d'%i,[(.032,.281),(.035,.298),(.035,.578)],RED,c)
            lathe(fixed,'hydraulic_top_%d'%i,[(.041,.568),(.041,.600)],GOLD,c)
            lathe(fixed,'hydraulic_rod_%d'%i,[(.017,.590),(.017,.730)],STEEL,c)
    if level>=7:
        box(fixed,'master_crossbeam',(0,-.165,.797),(.560,.035,.028),GOLD)
        lathe(fixed,'drive_gold_cap',[(.108,.880),(.117,.890),(.097,.904)],GOLD,(0,-.030))
    if level>=8:
        recolor(scene.objects['drill_mesh'],['stone_auger_flight','stone_drill_tip'],GOLD)
        recolor(module,['stone_rock_jaw_00','stone_rock_jaw_01','stone_rock_jaw_02'],GOLD)
    return fixed,moving,resource

def crystal(level,scene):
    fixed=Geometry();moving=Geometry();resource=Geometry();module=scene.objects['module_mesh']
    if level>=2:ring(fixed,'cup_gold_rim',.289,.317,.209,.258,GOLD,phase=math.pi/8)
    if level>=3:
        for i in range(4):fixed.prism('gold_prong_tip_%d'%i,[(.279,.485),(.341,.485),(.339,.512),(.310,.550),(.277,.533)],.045,i*math.pi/2,GOLD)
    if level>=4:
        recolor(module,['crystal_prong_foot_%02d'%i for i in range(4)],GOLD)
        for i in range(4):fixed.prism('gold_prong_armor_%d'%i,[(.351,.294),(.379,.312),(.359,.449),(.340,.449),(.356,.320)],.044,i*math.pi/2,GOLD)
    if level>=5:
        centers=[(-.225,-.205),(.225,-.205)]
        if level>=6:centers.append((.240,.205))
        for i,c in enumerate(centers):
            lathe(fixed,'capacitor_seat_%d'%i,[(.052,.204),(.058,.222),(.058,.253)],GOLD,c)
            lathe(fixed,'capacitor_red_cell_%d'%i,[(.042,.242),(.046,.259),(.046,.461)],RED,c)
            lathe(fixed,'capacitor_cap_%d'%i,[(.055,.450),(.055,.482),(.042,.493)],GOLD,c)
            lathe(fixed,'capacitor_purple_terminal_%d'%i,[(.025,.484),(.025,.508),(.017,.524)],PURPLE,c)
    if level>=7:
        for i in range(4):fixed.prism('crown_arm_%d'%i,[(.299,.515),(.330,.515),(.335,.650),(.225,.796),(.198,.781),(.300,.637)],.024,i*math.pi/2,GOLD)
        ring(fixed,'extraction_crown',.767,.802,.180,.225,GOLD,sides=8,phase=math.pi/8)
    if level>=8:
        recolor(module,['crystal_extraction_prong_%02d'%i for i in range(4)],GOLD)
        ring(fixed,'crown_orange_inlay',.777,.790,.219,.235,ORANGE,sides=8,phase=math.pi/8)
    return fixed,moving,resource

CHANGES={
'wood_harvester':['Исходная лесопилка','Золотой пояс и шайба ступицы','Обручи привода','Золотая рама, крепления и диск','Усиленный кожух пилы','Увеличенный мотор и третье бревно','Рамка запаса и красный пояс','Четвёртое бревно, тяжёлая подошва и ступица'],
'stone_harvester':['Исходная каменоломня','Золотой пояс и хомут привода','Усиление подшипника и мотора','Золотые стойки буровой','Рёбра охлаждения мотора','Две гидравлические стойки','Усиленная перекладина и крышка','Золотой шнек, зажимы и броня подошвы'],
'crystal_harvester':['Исходная кристальная шахта','Золотой пояс и край чаши','Золотые наконечники захватов','Броня и усиленные опоры','Два накопителя энергии','Третий накопитель','Рама и корона экстрактора','Золотые захваты, инкрустация короны и тяжёлая подошва']}
rows=[]
for asset in ASSETS:
    dst=ART/asset;dst.mkdir(exist_ok=True)
    for level in range(1,9):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        with bpy.data.libraries.load(str(ROOT/f'art/models/{asset}.blend'),link=False) as (src,data):data.scenes=[asset.upper()]
        scene=data.scenes[0];bpy.context.window.scene=scene;scene.name=f'{asset.upper()}_LVL{level:02d}'
        for other in list(bpy.data.scenes):
            if other!=scene:bpy.data.scenes.remove(other)
        bpy.context.view_layer.update()
        root=scene.objects['asset_root'];root['level']=level;root['level_kind']='complete variant'
        collection=next(c for c in scene.collection.children if c.name.endswith('_MODEL'))
        if level>=2:
            gbase=Geometry();common(gbase,level);merge(gbase,scene.objects['base_mesh'],collection)
            fixed,moving,res={'wood_harvester':wood,'stone_harvester':stone,'crystal_harvester':crystal}[asset](level,scene)
            merge(fixed,scene.objects['module_mesh'],collection);merge(res,scene.objects['resource_mesh'],collection)
            if moving.verts:merge(moving,scene.objects['saw_mesh' if asset=='wood_harvester' else 'drill_mesh'],collection)
        collider=scene.objects['COL_BOX_BODY'];collider.hide_set(False)
        # UI is root-bound and already leaves >=9 cm above every tier.
        bpy.context.view_layer.update();bpy.ops.object.select_all(action='DESELECT')
        selected=[o for o in scene.objects if o.type in ('MESH','EMPTY')]
        for o in selected:o.select_set(True)
        bpy.context.view_layer.objects.active=scene.objects['base_mesh']
        path=OUT/f'{asset}_lvl{level:02d}.glb'
        if level==1:shutil.copy2(ROOT/f'assets/models/buildings/{asset}.glb',path)
        else:bpy.ops.export_scene.gltf(filepath=str(path),use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_animations=False,export_extras=True)
        collider.hide_set(True)
        visible=[o for o in selected if o.type=='MESH' and not o.name.startswith('COL_')]
        pts=[o.matrix_world@v.co for o in visible for v in o.data.vertices]
        lo=[min(v[i] for v in pts) for i in range(3)];hi=[max(v[i] for v in pts) for i in range(3)]
        row={'asset':asset,'level':level,'changes':CHANGES[asset][level-1],'glb':str(path.relative_to(ROOT)),
            'blend':str((dst/f'level_{level:02d}.blend').relative_to(ROOT)),
            'beauty':str((dst/f'level_{level:02d}.png').relative_to(ROOT)),
            'visible_bounds_glb':{'min':[lo[0],lo[2],-hi[1]],'max':[hi[0],hi[2],-lo[1]]},
            'visible_meshes':len(visible),'triangles':sum(len(p.vertices)-2 for o in visible for p in o.data.polygons),
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=600;scene.render.resolution_y=600
        scene.render.resolution_percentage=100
        scene.camera.location=(-2.8,4,2.7);aim(scene.camera,(0,0,.45));scene.camera.data.ortho_scale=1.60
        text=bpy.data.curves.new('level_label','FONT');text.body=f'LEVEL {level}';text.align_x='CENTER';text.size=.065
        label=bpy.data.objects.new('level_label',text)
        next(c for c in scene.collection.children if c.name.endswith('_STUDIO')).objects.link(label)
        label.location=(0,.66,-.04);label.rotation_euler=(scene.camera.location-label.location).to_track_quat('Z','Y').to_euler()
        scene.render.filepath=str(ROOT/row['beauty']);bpy.ops.render.render(write_still=True)
        if level==8:
            label.hide_render=True
            for view,pos,target in [('front',(0,4,.46),(0,0,.46)),('side',(-4,0,.46),(0,0,.46)),('back',(0,-4,.46),(0,0,.46)),('top',(0,.001,4),(0,0,0))]:
                scene.camera.location=pos;aim(scene.camera,target);scene.camera.data.ortho_scale=1.20
                scene.render.filepath=str(dst/f'level_08_{view}.png');bpy.ops.render.render(write_still=True)
            scene.camera.location=(-2.8,4,2.7);aim(scene.camera,(0,0,.45));scene.camera.data.ortho_scale=1.60
            label.hide_render=False
        bpy.ops.object.select_all(action='DESELECT');bpy.context.view_layer.objects.active=None
        for area in bpy.context.screen.areas:
            if area.type=='VIEW_3D':
                area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
                area.spaces.active.overlay.show_extras=False
        bpy.context.preferences.filepaths.save_version=0
        bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/row['blend']),check_existing=False)
        rows.append(row);print('HARVESTER_TIER',asset,level,row['triangles'],flush=True)

# Editable overview: all twenty-four variants under the same light.
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.name='HARVESTER_LEVELS_OVERVIEW'
family.studio(scene);scene.render.engine='BLENDER_EEVEE'
for row in rows:
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ROOT/row['glb']));bpy.context.view_layer.update()
    imported=set(scene.objects)-before
    for o in imported:
        if o.type=='MESH' and not o.name.startswith('COL_'):
            matrix=o.matrix_world.copy();o.parent=None
            o.name=f'{row["asset"]}_l{row["level"]:02d}_{o.name.split(".")[0]}'
            o.matrix_world=Matrix.Translation((-(row['level']-4.5)*1.13,(ASSETS.index(row['asset'])-1)*2.25,0))@matrix
    for o in imported:
        if o.type!='MESH' or o.name.startswith('COL_'):bpy.data.objects.remove(o,do_unlink=True)
    text=bpy.data.curves.new('label','FONT');text.body=f'{row["asset"].split("_")[0].upper()}  {row["level"]}';text.align_x='CENTER';text.size=.10
    o=bpy.data.objects.new(f'label_{row["asset"]}_{row["level"]}',text);scene.collection.objects.link(o)
    o.location=(-(row['level']-4.5)*1.13,(ASSETS.index(row['asset'])-1)*2.25+.73,-.03)
    o.rotation_euler=(Vector((0,9,7))-o.location).to_track_quat('Z','Y').to_euler()
scene.camera.location=(0,9,7);aim(scene.camera,(0,0,.3));scene.camera.data.ortho_scale=9.8
scene.render.resolution_x=2800;scene.render.resolution_y=1550;scene.render.filepath=str(ART/'all_levels.png')
bpy.ops.render.render(write_still=True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.type='MATERIAL'
        area.spaces.active.overlay.show_extras=False
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'harvester_levels.blend'),check_existing=False)
# Flat sheets preserve the same beauty camera for side-by-side tier comparison.
for asset in ASSETS:
    imgs=[bpy.data.images.load(str(ART/asset/f'level_{i:02d}.png'),check_existing=False) for i in range(1,9)]
    w=h=600;sheet=bpy.data.images.new(asset+'_sheet',width=w*4,height=h*2,alpha=True)
    pixels=[0.0]*(w*4*h*2*4)
    for k,img in enumerate(imgs):
        data=list(img.pixels[:]);col=k%4;row=1-k//4
        for y in range(h):
            dest=((row*h+y)*(w*4)+col*w)*4;pixels[dest:dest+w*4]=data[y*w*4:(y+1)*w*4]
    sheet.pixels[:]=pixels;sheet.filepath_raw=str(ART/asset/'levels_1-8.png');sheet.file_format='PNG';sheet.save()
manifest={'family':'compact_harvesters_v1','level_convention':'8 total levels: L1 unchanged base plus 7 progressively enhanced variants',
 'delivery':'24 standalone complete GLBs, not additive overlays; draw exactly one file per building level',
 'original_sha256':ORIGINALS,'palette_sha256':hashlib.sha256((ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()).hexdigest(),
 'coordinates':'GLB metres, +Y up, -Z front; Blender +Z up, +Y front; all roots identity at floor',
 'footprint_m':[1,1],'colliders':'Each complete variant retains the original root-level COL_BOX_BODY; not walkable',
 'authoring':'itsalmostnight-models; inspected Tesla/flamethrower/mortar progression; original Kenney gold/red swatches',
 'levels':rows,'overview':'art/models/harvester_levels/harvester_levels.blend','overview_image':'art/models/harvester_levels/all_levels.png',
 'generator':'tools/create_harvester_levels.py','validator':'tools/validate_harvester_levels.py','loader_probe':'tools/validate_harvester_levels_loader.cpp','handoff':'art/models/harvester_levels/HANDOFF_RU.md',
 'integration':{'loader_checked':False,'production_connected':False,'level_selection_connected':False,'motion_connected':False,'playtested':False},
 'license':'Original project geometry; Kenney Tower Defense Kit 2.1 palette CC0'}
(ART/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
for f,h in ORIGINALS.items():assert hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==h
print('ALL_HARVESTER_LEVELS_CREATED',len(rows),flush=True)
