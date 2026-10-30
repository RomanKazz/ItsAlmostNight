"""Checks aggregation and N/A semantics for the Level-1 defense report."""
import importlib.util
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).parents[1] / "tools/defense_balance_report.py"
SPEC = importlib.util.spec_from_file_location("defense_balance_report", SOURCE)
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)


def fixture():
    rows = []
    for name in report.BUILDINGS:
        for scene in report.SCENARIOS:
            for seed in (1, 2):
                row = {field: "0" for field in report.FIELDS}
                row.update(building=name, scenario=scene, seed=str(seed), seconds="60",
                           wood="20", stone="10", crystals="0", hp="100",
                           can_target="0" if scene == "flying" and name == "machine_gun" else "1",
                           raw_damage_ui="30", effective_damage_ui="20", overkill_ui="10",
                           dps_ui=str(seed), kills="2", attacks="3", hits="4")
                rows.append(row)
    return rows


class DefenseBalanceReportTests(unittest.TestCase):
    def test_benchmark_self_check_does_not_write_game_data(self):
        binary = os.environ.get("IAN_DEFENSE_BALANCE_BINARY")
        if not binary:
            self.skipTest("CTest supplies the built benchmark binary")
        with tempfile.TemporaryDirectory() as scratch:
            data = Path(scratch) / "data"
            data.mkdir()
            for name in report.DATA_FILES:
                shutil.copy2(SOURCE.parents[1] / "assets/data" / name, data / name)
            before = {name: hashlib.sha256((data / name).read_bytes()).hexdigest()
                      for name in report.DATA_FILES}
            subprocess.run((binary, "--self-test", "--data", str(data)),
                           cwd=scratch, check=True, capture_output=True, text=True)
            after = {name: hashlib.sha256((data / name).read_bytes()).hexdigest()
                     for name in report.DATA_FILES}
            self.assertEqual(before, after)
            self.assertEqual(sorted(path.name for path in data.iterdir()), sorted(report.DATA_FILES))
            self.assertEqual(sorted(path.name for path in Path(scratch).iterdir()), ["data"])

    def test_mean_min_max_and_unavailable_air(self):
        values = report.aggregate(fixture())
        row = values["crossbow", "single_infinite"]
        self.assertEqual((row["dps_ui"], row["dps_ui_min"], row["dps_ui_max"]),
                         (1.5, 1, 2))
        self.assertEqual(row["turret_ratio"], 1)
        self.assertEqual(row["overkill_ui"], 10)
        self.assertIsNone(row["dps_per_crystal"])
        self.assertEqual(report.cell(values["machine_gun", "flying"]), "N/A")
        self.assertEqual(report.cell(values["crossbow", "flying"]), "1.5")

    def test_rejects_missing_duplicate_and_bad_damage_accounting(self):
        rows = fixture()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            report.aggregate(rows + [rows[0]])
        with self.assertRaisesRegex(ValueError, "missing case"):
            report.aggregate(rows[2:])
        rows[0]["overkill_ui"] = "9"
        with self.assertRaisesRegex(ValueError, "overkill accounting"):
            report.aggregate(rows)

    def test_aggregate_is_order_independent(self):
        rows = fixture()
        first = report.aggregate(rows)
        second = report.aggregate(list(reversed(rows)))
        self.assertEqual(first, second)
        self.assertEqual(len(first), len(report.BUILDINGS) * len(report.SCENARIOS))


if __name__ == "__main__":
    unittest.main()
