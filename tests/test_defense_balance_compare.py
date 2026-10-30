"""Regression checks for paired before/after and scaling reports."""
import importlib.util
from pathlib import Path
import unittest

SOURCE = Path(__file__).parents[1] / "tools/defense_balance_compare.py"
SPEC = importlib.util.spec_from_file_location("defense_balance_compare", SOURCE)
compare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compare)


class DefenseBalanceCompareTests(unittest.TestCase):
    def test_paired_runs_require_same_seed_and_scenario(self):
        rows = [{"building": "laser", "scenario": "mixed", "seed": "1", "seconds": "60"},
                {"building": "laser", "scenario": "mixed", "seed": "2", "seconds": "60"}]
        first, second = compare.validate_pair(rows, list(reversed(rows)),
                                              ("building", "scenario", "seed"))
        self.assertEqual(first.keys(), second.keys())
        with self.assertRaisesRegex(ValueError, "identical"):
            compare.validate_pair(rows, rows[:1], ("building", "scenario", "seed"))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            compare.validate_pair(rows + rows[:1], rows, ("building", "scenario", "seed"))

    def test_aggregate_upgrade_profiles_keeps_seed_pairs(self):
        samples = [{"building": "rocket", "profile": "count", "scenario": "mixed",
                    "seed": str(seed), "dps_ui": str(dps)}
                   for seed, dps in ((1, 20), (2, 30), (3, 40))]
        self.assertEqual(compare.means(samples, ("building", "profile", "scenario"),
                                       "dps_ui")[("rocket", "count", "mixed")], 30)


if __name__ == "__main__":
    unittest.main()
