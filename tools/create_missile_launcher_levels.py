"""Blender MCP authoring: eight standalone rigged launcher variants, no runtime edits."""
from pathlib import Path
import importlib.util, math, hashlib,json,struct
import bpy
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/missile_launcher_levels';ART.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('tesla_geometry',ROOT/'tools/create_tesla_upgrades.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
GOLD=helper.GOLD;RED=helper.RED;ORANGE=helper.ORANGE;DARK=helper.DARK
BASE=ROOT/'assets/models/buildings/missile_launcher.glb'

def box(g,name,center,size,color):
 x,y,z=center;hx,hy,hz=[v/2 for v in size]
 vs=[(x+dx,y+dy,z+dz) for dz in (-hz,hz) for dy in (-hy,hy) for dx in (-hx,hx)]
 g.add(name,vs,[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)],color)
def frame(g,name,y0,y1,outer,inner,color):
 # Closed chamfered rectangular strap around the launcher pod, clear of mouths.
 def rect(w,h):
  b=.035
  return [(-w+b,-h),(w-b,-h),(w,-h+b),(w,h-b),(w-b,h),(-w+b,h),(-w,h-b),(-w,-h+b)]
 vs=[]
 for y,wh in [(y0,inner),(y0,outer),(y1,outer),(y1,inner)]:
  vs.extend((x,y,z+1.09) for x,z in rect(*wh))
 fs=[]
 for k in range(4):
  for i in range(8):fs.append((k*8+i,k*8+(i+1)%8,((k+1)%4)*8+(i+1)%8,((k+1)%4)*8+i))
 g.add(name,vs,fs,color)
def armoring(level):
 gs={r:helper.Geometry() for r in ('base','yaw','pitch')}
 if level==1:return gs
 gs['base'].ring('gold_foot',.14,.205,.588,.625,GOLD)
 frame(gs['pitch'],'rear_gold_strap',-.32,-.265,(.286,.251),(.263,.227),GOLD)
 if level>=3:
  # Yoke armor stays outside pitch housing; upper ears do not intrude into it.
  for side in (-1,1):
   box(gs['yaw'],'side_armor_'+str(side),(side*.384,0,.87),(.035,.18,.34),GOLD)
   box(gs['yaw'],'side_inlay_'+str(side),(side*.407,0,.87),(.018,.125,.24),RED)
 if level>=4:
  frame(gs['pitch'],'front_gold_bezel',.354,.397,(.300,.265),(.246,.215),GOLD)
  # Exterior collars retain all four open bores and unchanged socket positions.
  for i,(x,z) in enumerate(((-.113,.113),(.113,.113),(-.113,-.113),(.113,-.113))):
   g=gs['pitch'];start=len(g.verts)
   g.ring('mouth_rim_'+str(i),.392,.415,.078,.101,GOLD,sides=12)
   for j in range(start,len(g.verts)):
    vx,vy,vz=g.verts[j];g.verts[j]=(vx+x,vz,1.09+z-vy)
 if level>=5:
  # Broad upper shoulder plates stop before the rear/front straps.
  for side in (-1,1):
   box(gs['pitch'],'roof_rail_'+str(side),(side*.206,-.025,1.342),(.09,.43,.045),GOLD)
   box(gs['pitch'],'roof_rail_inlay_'+str(side),(side*.206,-.025,1.371),(.056,.32,.016),RED)
  gs['base'].ring('lower_master_band',.093,.128,.588,.636,GOLD)
 if level>=6:
  # Side armor is restricted to the upper pod: the trunnion/yoke gap stays clear.
  for side in (-1,1):
   box(gs['pitch'],'upper_side_plate_'+str(side),(side*.293,-.018,1.248),(.042,.42,.13),GOLD)
   box(gs['pitch'],'upper_side_panel_'+str(side),(side*.321,-.018,1.248),(.017,.32,.082),RED)
  gs['base'].ring('red_foot_band',.212,.248,.589,.628,RED)
 if level>=7:
  # Compact dorsal guidance housing follows pitch, leaves four-tube identity intact.
  box(gs['pitch'],'guidance_mount',(0,-.085,1.378),(.22,.24,.04),GOLD)
  box(gs['pitch'],'guidance_housing',(0,-.085,1.423),(.17,.21,.05),RED)
  box(gs['pitch'],'guidance_cap',(0,-.085,1.458),(.19,.23,.025),GOLD)
  box(gs['pitch'],'guidance_window',(0,.027,1.423),(.11,.016,.03),helper.CYAN)
 if level>=8:
  frame(gs['pitch'],'master_rear_flange',-.365,-.325,(.286,.230),(.257,.211),GOLD)
  for side in (-1,1):
   for i in range(3):
    box(gs['pitch'],'rank_'+str(side)+'_'+str(i),(side*.333,-.12+i*.095,1.248),(.014,.025,.06),GOLD)
  gs['base'].ring('upper_base_collar',.282,.315,.377,.409,GOLD)
 return gs

def build():
 global scenes
 scenes={};report=[]
 for level in range(1,9):
  scene=helper.new_scene('MISSILE_LAUNCHER_L%02d'%level);helper.studio(scene)
  col=bpy.data.collections.new('LAUNCHER_L%02d_MODEL'%level);scene.collection.children.link(col)
  before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(BASE));bpy.context.view_layer.update()
  objects=list(set(scene.objects)-before)
  byname={o.name.split('.')[0]:o for o in objects}
  for o in objects:
   for c in list(o.users_collection):c.objects.unlink(o)
   col.objects.link(o)
   if o.name.startswith('COL_'):o.hide_render=True
  palette=byname['base_mesh'].data.materials[0]
  for role,g in armoring(level).items():
   if not g.verts:continue
   base=byname[role+'_mesh'];parent=base.parent
   extra=g.object('L%02d_%s_armor'%(level,role),col,palette)
   bpy.context.view_layer.update()
   extra.data.transform(base.matrix_world.inverted());extra.parent=parent;extra.matrix_basis=base.matrix_basis.copy()
   bpy.ops.object.select_all(action='DESELECT');base.select_set(True);extra.select_set(True);bpy.context.view_layer.objects.active=base
   bpy.ops.object.join()
  # Keep canonical exported names despite other live scenes owning same names.
  for o in objects:o['canonical_name']=o.name.split('.')[0]
  bpy.ops.object.select_all(action='DESELECT')
  for o in objects:o.select_set(True)
  path=ART/f'missile_launcher_lvl{level:02d}.glb'
  bpy.ops.export_scene.gltf(filepath=str(path),use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,
    export_yup=True,export_normals=True,export_texcoords=True,export_materials='EXPORT',export_animations=False,export_extras=True)
  # Strip Blender's global uniqueness suffixes from node and mesh names in this standalone asset.
  raw=path.read_bytes();n=struct.unpack_from('<I',raw,12)[0];d=json.loads(raw[20:20+n]);tail=raw[20+n:]
  for node in d['nodes']:node['name']=node.get('extras',{}).get('canonical_name',node['name'].split('.')[0])
  for node in d['nodes']:
   if 'mesh' in node:d['meshes'][node['mesh']]['name']=node['name']
  encoded=json.dumps(d,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
  path.write_bytes(struct.pack('<III',0x46546c67,2,20+len(encoded)+len(tail))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+tail)
  tris=sum(d['accessors'][p['indices']]['count']//3 for node in d['nodes'] if 'mesh' in node and not node['name'].startswith('COL_') for p in d['meshes'][node['mesh']]['primitives'])
  report.append({'level':level,'file':path.name,'visible_triangles':tris,'visible_meshes':3,'mode':'complete replacement model'})
  scenes[level]=scene
 (ART/'manifest.json').write_text(json.dumps({'levels':report,'base_sha256':hashlib.sha256(BASE.read_bytes()).hexdigest(),
 'palette_sha256':hashlib.sha256((ROOT/'assets/models/buildings/Textures/colormap.png').read_bytes()).hexdigest(),
 'integration':'Not connected; standalone full variants, not additive overlays','pitch_pivot_glb':[0,1.09,0],
 'yaw_pivot_glb':[0,.6,0],'footprint_m':[2,2],'license':'Kenney palette CC0; project geometry'},indent=2)+'\n')
 bpy.context.window.scene=scenes[8]
 bpy.data.libraries.write(str(ART/'missile_launcher_levels.blend'),set(scenes.values()),fake_user=True)
 print('LAUNCHER_LEVELS_CREATED',report)
def render(level,view='beauty'):
 scene=scenes[level];bpy.context.window.scene=scene;cam=scene.camera
 views={'beauty':((2.5,4,2.7),(0,0,.75),2.15),'front':((0,5,.8),(0,0,.8),1.9),
 'side':((5,0,.8),(0,0,.8),1.9),'back':((0,-5,.8),(0,0,.8),1.9),'top':((0,.001,5),(0,0,0),1.65)}
 pos,target,scale=views[view];cam.location=pos;helper.aim(cam,target);cam.data.ortho_scale=scale
 scene.render.filepath=str(ART/f'level_{level}_{view}.png');bpy.ops.render.render(write_still=True)

def render_overview():
    scene=helper.new_scene('MISSILE_ALL_LEVELS');helper.studio(scene)
    for level in range(1,9):
        dx=(1.5-(level-1)%4)*1.65;dz=(1-(level-1)//4)*1.9
        for obj in scenes[level].objects:
            if obj.type=='MESH' and not obj.name.startswith('COL_'):
                copy=bpy.data.objects.new('preview_L%d_'%level+obj.name,obj.data);scene.collection.objects.link(copy)
                copy.matrix_world=Matrix.Translation((dx,0,dz))@obj.matrix_world
        curve=bpy.data.curves.new('level_label','FONT');curve.body='LEVEL %d'%level;curve.align_x='CENTER';curve.size=.11
        obj=bpy.data.objects.new(curve.name,curve);scene.collection.objects.link(obj)
        obj.location=(dx,.65,dz-.10);obj.rotation_euler=(math.pi/2,0,math.pi)
    scene.camera.location=(0,12,5);helper.aim(scene.camera,(0,0,1.65));scene.camera.data.ortho_scale=7
    scene.render.resolution_x=1600;scene.render.resolution_y=1050;scene.render.filepath=str(ART/'all_levels.png')
    bpy.ops.render.render(write_still=True)
    family=helper.new_scene('MISSILE_LEVEL_FAMILY');helper.studio(family)
    helper.import_visible(ROOT/'assets/models/buildings/cannon.glb',lambda o:'upgrade_' not in o.name or '_l8_' in o.name,-1.7)
    helper.import_visible(ART/'missile_launcher_lvl08.glb')
    helper.import_visible(ROOT/'assets/models/buildings/flamethrower.glb',offset=1.7)
    helper.import_visible(ROOT/'assets/models/buildings/flamethrower_upgrades.glb',lambda o:'_l8_' in o.name,1.7)
    family.camera.location=(2,9,4.5);helper.aim(family.camera,(0,0,.7));family.camera.data.ortho_scale=5.6
    family.render.resolution_x=1500;family.render.resolution_y=650;family.render.filepath=str(ART/'family_comparison.png')
    bpy.ops.render.render(write_still=True)
    bpy.context.window.scene=scenes[8]

if __name__=='__main__':
    build()
    for level in range(1,9):render(level)
    for view in ('front','side','back','top'):render(8,view)
    render_overview()
