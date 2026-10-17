#!/usr/bin/env python3
"""Summarize --benchmark-frames output; timings are render-only, not gameplay FPS."""
import argparse
import csv
import statistics

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('csv')
args = parser.parse_args()
with open(args.csv, newline='', encoding='utf-8') as stream:
    rows = list(csv.DictReader(stream))
for scene in sorted({row['scene'] for row in rows}):
    group = [row for row in rows if row['scene'] == scene]
    print(f'Scene {scene}: {len(group)} frames, '
          f'{max(int(row["visible_enemies"]) for row in group)} visible enemies')
    for key in ('frame_ms', 'ui_ms', 'text_ms', 'world_ms', 'overlays_ms', 'present_ms'):
        values = sorted(float(row[key]) for row in group)
        print(f'  {key:14} p50 {statistics.median(values):7.3f} '
              f'p95 {values[min(len(values)-1, int(len(values)*.95))]:7.3f} '
              f'p99 {values[min(len(values)-1, int(len(values)*.99))]:7.3f} ms')
