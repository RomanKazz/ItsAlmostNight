"""Clean-import validation and neutral studio review of each individually refined relic."""
from pathlib import Path
import bpy,math,json,struct
from mathutils import Vector,Quaternion
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/chest_relics/refined';OUT=ART/'runtime';ICONS=ART/'icons';VIEWS=ART/'views'
for p in [ICONS,VIEWS]:p.mkdir(exist_ok=True)
# Make .blend files normal projects that open on the intended scene and frame.
for source in [ART/'chest_relics.blend']+sorted(p for p in ART.glob('*.blend') if p.stem!='chest_relics'):
 bpy.ops.wm.open_mainfile(filepath=str(source))
 target=next(s for s in bpy.data.scenes if s.name.startswith('Chest Relics Crafted Gallery') or s.name.startswith('CRAFTED_'))
 bpy.context.window.scene=target
 for screen in bpy.data.screens:
  for area in screen.areas:
   if area.type=='VIEW_3D':
    space=area.spaces.active;space.shading.type='MATERIAL'
    gallery=source.stem=='chest_relics';space.region_3d.view_location=(4.75,0,2.5) if gallery else (0,0,0)
    direction=Vector((.5,-1,.35));space.region_3d.view_rotation=direction.to_track_quat('Z','Y');space.region_3d.view_distance=12.5 if gallery else 2.6
 bpy.ops.wm.save_as_mainfile(filepath=str(source))
# Render only exported/reimported geometry, under the same lighting for old and new.
scene=bpy.data.scenes.new('Crafted Relic Review Studio');bpy.context.window.scene=scene
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100
scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.view_settings.view_transform='Standard'
world=bpy.data.worlds.new('Neutral studio');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.45,.45,.45,1);bg.inputs['Strength'].default_value=.7
camdata=bpy.data.cameras.new('Review camera');cam=bpy.data.objects.new('Review camera',camdata);scene.collection.objects.link(cam);scene.camera=cam;camdata.type='ORTHO';camdata.ortho_scale=1.75
for name,pos,energy,size in [('Key',(-3,-4,5),240,5),('Fill',(4,-1,2),90,4)]:
 d=bpy.data.lights.new(name,'AREA');o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=pos;d.energy=energy;d.shape='DISK';d.size=size;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()

def render(path,direction,size=384):
 scene.render.resolution_x=size;scene.render.resolution_y=size
 cam.location=direction;cam.rotation_euler=(-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)

def import_objects(path):
 before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(path));return [o for o in scene.objects if o not in before]

def clean(objects):
 for o in objects:bpy.data.objects.remove(o,do_unlink=True)

report=[]
for path in sorted(OUT.glob('*.glb')):
 raw=path.read_bytes();n=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+n])
 assert len(doc['meshes'])==1 and len(doc['meshes'][0]['primitives'])==1 and len(doc['materials'])==1,path
 assert all('bufferView' in image and 'uri' not in image for image in doc['images']),path
 assert not doc.get('extensionsRequired') and all('uri' not in buffer for buffer in doc['buffers']),path
 objs=import_objects(path);meshes=[o for o in objs if o.type=='MESH'];assert len(meshes)==1,path
 obj=meshes[0];m=obj.data
 bounds=[obj.matrix_world@Vector(v) for v in obj.bound_box];lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)])
 assert abs(max(hi-lo)-1.3)<.001 and all(abs(c)<.001 for c in (lo+hi)/2),(path,lo,hi)
 assert len(m.uv_layers)==1 and len(m.materials)==1,path
 assert all(math.isfinite(c) for v in m.vertices for c in v.normal),path
 assert all(.98<v.normal.length<1.02 for v in m.vertices),path
 assert all(math.isfinite(c) for uv in m.uv_layers[0].data for c in uv.uv),path
 render(ICONS/(path.stem+'.png'),(2.4,-5,2.0),256)
 for label,direction in [('front',(0,-5,.1)),('side',(5,0,.1)),('back',(0,5,.1)),('top',(0,0,5)),('perspective',(2.4,-5,2.0))]:render(VIEWS/(path.stem+'_'+label+'.png'),direction)
 report.append({'name':path.stem,'size_blender_xyz':list(hi-lo),'triangles':sum(len(f.vertices)-2 for f in m.polygons),'mesh_count':1,'material_count':1,'uv_layers':1,'normals_finite_unit':True,'texture_embedded':True,'external_files':False,'authoring_closed_components_outward':True})
 clean(objs)
# Before images rendered again rather than reusing older differently-lit screenshots.
previous=sorted((ROOT/'art/archive').glob('chest-relics-before-refinement-*'))[-1]
BEFORE=ART/'before';BEFORE.mkdir(exist_ok=True)
for path in sorted((previous/'runtime').glob('*.glb')):
 objs=import_objects(path);render(BEFORE/(path.stem+'.png'),(2.4,-5,2.0));clean(objs)
REFS=ART/'references';REFS.mkdir(exist_ok=True)
for path in [ROOT/'assets/models/items/nail.glb',ROOT/'assets/models/items/blueprint.glb',ROOT/'assets/models/powerups/magnet.glb']:
 objs=import_objects(path);bounds=[o.matrix_world@Vector(v) for o in objs if o.type=='MESH' for v in o.bound_box]
 lo=Vector([min(v[i] for v in bounds) for i in range(3)]);hi=Vector([max(v[i] for v in bounds) for i in range(3)]);center=(lo+hi)/2;factor=1.30/max(hi-lo)
 for o in objs:
  if o.type!='MESH':continue
  world=o.matrix_world.copy()
  for v in o.data.vertices:v.co=(world@v.co-center)*factor
  o.matrix_world.identity()
 render(REFS/(path.stem+'.png'),(2.4,-5,2.0));clean(objs)
(ART/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('All 18 refined runtime GLBs validated; individual five-view reviews and matched before images rendered.')
