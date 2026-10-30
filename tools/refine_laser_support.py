"""Revise the current editable LASER scene without rebuilding other parts or scenes.
Run through Blender MCP, or Blender --background art/models/laser.blend --python this_file.
Backs up the full current workspace and associated authoring files before the revision.
"""
from pathlib import Path
from datetime import datetime
import shutil,math,json,subprocess
import bpy
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models';VIEWS=ART/'laser_views'
studio=bpy.data.collections['LASER_STUDIO']
scene=bpy.data.scenes['LASER'];bpy.context.window.scene=scene;bpy.context.view_layer.update()
assert not scene.get('support_revision_20261015') and abs(scene.objects['pitch_pivot'].location.y)<1e-5, 'Revision already applied; do not move the pivot a second time'
revision=ROOT/'output/laser_revisions'/datetime.now().strftime('%Y%m%d_%H%M%S_support')
revision.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(revision/'workspace_before.blend'),copy=True,check_existing=False)
for relative in ['assets/models/buildings/laser.glb','tools/create_laser_model.py','tools/validate_laser_model.py',
                 'tools/validate_laser_loader.cpp','art/models/laser_manifest.json','art/models/laser_handoff.md',
                 'art/models/laser_preview.png','art/models/laser_comparison.png']:
    src=ROOT/relative
    if src.exists():shutil.copy2(src,revision/src.name)
if VIEWS.exists():shutil.copytree(VIEWS,revision/'views')

def group_ids(obj,name):
    group=obj.vertex_groups[name].index
    return {v.index for v in obj.data.vertices if any(g.group==group for g in v.groups)}
def replace_group(obj,name,profile,swatch):
    old=obj.data;removed=group_ids(obj,name)
    keep=[v for v in old.vertices if v.index not in removed];remap={v.index:i for i,v in enumerate(keep)}
    verts=[tuple(v.co) for v in keep];faces=[];uvs=[];smooth=[];materials=[]
    groups={g.name:[remap[v.index] for v in keep if any(w.group==g.index for w in v.groups)] for g in obj.vertex_groups if g.name!=name}
    for p in old.polygons:
        if all(i in removed for i in p.vertices):continue
        assert not any(i in removed for i in p.vertices),'part has shared vertices'
        faces.append(tuple(remap[i] for i in p.vertices));smooth.append(p.use_smooth);materials.append(p.material_index)
        uvs.extend(tuple(old.uv_layers.active.data[i].uv) for i in p.loop_indices)
    start=len(verts);segments=12
    for r,z in profile:
        verts.extend((r*math.cos(math.tau*i/segments),r*math.sin(math.tau*i/segments),z-.640) for i in range(segments))
    new_faces=[];new_smooth=[]
    for ring in range(len(profile)-1):
        for i in range(segments):
            a=start+ring*segments+i;b=start+ring*segments+(i+1)%segments
            new_faces.append((a,b,b+segments,a+segments));new_smooth.append(abs(profile[ring+1][1]-profile[ring][1])>1e-6)
    new_faces.extend((tuple(reversed(range(start,start+segments))),tuple(start+(len(profile)-1)*segments+i for i in range(segments))))
    new_smooth.extend((False,False))
    lo=min(z for r,z in profile);hi=max(z for r,z in profile)
    for face,flag in zip(new_faces,new_smooth):
        faces.append(face);smooth.append(flag);materials.append(0)
        uvs.extend((swatch[0],swatch[1]+(verts[i][2]+.640-lo)/(hi-lo)*(swatch[2]-swatch[1])) for i in face)
    data=bpy.data.meshes.new(old.name+'_support_revision');data.from_pydata(verts,[],faces);data.update()
    for material in old.materials:data.materials.append(material)
    uv=data.uv_layers.new(name='UVMap')
    for i,co in enumerate(uvs):uv.data[i].uv=co
    for p,flag,material in zip(data.polygons,smooth,materials):p.use_smooth=flag;p.material_index=material
    data.set_sharp_from_angle(angle=math.radians(45));obj.data=data
    obj.vertex_groups.clear();groups[name]=list(range(start,len(verts)))
    for label,indices in groups.items():obj.vertex_groups.new(name=label).add(indices,1,'REPLACE')
    if old.users==0:bpy.data.meshes.remove(old)

base=scene.objects['base_mesh'];mount=scene.objects['yaw_mesh'];head=scene.objects['pitch_mesh'];core=scene.objects['energy_mesh']
for obj in (mount,head,core):
    if obj.data.users>1:obj.data=obj.data.copy()
replace_group(mount,'laser_turntable',[(.228,.615),(.228,.633),(.410,.633),(.455,.646),(.455,.667),(.430,.679)],(.59375,.025,.225))
replace_group(mount,'laser_mount_foot',[(.405,.671),(.435,.681),(.435,.706),(.405,.716)],(.84375,.275,.475))
for label in ('laser_yoke_left','laser_yoke_right','laser_pivot_bolt_left','laser_pivot_bolt_right'):
    for i in group_ids(mount,label):mount.data.vertices[i].co.y-=.170
# Preserve the head's neutral world geometry while moving its actual rotation centre.
pitch=scene.objects['pitch_pivot'];pitch.location.y-=.170
for obj in (head,core):
    for v in obj.data.vertices:v.co.y+=.170
for name in ('MUZZLE_01','socket_effect_energy'):scene.objects[name].location.y+=.170
# Only the axle itself moves back to the new bearings, inside the solid reactor housing.
for i in group_ids(head,'laser_trunnion_axle'):head.data.vertices[i].co.y-=.170
for obj in (mount,head,core):obj.data.update()
scene['support_revision_20261015']='Full support platform and reactor-centred trunnion; previous workspace archived'
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
scene.objects['COL_BOX_BODY'].hide_set(False)
for c in ('LASER_MODEL','LASER_COLLISION'):
    for o in bpy.data.collections[c].objects:o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/laser.glb'),use_selection=True,use_active_scene=True,
    use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,export_normals=True,
    export_materials='EXPORT',export_animations=False,export_extras=True)
scene.objects['COL_BOX_BODY'].hide_set(True)

def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
cam=scene.camera;data=cam.data
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'laser_preview.png');bpy.ops.render.render(write_still=True)
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
# Keep the user's full workspace in the backup; save an isolated regular authoring file.
bpy.data.libraries.write(str(ART/'laser.blend'),{scene},path_remap='ABSOLUTE',fake_user=True)
finalize="""import bpy
s=bpy.data.scenes['LASER'];bpy.context.window.scene=s
for other in list(bpy.data.scenes):
    if other!=s:bpy.data.scenes.remove(other)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath,check_existing=False)
"""
subprocess.run([bpy.app.binary_path,'--background',str(ART/'laser.blend'),
    '--python-exit-code','1','--python-expr',finalize],check=True)
# Reuse the comparison setup only; never run the old generator over the editable source.
source=(ROOT/'tools/create_laser_model.py').read_text()
comparison_code=source[source.index("comparison=bpy.data.scenes.new('Laser_comparison')"):]
exec(compile(comparison_code,'laser_comparison_setup','exec'))
print('LASER_SUPPORT_REVISED',json.dumps({'backup':str(revision),'turntable_radius':.455,'support_radius':.435,'pitch_pivot_blender':[0,-.17,1.02]}))
