"""특성 성좌(Perks.csv)를 만든다. PoE 패시브 트리처럼 중심에서 8갈래로 뻗는다.

  중심 별 ─ 안쪽 고리 8개 ─ 갈래마다 작은 별 줄 + 큰 별 2개 ─ 이웃 갈래와 다리
  갈래: 졸병 · 영웅 · 궁수 · 금고 · 대장간 · 스펠 · 방패 · 전군

큰 별 이름은 Localization.csv 의 Perk.<이름> 에 있다.
별을 늘리려면 아래 SPOKES 의 small / notables 를 고치고 다시 돌린다.
"""
import csv
import io
import math
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Content", "YJJWave", "Data", "Perks.csv")

INNER_R = 180
STEP_R = 120
CHAIN = 6                 # 갈래마다 작은 별·큰 별 줄 길이
NOTABLE_AT = (2, 5)       # 줄에서 큰 별 자리
BRIDGE_AT = 2             # 이웃 갈래와 다리를 놓는 줄 위치

# 갈래: (이름, 각도, 작은 별 효과 순환, 큰 별 2개)
# 효과 = (Stat, Target, Value). Target이 비면 모든 병영. 비율 값은 0.1 = 10%
SPOKES = [
    ("Barracks", 180, [("HP", "Barracks", 0.10), ("Damage", "Barracks", 0.10), ("SpawnRate", "Barracks", 0.08)],
     [("PotHelmet", [("HP", "Barracks", 0.40), ("Damage", "Barracks", 0.20)]),
      ("EndlessGrunts", [("SpawnCount", "Barracks", 2)])]),
    ("Hero", 225, [("HeroHP", "", 0.15)],
     [("StoneWall", [("HeroHP", "", 0.60)]),
      ("Fortress", [("HeroHP", "", 1.00), ("HP", "", 0.05)])]),
    ("Archer", 270, [("Damage", "ArcherPost", 0.10), ("AttackSpeed", "ArcherPost", 0.08)],
     [("DoubleShot", [("Multishot", "ArcherPost", 1)]),
      ("ArrowRain", [("Multishot", "ArcherPost", 1), ("SpawnRate", "ArcherPost", 0.15)])]),
    ("Treasury", 315, [("Gold", "", 0.08), ("Souls", "", 0.06)],
     [("GoldRush", [("Gold", "", 0.50)]),
      ("SoulHarvest", [("Souls", "", 0.40), ("Gold", "", 0.20)])]),
    ("Forge", 0, [("Splash", "Forge", 0.15), ("Damage", "Forge", 0.10)],
     [("BigHammer", [("Splash", "Forge", 0.40), ("Damage", "Forge", 0.20)]),
      ("Smithy", [("SpawnCount", "Forge", 1), ("SpawnRate", "Forge", 0.20)])]),
    ("Spells", 45, [("SpellPower", "", 0.10), ("SpellCooldown", "", 0.05)],
     [("Meteorology", [("SpellPower", "", 0.50)]),
      ("QuickCast", [("SpellCooldown", "", 0.25)])]),
    ("Shield", 90, [("HP", "ShieldHall", 0.15)],
     [("IronWall", [("HP", "ShieldHall", 0.60)]),
      ("ShieldBash", [("Damage", "ShieldHall", 0.50), ("SpawnCount", "ShieldHall", 1)])]),
    ("Army", 135, [("HP", "", 0.05), ("Damage", "", 0.05), ("MoveSpeed", "", 0.05), ("AttackSpeed", "", 0.05)],
     [("Charge", [("MoveSpeed", "", 0.15), ("AttackSpeed", "", 0.15)]),
      ("HordeTactics", [("SpawnRate", "", 0.25)])]),
]
INNER = [("HP", "", 0.04), ("Damage", "", 0.04)]


class Perk:
    def __init__(self, name, kind, x, y, mods, name_key=""):
        self.name, self.kind, self.x, self.y, self.mods, self.name_key = name, kind, x, y, mods, name_key
        self.links = set()


def link(a, b):
    a.links.add(b.name)
    b.links.add(a.name)


def polar(r, deg):
    a = math.radians(deg)
    return round(r * math.cos(a), 1), round(r * math.sin(a), 1)


def main():
    perks = []
    start = Perk("Start", "Start", 0, 0, [])
    perks.append(start)

    inner = []
    for i, (_, deg, _, _) in enumerate(SPOKES):
        p = Perk("Inner%d" % i, "Small", *polar(INNER_R, deg), [INNER[i % len(INNER)]])
        perks.append(p)
        inner.append(p)
        link(p, start)
    for i in range(len(inner)):
        link(inner[i], inner[(i + 1) % len(inner)])

    chains = []
    for i, (spoke, deg, small, notables) in enumerate(SPOKES):
        prev = inner[i]
        chain = []
        notable_index = 0
        small_index = 0
        for step in range(CHAIN):
            r = INNER_R + STEP_R * (step + 1)
            # 줄이 살짝 휘어 별자리처럼 보이게
            bend = 6 * math.sin(step * 1.3 + i)
            if step in NOTABLE_AT:
                key, mods = notables[notable_index]
                notable_index += 1
                p = Perk("%s_%s" % (spoke, key), "Notable", *polar(r, deg + bend), mods, "Perk." + key)
            else:
                p = Perk("%s%d" % (spoke, step), "Small", *polar(r, deg + bend), [small[small_index % len(small)]])
                small_index += 1
            perks.append(p)
            link(p, prev)
            prev = p
            chain.append(p)
        chains.append(chain)

    # 이웃 갈래 사이 다리 (가운데 작은 별 하나)
    for i in range(len(chains)):
        a = chains[i][BRIDGE_AT - 1]
        b = chains[(i + 1) % len(chains)][BRIDGE_AT - 1]
        deg = SPOKES[i][1] + 22.5
        r = INNER_R + STEP_R * BRIDGE_AT
        p = Perk("Bridge%d" % i, "Small", *polar(r, deg), [INNER[(i + 1) % len(INNER)]])
        perks.append(p)
        link(p, a)
        link(p, b)

    out = io.StringIO()
    w = csv.writer(out, lineterminator="\n")
    w.writerow(["Name", "Kind", "X", "Y", "Stat", "Target", "Value", "Stat2", "Target2", "Value2", "Links", "NameKey"])
    for p in perks:
        m1 = p.mods[0] if len(p.mods) > 0 else ("", "", 0)
        m2 = p.mods[1] if len(p.mods) > 1 else ("", "", 0)
        w.writerow([p.name, p.kind, p.x, p.y, m1[0], m1[1], m1[2], m2[0], m2[1], m2[2], " ".join(sorted(p.links)), p.name_key])
    with open(OUT, "w", encoding="utf-8-sig", newline="") as f:
        f.write(out.getvalue())
    print("perks", len(perks), "notables", sum(1 for p in perks if p.kind == "Notable"))


main()
