#!/usr/bin/env python3
"""Summarize frame CSVs without third-party packages.

Usage: python3 tools/analyze_performance.py performance_logs/session_*.csv
"""

from __future__ import annotations

import argparse
import csv
import heapq
import math
from collections import Counter, defaultdict
from pathlib import Path


STATES = {
    0: "menu", 1: "gathering", 2: "build", 3: "sunset",
    4: "wave", 5: "wave complete", 6: "stage clear",
    7: "victory", 8: "defeat", 9: "paused",
}
SELECTIONS = {
    0: "none", 1: "resource", 2: "enemy", 3: "building",
    4: "chest", 5: "modular", 6: "challenge", 7: "landmark",
    8: "loot", 9: "preview",
}
# These are mostly disjoint CPU-thread intervals. Nested detail is below.
STAGES = {
    "input_ms": "Input",
    "update_ms": "Update",
    "render_prep_ms": "Render preparation",
    "shadow_ms": "Shadow pass",
    "selection_ms": "Selection mask",
    "terrain_ms": "Terrain + sky",
    "world_objects_ms": "World objects",
    "environment_ms": "Water + clouds",
    "overlays_ms": "World overlays",
    "postprocess_ms": "Postprocess",
    "selection_composite_ms": "Selection composite",
    "resource_impact_overlays_ms": "Resource impact overlay",
    "ui_ms": "HUD/UI",
    "present_ms": "EndDrawing + pacing",
}
DETAIL = {
    "sim_tick_total_ms": "Simulation ticks, total",
    "enemy_ai_ms": "Enemy system, last tick",
    "enemy_collision_ms": "Enemy collision, last tick",
    "enemy_spatial_rebuild_ms": "Enemy spatial rebuild, last tick",
    "decorations_ms": "Decorations",
    "grass_ms": "Grass",
    "enemy_draw_ms": "Enemies",
    "blob_shadows_ms": "Blob shadows",
    "minimap_ms": "Minimap",
    "ui_text_ms": "UI text",
    "viewmodel_ms": "Viewmodel CPU",
    "world_ui_ms": "World-space HUD",
    "hud_core_ms": "Core HUD",
    "hud_text_flush_ms": "HUD text flush",
    "minimap_prewarm_ms": "Minimap prewarm (run loading)",
    "minimap_target_refresh_ms": "Minimap target refresh",
    "minimap_frame_ms": "Minimap frame",
    "present_pacing_ms": "Frame cap wait",
    "present_swap_ms": "EndDrawing (swap + events)",
    "final_batch_submission_ms": "Final batch submission",
}
PRESENT_CPU = {
    "present_thread_cpu_ms": "EndDrawing thread CPU",
    "present_not_running_ms": "EndDrawing not running on CPU",
}
WORLD_OBJECT_DETAIL = {
    "objects_decorations_ms": "Objects: decorations/setup",
    "objects_resources_ms": "Objects: resource preparation/draw",
    "objects_rocks_ms": "Objects: rock batches",
    "objects_trees_ms": "Objects: tree batches",
    "objects_loot_props_ms": "Objects: chests/landmarks/glows",
    "objects_pickups_ms": "Objects: coin/XP pickups",
    "objects_destroyed_resources_ms": "Objects: destroyed resources",
    "objects_buildings_ms": "Objects: buildings/modular",
    "objects_projectiles_portals_ms": "Objects: projectiles/portals",
    "objects_enemies_ms": "Objects: enemies/submission",
    "objects_grass_ms": "Objects: grass/submission",
}
DETAIL.update(WORLD_OBJECT_DETAIL)
GPU = {
    "gpu_world_ms": ("GPU world", "gpu_world_samples"),
    "gpu_shadow_ms": ("GPU shadow", "gpu_shadow_samples"),
    "gpu_selection_ms": ("GPU selection", "gpu_selection_samples"),
    "gpu_scene_ms": ("GPU scene", "gpu_scene_samples"),
    "gpu_terrain_ms": ("GPU terrain + sky", "gpu_terrain_samples"),
    "gpu_objects_ms": ("GPU world objects", "gpu_objects_samples"),
    "gpu_environment_ms": ("GPU water + clouds", "gpu_environment_samples"),
    "gpu_overlays_ms": ("GPU world overlays", "gpu_overlays_samples"),
    "gpu_postprocess_ms": ("GPU postprocess", "gpu_postprocess_samples"),
    "gpu_ui_ms": ("GPU UI", "gpu_ui_samples"),
    "gpu_viewmodel_target_ms": ("GPU viewmodel target", "gpu_viewmodel_target_samples"),
    "gpu_viewmodel_composite_ms": ("GPU viewmodel composite", "gpu_viewmodel_composite_samples"),
}
COUNTERS = (
    "active_enemies", "visible_enemies", "resource_nodes",
    "coin_pickups", "presentation_effects", "instanced_enemies",
    "enemy_batches", "blob_shadow_triangles", "ui_text_submissions",
    "terrain_chunks_submitted", "terrain_chunks_culled", "gpu_queries_issued",
    "window_focused", "window_minimized", "window_focus_changed",
)


def number(row: dict[str, str], key: str) -> float | None:
    try:
        value = float(row[key])
    except (KeyError, TypeError, ValueError):
        return None
    return value if math.isfinite(value) else None


def percentile(values: list[float], percent: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    position = (len(ordered) - 1) * percent / 100.0
    lower = math.floor(position)
    upper = math.ceil(position)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def summary(values: list[float]) -> str:
    return (
        f"p50 {percentile(values, 50):6.2f}  "
        f"p95 {percentile(values, 95):6.2f}  "
        f"p99 {percentile(values, 99):6.2f}  "
        f"max {max(values, default=0.0):6.2f} ms"
    )


def included(row: dict[str, str], all_states: bool, warmup: float) -> bool:
    seconds = number(row, "session_seconds")
    state = number(row, "run_state")
    return (seconds is not None and seconds >= warmup and
            (all_states or state in (1, 2, 3, 4, 5, 6)))


def cadence_duration(row: dict[str, str]) -> float | None:
    # Start-to-start includes logger and settings work after rendering.
    interval = number(row, "frame_interval_ms")
    return interval if interval is not None and interval > 0.0 else number(
        row, "frame_ms")


def timed_duration(row: dict[str, str]) -> float | None:
    duration = number(row, "frame_ms")
    return duration if duration is not None and duration > 0.0 else None


def analyze(path: Path, target_fps: float, all_states: bool,
            warmup: float) -> str:
    columns = set(STAGES) | set(DETAIL) | set(GPU) | set(COUNTERS) | set(PRESENT_CPU)
    values: dict[str, list[float]] = {key: [] for key in columns}
    selection_frames: dict[int, list[float]] = defaultdict(list)
    selection_mask: dict[int, list[float]] = defaultdict(list)
    selection_composite: dict[int, list[float]] = defaultdict(list)
    states: Counter[int] = Counter()
    phase_frames: dict[int, list[float]] = defaultdict(list)
    focused_frames: list[float] = []
    settings: Counter[tuple[str, ...]] = Counter()
    gpu_sample_counts: dict[str, float] = {}
    slowest: list[tuple[float, int, float, int, tuple]] = []
    frames: list[float] = []
    timed_frames: list[float] = []
    outside_timers: list[float] = []
    previous_frame_number: int | None = None
    previous_timed: float | None = None
    total_rows = 0
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or "frame_ms" not in reader.fieldnames:
            raise ValueError(f"{path}: missing frame_ms column")
        fields = set(reader.fieldnames)
        for row in reader:
            total_rows += 1
            current_frame_number = int(number(row, "frame") or 0)
            cadence = cadence_duration(row)
            timed = timed_duration(row)
            if (cadence is not None and previous_timed is not None and
                    previous_frame_number is not None and
                    current_frame_number == previous_frame_number + 1 and
                    "frame_interval_ms" in fields and
                    included(row, all_states, warmup)):
                # The interval ends at this frame's start, so it contains
                # the *previous* frame's measured work.
                outside_timers.append(max(0.0, cadence - previous_timed))
            previous_frame_number = current_frame_number
            previous_timed = timed
            if not included(row, all_states, warmup):
                continue
            if cadence is None or timed is None:
                continue
            frames.append(cadence)
            timed_frames.append(timed)
            state = int(number(row, "run_state") or 0)
            kind = int(number(row, "selection_kind") or 0)
            states[state] += 1
            phase_frames[state].append(timed)
            if number(row, "window_focused") == 1 and number(row, "window_focus_changed") == 0:
                focused_frames.append(timed)
            settings[(row.get("window_width", "?"),
                      row.get("window_height", "?"),
                      row.get("render_width", "?"),
                      row.get("render_height", "?"),
                      row.get("fps_limit", "?"),
                      row.get("graphics_quality", "?"))] += 1
            selection_frames[kind].append(timed)
            for destination, key in ((selection_mask, "selection_ms"),
                                     (selection_composite,
                                      "selection_composite_ms")):
                value = number(row, key)
                if value is not None:
                    destination[kind].append(value)
            for key in columns:
                value = number(row, key)
                if value is None:
                    continue
                if key in PRESENT_CPU and number(row, "present_thread_cpu_valid") != 1:
                    continue
                if key in GPU:
                    sample_column = GPU[key][1]
                    if key.startswith("gpu_viewmodel_") and sample_column not in fields:
                        continue  # Older logs repeat stale values without freshness counters.
                    count = number(row, sample_column)
                    if sample_column in fields:
                        if count is None or count <= gpu_sample_counts.get(
                                key, 0.0):
                            continue
                        gpu_sample_counts[key] = count
                    elif value <= 0.0:
                        continue
                values[key].append(value)
            object_detail = tuple(sorted(
                ((value, key) for key in (set(STAGES) | {"minimap_prewarm_ms"})
                 if (value := number(row, key)) is not None and value > 0.0),
                reverse=True)[:3])
            if number(row, "present_thread_cpu_valid") == 1:
                object_detail += tuple(
                    (value, key) for key in PRESENT_CPU
                    if (value := number(row, key)) is not None)
            item = (timed, current_frame_number,
                    number(row, "session_seconds") or 0.0, kind, object_detail)
            if len(slowest) < 8:
                heapq.heappush(slowest, item)
            elif timed > slowest[0][0]:
                heapq.heapreplace(slowest, item)

    if not frames:
        return (f"{path}: no matching frames out of {total_rows}; "
                "try --all-states or --warmup 0")
    budget = 1000.0 / target_fps
    threshold = percentile(timed_frames, 95)
    slow_sums: dict[str, float] = defaultdict(float)
    normal_sums: dict[str, float] = defaultdict(float)
    slow_count = normal_count = 0
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if not included(row, all_states, warmup):
                continue
            timed = timed_duration(row)
            if timed is None:
                continue
            destination = slow_sums if timed >= threshold else normal_sums
            if timed >= threshold:
                slow_count += 1
            else:
                normal_count += 1
            for key in STAGES:
                destination[key] += number(row, key) or 0.0

    lines = [
        f"FILE {path} | frames {len(frames)}/{total_rows}",
        f"Target {target_fps:g} FPS: {budget:.2f} ms; "
        f"over budget {sum(value > budget for value in frames) / len(frames):.1%}",
        f"Cadence: {summary(frames)}",
        "States: " + ", ".join(
            f"{STATES.get(state, state)} {count}"
            for state, count in sorted(states.items())),
    ]
    for setting, count in settings.most_common(3):
        window_width, window_height, render_width, render_height, fps, quality = setting
        lines.append(
            f"Settings ({count} frames): window {window_width}x{window_height}, "
            f"render {render_width}x{render_height}, "
            f"cap {fps}, quality {quality}")
    if len(settings) > 3:
        lines.append(f"Other settings configurations: {len(settings) - 3}")
    if outside_timers:
        lines.append(f"Timed frame: {summary(timed_frames)}")
        lines.append(f"Between frames (after previous timer): {summary(outside_timers)}")
    if "frame_interval_ms" not in fields:
        lines.append("Old CSV: cadence uses frame_ms and omits logger overhead")
    if "gpu_world_samples" not in fields:
        lines.append("Old CSV: GPU values may repeat until the next query arrives")
    most_common_settings = settings.most_common(1)[0][0]
    try:
        active_cap = int(most_common_settings[4])
    except ValueError:
        active_cap = 0
    if 0 < active_cap < target_fps:
        lines.append(
            f"FPS cap {active_cap} is below target {target_fps:g}; "
            "uncap before judging the target")

    lines.append("\nPer phase (timed frame, includes loading/focus interruptions):")
    for state, group in sorted(phase_frames.items()):
        lines.append(f"  {STATES.get(state, str(state)):14} n={len(group):6} {summary(group)}")
    if focused_frames:
        lines.append(f"Focused frames (no focus change): n={len(focused_frames)} {summary(focused_frames)}")
    else:
        lines.append("Focus state unavailable in this older CSV; OS/window pauses cannot be excluded")

    lines.append("\nCPU stage wall times (ranked by added time in slowest 5%):")
    ranked = sorted(
        (key for key in STAGES if values[key]),
        key=lambda key: slow_sums[key] / max(slow_count, 1) -
        normal_sums[key] / max(normal_count, 1),
        reverse=True)
    for key in ranked:
        delta = slow_sums[key] / max(slow_count, 1) - normal_sums[key] / max(normal_count, 1)
        lines.append(f"  {STAGES[key]:25} {summary(values[key])}  slow {delta:+.2f}")

    lines.append("\nNested detail (included in stages above):")
    for key, label in DETAIL.items():
        if values[key]:
            lines.append(f"  {label:29} {summary(values[key])}")

    if "present_thread_cpu_valid" in fields:
        lines.append("\nEndDrawing thread diagnostics (valid samples only):")
        for key, label in PRESENT_CPU.items():
            if values[key]:
                lines.append(f"  {label:29} {summary(values[key])}  n={len(values[key])}")
        if not any(values[key] for key in PRESENT_CPU):
            lines.append("  thread CPU clock unavailable or diagnostics disabled")
        lines.append("  Not-running time includes blocking/descheduling; it does not distinguish GPU, swap or OS event waits")

    lines.append("\nGPU queries (asynchronous; one row per fresh query):")
    invalid_world_breakdown = (
        bool(values["gpu_scene_ms"]) and
        max(values["gpu_scene_ms"]) == 0.0 and
        bool(values["gpu_world_ms"]) and
        max(values["gpu_world_ms"]) > 0.0)
    for key, (label, _) in GPU.items():
        if values[key]:
            if invalid_world_breakdown and key in (
                    "gpu_shadow_ms", "gpu_selection_ms", "gpu_scene_ms"):
                lines.append(f"  {label:25} invalid (all GPU timestamps zero)")
                continue
            lines.append(f"  {label:25} {summary(values[key])}  n={len(values[key])}")
    if not any(values[key] for key in GPU):
        lines.append("  unavailable; GPU times cannot be inferred from CPU stages")
    elif values["gpu_terrain_ms"]:
        lines.append(
            "  Subpass queries may include deferred driver work; "
            "do not add them or treat them as isolated costs")

    lines.append("\nSelection groups (association, not isolated cause):")
    if "selection_kind" not in fields:
        lines.append("  unavailable in this older CSV")
    else:
        for kind, group in sorted(selection_frames.items(), key=lambda item: -len(item[1])):
            if len(group) < 10:
                continue
            mask = selection_mask[kind]
            composite = selection_composite[kind]
            lines.append(
                f"  {SELECTIONS.get(kind, str(kind)):12} n={len(group):6} "
                f"timed frame p50/p95 {percentile(group, 50):.2f}/"
                f"{percentile(group, 95):.2f} ms; "
                f"mask p95 {percentile(mask, 95):.2f}; "
                f"composite p95 {percentile(composite, 95):.2f}")

    lines.append("\nScene counters (p50/p95/max):")
    for key in COUNTERS:
        if values[key]:
            lines.append(
                f"  {key:25} {percentile(values[key], 50):.0f}/"
                f"{percentile(values[key], 95):.0f}/"
                f"{max(values[key]):.0f}")
    lines.append("\nWorst frames (frame @ seconds: ms, selection):")
    for duration, frame, seconds, kind, object_detail in sorted(slowest, reverse=True):
        lines.append(
            f"  {frame} @ {seconds:.2f}s: {duration:.2f} ms, "
            f"{SELECTIONS.get(kind, str(kind))}")
        if object_detail:
            lines.append("    " + "; ".join(
                f"{STAGES.get(key, DETAIL.get(key, PRESENT_CPU.get(key, key)))} {value:.2f} ms"
                for value, key in object_detail))
    lines.append(
        "Note: CPU stage times can include driver waits; GPU queries arrive "
        "later. EndDrawing includes buffer swap AND native input/event polling, "
        "so its spikes are not proof of GPU overload. Enemy per-tick detail is the last tick when multiple fixed "
        "ticks occur in one frame.")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--target-fps", type=float, default=200.0)
    parser.add_argument("--warmup", type=float, default=2.0,
                        help="skip first N session seconds (default: 2)")
    parser.add_argument("--all-states", action="store_true",
                        help="include menus, pause and end screens")
    parser.add_argument("--output", type=Path,
                        help="write the same report to a text file")
    args = parser.parse_args()
    if args.target_fps <= 0 or args.warmup < 0:
        parser.error("target FPS must be positive and warmup nonnegative")
    report = "\n\n".join(
        analyze(path, args.target_fps, args.all_states, args.warmup)
        for path in args.logs)
    print(report)
    if args.output:
        args.output.write_text(report + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
