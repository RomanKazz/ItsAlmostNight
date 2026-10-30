"""Refine only the current beacon's lantern groups; preserve all other live edits."""
from pathlib import Path
from datetime import datetime
import bpy,math,shutil,json
from mathutils import Matrix,Vector

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models';VIEWS=ART/'decoy_beacon_views'
scene=bpy.data.scenes['DECOY_BEACON'];bpy.context.window.scene=scene
assert not scene.get('lantern_focus_revision'),'Lantern focus revision already applied'
folder=ROOT/'output/decoy_beacon_revisions'/datetime.now().strftime('%Y%m%d_%H%M%S_focus')
folder.mkdir(parents=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/'session_before.blend'),copy=True,check_existing=False)
for relative in ['assets/models/buildings/decoy_beacon.glb',
    'art/models/decoy_beacon_preview.png','art/models/decoy_beacon_comparison.png',
    'art/models/decoy_beacon_manifest.json','art/models/decoy_beacon_handoff.md']:
    p=ROOT/relative
    if p.exists():shutil.copy2(p,folder/p.name)
source=(ROOT/'tools/create_decoy_beacon_model.py').read_text()
helpers=source[source.index('def focused_vertex('):source.index('# Apply the focus proportions')]
exec(compile(helpers,'decoy_beacon_focus_transform','exec'))
base=scene.objects['base_mesh'];service=scene.objects['light_mesh']
for obj in (base,service):
    names={g.index:g.name for g in obj.vertex_groups}
    for v in obj.data.vertices:
        memberships=[names[g.group] for g in v.groups if g.weight>.5]
        assert len(memberships)==1,'Ambiguous part membership'
        v.co=focused_vertex(memberships[0],v.co)
    obj.data.update()
scene.objects['socket_effect_attract'].location.z=.6536
for i in range(1,5):scene.objects['socket_light_%02d'%i].location.z=.6705
scene['lantern_focus_revision']=True
bpy.context.view_layer.update()
col=scene.objects['COL_BOX_BODY'];col.hide_set(False)
bpy.ops.object.select_all(action='DESELECT')
for obj in scene.objects:
    if obj.type in ('MESH','EMPTY'):obj.select_set(True)
bpy.context.view_layer.objects.active=base
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/models/buildings/decoy_beacon.glb'),
    use_selection=True,use_active_scene=True,use_visible=False,use_renderable=False,
    export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
    export_animations=False,export_extras=True)
col.hide_set(True)
def aim(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
cam=scene.camera;data=cam.data
render_views=source[source.index('views=['):source.index('for area in bpy.context.screen.areas:')]
exec(compile(render_views,'decoy_beacon_views','exec'))
bpy.ops.object.select_all(action='DESELECT')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'decoy_beacon.blend'),check_existing=False)
studio=bpy.data.collections['DECOY_BEACON_STUDIO']
exec(compile(source[source.index("comparison=bpy.data.scenes.new('Decoy beacon_comparison')"):],
    'decoy_beacon_comparison','exec'))
# Comparison is an inspection scene; the editable file remains isolated.
comparison_scene=bpy.data.scenes.get('Decoy beacon_comparison')
if comparison_scene:bpy.data.scenes.remove(comparison_scene)
scene.camera=scene.objects['decoy_beacon_camera']
scene.render.filepath=str(ART/'decoy_beacon_preview.png')
bpy.ops.render.render(write_still=True)
bpy.ops.render.view_show()
print('DECOY_BEACON_FOCUS_REVISED',str(folder))
