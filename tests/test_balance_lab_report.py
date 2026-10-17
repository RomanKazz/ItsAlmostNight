"""Report checks use small hand-calculated combat outcomes, without a display."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('balance_report', Path(__file__).parents[1] / 'tools/run_balance_lab.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


def row(building, profile='base', seed='1', damage=0, kills=0, leaked=5):
    return dict(building=building, level='4', scene='dense', profile=profile, seed=seed,
                spawned='10', kills=str(kills), leaked=str(leaked), alive=str(10-kills-leaked),
                dps_hp=str(damage), seconds='1', bonus_cards='0' if profile == 'base' else '4',
                applicable_cards='0', wood='10', stone='5', crystals='2', slow_enemy_seconds='0',
                push_enemy_seconds='0', dropped_projectiles='0', peak_projectiles='1')


class BalanceReportTests(unittest.TestCase):
    def test_growth_and_leak_comparison(self):
        rows = [row('no_defense'), row('cannon', damage=10, kills=1),
                row('cannon', 'quantity', damage=30, kills=3, leaked=2)]
        value = report.aggregate(rows)[-1]
        self.assertEqual(value['gain'], 3)
        self.assertEqual(value['prevented_leaks'], 3)
        self.assertEqual(value['dps_min'], 30)

    def test_control_zero_dps_has_no_invented_multiplier(self):
        values = report.aggregate([row('no_defense'), row('slow_trap'), row('slow_trap', 'quantity')])
        self.assertIsNone(values[-1]['gain'])

    def test_invalid_enemy_accounting_and_duplicate_cases(self):
        invalid = row('no_defense'); invalid['spawned'] = '11'
        with self.assertRaises(ValueError): report.aggregate([invalid])
        with self.assertRaises(ValueError): report.aggregate([row('no_defense'), row('no_defense')])

    def test_unmatched_seed_and_missing_baseline(self):
        with self.assertRaises(ValueError): report.aggregate([row('no_defense'), row('cannon', seed='2')])
        with self.assertRaises(ValueError): report.aggregate([row('no_defense'), row('cannon', 'quantity')])

    def test_rendered_report_has_no_external_dependency(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            report.write_report(output, [row('no_defense'), row('slow_trap')], {})
            page = (output / 'report.html').read_text(encoding='utf-8')
            self.assertNotIn('__DATA__', page)
            self.assertNotIn('NaN', page)
            self.assertNotIn('<script src=', page)
            self.assertTrue((output / 'summary.md').exists())


if __name__ == '__main__':
    unittest.main()
