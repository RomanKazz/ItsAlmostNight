"""Replace only the current mortar barrel; preserve the user's base and rig."""
from pathlib import Path
from datetime import datetime
import shutil,math,json,bpy
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models';VIEWS=ART/'mortar_views'
scene=bpy.data.scenes['MORTAR'];bpy.context.window.scene=scene;bpy.context.view_layer.update()
assert not scene.get('barrel_revision_20261015'), 'Revised barrel already applied'
folder=ROOT/'output/mortar_revisions'/datetime.now().strftime('%Y%m%d_%H%M%S_barrel');folder.mkdir(parents=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/'mortar_before.blend'),copy=True,check_existing=False)
for relative in ['assets/models/buildings/mortar.glb','art/models/mortar_preview.png','art/models/mortar_comparison.png',
    'art/models/mortar_manifest.json','art/models/mortar_handoff.md']:
    source=ROOT/relative
    if source.exists():shutil.copy2(source,folder/source.name)
model=bpy.data.collections['MORTAR_MODEL'];studio=bpy.data.collections['MORTAR_STUDIO']
base=scene.objects['base_mesh'];mount=scene.objects['yaw_mesh'];head=scene.objects['pitch_mesh'];pitch=scene.objects['pitch_pivot']
palette=head.data.materials[0]
WOOD=(.09375,.275,.475);DARK_STEEL=(.59375,.025,.225);CHARCOAL=(.46875,.025,.225);STEEL=(.84375,.025,.225)
source=(ROOT/'tools/create_mortar_model.py').read_text()
helpers=source[source.index('def mesh('):source.index("root=empty(")]
exec(compile(helpers,'mortar_geometry_helpers','exec'))
# Copy only the two short side axles from the current editable source.
parts=[]
for label in ('mortar_trunnion_left','mortar_trunnion_right'):
    group=head.vertex_groups[label].index
    selected=[v for v in head.data.vertices if any(g.group==group for g in v.groups)]
    remap={v.index:i for i,v in enumerate(selected)}
    verts=[tuple(head.matrix_world@v.co) for v in selected]
    faces=[tuple(remap[i] for i in p.vertices) for p in head.data.polygons if all(i in remap for i in p.vertices)]
    parts.append(mesh(label,verts,faces,DARK_STEEL,smooth=()))
pitch_parts=parts
start=source.index("# Build the tube around +Y")
end=source.index("base=combine(",start)
exec(compile(source[start:end],'revised_mortar_barrel','exec'))
replacement=combine('mortar_revised_barrel',pitch_parts,pitch,palette)
saved_groups=[(g.name,[v.index for v in replacement.data.vertices if any(w.group==g.index for w in v.groups)]) for g in replacement.vertex_groups]
old=head.data;head.data=replacement.data
head.vertex_groups.clear()
for label,indices in saved_groups:
    head.vertex_groups.new(name=label).add(indices,1,'REPLACE')
bpy.data.objects.remove(replacement,do_unlink=True)
if old.users==0:bpy.data.meshes.remove(old)
head.data.name='pitch_mesh'
scene.objects['MUZZLE_01'].location=(0,.480/math.sqrt(2),.480/math.sqrt(2))
scene['barrel_revision_20261015']='Short tapered barrel, rounded breech, narrow lip and thick bore walls'
bpy.context.view_layer.update()
col=scene.objects['COL_BOX_BODY'];col.hide_set(False)
bpy.ops.object.select_all(action='DESELECT')
for o in scene.objects:
    if o.type in ('MESH','EMPTY'):o.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/mortar.glb'),use_selection=True,use_active_scene=True,
    use_visible=False,use_renderable=False,export_yup=True,export_texcoords=True,export_normals=True,
    export_materials='EXPORT',export_animations=False,export_extras=True)
col.hide_set(True)
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
cam=scene.camera;data=cam.data
views=[('beauty',(-2.8,4,2.7),(0,0,.65),1.95),('front',(0,4,.66),(0,0,.66),1.85),('side',(-4,0,.66),(0,0,.66),1.85),('back',(0,-4,.66),(0,0,.66),1.85),('top',(0,.001,4),(0,0,0),1.55)]
for name,pos,target,scale in views:
    cam.location=pos;aim(cam,target);data.ortho_scale=scale
    scene.render.filepath=str(VIEWS/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=views[0][1];aim(cam,views[0][2]);data.ortho_scale=views[0][3]
scene.render.filepath=str(ART/'mortar_preview.png');bpy.ops.render.render(write_still=True)
bpy.ops.object.select_all(action='DESELECT');base.select_set(True);bpy.context.view_layer.objects.active=base
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'mortar.blend'),check_existing=False)
exec(compile(source[source.index("comparison=bpy.data.scenes.new('Mortar_comparison')"):],'mortar_comparison','exec'))
print('MORTAR_BARREL_REVISED',str(folder))
