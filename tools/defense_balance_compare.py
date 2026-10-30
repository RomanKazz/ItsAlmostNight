#!/usr/bin/env python3
"""Compare paired real-combat defense runs; no balance values are edited."""
import argparse
import csv
import json
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ("single_infinite", "dense_cluster", "spread_group", "fast", "mixed", "passing")
TTK = ("single_normal", "single_fast", "single_heavy", "single_boss")
CONTROL = {"wind_blower", "slow_trap"}


def read_csv(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def index(rows, keys):
    result = {}
    for row in rows:
        key = tuple(row[field] for field in keys)
        if key in result:
            raise ValueError(f"duplicate row {key}")
        result[key] = row
    return result


def validate_pair(before, after, keys):
    first, second = index(before, keys), index(after, keys)
    if first.keys() != second.keys():
        raise ValueError("before/after do not contain identical buildings, scenarios, profiles and seeds")
    for key in first:
        for field in ("seed", "seconds", "building", "scenario"):
            if first[key][field] != second[key][field]:
                raise ValueError(f"unpaired {field}: {key}")
    return first, second


def means(rows, keys, field):
    values = {}
    for row in rows:
        key = tuple(row[k] for k in keys)
        values.setdefault(key, []).append(float(row[field]))
    return {key: statistics.mean(samples) for key, samples in values.items()}


def pair(before, after, key, field):
    return f"{before[key][field]:.1f} → {after[key][field]:.1f}"


def compare(before_dir, after_dir, output):
    before_raw = read_csv(before_dir / "raw.csv")
    after_raw = read_csv(after_dir / "raw.csv")
    validate_pair(before_raw, after_raw, ("building", "scenario", "seed"))
    before_up = read_csv(before_dir / "upgrades.csv")
    after_up = read_csv(after_dir / "upgrades.csv")
    validate_pair(before_up, after_up, ("building", "profile", "scenario", "seed"))
    before_meta = json.loads((before_dir / "metadata.json").read_text())
    after_meta = json.loads((after_dir / "metadata.json").read_text())
    for field in ("seconds", "seeds", "tick_hz"):
        if before_meta[field] != after_meta[field]:
            raise ValueError(f"benchmark setting changed: {field}")
    if before_meta["config_sha256"] != after_meta["config_sha256"]:
        raise ValueError("game data JSON changed between before and after")
    before = {(row["building"], row["scenario"]): row for row in
              json.loads((before_dir / "summary.json").read_text())["results"]}
    after = {(row["building"], row["scenario"]): row for row in
             json.loads((after_dir / "summary.json").read_text())["results"]}
    buildings = json.loads((after_dir / "summary.json").read_text())["buildings"]
    lines = ["# Defensive buildings — controlled balance pass", "",
             f"60 seconds × {after_meta['seeds']} paired seeds per scenario; real combat systems; damage in UI units.",
             "Before and after use identical scenario code, enemy placement, seeds, data JSON and duration.",
             "", "## Before / after effective DPS", "",
             "| Building | Infinite | Cluster | Spread | Fast | Mixed / slot | Passing | Mixed overkill % | Cluster targets/attack |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    full_rows = []
    for name in buildings:
        row = {"building": name}
        for scene in SCENARIOS:
            key = (name, scene)
            row[f"{scene}_before"] = before[key]["dps_ui"]
            row[f"{scene}_after"] = after[key]["dps_ui"]
        row["overkill_before"] = before[name, "mixed"]["overkill_pct"]
        row["overkill_after"] = after[name, "mixed"]["overkill_pct"]
        row["cluster_targets_before"] = before[name, "dense_cluster"]["avg_targets_per_attack"]
        row["cluster_targets_after"] = after[name, "dense_cluster"]["avg_targets_per_attack"]
        full_rows.append(row)
        columns = [name] + [pair(before, after, (name, scene), "dps_ui") for scene in SCENARIOS]
        columns += [f"{row['overkill_before']:.1f} → {row['overkill_after']:.1f}",
                    f"{row['cluster_targets_before']:.2f} → {row['cluster_targets_after']:.2f}"]
        lines.append("| " + " | ".join(columns) + " |")
    def metric(name, scene, field, digits=1):
        return (f"{before[name,scene][field]:.{digits}f} → "
                f"{after[name,scene][field]:.{digits}f}")
    lines += ["", "## Changed buildings — mechanic breakdown", "",
              f"- Mortar: cluster {metric('mortar','dense_cluster','dps_ui')} DPS; "
              f"spread {metric('mortar','spread_group','dps_ui')}; mixed "
              f"{metric('mortar','mixed','dps_ui')}; passing "
              f"{metric('mortar','passing','dps_ui')}; direct hits per attack in cluster "
              f"{metric('mortar','dense_cluster','avg_targets_per_attack',2)}.",
              f"- Spikes: mixed {metric('spikes','mixed','dps_ui')} DPS; "
              f"overkill {metric('spikes','mixed','overkill_pct')}%; "
              f"kills/min {metric('spikes','mixed','kills')}; "
              f"activations {metric('spikes','mixed','activations')}.",
              f"- Flamethrower: infinite single {metric('flamethrower','single_infinite','dps_ui')} DPS; "
              f"dense line {metric('flamethrower','dense_line','dps_ui')}; "
              f"cluster {metric('flamethrower','dense_cluster','dps_ui')}; "
              f"mixed {metric('flamethrower','mixed','dps_ui')}; "
              f"cluster burn contribution {metric('flamethrower','dense_cluster','burn_damage_ui')} UI damage.",
              f"- Rocket: infinite single {metric('rocket','single_infinite','dps_ui')} DPS; "
              f"cluster {metric('rocket','dense_cluster','dps_ui')}; "
              f"spread {metric('rocket','spread_group','dps_ui')}; "
              f"mixed {metric('rocket','mixed','dps_ui')}; "
              f"cluster hits per attack {metric('rocket','dense_cluster','avg_targets_per_attack',2)}; "
              f"splash contribution {metric('rocket','dense_cluster','secondary_damage_ui')} UI damage."]
    lines += ["", "## Real-HP TTK (seconds)", "",
              "A dash means no kill within the 6000-second cap. Targets are stationary archetypes with real base HP and damage mitigation; Boss uses HP 2400 without wave multipliers or AI summons.",
              "", "| Building | Normal | Fast | Heavy | Boss | Boss raw UI before/after | Boss focus max before/after |",
              "|---|---:|---:|---:|---:|---:|---:|"]
    ttk_rows = []
    for name in buildings:
        row = {"building": name}
        columns = [name]
        for scene in TTK:
            b, a = before[name, scene], after[name, scene]
            row[f"{scene}_before"] = b["ttk_seconds"] if b["ttk_complete"] else None
            row[f"{scene}_after"] = a["ttk_seconds"] if a["ttk_complete"] else None
            fmt = lambda item: f"{item['ttk_seconds']:.2f}" if item["ttk_complete"] else "—"
            columns.append(f"{fmt(b)} → {fmt(a)}")
        columns += [f"{before[name,'single_boss']['raw_damage_ui']:.0f} → {after[name,'single_boss']['raw_damage_ui']:.0f}",
                    f"{before[name,'single_boss']['focus_max_multiplier']:.2f} → {after[name,'single_boss']['focus_max_multiplier']:.2f}"]
        lines.append("| " + " | ".join(columns) + " |")
        ttk_rows.append(row)
    lines += ["", "Laser focus milestones (after patch): Normal reaches x2 at "
              f"{after['laser','single_normal']['focus_x2_seconds']:.2f}s and dies before x3; "
              f"Heavy reaches x2/x3/x4 at "
              f"{after['laser','single_heavy']['focus_x2_seconds']:.2f}/"
              f"{after['laser','single_heavy']['focus_x3_seconds']:.2f}/"
              f"{after['laser','single_heavy']['focus_x4_seconds']:.2f}s; "
              f"Boss reaches x4 and averages {after['laser','single_boss']['mean_focus_multiplier']:.2f}× active focus."]
    lines += ["", "| Laser target | Below x2 | x2–x3 | x3–x4 | At x4 |",
              "|---|---:|---:|---:|---:|"]
    for scene in ("single_normal", "single_heavy", "single_boss"):
        row = after["laser", scene]
        death = row["ttk_seconds"]
        thresholds = [row[f"focus_x{level}_seconds"] for level in (2, 3, 4)]
        boundaries = [0] + [death if t < 0 else min(t, death) for t in thresholds] + [death]
        boundaries = [max(boundaries[i - 1], t) if i else t for i, t in enumerate(boundaries)]
        durations = [boundaries[i + 1] - boundaries[i] for i in range(4)]
        lines.append("| " + scene + " | " + " | ".join(f"{t:.2f}s" for t in durations) + " |")
    lines += ["", "## Ratios to Machine Gun Turret after patch", "",
              "The turret is 1.00× in each scenario separately; no combined score.", "",
              "| Building | Infinite | Cluster | Spread | Mixed | Passing |",
              "|---|---:|---:|---:|---:|---:|"]
    for name in buildings:
        if name in CONTROL:
            continue
        lines.append("| " + name + " | " + " | ".join(
            f"{after[name,scene]['turret_ratio']:.2f}×" for scene in
            ("single_infinite", "dense_cluster", "spread_group", "mixed", "passing")) + " |")
    lines += ["", "## Rankings after patch", ""]
    for scene in SCENARIOS:
        ranks = sorted((after[name, scene] for name in buildings if name not in CONTROL and
                        after[name, scene]["can_target"]), key=lambda row: -row["dps_ui"])
        lines.append(f"- {scene}: " + ", ".join(f"{r['building']} {r['dps_ui']:.1f}" for r in ranks))
    lines += ["", "## Control and support", ""]
    for name in CONTROL:
        key = name, "passing"
        lines.append(f"- {name}: added exposure {before[key]['added_exposure_seconds']:.1f} → "
                     f"{after[key]['added_exposure_seconds']:.1f} enemy-seconds.")
    before_support = read_csv(before_dir / "support.csv")
    after_support = read_csv(after_dir / "support.csv")
    validate_pair(before_support, after_support, ("building", "scenario", "seed"))
    lines.append("- Repair Station and Decoy support samples are paired in `support.csv` in each run.")

    upgrades = means(after_up, ("building", "profile", "scenario"), "dps_ui")
    control_exposure = means(after_up, ("building", "profile", "scenario"),
                             "control_exposure_seconds")
    before_upgrades = means(before_up, ("building", "profile", "scenario"), "dps_ui")
    scaling_rows = []
    lines += ["", "## Upgrade scaling after patch", "",
              "Each listed stat is one Common card applied through the production BuildingStatUpgrades path.",
              "The table shows the dense-cluster scenario; `scaling.csv` contains infinite, line and mixed results too.",
              "", "| Building | Package | Base DPS | Upgraded DPS | Multiplier | Before-patch upgraded DPS |",
              "|---|---|---:|---:|---:|---:|"]
    for (name, profile, scene), upgraded in sorted(upgrades.items()):
        base = upgrades[name, "base", scene]
        old = before_upgrades[name, profile, scene]
        record = {"building": name, "profile": profile, "scenario": scene,
                  "base_dps": base, "upgraded_dps": upgraded,
                  "multiplier": upgraded / base if base else None,
                  "before_upgraded_dps": old,
                  "base_control_exposure": control_exposure[name, "base", scene],
                  "upgraded_control_exposure": control_exposure[name, profile, scene]}
        scaling_rows.append(record)
        if scene == "dense_cluster":
            multiplier = f"{record['multiplier']:.2f}×" if record["multiplier"] is not None else "N/A"
            lines.append(f"| {name} | {profile} | {base:.1f} | {upgraded:.1f} | {multiplier} | {old:.1f} |")
    lines += ["", "Control profiles use added exposure instead of DPS (mixed wave):", "",
              "| Building | Profile | Base enemy-s | Upgraded enemy-s | Multiplier |",
              "|---|---|---:|---:|---:|"]
    for row in scaling_rows:
        if row["building"] not in CONTROL or row["scenario"] != "mixed":
            continue
        base = row["base_control_exposure"]
        upgraded = row["upgraded_control_exposure"]
        lines.append(f"| {row['building']} | {row['profile']} | {base:.1f} | "
                     f"{upgraded:.1f} | {upgraded/base:.2f}× |" if base else
                     f"| {row['building']} | {row['profile']} | 0 | {upgraded:.1f} | N/A |")
    lines += ["", "## Multiplicative synergy candidates", "",
              "Flag when combined DPS exceeds the additive prediction by more than 8% and 2 UI DPS. A flag is diagnostic, not an automatic nerf.", ""]
    synergy_pairs = {
        "crossbow": ("count", "pierce", "count_pierce"),
        "tesla": ("bounces", "attack_count", "bounces_attack_count"),
        "rocket": ("count", "size", "count_size"),
        "cannon": ("count", "size", "count_size"),
        "catapult": ("count", "size", "count_size"),
        "mortar": ("count", "size", "count_size"),
        "flamethrower": ("size", "duration", "size_duration"),
        "laser": ("duration", "pierce", "duration_pierce"),
    }
    synergy = []
    for name, (a, b, combo) in synergy_pairs.items():
        for scene in ("dense_line", "dense_cluster", "mixed"):
            base = upgrades[name, "base", scene]
            expected = upgrades[name, a, scene] + upgrades[name, b, scene] - base
            actual = upgrades[name, combo, scene]
            delta = actual - expected
            if expected > 0 and delta > 2 and actual > 1.08 * expected:
                note = f"{name} {scene}: {a}+{b} = {actual:.1f} DPS; additive {expected:.1f}; excess {delta:+.1f}"
                lines.append("- " + note)
                synergy.append(note)
    if not synergy:
        lines.append("- None above threshold in these standardized scenes.")
    output.mkdir(parents=True, exist_ok=True)
    with (output / "comparison.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(full_rows[0]))
        writer.writeheader(); writer.writerows(full_rows)
    with (output / "scaling.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(scaling_rows[0]))
        writer.writeheader(); writer.writerows(scaling_rows)
    (output / "comparison.json").write_text(json.dumps({"before": str(before_dir),
        "after": str(after_dir), "before_after": full_rows, "ttk": ttk_rows,
        "scaling": scaling_rows, "synergy_flags": synergy}, indent=2) + "\n")
    (output / "comparison.md").write_text("\n".join(lines) + "\n")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, default=ROOT / "output/defense_balance/before")
    parser.add_argument("--after", type=Path, default=ROOT / "output/defense_balance/after")
    parser.add_argument("--output", type=Path, default=ROOT / "output/defense_balance/comparison")
    args = parser.parse_args()
    print(compare(args.before.resolve(), args.after.resolve(), args.output.resolve()))


if __name__ == "__main__":
    main()
