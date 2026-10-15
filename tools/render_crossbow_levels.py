"""Render all crossbow tiers together from the saved editable source."""
from pathlib import Path
import importlib.util,math
import bpy
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/crossbow_levels'
assert bpy.app.background
bpy.ops.wm.open_mainfile(filepath=str(ART/'crossbow_levels.blend'))
source=bpy.context.scene;bpy.context.view_layer.update()
parts=[o for o in source.objects if o.type=='MESH' and not o.name.startswith('COL_')]
spec=importlib.util.spec_from_file_location('crossbow_grid_studio',ROOT/'tools/create_tesla_upgrades.py')
family=importlib.util.module_from_spec(spec);spec.loader.exec_module(family)
scene=family.new_scene('CROSSBOW_LEVEL_GRID');family.studio(scene)
for level in range(1,9):
    x=2.625-((level-1)%4)*1.75;y=1.4 if level<=4 else -1.4
    for obj in parts:
        if obj.name.startswith('upgrade_') and obj.get('level')!=level:continue
        clone=bpy.data.objects.new(f'Preview_L{level}_{obj.name}',obj.data);scene.collection.objects.link(clone)
        clone.matrix_world=Matrix.Translation((x,y,0))@obj.matrix_world
    font=bpy.data.curves.new(f'L{level}_label','FONT');font.body=f'LEVEL {level}';font.align_x='CENTER';font.size=.15
    label=bpy.data.objects.new(font.name,font);scene.collection.objects.link(label)
    label.location=(x,y+.8,.01);label.rotation_euler.z=math.pi
scene.camera.location=(3,8,9);family.aim(scene.camera,(0,0,.45));scene.camera.data.ortho_scale=8.5
scene.render.resolution_x=1600;scene.render.resolution_y=1100
scene.render.filepath=str(ART/'levels_1-8.png');bpy.ops.render.render(write_still=True)
