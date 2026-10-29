"""Author the defense game's quest graph and its offline C++ fallback together.

Run from any directory with Python 3; no third-party packages required.
Megabonk's action/mastery/challenge -> silver purchase progression is the design
reference, not its enemy roster or numeric grind requirements.
"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
quests = []
retired_shop_unlocks={"meta.luck","meta.defense_damage","meta.defense_fire_rate",
    "meta.building_health","meta.production_speed","meta.resource_yield"}
# The C++ access catalog is the source of truth for free starter content.
starter_unlocks = set(re.findall(
    r'(?:CardUnlockDefinition|ItemUnlockDefinition)\{[^{}\n]*?,\s*"([^"]+)",\s*true\}',
    (ROOT / "src/progression/ContentUnlocks.hpp").read_text()))
assert starter_unlocks, "Starter catalog must be readable before authoring rewards"


def write_artifact(path, content):
    if path.exists() and path.read_text() == content:
        return
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(content)
    temporary.replace(path)


def condition(metric, target, subject="", scope="lifetime", comparison="at_least", player=""):
    return dict(metric=metric, scope=scope, target=target, comparison=comparison,
                subject=subject, playerClass=player)


def quest(id, text, category, conditions, unlock="", price=0, silver=2, prerequisites=()):
    if unlock in starter_unlocks or unlock in retired_shop_unlocks:
        # Keep mastery goals and earned/claimed rewards under their stable IDs,
        # but never offer a purchase for content available from the first run.
        unlock, price = "", 0
    quests.append(dict(id=id, title=text, description=text, category=category,
                       conditions=conditions, prerequisites=list(prerequisites),
                       reward=dict(silver=silver, unlockId=unlock, purchaseCost=price)))


def unlock(id, text, category, metric, target, subject="", scope="lifetime", price=4,
           silver=2, extra=(), prerequisites=()):
    quest("unlock_" + id.replace(".", "_"), text, category,
          [condition(metric, target, subject, scope), *extra], id, price, silver, prerequisites)


# Preserve old quest IDs and earned silver; retired class rewards become general goals.
for id, text, metric, target, price in [
    ("berserker", "Reach night 6", "wave_reached", 6, 6),
    ("vampire", "Defeat 250 enemies", "enemies_defeated", 250, 6),
    ("alchemist", "Collect 20 chest items", "items_collected", 20, 8),
    ("chronomancer", "Clear the stage", "stage_clears", 1, 10),
]:
    quest("class_" + id, text, "general", [condition(metric, target)])

# Defense branches: cheap first discoveries, then mastery of an already accessible defense.
for args in [
    ("spike_trap", "Place 30 walls", "buildings_placed", 30, "wall", "lifetime", 3),
    ("slow_trap", "Defeat 25 fast enemies", "enemy_type_defeated", 25, "fast", "lifetime", 4),
    ("catapult", "Get Cannon to level 3 in one run", "upgrade_level", 3, "unlock_cannon", "run", 5),
    ("flamethrower", "Defeat 150 enemies with gun turrets in one run", "enemies_defeated", 150, "gun_turret", "run", 6),
    ("tesla_tower", "Defeat 50 flying enemies", "enemy_type_defeated", 50, "flying", "lifetime", 7),
    ("missile_launcher", "Get Cannon to level 6 in one run", "upgrade_level", 6, "unlock_cannon", "run", 9),
    ("laser", "Get Precision Tome to level 5 in one run", "upgrade_level", 5, "critical_chance", "run", 8),
    ("mortar", "Defeat 200 enemies with cannons", "enemies_defeated", 200, "cannon", "lifetime", 6),
    ("repair_station", "Repair 40 damaged structures", "structures_repaired", 40, "", "lifetime", 4),
    ("decoy_beacon", "Defeat 25 ranged enemies", "enemy_type_defeated", 25, "ranged", "lifetime", 5),
    ("wind_blower", "Survive night 4 without losing a structure", "waves_completed", 4, "", "run", 5),
]:
    id, text, metric, target, subject, scope, price = args
    extra = [condition("structures_lost", 0, scope="run", comparison="at_most")] if id == "wind_blower" else []
    unlock("building." + id, text, "buildings", metric, target, subject, scope, price, extra=extra)

# Starter defenses no longer require purchases. Keep stable quest IDs and
# existing rewards claimed; these now reward mastery with silver only.
quest("unlock_building_turret", "Defeat 200 enemies with crossbows", "buildings",
      [condition("enemies_defeated", 200, "turret")], silver=3)
quest("unlock_building_cannon", "Defeat 300 enemies with cannons", "buildings",
      [condition("enemies_defeated", 300, "cannon")], silver=4)

for id, text, metric, target, subject, price in [
    ("rifle", "Defeat 50 enemies yourself in one run", "enemies_defeated", 50, "player", 3),
    ("bomb", "Defeat 10 heavy enemies", "enemy_type_defeated", 10, "heavy", 4),
    ("ice_wand", "Activate 3 shrines", "shrines_activated", 3, "", 5),
    ("fire_wand", "Get Might Tome to level 5 in one run", "upgrade_level", 5, "damage", 5),
]:
    unlock("weapon." + id, text, "weapons", metric, target, subject,
           "run" if id in ("rifle", "fire_wand") else "lifetime", price)

for id, text, metric, target, subject, scope, price in [
    ("building_health", "Repair 12 damaged structures", "structures_repaired", 12, "", "lifetime", 2),
    ("production_speed", "Build 6 resource producers in one run", "producers_built", 6, "", "run", 3),
    ("resource_capacity", "Fully harvest 40 resource nodes", "nodes_depleted", 40, "", "lifetime", 2),
    ("luck", "Open 12 chests", "chests_opened", 12, "", "lifetime", 3),
    ("projectile_count", "Fire 500 defense shots", "defense_shots", 500, "", "lifetime", 5),
    ("projectile_speed", "Get Crossbow to level 3 in one run", "upgrade_level", 3, "unlock_turret", "run", 3),
    ("attack_duration", "Get Ice Wand to level 3 in one run", "upgrade_level", 3, "unlock_ice_wand", "run", 4),
    ("attack_size", "Get Bombs to level 3 in one run", "upgrade_level", 3, "unlock_bomb", "run", 4),
    ("critical_chance", "Defeat 100 enemies yourself", "enemies_defeated", 100, "player", "lifetime", 3),
    ("attack_knockback", "Defeat 25 heavy enemies", "enemy_type_defeated", 25, "heavy", "lifetime", 3),
    ("experience_gain", "Reach player level 10 in one run", "player_level", 10, "", "run", 4),
]:
    unlock("tome." + id, text, "upgrades", metric, target, subject, scope, price)

for id, text, metric, target, subject, scope, price, silver, extra in [
    ("abacus", "Fully harvest 75 resource nodes", "nodes_depleted", 75, "", "lifetime", 3, 2, []),
    ("weapon_token", "Get Club to level 5 in one run", "upgrade_level", 5, "unlock_club", "run", 4, 2, []),
    ("idle_spring", "Get Gun Turret to level 5 in one run", "upgrade_level", 5, "unlock_gun_turret", "run", 4, 2, []),
    ("repair_magnet", "Repair 10 damaged structures in one run", "structures_repaired", 10, "", "run", 4, 2, []),
    ("siren", "Survive night 3 without losing a structure", "waves_completed", 3, "", "run", 5, 2, [condition("structures_lost", 0, scope="run", comparison="at_most")]),
    ("clover", "Open 30 chests", "chests_opened", 30, "", "lifetime", 4, 2, []),
    ("mining_hammer", "Fully mine 50 stone deposits", "nodes_depleted", 50, "stone", "lifetime", 6, 3, []),
    ("dawn_coin", "Survive 5 nights and bank 50 crystals at dawn", "waves_completed", 5, "", "run", 6, 3, [condition("dawn_reserve", 50, "crystal", "run")]),
    ("ghost_candle", "Lose 5 defenses in combat", "structures_lost", 5, "defense", "lifetime", 6, 3, []),
    ("core_fuse", "Survive night 8 without dying", "waves_completed", 8, "", "run", 10, 4, [condition("player_deaths", 0, scope="run", comparison="at_most")]),
    ("crystal_safe", "Survive a night with 75 crystals in reserve", "dawn_reserve", 75, "crystal", "run", 8, 4, []),
    ("rebuild_blueprint", "Survive night 6 as Mechanic without losing a structure", "waves_completed", 6, "", "run", 8, 4, [condition("structures_lost", 0, scope="run", comparison="at_most"), condition("waves_completed", 6, scope="run", player="engineer")]),
    ("lucky_die", "Get Fortune Tome to level 8 in one run", "upgrade_level", 8, "luck", "run", 10, 4, []),
    ("upgrade_mirror", "Reach player level 25 without opening a chest", "player_level", 25, "", "run", 10, 4, [condition("chests_opened", 0, scope="run", comparison="at_most")]),
]:
    unlock("item." + id, text, "items", metric, target, subject, scope, price, silver, extra)

# Utility progression; no empty cosmetic/fake rewards.
for id, text, target, price in [
    ("refresh", "Complete 20 quests", 20, 0),
    ("weapon_slots", "Complete 25 quests", 25, 0),
    ("tome_slots", "Complete 35 quests", 35, 0),
    ("building_slots", "Complete 25 quests", 25, 6),
]:
    unlock("meta." + id, text, "general", "quests_completed", target, price=price)
unlock("meta.skip", "Complete 30 quests", "general", "quests_completed", 30, price=0)
unlock("meta.banish", "Complete 40 quests", "general", "quests_completed", 40, price=0)
unlock("meta.toggler", "Purchase 8 permanent unlocks", "general", "unlocks_purchased", 8, price=0)
for id, text, metric, target, scope in [
    ("luck", "Open 45 chests", "chests_opened", 45, "lifetime"),
    ("defense_damage", "Defeat 250 enemies in one run", "enemies_defeated", 250, "run"),
    ("defense_fire_rate", "Get Rapid Tome to level 5 in one run", "upgrade_level", 5, "run"),
    ("building_health", "Repair 75 damaged structures", "structures_repaired", 75, "lifetime"),
    ("production_speed", "Gather 5000 resources", "resources_gathered", 5000, "lifetime"),
    ("resource_yield", "Fully harvest 100 resource nodes", "nodes_depleted", 100, "lifetime"),
    ("silver_gain", "Survive night 5", "waves_completed", 5, "run"),
]:
    unlock("meta." + id, text, "general", metric, target,
           "attack_speed" if id == "defense_fire_rate" else "", scope, 4)

# Substantial, staggered milestones; no rewards for the first tutorial action.
# Stable ID suffixes intentionally remain independent of rebalanced thresholds.
for metric, label, thresholds in [
    ("nodes_depleted", "Fully harvest {} resource nodes", [(1, 20), (10, 60), (50, 150), (200, 400)]),
    ("enemies_defeated", "Defeat {} enemies", [(10, 75), (100, 200), (500, 750), (2000, 2500)]),
    ("buildings_placed", "Place {} structures", [(1, 15), (10, 50), (50, 150)]),
    ("waves_completed", "Survive {} nights in total", [(1, 3), (5, 10), (20, 30)]),
    ("chests_opened", "Open {} chests", [(1, 8), (25, 25), (50, 60)]),
    ("unlocks_purchased", "Purchase {} permanent unlocks", [(5, 3), (15, 7), (30, 12)]),
    ("challenges_completed", "Complete {} portal challenges", [(1, 3), (5, 8), (15, 20)]),
]:
    for i, (stable_id, target) in enumerate(thresholds):
        quest("milestone_" + metric + "_" + str(stable_id), label.format(target),
              "challenges" if metric == "challenges_completed" else "general",
              [condition(metric, target)], silver=2 + i)
for id, text, metric, target, constraints in [
    ("clean_night", "Survive 3 nights without taking damage", "waves_completed", 3, [condition("damage_taken", 0, scope="run", comparison="at_most")]),
    ("sealed_chests", "Survive 5 nights without opening chests", "waves_completed", 5, [condition("chests_opened", 0, scope="run", comparison="at_most")]),
    ("perfect_core", "Survive 6 nights without damage to the Core", "waves_completed", 6, [condition("core_damage_taken", 0, scope="run", comparison="at_most")]),
    ("builder", "Survive 5 nights with 15 structures placed", "waves_completed", 5, [condition("buildings_placed", 15, scope="run")]),
    ("mastery", "Get Might and Rapid Tomes to level 5 in one run", "upgrade_level", 5, [condition("upgrade_level", 5, "attack_speed", "run")]),
    ("stage_no_chests", "Clear the stage without opening chests", "stage_clears", 1, [condition("chests_opened", 0, scope="run", comparison="at_most")]),
]:
    quest("challenge_" + id, text, "challenges", [condition(metric, target, "damage" if id == "mastery" else "", "run"), *constraints], silver=5)

# Shop gates make ranks available; the rank itself is purchased in the Shop.
# Do not charge a discovery fee and then charge the player again for the rank.
# All utility gates are free; uses are purchased as ranks in the Shop.
for q in quests:
    if q["reward"]["unlockId"].startswith("meta."):
        q["reward"]["purchaseCost"] = 0

def notification(q):
    c = q["conditions"][0]
    n, m = c["target"], c["metric"]
    labels = {
        "nodes_depleted": f"{n} nodes harvested", "enemies_defeated": f"{n} enemies defeated",
        "enemy_type_defeated": f"{n} {c['subject']} enemies defeated",
        "buildings_placed": f"{n} structures built", "waves_completed": f"{n} nights survived",
        "wave_reached": f"Night {n} reached", "chests_opened": f"{n} chests opened",
        "unlocks_purchased": f"{n} unlocks purchased", "challenges_completed": f"{n} challenges cleared",
        "structures_repaired": f"{n} repairs completed", "producers_built": f"{n} producers built",
        "shrines_activated": f"{n} shrines activated", "defense_shots": f"{n} defense shots",
        "items_collected": f"{n} items collected", "player_level": f"Player level {n}",
        "structures_lost": f"{n} defenses lost", "dawn_reserve": f"{n} crystals at dawn",
        "quests_completed": f"{n} quests completed", "resources_gathered": f"{n} resources gathered",
        "stage_clears": "Stage cleared",
    }
    if m == "upgrade_level":
        names = {"unlock_cannon": "Cannon", "unlock_turret": "Crossbow", "unlock_club": "Club",
                 "unlock_gun_turret": "Gun Turret", "unlock_bomb": "Bombs", "unlock_ice_wand": "Ice Wand",
                 "damage": "Might Tome", "attack_speed": "Rapid Tome", "critical_chance": "Precision Tome", "luck": "Fortune Tome"}
        return f"{names[c['subject']]} level {n}"
    return labels[m]

for q in quests:
    q["notification"] = notification(q)

root = dict(version=1, progressionRevision=6, quests=quests)
write_artifact(ROOT / "assets/data/meta_quests.json", json.dumps(root, indent=2) + "\n")

# Compile the very same catalog into the fallback: no silent regression to old quests.
def enum(value):
    return "".join(word.capitalize() for word in value.split("_"))


def string(value):
    return json.dumps(value, ensure_ascii=True)


lines = ["// Generated by tools/generate_meta_quests.py; edit the authoring script.", "return {"]
for q in quests:
    conditions = ", ".join("{QuestMetric::%s, QuestScope::%s, %.1f, QuestComparison::%s, %s, %s}" %
                           (enum(c["metric"]), enum(c["scope"]), c["target"], enum(c["comparison"]), string(c["subject"]), string(c["playerClass"])) for c in q["conditions"])
    reward = q["reward"]
    lines.append("    {%s, %s, %s, QuestCategory::%s, {%s}, {%s}, {%d, %s, %d}, %s}," %
                 (string(q["id"]), string(q["title"]), string(q["description"]), enum(q["category"]), conditions,
                  ", ".join(map(string, q["prerequisites"])), reward["silver"], string(reward["unlockId"]), reward["purchaseCost"], string(q["notification"])))
lines.append("};")
write_artifact(ROOT / "src/progression/QuestCatalogData.inc", "\n".join(lines) + "\n")
print(f"Authored {len(quests)} quests, {sum(q['reward']['purchaseCost'] > 0 for q in quests)} purchasable discoveries and 8 shop gates")

# Localized objectives are derived from their actual AND conditions. A changed
# target cannot silently retain a stale translated number or run restriction.
localization_path = ROOT / "assets/data/localization.json"
localization = json.loads(localization_path.read_text())
ru = localization["languages"]["ru"]
subjects = {
    "turret": "арбалетов", "gun_turret": "пулемётных турелей", "cannon": "пушек", "player": "лично",
    "fast": "быстрых врагов", "heavy": "тяжёлых врагов", "flying": "летающих врагов",
    "ranged": "врагов дальнего боя", "wall": "стен", "stone": "каменных залежей",
    "wood": "деревьев", "crystal": "кристаллов", "defense": "оборонительных построек",
    "unlock_gun_turret": "Пулемётную турель", "unlock_turret": "Арбалет",
    "unlock_cannon": "Пушку", "unlock_club": "Дубину", "unlock_bomb": "Бомбы",
    "unlock_ice_wand": "Ледяной жезл", "damage": "Том могущества",
    "attack_speed": "Том скорострельности", "critical_chance": "Том точности",
    "luck": "Том удачи",
}


def count_word(n, one, few, many):
    if 11 <= n % 100 <= 14: return many
    if n % 10 == 1: return one
    return few if 2 <= n % 10 <= 4 else many


def russian_condition(c):
    metric, target, subject = c["metric"], c["target"], c["subject"]
    n = str(target)
    if c["comparison"] == "at_most":
        return {"structures_lost": "без потери построек", "player_deaths": "без смертей",
                "chests_opened": "не открывая сундуки", "damage_taken": "не получая урон",
                "core_damage_taken": "без урона по Ядру"}[metric]
    if metric == "enemies_defeated":
        text = f"Победите {n} врагов"
        if subject == "player": text += " лично"
        elif subject: text += " с помощью " + subjects[subject]
    elif metric == "enemy_type_defeated": text = f"Победите {n} " + subjects[subject]
    elif metric == "upgrade_level": text = f"Улучшите {subjects[subject]} до уровня {n}"
    elif metric == "nodes_depleted": text = f"Полностью добудьте {n} " + (subjects[subject] if subject else count_word(target, "ресурсный узел", "ресурсных узла", "ресурсных узлов"))
    elif metric == "buildings_placed": text = f"Разместите {n} " + (subjects[subject] if subject else count_word(target, "постройку", "постройки", "построек"))
    elif metric == "waves_completed": text = f"Переживите {n} " + count_word(target, "ночь", "ночи", "ночей")
    elif metric == "wave_reached": text = f"Дойдите до ночи {n}"
    elif metric == "structures_repaired": text = f"Отремонтируйте повреждённые постройки {n} раз"
    elif metric == "producers_built": text = f"Постройте {n} добывающих здания"
    elif metric == "shrines_activated": text = f"Активируйте {n} " + count_word(target, "шрайн", "шрайна", "шрайнов")
    elif metric == "defense_shots": text = f"Сделайте {n} выстрелов постройками"
    elif metric == "chests_opened": text = f"Откройте {n} " + count_word(target, "сундук", "сундука", "сундуков")
    elif metric == "items_collected": text = f"Подберите {n} предметов из сундуков"
    elif metric == "player_level": text = f"Достигните уровня игрока {n}"
    elif metric == "structures_lost": text = f"Потеряйте {n} оборонительных построек в бою"
    elif metric == "dawn_reserve": text = f"Встретьте рассвет с запасом {n} кристаллов"
    elif metric == "quests_completed": text = f"Выполните {n} заданий"
    elif metric == "resources_gathered": text = f"Соберите {n} ресурсов"
    elif metric == "unlocks_purchased": text = f"Купите {n} " + count_word(target, "постоянное открытие", "постоянных открытия", "постоянных открытий")
    elif metric == "challenges_completed": text = f"Пройдите {n} " + count_word(target, "испытание", "испытания", "испытаний") + " портала"
    elif metric == "stage_clears": text = "Пройдите этап"
    else: raise ValueError(c)
    if c["playerClass"] == "engineer": text += " за Механика"
    return text


def russian_notification(q):
    c = q["conditions"][0]
    n, m = c["target"], c["metric"]
    labels = {
        "nodes_depleted": f"Ресурсов добыто: {n}", "enemies_defeated": f"Врагов побеждено: {n}",
        "enemy_type_defeated": {"fast": "Быстрых врагов", "heavy": "Тяжёлых врагов",
                                "flying": "Летающих врагов", "ranged": "Стрелков"}.get(c["subject"], "Врагов") + f": {n}",
        "buildings_placed": f"Построек возведено: {n}", "waves_completed": f"Ночей пережито: {n}",
        "wave_reached": f"Достигнута ночь {n}", "chests_opened": f"Сундуков открыто: {n}",
        "unlocks_purchased": f"Открытий куплено: {n}", "challenges_completed": f"Испытаний пройдено: {n}",
        "structures_repaired": f"Ремонтов выполнено: {n}", "producers_built": f"Добывающих зданий: {n}",
        "shrines_activated": f"Шрайнов активировано: {n}", "defense_shots": f"Выстрелов башен: {n}",
        "items_collected": f"Предметов собрано: {n}", "player_level": f"Уровень игрока: {n}",
        "structures_lost": f"Потеряно защитных башен: {n}", "dawn_reserve": f"{n} кристаллов на рассвете",
        "quests_completed": f"Заданий выполнено: {n}", "resources_gathered": f"Ресурсов собрано: {n}",
        "stage_clears": "Этап пройден",
    }
    if m == "upgrade_level":
        names = {"unlock_cannon": "Пушка", "unlock_turret": "Арбалет", "unlock_club": "Дубина",
                 "unlock_gun_turret": "Турель", "unlock_bomb": "Бомбы", "unlock_ice_wand": "Ледяной жезл",
                 "damage": "Том могущества", "attack_speed": "Том скорострельности",
                 "critical_chance": "Том точности", "luck": "Том удачи"}
        return f"{names[c['subject']]}: ур. {n}"
    return labels[m]

for q in quests:
    description = "; ".join(russian_condition(c) for c in q["conditions"])
    if any(c["scope"] == "run" for c in q["conditions"]): description += " за один забег"
    ru[q["title"]] = description
    ru[q["notification"]] = russian_notification(q)
ru.update({
    "REACH NIGHT 6, THEN BUY IN UNLOCKS": "ДОЙДИТЕ ДО НОЧИ 6, ЗАТЕМ КУПИТЕ В ОТКРЫТИЯХ",
    "DEFEAT 250 ENEMIES, THEN BUY IN UNLOCKS": "ПОБЕДИТЕ 250 ВРАГОВ, ЗАТЕМ КУПИТЕ В ОТКРЫТИЯХ",
    "COLLECT 20 ITEMS, THEN BUY IN UNLOCKS": "СОБЕРИТЕ 20 ПРЕДМЕТОВ, ЗАТЕМ КУПИТЕ В ОТКРЫТИЯХ",
    "CLEAR THE STAGE, THEN BUY IN UNLOCKS": "ПРОЙДИТЕ ЭТАП, ЗАТЕМ КУПИТЕ В ОТКРЫТИЯХ",
    "OPEN SHOP": "МАГАЗИН",
    "Purchase ranks in the Shop. Applies from your next run.": "Покупайте уровни в Магазине. Бонус действует со следующего забега.",
    "Start with 3 tome slots. Each rank adds one slot, up to 5.": "В начале 3 слота томов. Каждый ранг добавляет слот, максимум 5.",
    "3 defense types. Each rank adds one slot, up to 5.": "В начале 3 слота обороны. Каждый ранг добавляет слот, максимум 5.",
    "TOMES": "ТОМА", "AVAILABLE TO BUY": "ДОСТУПНО ДЛЯ ПОКУПКИ",
    "QUEST COMPLETE — AVAILABLE TO BUY": "КВЕСТ ВЫПОЛНЕН — МОЖНО КУПИТЬ",
    "Unlocks permanent upgrades in the Shop.": "Открывает постоянные улучшения в Магазине.",
    "Complete its quest to unlock this upgrade.": "Выполните квест, чтобы открыть это улучшение.",
    "Available in future runs. Find its blueprint during a run.": "Доступно в новых забегах. Получите чертёж во время забега.",
    "Available in future runs and character loadouts.": "Доступно в новых забегах и снаряжении классов.",
})
write_artifact(localization_path, json.dumps(localization, ensure_ascii=False, indent=2) + "\n")
