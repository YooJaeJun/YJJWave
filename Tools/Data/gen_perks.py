"""특성 성좌(Perks.csv)를 만든다. PoE 패시브 트리처럼 중심에서 8갈래로 뻗는다.

  중심 별 ─ 안쪽 고리 8개 ─ 갈래마다 작은 별 줄 + 중간 별 2개 (+ 몇 갈래 끝에 쐐기) ─ 이웃 갈래와 다리
  갈래: 졸병 · 영웅 · 궁수 · 금고 · 대장간 · 스펠 · 방패 · 전군

  작은 별(Small)   : 능력치 숫자만 조금씩 (체력 +10% …)
  중간 별(Notable) : 특수 효과 (Special 열). 예) 졸병 체력이 30% 아래로 떨어지면 1회 50% 회복
  쐐기(Keystone)   : 대가가 있는 큰 규칙 변경 (능력치 +와 - 를 함께)
  주문 별(Spell)   : 스펠 갈래 옆 가지. 여러 번 찍어 주문 Lv을 올린다 (수치·특수 효과는 Spells.csv)

이름(큰 별·쐐기)은 Localization.csv 의 Perk.<이름>, 효과 설명은 Special.<종류>.
특수 효과 종류와 A·B 값의 뜻은 Source/YJJWave/Sim/YJJWaveTypes.h 의 EYJJSpecial.
"""
import csv
import io
import math
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Content", "YJJWave", "Data", "Perks.csv")

INNER_R = 180
STEP_R = 120
CHAIN = 6                 # 갈래마다 작은 별·중간 별 줄 길이
NOTABLE_AT = (2, 5)       # 줄에서 중간 별 자리
BRIDGE_AT = 2             # 이웃 갈래와 다리를 놓는 줄 위치

# 갈래: (이름, 각도, 작은 별 능력치 순환, 중간 별 2개, 쐐기 또는 None)
#   능력치 = (Stat, Target, Value). Target이 비면 모든 병영. 비율 값은 0.1 = 10%
#   중간 별 = (이름, Special, SpecialTarget, A, B). SpecialTarget: 병영 이름 / Hero / 비우면 모든 아군
#   쐐기   = (이름, [능력치…]) 최대 2개
SPOKES = [
    ("Barracks", 180, [("HP", "Barracks", 0.10), ("Damage", "Barracks", 0.10), ("SpawnRate", "Barracks", 0.08)],
     [("FirstAid", "LastStand", "Barracks", 0.3, 0.5),
      ("StubbornGrunts", "Revive", "Barracks", 0.5, 0)],
     ("GruntParadise", [("SpawnCount", "Barracks", 4), ("Damage", "Barracks", -0.35)])),
    ("Hero", 225, [("HeroHP", "", 0.15)],
     [("HeroBreath", "HeroRegen", "Hero", 40, 0),
      ("FuriousCaptain", "Fury", "Hero", 0.5, 1.0)],
     ("LoneHero", [("HeroHP", "", 2.0), ("SpawnRate", "", -0.25)])),
    ("Archer", 270, [("Damage", "ArcherPost", 0.10), ("AttackSpeed", "ArcherPost", 0.08)],
     [("BurstArrows", "ArrowBurst", "ArcherPost", 30, 0),
      ("DoubleShot", "ExtraShot", "ArcherPost", 1, 0)],
     None),
    ("Treasury", 315, [("Gold", "", 0.08), ("Souls", "", 0.06)],
     [("GoldenSkull", "GoldBurst", "", 0.05, 10),
      ("SoulBurst", "SoulBurst", "", 0.05, 10)],
     None),
    ("Forge", 0, [("Splash", "Forge", 0.15), ("Damage", "Forge", 0.10)],
     [("Shockwave", "Shockwave", "Forge", 2.5, 0),
      ("SmithGrit", "LastStand", "Forge", 0.3, 0.6)],
     None),
    ("Spells", 45, [("SpellPower", "", 0.10), ("SpellCooldown", "", 0.05)],
     [("MeteorChain", "MeteorTwice", "", 0.35, 0),
      ("HealingCry", "RallyHeal", "", 0.3, 0)],
     None),
    ("Shield", 90, [("HP", "ShieldHall", 0.15)],
     [("ThornShield", "Thorns", "ShieldHall", 0.5, 0),
      ("IronWill", "LastStand", "ShieldHall", 0.4, 0.8)],
     None),
    ("Army", 135, [("HP", "", 0.05), ("Damage", "", 0.05), ("MoveSpeed", "", 0.05), ("AttackSpeed", "", 0.05)],
     [("BloodArmy", "Lifesteal", "", 0.1, 0),
      ("Frenzy", "Fury", "", 0.3, 0.5)],
     ("HumanWave", [("HP", "", -0.4), ("SpawnCount", "", 2)])),
]
INNER = [("HP", "", 0.04), ("Damage", "", 0.04)]

# 주문 별: (주문, 스펠 갈래 줄 몇 번째에 붙는지, 반지름 칸, 각도 차이)
SPELL_SPOKE = "Spells"
SPELL_NODES = [("Meteor", 1, 3, -20), ("Rally", 1, 3, 20), ("Ladle", 3, 5, -14)]
MIN_GAP = 110   # 별 사이 최소 거리 (성좌 좌표)

# 중간 별 효과 그림: UI.png 칸 (Tools/Art/generate_art.py ui_icon 18~32). 주문 별은 HUD가 주문 그림을 쓴다
NOTABLE_ICONS = {
    "FirstAid": 18, "StubbornGrunts": 19, "HeroBreath": 20, "FuriousCaptain": 21, "BurstArrows": 22, "DoubleShot": 23,
    "GoldenSkull": 24, "SoulBurst": 25, "Shockwave": 26, "SmithGrit": 27, "MeteorChain": 28, "HealingCry": 29,
    "ThornShield": 30, "IronWill": 31, "BloodArmy": 32, "Frenzy": 21,
}


class Perk:
    def __init__(self, name, kind, x, y, mods=(), name_key="", special=("", "", 0, 0), spell=""):
        self.name, self.kind, self.x, self.y = name, kind, x, y
        self.mods, self.name_key, self.special, self.spell = list(mods), name_key, special, spell
        self.links = set()
        self.icon = NOTABLE_ICONS[name_key[5:]] if kind == "Notable" else -1


def link(a, b):
    a.links.add(b.name)
    b.links.add(a.name)


def polar(r, deg):
    a = math.radians(deg)
    return round(r * math.cos(a), 1), round(r * math.sin(a), 1)


def main():
    perks = []
    start = Perk("Start", "Start", 0, 0)
    perks.append(start)

    inner = []
    for i, spoke in enumerate(SPOKES):
        p = Perk("Inner%d" % i, "Small", *polar(INNER_R, spoke[1]), [INNER[i % len(INNER)]])
        perks.append(p)
        inner.append(p)
        link(p, start)
    for i in range(len(inner)):
        link(inner[i], inner[(i + 1) % len(inner)])

    chains = []
    for i, (spoke, deg, small, notables, keystone) in enumerate(SPOKES):
        prev = inner[i]
        chain = []
        notable_index = 0
        small_index = 0
        for step in range(CHAIN):
            r = INNER_R + STEP_R * (step + 1)
            # 줄이 살짝 휘어 별자리처럼 보이게
            bend = 6 * math.sin(step * 1.3 + i)
            if step in NOTABLE_AT:
                key, kind, target, a, b = notables[notable_index]
                notable_index += 1
                p = Perk("%s_%s" % (spoke, key), "Notable", *polar(r, deg + bend), name_key="Perk." + key, special=(kind, target, a, b))
            else:
                p = Perk("%s%d" % (spoke, step), "Small", *polar(r, deg + bend), [small[small_index % len(small)]])
                small_index += 1
            perks.append(p)
            link(p, prev)
            prev = p
            chain.append(p)
        if keystone:
            key, mods = keystone
            p = Perk("%s_%s" % (spoke, key), "Keystone", *polar(INNER_R + STEP_R * (CHAIN + 1), deg), mods, "Perk." + key)
            perks.append(p)
            link(p, prev)
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

    # 주문 별: 스펠 갈래 옆 가지
    spoke_index = next(i for i, s in enumerate(SPOKES) if s[0] == SPELL_SPOKE)
    for spell, step, ring, offset in SPELL_NODES:
        p = Perk("Spell_%s" % spell, "Spell", *polar(INNER_R + STEP_R * ring, SPOKES[spoke_index][1] + offset),
                 name_key="Spell." + spell, spell=spell)
        perks.append(p)
        link(p, chains[spoke_index][step])

    for i, a in enumerate(perks):
        for b in perks[i + 1:]:
            d = math.hypot(a.x - b.x, a.y - b.y)
            assert d >= MIN_GAP or "Spell" not in (a.kind, b.kind), (a.name, b.name, d)

    out = io.StringIO()
    w = csv.writer(out, lineterminator="\n")
    w.writerow(["Name", "Kind", "X", "Y", "Stat", "Target", "Value", "Stat2", "Target2", "Value2",
                "Special", "SpecialTarget", "SpecialA", "SpecialB", "Links", "NameKey", "Spell", "Icon"])
    for p in perks:
        m1 = p.mods[0] if len(p.mods) > 0 else ("", "", 0)
        m2 = p.mods[1] if len(p.mods) > 1 else ("", "", 0)
        w.writerow([p.name, p.kind, p.x, p.y, m1[0], m1[1], m1[2], m2[0], m2[1], m2[2],
                    p.special[0], p.special[1], p.special[2], p.special[3], " ".join(sorted(p.links)), p.name_key, p.spell, p.icon])
    with open(OUT, "w", encoding="utf-8-sig", newline="") as f:
        f.write(out.getvalue())
    print("perks", len(perks), "notables", sum(1 for p in perks if p.kind == "Notable"), "keystones", sum(1 for p in perks if p.kind == "Keystone"))


main()
