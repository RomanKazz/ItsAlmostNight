#!/usr/bin/env python3
"""Build/run the real combat systems and generate a local, interactive balance report."""
import argparse
import csv
from datetime import datetime
import hashlib
import html
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import webbrowser

ROOT = Path(__file__).resolve().parents[1]
LABELS = dict(zip(
    ["gun_turret", "crossbow", "cannon", "catapult", "flamethrower", "missile_launcher",
     "tesla", "laser", "mortar", "spikes", "slow_trap", "wind_blower"],
    ["Турель", "Арбалет", "Пушка", "Катапульта", "Огнемёт", "Ракетница", "Тесла",
     "Лазер", "Мортира", "Шипы", "Замедляющая ловушка", "Ветродуй"]))
PROFILES = {
    "bounces": "4 рикошета",
    "pierce": "4 пробивания",
    "base": "Без бонусов",
    "damage_rate": "2 урона + 2 скорости атаки",
    "quantity": "4 дополнительных снаряда",
    "quantity_bounce_pierce": "2 снаряда + 1 отскок + 1 пробитие",
}
SCENES = {"heavy": "Одиночный танк", "dense": "Плотная толпа",
          "fast": "Быстрые разреженные", "flying": "Воздушная группа"}
DATA_FILES = ["enemies.json", "waves.json", "buildings.json", "weapons.json", "economy.json", "gameplay.json"]


def aggregate(rows):
    groups = {}
    identities = set()
    for row in rows:
        identity = tuple(row[k] for k in ("building", "level", "scene", "profile", "seed"))
        if identity in identities:
            raise ValueError(f"Duplicate case: {identity}")
        identities.add(identity)
        if int(row["spawned"]) != sum(int(row[k]) for k in ("kills", "leaked", "alive")):
            raise ValueError(f"Invalid enemy accounting: {identity}")
        key = identity[:4]
        groups.setdefault(key, []).append(row)
    result = []
    for (building, level, scene, profile), members in groups.items():
        value = {"building": building, "level": int(level), "scene": scene, "profile": profile,
                 "samples": len(members)}
        for key in members[0]:
            if key not in ("building", "level", "scene", "profile", "seed"):
                value[key] = statistics.mean(float(row[key]) for row in members)
        value["dps_min"] = min(float(row["dps_hp"]) for row in members)
        value["dps_max"] = max(float(row["dps_hp"]) for row in members)
        result.append(value)
    lookup = {(row["building"], row["level"], row["scene"], row["profile"]): row for row in result}
    for row in result:
        base = lookup.get((row["building"], row["level"], row["scene"], "base"))
        empty = lookup.get(("no_defense", row["level"], row["scene"], "base"))
        if base is None or empty is None or row["samples"] != base["samples"] or row["samples"] != empty["samples"]:
            raise ValueError("Missing or incomplete baseline; regenerate the full matrix")
        own_seeds = {member['seed'] for member in groups[(row['building'], str(row['level']), row['scene'], row['profile'])]}
        base_seeds = {member['seed'] for member in groups[(row['building'], str(row['level']), row['scene'], 'base')]}
        empty_seeds = {member['seed'] for member in groups[('no_defense', str(row['level']), row['scene'], 'base')]}
        if own_seeds != base_seeds or own_seeds != empty_seeds:
            raise ValueError("Unpaired layout seeds; regenerate matching baselines")
        row["gain"] = row["dps_hp"] / base["dps_hp"] if base["dps_hp"] else None
        row["prevented_leaks"] = empty["leaked"] - row["leaked"]
        row["control"] = row["slow_enemy_seconds"] + row["push_enemy_seconds"]
    return result


PAGE = r'''<!doctype html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Боевой полигон</title>
<style>
:root{color-scheme:dark;font:15px system-ui;background:#131b1b;color:#edf1e8}body{max-width:1500px;margin:32px auto;padding:0 24px}
h1{font-size:32px;margin-bottom:8px;color:#ffcf76}p{line-height:1.55;color:#b7c5bf}code{color:#e2bc77}
.filters{display:flex;gap:18px;flex-wrap:wrap;padding:20px;background:#202d2c;border-radius:14px;margin:24px 0}
label{display:grid;gap:8px;color:#ccd6ce}select{font:inherit;border:1px solid #526862;border-radius:8px;padding:10px;background:#152221;color:#fff}
.table{overflow:auto;border:1px solid #3d514b;border-radius:12px}table{border-collapse:collapse;width:100%;white-space:nowrap}
th,td{padding:14px 12px;border-bottom:1px solid #34433f;text-align:right}th{background:#263630;font-size:12px;color:#c5d4ca}
th:first-child,td:first-child{text-align:left}tr:hover{background:#25352f}.good{color:#8adea5}.bad{color:#ffa59b}.muted{color:#82968c}
.cards{display:flex;gap:12px;flex-wrap:wrap}.card{padding:16px 22px;border-radius:12px;background:#202d2c;min-width:150px}.card b{display:block;font-size:25px;margin-top:8px}
details{margin-top:24px;background:#202d2c;padding:18px;border-radius:12px}summary{cursor:pointer;color:#ffcf76}li{margin:8px 0;line-height:1.5;color:#c7d2cb}
small{display:block;margin-top:24px;color:#8fa399;line-height:1.6}a{color:#ffcf76}
</style><h1>Боевой полигон</h1><p>Реальные боевые системы · 60 тиков/с · плоская открытая площадка · один экземпляр сооружения.<br>
Это сравнение атак и контроля, а не итоговый рейтинг полезности зданий.</p>
<div class="cards"><div class="card">Уровни<b>1 · 4 · 8</b></div><div class="card">Атакующих / контрольных типов<b>12</b></div>
<div class="card">Сырых прогонов<b>__COUNT__</b></div><div class="card">Длительность прогона<b>__SECONDS__ с</b></div></div>
<div class="filters">
<label>Уровень<select id="level"><option>1</option><option selected>4</option><option>8</option></select></label>
<label>Ситуация<select id="scene"><option value="heavy">Одиночный танк</option><option value="dense" selected>Плотная толпа</option><option value="fast">Быстрые разреженные</option><option value="flying">Воздушная группа</option></select></label>
<label>Бонусы<select id="profile"><option value="base">Без бонусов</option><option value="damage_rate">2 урона + 2 скорости атаки</option><option value="quantity">4 дополнительных снаряда</option><option value="quantity_bounce_pierce" selected>2 снаряда + 1 отскок + 1 пробитие</option><option value="bounces">4 рикошета</option><option value="pierce">4 пробивания</option></select></label>
<label>Сортировка<select id="sort"><option value="dps_hp">Фактический DPS</option><option value="kills">Убийства</option><option value="prevented_leaks">Сдержанные прорывы</option><option value="control">Контроль</option><option value="gain">Рост от бонусов</option></select></label></div>
<div class="table"><table><thead><tr><th>Сооружение</th><th>DPS, HP/с</th><th>К своей базе</th><th>Убийства</th><th>Прорывы</th><th>Сдержано*</th><th>Замедление**</th><th>Отбрасывание**</th><th>Полезные карточки</th><th>Стройка: Д / К / Кр</th><th>Пик снарядов</th></tr></thead><tbody id="rows"></tbody></table></div>
<p id="notes"></p><p>* Разница числа прорывов относительно пустой линии за то же время. При непрерывном пополнении состав входящего потока зависит от скорости убийств.<br>
** Замедление — сумма времени врагов, взвешенная по силе эффекта. Отбрасывание — суммарное время врагов с ненулевой горизонтальной скоростью отталкивания. Эти показатели могут пересекаться.</p>
<details><summary>Как читать результаты и ограничения</summary><ul>
<li>Каждый усиленный пресет стоит четыре обычные карточки; база — ноль. Бонусы используют игровые формулы и совместимость статов. Не все пресеты полезны всем зданиям: столбец полезных карточек это показывает.</li>
<li>Начальные группы: 1 танк, 32 обычных врага, 12 быстрых или 12 летающих. После убийства или прорыва группа пополняется следующим тиком. Это постоянная нагрузка, не бюджет конкретной ночи.</li>
<li>Навигация, полёт, попадания, взрывы, отскоки, пробитие, горение, замедление и отталкивание работают настоящими системами игры. Урон считается после защиты и ограничен оставшимся HP.</li>
<li>Орудие стоит в (0,0), ядро — в (0,6), враги идут с отрицательной Z. Навигация видит только ядро: само орудие не блокирует путь и не получает урон. Прорыв — пересечение Z=4 или начало атаки ядра. Прорвавшийся враг удаляется и не считается убитым.</li>
<li>HP растут по enemyWaveHealthGrowth(уровень). Это согласованная нагрузка для сравнения, а не полная формула ночи. Уровень здания и HP-масштаб выводятся в CSV.</li>
<li>Три ресурса не складываются в выдуманную общую цену. Цена в таблице — базовая стоимость первой копии, без платформ, повторного строительства и стоимости открытия карточки.</li>
<li>Уровень 8 не означает наличие эволюции. Предметы, метабонусы, классы, шрайны, ярость, ядро как источник урона, рельеф, босс и стоимость ремонта здесь не участвуют.</li>
<li>Стены, приманка, ремонтная станция и добывалки не оцениваются по DPS; для них нужен отдельный сценарий выживания базы. Выводы полигона не меняют баланс автоматически.</li>
</ul></details><small>__META__<br><a href="raw.csv">Исходный CSV</a> · <a href="summary.md">Краткая таблица</a> · <a href="metadata.json">Версии и параметры</a></small>
<script>const data=__DATA__, names=__LABELS__;
const byId=id=>document.getElementById(id), fmt=(x,n=1)=>Number(x).toLocaleString('ru-RU',{maximumFractionDigits:n});
function draw(){const rows=data.filter(r=>r.building!=='no_defense'&&r.level===Number(byId('level').value)&&r.scene===byId('scene').value&&r.profile===byId('profile').value);
const sort=byId('sort').value;rows.sort((a,b)=>(b[sort]??-1)-(a[sort]??-1));
byId('rows').innerHTML=rows.map(r=>`<tr><td>${names[r.building]}</td><td title="Разброс: ${fmt(r.dps_min)}–${fmt(r.dps_max)}">${fmt(r.dps_hp)}</td><td>${r.gain===null?'—':fmt(r.gain,2)+'×'}</td><td>${fmt(r.kills)}</td><td>${fmt(r.leaked)}</td><td class="${r.prevented_leaks<0?'bad':'good'}">${fmt(r.prevented_leaks)}</td><td>${fmt(r.slow_enemy_seconds)}</td><td>${fmt(r.push_enemy_seconds)}</td><td class="${r.bonus_cards&&r.applicable_cards<r.bonus_cards?'bad':''}">${r.bonus_cards?fmt(r.applicable_cards,0)+' / '+fmt(r.bonus_cards,0):'—'}</td><td>${fmt(r.wood,0)} / ${fmt(r.stone,0)} / ${fmt(r.crystals,0)}</td><td class="${r.dropped_projectiles?'bad':''}">${fmt(r.peak_projectiles,0)}${r.dropped_projectiles?' ⚠ потери: '+fmt(r.dropped_projectiles,0):''}</td></tr>`).join('');
const empty=data.find(r=>r.building==='no_defense'&&r.level===Number(byId('level').value)&&r.scene===byId('scene').value);
byId('notes').textContent=`Среднее по ${rows[0]?.samples??0} раскладкам. Прорывов без обороны: ${fmt(empty?.leaked??0)}. Наведи курсор на DPS, чтобы увидеть разброс.`;}
for(const id of ['level','scene','profile','sort'])byId(id).addEventListener('change',draw);draw();</script></html>'''


def write_report(output, rows, metadata):
    values = aggregate(rows)
    encoded = json.dumps(values, ensure_ascii=False, allow_nan=False).replace('</', '<\\/')
    page = PAGE.replace('__DATA__', encoded).replace('__LABELS__', json.dumps(LABELS, ensure_ascii=False))
    page = page.replace('__COUNT__', str(len(rows))).replace('__SECONDS__', str(rows[0]['seconds']))
    page = page.replace('__META__', html.escape(f"Git: {metadata.get('git_revision', 'unknown')} · "
        f"рабочие изменения: {metadata.get('working_tree_dirty', 'unknown')} · "
        f"источники: {metadata.get('source_digest', 'unknown')[:16]}"))
    (output / 'report.html').write_text(page, encoding='utf-8')
    summary = ["# Боевой полигон", "", "Уровень 4, плотная толпа. Урон в отображаемых HP. Усиленный пресет: 2 снаряда + отскок + пробитие.", "",
               "| Сооружение | DPS база | DPS с бонусами | Рост | Убийства | Прорывы | Полезные карточки |",
               "|---|---:|---:|---:|---:|---:|---:|"]
    for row in sorted(values, key=lambda row: -row['dps_hp']):
        if row['building'] == 'no_defense' or (row['level'], row['scene'], row['profile']) != (4, 'dense', 'quantity_bounce_pierce'):
            continue
        base = next(x for x in values if (x['building'], x['level'], x['scene'], x['profile']) == (row['building'], 4, 'dense', 'base'))
        gain = f"{row['gain']:.2f}x" if row['gain'] is not None else '—'
        summary.append(f"| {LABELS[row['building']]} | {base['dps_hp']:.1f} | {row['dps_hp']:.1f} | {gain} | {row['kills']:.1f} | {row['leaked']:.1f} | {row['applicable_cards']:.0f}/4 |")
    summary += ["", "Одинаковые вложения не означают одинаковую полезность томов. Нулевой DPS контроля не является оценкой его силы.",
                "Подробные условия и ограничения — в report.html. В CSV остаются все уровни, пресеты и отдельные раскладки."]
    (output / 'summary.md').write_text('\n'.join(summary) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--preset', choices=['release', 'debug', 'sanitizers'], default='release')
    parser.add_argument('--seconds', type=int, choices=range(1, 121), default=30, metavar='1..120')
    parser.add_argument('--seeds', type=int, choices=range(1, 9), default=3, metavar='1..8')
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--csv', type=Path, help='Regenerate reports from an existing full CSV')
    parser.add_argument('--open', action='store_true', help='Open the local HTML report')
    args = parser.parse_args()
    output = (args.output or ROOT / 'output' / 'balance_lab' / datetime.now().strftime('%Y%m%d-%H%M%S')).resolve()
    output.mkdir(parents=True, exist_ok=True)
    metadata = {}
    if args.csv:
        source = args.csv.resolve()
        raw = output / 'raw.csv'
        if source != raw:
            shutil.copy2(source, raw)
        source_meta = source.parent / 'metadata.json'
        if source_meta.exists():
            metadata = json.loads(source_meta.read_text(encoding='utf-8'))
    else:
        if not args.no_build:
            subprocess.run(['cmake', '--preset', args.preset], cwd=ROOT, check=True)
            subprocess.run(['cmake', '--build', '--preset', args.preset, '--target', 'ian_balance_lab', '-j', '8'], cwd=ROOT, check=True)
        binary = ROOT / 'build' / args.preset / ('ian_balance_lab.exe' if sys.platform == 'win32' else 'ian_balance_lab')
        subprocess.run([str(binary), '--output', str(output / 'raw.csv'), '--data', str(ROOT / 'assets/data'),
                        '--seconds', str(args.seconds), '--seeds', str(args.seeds)], cwd=ROOT, check=True)
        revision = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, capture_output=True, text=True)
        status = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True)
        sources = sorted([*(path for path in (ROOT / 'src').rglob('*') if path.suffix in ('.cpp', '.hpp')),
                          ROOT / 'tools/BalanceLab.cpp', ROOT / 'tools/run_balance_lab.py',
                          ROOT / 'CMakeLists.txt', ROOT / 'CMakePresets.json',
                          *(ROOT / 'assets/data' / name for name in DATA_FILES)])
        digest = hashlib.sha256()
        for path in sources:
            digest.update(str(path.relative_to(ROOT)).encode()); digest.update(path.read_bytes())
        metadata = {'git_revision': revision.stdout.strip(), 'working_tree_dirty': bool(status.stdout),
                    'source_digest': digest.hexdigest(), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'preset': args.preset, 'seconds': args.seconds, 'seeds': args.seeds, 'tick_hz': 60,
                    'created': datetime.now().isoformat(timespec='seconds'), 'build_requested': not args.no_build}
        (output / 'config').mkdir(exist_ok=True)
        for name in DATA_FILES:
            shutil.copy2(ROOT / 'assets/data' / name, output / 'config' / name)
    with (output / 'raw.csv').open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError('Empty CSV')
    write_report(output, rows, metadata)
    (output / 'metadata.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Report: {output / 'report.html'}")
    if args.open:
        webbrowser.open((output / 'report.html').as_uri())


if __name__ == '__main__':
    main()
