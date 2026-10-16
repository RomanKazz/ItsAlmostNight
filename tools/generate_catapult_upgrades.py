"""Compatibility entry point for the Blender-authored catapult upgrade models."""
from pathlib import Path
import os
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
blender = os.environ.get('BLENDER_EXECUTABLE') or shutil.which('blender')
if not blender and Path('/Applications/Blender.app/Contents/MacOS/Blender').is_file():
    blender = '/Applications/Blender.app/Contents/MacOS/Blender'
if not blender:
    raise SystemExit('Blender is required; set BLENDER_EXECUTABLE to the Blender binary.')
subprocess.run([blender, '--factory-startup', '--background', '--python-exit-code', '1',
                '--python', str(root / 'tools/create_catapult_levels.py')], cwd=root, check=True)
