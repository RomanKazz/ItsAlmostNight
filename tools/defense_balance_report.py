#!/usr/bin/env python3
"""Run the Lv1 real-combat benchmark and summarize raw, paired seed results."""
import argparse
import csv
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = (
    "single_infinite", "single_infinite_heavy", "dense_line", "dense_cluster",
    "spread_group", "fast", "flying", "mixed", "passing",
    "single_normal", "single_fast", "single_heavy", "single_boss",
)
COMBAT_SCENARIOS = SCENARIOS[:9]
TTK_SCENARIOS = SCENARIOS[9:]
BUILDINGS = (
    "crossbow", "machine_gun", "cannon", "catapult", "mortar",
    "flamethrower", "rocket", "tesla", "laser", "wind_blower",
    "spikes", "slow_trap",
)
CONTROL = {"wind_blower", "slow_trap"}
DATA_FILES = ("enemies.json", "waves.json", "buildings.json", "weapons.json",
              "economy.json", "gameplay.json")
FIELDS = ("raw_damage_ui", "effective_damage_ui", "overkill_ui", "overkill_pct",
          "dps_ui", "kills", "attacks", "projectiles", "hits", "unique_enemies",
          "avg_targets_per_attack", "peak_targets", "attack_event_tick_fraction",
          "target_engagement_fraction", "idle_with_valid_enemy_seconds", "first_attack_delay_seconds",
          "slow_enemy_seconds", "slow_weighted_seconds", "slow_presence_fraction", "push_distance",
          "added_exposure_seconds", "affected", "activations", "direct_damage_ui",
          "burn_damage_ui", "primary_damage_ui", "secondary_damage_ui",
          "focus_extra_ui_estimate", "mean_focus_multiplier",
          "peak_projectiles", "dropped_projectiles", "leaked",
          "ttk_seconds", "ttk_complete", "focus_max_multiplier", "focus_at_kill",
          "focus_x2_seconds", "focus_x3_seconds", "focus_x4_seconds")


def aggregate(rows):
    groups = {}
    ids = set()
    for row in rows:
        ident = (row["building"], row["scenario"], int(row["seed"]))
        if ident in ids:
            raise ValueError(f"duplicate case {ident}")
        ids.add(ident)
        tolerance = 1e-7 * (1 + float(row["raw_damage_ui"]))
        if float(row["raw_damage_ui"]) + tolerance < float(row["effective_damage_ui"]):
            raise ValueError(f"raw damage below effective damage {ident}")
        if abs(float(row["raw_damage_ui"]) - float(row["effective_damage_ui"]) -
               float(row["overkill_ui"])) > tolerance:
            raise ValueError(f"overkill accounting {ident}")
        groups.setdefault(ident[:2], []).append(row)
    result = {}
    for building in BUILDINGS:
        for scene in SCENARIOS:
            members = groups.get((building, scene))
            if not members:
                raise ValueError(f"missing case: {building}/{scene}")
            seeds = sorted(int(row["seed"]) for row in members)
            if seeds != list(range(1, len(seeds) + 1)):
                raise ValueError(f"non-contiguous seeds: {building}/{scene}")
            summary = {"building": building, "scenario": scene, "samples": len(members),
                       "can_target": all(int(row["can_target"]) for row in members)}
            for key in ("wood", "stone", "crystals", "hp", "seconds"):
                values = [float(row[key]) for row in members]
                if min(values) != max(values):
                    raise ValueError(f"changing {key}: {building}/{scene}")
                summary[key] = values[0]
            for key in FIELDS:
                values = [float(row[key]) for row in members]
                summary[key] = statistics.mean(values)
                summary[key + "_min"] = min(values)
                summary[key + "_max"] = max(values)
            summary["damage_per_slot"] = summary["dps_ui"]
            summary["average_slow_pct"] = (100*summary["slow_weighted_seconds"] / summary["slow_enemy_seconds"]
                                           if summary["slow_enemy_seconds"] else 0)
            population = (1 if scene.startswith("single") else
                          24 if scene in ("dense_cluster", "spread_group") else 16)
            summary["slow_uptime"] = summary["slow_enemy_seconds"] / (summary["seconds"] * population)
            summary["average_targets_per_activation"] = (summary["affected"] / summary["activations"]
                                                          if summary["activations"] else 0)
            summary["dps_per_wood"] = summary["dps_ui"] / summary["wood"] if summary["wood"] else None
            summary["dps_per_stone"] = summary["dps_ui"] / summary["stone"] if summary["stone"] else None
            summary["dps_per_crystal"] = summary["dps_ui"] / summary["crystals"] if summary["crystals"] else None
            result[(building, scene)] = summary
    if len({row["samples"] for row in result.values()}) != 1:
        raise ValueError("scenario seed counts do not match")
    for row in result.values():
        base = result[("machine_gun", row["scenario"])]["dps_ui"]
        row["turret_ratio"] = row["dps_ui"] / base if base else None
    return result


def ranked(values, scene):
    return sorted((values[(name, scene)] for name in BUILDINGS
                   if name not in CONTROL and values[(name, scene)]["can_target"]),
                  key=lambda row: -row["dps_ui"])


def flags(values):
    warnings = []
    base = values[("machine_gun", "single_infinite")]
    for name in BUILDINGS:
        if name in CONTROL:
            if values[(name, "passing")]["added_exposure_seconds"] < 2:
                warnings.append(f"{name}: <2 enemy-seconds of added exposure on passing lane")
            continue
        single = values[(name, "single_infinite")]
        crowd = values[(name, "dense_cluster")]
        crowd_base = values[("machine_gun", "dense_cluster")]["dps_ui"]
        if crowd_base and crowd["dps_ui"] > 3*crowd_base:
            warnings.append(f"{name}: cluster DPS is {crowd['dps_ui']/crowd_base:.1f}x turret")
        if (single["dps_ui"] > 1.5*base["dps_ui"] and
                values[(name, "mixed")]["dps_ui"] < .8*values[("machine_gun", "mixed")]["dps_ui"]):
            warnings.append(f"{name}: strong infinite-target DPS but weak mixed-wave value")
        if (name != "machine_gun" and single["dps_ui"] > 2*base["dps_ui"] and
                all(single[k] <= 1.2*base[k] for k in ("wood", "stone", "crystals"))):
            warnings.append(f"{name}: >2x turret single DPS at no more than 1.2x each resource")
        if name in {"cannon", "catapult", "mortar", "flamethrower", "rocket"}:
            if (single["dps_ui"] >= base["dps_ui"] and
                    crowd["dps_ui"] == ranked(values, "dense_cluster")[0]["dps_ui"]):
                warnings.append(f"{name}: high single-target DPS and highest cluster DPS")
        if values[(name, "mixed")]["overkill_pct"] > 35:
            warnings.append(f"{name}: mixed-wave overkill {values[(name, 'mixed')]['overkill_pct']:.0f}%")
        if name == "crossbow" and values[(name, "dense_line")]["avg_targets_per_attack"] < 1.2:
            warnings.append("crossbow: pierce rarely adds targets in dense line")
        if name == "tesla" and values[(name, "dense_cluster")]["secondary_damage_ui"] < .1*crowd["effective_damage_ui"]:
            warnings.append("tesla: bounce damage is below 10% in cluster")
        if name == "flamethrower" and values[(name, "dense_cluster")]["burn_damage_ui"] < .05*crowd["effective_damage_ui"]:
            warnings.append("flamethrower: burn is below 5% of cluster damage")
        if (name == "rocket" and crowd["dps_ui"] < crowd_base and
                all(crowd[k] > values[("machine_gun", "dense_cluster")][k]
                    for k in ("wood", "stone", "crystals"))):
            warnings.append("rocket: costs more of every resource yet trails turret in cluster DPS")
        if sum(values[(name, scene)]["dps_ui"] < .5*values[("machine_gun", scene)]["dps_ui"]
               for scene in ("single_infinite", "dense_line", "dense_cluster", "spread_group", "fast", "mixed", "passing")) >= 6:
            warnings.append(f"{name}: below half of turret DPS in at least six comparable scenarios")
    return warnings


def cell(row, key="dps_ui"):
    if not row["can_target"]:
        return "N/A"
    return f"{row[key]:.1f}"


def markdown(values, support):
    lines = ["# Defensive buildings · Lv1 · current balance", "",
             "Real EnemySystem/TowerSystem/CannonSystem/TrapSystem; 60 Hz. DPS and damage are UI units (simulation damage ×10).",
             "Mean across fixed seeds; raw.csv has each sample and min/max appear in summary.json.",
             "", "| Building | Cost W/S/C | HP | Single | Tank | Line | Cluster | Spread | Fast | Air | Mixed | Passing | Kills/min¹ | Overkill %² | Hits/event³ | Damage/slot⁴ | Control enemy-s |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for name in BUILDINGS:
        r = lambda scene: values[(name, scene)]
        base = r("mixed")
        cost = f"{base['wood']:.0f}/{base['stone']:.0f}/{base['crystals']:.0f}"
        cells = [name, cost, f"{base['hp']:.0f}"] + [cell(r(scene)) for scene in COMBAT_SCENARIOS]
        cells += [f"{base['kills']:.1f}", f"{base['overkill_pct']:.1f}",
                  f"{base['avg_targets_per_attack']:.2f}", f"{base['damage_per_slot']:.1f}",
                  f"{r('passing')['added_exposure_seconds']:.1f}"]
        lines.append("| " + " | ".join(cells) + " |")
    lines += ["", "¹ Kills in the 60-second mixed scenario. ² Mixed wave. ³ Direct hits per attack event; a burst or lingering effect can produce several hits. ⁴ One building occupies one defense slot.",
              "", "## Cost efficiency · mixed wave (UI DPS per one resource)", "",
              "| Building | DPS/W | DPS/S | DPS/C | vs Machine Gun, single | vs Machine Gun, cluster | vs Machine Gun, mixed |",
              "|---|---:|---:|---:|---:|---:|---:|"]
    for name in BUILDINGS:
        row = values[(name, "mixed")]
        def per(key):
            value = row[key]
            return f"{value:.2f}" if value is not None else "N/A"
        def ratio(scene):
            value = values[(name, scene)]["turret_ratio"]
            return f"{value:.2f}×" if value is not None else "N/A"
        lines.append(f"| {name} | {per('dps_per_wood')} | {per('dps_per_stone')} | "
                     f"{per('dps_per_crystal')} | {ratio('single_infinite')} | "
                     f"{ratio('dense_cluster')} | {ratio('mixed')} |")
    lines += ["", "## Control · passing lane", "",
              "| Building | Slowed enemy-s | Average slow | Slow presence | Backward push distance | Added exposure enemy-s | Affected enemies | Targets/activation |",
              "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for name in ("wind_blower", "slow_trap"):
        row = values[(name, "passing")]
        targets_per_activation = (f"{row['average_targets_per_activation']:.2f}"
                                  if name == "slow_trap" else "N/A")
        lines.append(f"| {name} | {row['slow_enemy_seconds']:.1f} | {row['average_slow_pct']:.0f}% | "
                     f"{row['slow_presence_fraction']:.0%} | {row['push_distance']:.1f} m | "
                     f"{row['added_exposure_seconds']:.1f} | {row['affected']:.0f} | "
                     f"{targets_per_activation} |")
    lines += ["", "## Defining mechanics", ""]
    for name, scene, label in (("crossbow", "dense_line", "pierce"),
                              ("tesla", "dense_cluster", "bounce"),
                              ("rocket", "dense_cluster", "splash"),
                              ("laser", "dense_line", "pierce")):
        row = values[(name, scene)]
        percent = 100*row["secondary_damage_ui"]/row["effective_damage_ui"] if row["effective_damage_ui"] else 0
        lines.append(f"- {name} {label} ({scene}): {row['secondary_damage_ui']:.1f} UI damage, "
                     f"{percent:.0f}% of effective damage; {row['avg_targets_per_attack']:.2f} direct hits/event.")
    flame = values[("flamethrower", "dense_cluster")]
    laser = values[("laser", "single_infinite")]
    spikes = values[("spikes", "mixed")]
    lines.append(f"- flamethrower burn (cluster): {flame['burn_damage_ui']:.1f} UI damage "
                 f"of {flame['effective_damage_ui']:.1f} total.")
    lines.append(f"- laser focus (single): mean active multiplier {laser['mean_focus_multiplier']:.2f}×; "
                 f"estimated extra {laser['focus_extra_ui_estimate']:.1f} UI damage.")
    lines.append(f"- spikes (mixed): {spikes['activations']:.1f} activations, "
                 f"{spikes['avg_targets_per_attack']:.2f} direct hits/activation, "
                 f"{spikes['overkill_pct']:.1f}% overkill.")
    lines += ["", "## Rankings (DPS, UI units/s)", ""]
    for label, scene in (("Single target", "single_infinite"), ("Dense crowd", "dense_cluster"),
                         ("Spread crowd", "spread_group"), ("Anti-fast", "fast"),
                         ("Anti-air", "flying"), ("Damage per slot / mixed", "mixed")):
        entries = ranked(values, scene)
        lines.append(f"- **{label}:** " + ", ".join(f"{row['building']} {row['dps_ui']:.1f}" for row in entries))
    controls = sorted((values[(name, "passing")] for name in CONTROL),
                      key=lambda row: -row["added_exposure_seconds"])
    lines.append("- **Control (passing):** " + ", ".join(
        f"{row['building']} {row['added_exposure_seconds']:.1f} enemy-s" for row in controls))
    lines += ["", "## Real-HP single-target TTK (seconds)", "",
              "Base enemy HP and damage mitigation, no wave multipliers or movement/AI summons; stationary isolated target. A dash means no kill within 6000 seconds.",
              "", "| Building | Normal | Fast | Heavy | Boss | Boss focus max | Boss mean focus |",
              "|---|---:|---:|---:|---:|---:|---:|"]
    for name in BUILDINGS:
        def ttk(scene):
            row = values[(name, scene)]
            return f"{row['ttk_seconds']:.2f}" if row['ttk_complete'] == 1 else "—"
        boss = values[(name, "single_boss")]
        lines.append(f"| {name} | {ttk('single_normal')} | {ttk('single_fast')} | "
                     f"{ttk('single_heavy')} | {ttk('single_boss')} | "
                     f"{boss['focus_max_multiplier']:.2f}× | {boss['mean_focus_multiplier']:.2f}× |")
    lines += ["", "## Repair / decoy", ""]
    for row in support:
        if row["building"] == "repair_station":
            lines.append(f"- Repair {row['targets']} targets: {float(row['total_hp_restored']):.1f} HP / 60s, "
                         f"{float(row['hp_per_second_per_target']):.1f} HP/s each, "
                         f"{float(row['aggregate_hp_per_second']):.1f} HP/s aggregate.")
        else:
            lines.append(f"- Decoy seed {row['seed']}: {row['taunted_unique']} taunted, "
                         f"{float(row['taunt_enemy_seconds']):.1f} enemy-s, "
                         f"{float(row['damage_absorbed_ui']):.1f} UI damage absorbed, "
                         f"survival {float(row['survival_seconds']):.1f}s.")
    lines += ["", "## Automatic balance flags", ""]
    lines.extend(f"- {flag}" for flag in flags(values))
    lines += ["", "## Interpretation limits", "",
              "- Stationary ideal layouts use enemy speed 0 and an in-range target position; passing and wave layouts use the game's base enemy speeds. Defense attack and damage code is unchanged.",
              "- Passing enemies use a common straight lane 2m from ranged buildings. Ground traps are placed on that lane. Waves refill after kills and leaks; they have no total enemy cap.",
              "- The `single_infinite` targets have effectively infinite HP; the four TTK targets use real base HP without wave multipliers. Crowd overkill uses the exact post-mitigation, pre-HP-clamp damage sample.",
              "- In summary.json, secondary damage is Crossbow/Laser pierce, Tesla bounces, or Rocket splash as tagged by the combat system. Laser focus extra is an estimate from effective hits and the active focus multiplier; overkill can make its counterfactual lower.",
              "- The raw data has attack-event tick fraction and target-engagement fraction separately. Valid-enemy idle time excludes cooldown while a target remains acquired. First-attack delay -1 means no attack. Projectile count includes emitted events; hit rate is not a strict physical miss rate for beams and area effects.",
              "- Exposure is an estimate from observed slow and knockback velocities divided by normal speed. It is not converted to damage. No defensible core-attack prevention number can be inferred from the isolated decoy run.",
              "- Ratios to Machine Gun Turret are per scenario in summary.json. Prices remain a three-resource vector; no exchange rate or single overall score is invented.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "output/defense_balance/current")
    parser.add_argument("--preset", choices=("release", "debug", "sanitizers"), default="release")
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--from-csv", action="store_true")
    parser.add_argument("--upgrades", action="store_true", help="also run card-stat profiles through real combat")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 120 or not 1 <= args.seeds <= 8:
        parser.error("seconds must be 1..120 and seeds 1..8")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    metadata_path = output / "metadata.json"
    metadata = json.loads(metadata_path.read_text()) if args.from_csv and metadata_path.exists() else {}
    if not args.from_csv:
        if not args.no_build:
            subprocess.run(("cmake", "--preset", args.preset), cwd=ROOT, check=True)
            subprocess.run(("cmake", "--build", "--preset", args.preset,
                            "--target", "ian_defense_balance", "-j", "8"), cwd=ROOT, check=True)
        binary = ROOT / "build" / args.preset / "ian_defense_balance"
        command = [str(binary), "--output", str(output / "raw.csv"),
                   "--data", str(ROOT / "assets/data"),
                   "--seconds", str(args.seconds), "--seeds", str(args.seeds)]
        if args.upgrades:
            command += ["--upgrades-output", str(output / "upgrades.csv")]
        subprocess.run(command, cwd=ROOT, check=True)
        revision = subprocess.run(("git", "rev-parse", "--short", "HEAD"),
                                  cwd=ROOT, capture_output=True, text=True, check=True)
        dirty = subprocess.run(("git", "status", "--porcelain"),
                               cwd=ROOT, capture_output=True, text=True, check=True)
        config_dir = output / "config"
        config_dir.mkdir(exist_ok=True)
        config_hashes = {}
        for name in DATA_FILES:
            source = ROOT / "assets/data" / name
            shutil.copy2(source, config_dir / name)
            config_hashes[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        metadata = {"git_revision": revision.stdout.strip(),
                    "working_tree_dirty": bool(dirty.stdout),
                    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                    "config_sha256": config_hashes, "preset": args.preset,
                    "seconds": args.seconds, "seeds": args.seeds, "tick_hz": 60}
        metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
    with (output / "raw.csv").open(newline="") as stream:
        values = aggregate(list(csv.DictReader(stream)))
    with (output / "support.csv").open(newline="") as stream:
        support = list(csv.DictReader(stream))
    document = {"created": datetime.now().isoformat(timespec="seconds"),
                "metadata": metadata,
                "scenarios": SCENARIOS, "buildings": BUILDINGS,
                "results": list(values.values()), "support": support, "flags": flags(values)}
    (output / "summary.json").write_text(json.dumps(document, indent=2) + "\n")
    report = markdown(values, support)
    (output / "report.md").write_text(report + "\n")
    print(report)
    print(f"\nFiles: {output / 'raw.csv'}, {output / 'support.csv'}, {output / 'summary.json'}, {output / 'report.md'}")


if __name__ == "__main__":
    main()
