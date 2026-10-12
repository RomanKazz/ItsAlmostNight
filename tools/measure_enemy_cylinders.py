"""Measure native enemy silhouettes at runtime scale (run with Blender).

blender --background --factory-startup --python tools/measure_enemy_cylinders.py
Outputs docs/enemy_cylinder_model_bounds.json for reviewing physical dimensions.
This is authoring tooling; simulation never loads a model or invokes Blender.
"""
import json
import math
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[1]
MODELS = [
    ("basic", "pink_blob.gltf", .94),
    ("fast", "ultimate/ninja.gltf", .55),
    ("heavy", "ultimate/mushnub_evolved.gltf", .67),
    ("boss", "ultimate/boss.gltf", .85),
    ("ranged", "ultimate/wizard.gltf", .94),
    ("sapper", "ultimate/sapper.gltf", .52),
    ("flying", "ultimate/flying.gltf", .52),
    ("splitter", "ultimate/green_spiky_blob.gltf", .58),
    ("splitling", "ultimate/green_blob.gltf", .60),
]
report = {}
for name, asset, scale in MODELS:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(ROOT / "assets/models/enemies" / asset))
    # glTF's Blender importer creates meshes for bone display. They are not
    # exported/rendered geometry and must not enlarge the gameplay cylinder.
    custom_shapes = {bone.custom_shape for obj in bpy.context.scene.objects
                     if obj.type == "ARMATURE" for bone in obj.pose.bones
                     if bone.custom_shape is not None}
    vertices = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH" or obj in custom_shapes:
            continue
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        vertices.extend(evaluated.matrix_world @ vertex.co for vertex in mesh.vertices)
        evaluated.to_mesh_clear()
    factor = .82 * scale
    bottom = min(v.z for v in vertices) * factor
    report[name] = {
        "asset": asset,
        "gameplay_scale": scale,
        "radius": max(math.hypot(v.x, v.y) for v in vertices) * factor,
        "bottom": bottom,
        "height": max(v.z for v in vertices) * factor - bottom,
    }
output = ROOT / "docs/enemy_cylinder_model_bounds.json"
output.write_text(json.dumps(report, indent=2) + "\n")
print(output)
