"""AI 원화 유닛 애니메이션: 승인된 원화(generate_art.art_source: 바탕 지우고 자른 그림)를 조각으로 나눠
축 둘레로 돌리고 옮겨 동작 프레임을 만든다. 좌표는 잘라 둔 원화 기준 픽셀.
  frame(이름, 동작, 위상 0~1, 칸 크기) → RGBA 칸
동작: idle(숨쉬기) walk(걷기) attack(휘두르기) hit(맞음) dazed(휘청) corpse(쓰러짐)"""
import math

from PIL import Image, ImageDraw

RIGS = {
    # 영웅급: 왕관 투구·깃털, 치켜든 금빛 대검, 금테 망토, 두 다리
    "Hero": dict(
        feet=(430, 900),
        parts={
            "cape": dict(poly=[(0, 830), (40, 700), (160, 600), (270, 545), (300, 560), (290, 630), (250, 640), (245, 720), (300, 760), (300, 850), (150, 850), (40, 840)], pivot=(300, 560)),
            "back_leg": dict(poly=[(225, 822), (348, 822), (348, 905), (225, 905)], pivot=(300, 822)),
            "front_leg": dict(poly=[(468, 822), (632, 822), (632, 905), (468, 905)], pivot=(520, 822)),
            "crown": dict(poly=[(150, 180), (260, 60), (400, 170), (445, 255), (535, 360), (522, 398), (262, 405), (238, 352), (150, 250)], pivot=(390, 405)),
            "sword": dict(poly=[(545, 560), (600, 520), (648, 452), (618, 410), (758, 0), (867, 0), (867, 62), (762, 462), (722, 522), (692, 562), (622, 592), (560, 602)], pivot=(555, 560)),
        },
        windup=-32, strike=100,
    ),
    # 베테랑급: 냄비 투구·깃털, 막대 팔 끝의 칼, 빨간 망토, 막대 다리
    "Veteran": dict(
        feet=(175, 498),
        parts={
            "cape": dict(poly=[(0, 385), (40, 300), (90, 235), (110, 240), (100, 300), (95, 380), (70, 405), (20, 395)], pivot=(100, 240)),
            "back_leg": dict(poly=[(85, 415), (150, 415), (150, 502), (85, 502)], pivot=(125, 415)),
            "front_leg": dict(poly=[(195, 415), (262, 415), (262, 502), (195, 502)], pivot=(215, 415)),
            "crown": dict(poly=[(80, 0), (160, 0), (160, 70), (240, 90), (266, 130), (252, 162), (80, 166), (68, 150), (80, 110)], pivot=(170, 162)),
            "sword": dict(poly=[(240, 290), (300, 320), (330, 300), (440, 190), (470, 165), (477, 175), (477, 200), (360, 340), (350, 355), (320, 372), (295, 374), (288, 350), (240, 322)], pivot=(245, 300)),
        },
        windup=-70, strike=55,
    ),
    # 금발 해골 보스급: 머리(해골·금발·왕관)·망토·대검. 다리는 망토 앞이라 떼지 않고 몸째 들썩인다
    "BoneKing": dict(
        feet=(420, 930),
        parts={
            "cape": dict(poly=[(0, 880), (80, 780), (150, 620), (200, 560), (262, 600), (302, 700), (312, 900), (232, 941), (0, 941)], pivot=(250, 560)),
            "crown": dict(poly=[(0, 330), (60, 250), (200, 160), (210, 0), (470, 0), (528, 160), (528, 330), (482, 372), (362, 378), (330, 352), (250, 420), (160, 582), (60, 562), (0, 470)], pivot=(400, 372)),
            "sword": dict(poly=[(500, 372), (612, 362), (628, 470), (642, 560), (602, 602), (584, 935), (508, 935), (503, 560), (518, 520), (558, 470), (558, 440)], pivot=(585, 460)),
        },
        windup=-200, strike=-40,
    ),
}
ORDER = ["cape", "back_leg", "body", "front_leg", "crown", "sword"]
FILL = 0.84        # 칸 높이 중 그림 비율 (generate_art.ART_FILL과 같게)
FOOT = 0.92        # 칸에서 발 높이 (generate_art.FOOT)
_cache = {}


def _layers(name, source):
    if name in _cache:
        return _cache[name]
    rig = RIGS[name]
    layers = {}
    body = source.copy()
    for part, spec in rig["parts"].items():
        mask = Image.new("L", source.size, 0)
        ImageDraw.Draw(mask).polygon(spec["poly"], fill=255)
        layer = Image.new("RGBA", source.size, (0, 0, 0, 0))
        layer.paste(source, (0, 0), mask)
        layers[part] = layer
        body.paste(Image.new("RGBA", source.size, (0, 0, 0, 0)), (0, 0), mask)
    layers["body"] = body
    _cache[name] = layers
    return layers


def _ease(keys, t):
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t <= t1:
            u = 0 if t1 == t0 else (t - t0) / (t1 - t0)
            u = u * u * (3 - 2 * u)
            return v0 + (v1 - v0) * u
    return keys[-1][1]


def pose(name, action, p):
    """동작·위상 → 조각 각도(도, 시계 +)와 이동(원화 픽셀), 몸 전체 기울기·이동, 몸 납작함"""
    rig = RIGS[name]
    h = rig["feet"][1]
    w = math.tau * p
    q = dict(cape=0.0, back_leg=0.0, front_leg=0.0, crown=0.0, sword=0.0, lean=0.0, dx=0.0, dy=0.0, crown_dy=0.0, squash=0.0)
    if action == "idle":
        q.update(squash=math.sin(w) * 0.025, crown_dy=math.sin(w + 0.7) * h * 0.006, crown=math.sin(w + 0.4) * 2,
                 cape=math.sin(w + 1.0) * 4, sword=math.sin(w) * 3)
    elif action == "walk":
        swing = math.sin(w) * 18
        q.update(back_leg=-swing, front_leg=swing, dy=-abs(math.sin(w)) * h * 0.025, lean=3,
                 crown=math.sin(w * 2) * 3, cape=6 + math.sin(w * 2 + 0.8) * 6, sword=math.sin(w) * 5, squash=-abs(math.sin(w)) * 0.02)
    elif action == "attack":
        q["sword"] = _ease([(0, 0), (0.32, rig["windup"]), (0.45, rig["windup"] * 1.08), (0.58, rig["strike"]), (0.74, rig["strike"] * 0.9), (1.0, 0)], p)
        q["lean"] = _ease([(0, 0), (0.32, -6), (0.45, -7), (0.58, 9), (0.8, 6), (1.0, 0)], p)
        q["dx"] = _ease([(0, 0), (0.32, -h * 0.02), (0.58, h * 0.04), (0.8, h * 0.03), (1.0, 0)], p)
        q["squash"] = _ease([(0, 0), (0.32, 0.03), (0.58, -0.04), (1.0, 0)], p)
        q["crown"] = q["lean"] * 0.8
        q["cape"] = _ease([(0, 0), (0.45, 6), (0.58, -14), (1.0, 0)], p)
        q["front_leg"] = _ease([(0, 0), (0.45, -6), (0.58, 12), (1.0, 0)], p)
        q["back_leg"] = _ease([(0, 0), (0.45, 6), (0.58, -10), (1.0, 0)], p)
    elif action == "hit":
        k = math.sin(math.pi * p)
        q.update(lean=-10 * k, dx=-h * 0.03 * k, crown=-8 * k, cape=10 * k, sword=-12 * k, squash=0.05 * k)
    elif action == "dazed":
        q.update(lean=math.sin(w) * 10, dx=math.sin(w) * h * 0.015, crown=math.sin(w + 1) * 10, cape=math.sin(w + 2) * 8, sword=math.sin(w) * 14)
    return q


def frame(name, action, p, out):
    from generate_art import art_source
    source = art_source(name)
    rig = RIGS[name]
    layers = _layers(name, source)
    q = pose(name, action, p)
    full = Image.new("RGBA", source.size, (0, 0, 0, 0))
    for part in ORDER:
        if part != "body" and part not in rig["parts"]:
            continue
        if part == "body":
            full.alpha_composite(layers["body"])
            continue
        spec = rig["parts"][part]
        shift = (0, q["crown_dy"]) if part == "crown" else (0, 0)
        full.alpha_composite(layers[part].rotate(-q[part], resample=Image.BICUBIC, center=spec["pivot"], translate=shift))
    fx, fy = rig["feet"]
    full = full.rotate(-q["lean"], resample=Image.BICUBIC, center=(fx, fy))
    # 납작해졌다 늘어났다 (발은 그대로)
    sy = 1.0 - q["squash"]
    sx = 1.0 + q["squash"] * 0.6
    scale = min(out * FILL / source.size[1], out * 0.96 / source.size[0])
    small = full.resize((max(1, round(full.size[0] * scale * sx)), max(1, round(full.size[1] * scale * sy))), Image.LANCZOS)
    cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
    ox = round(out / 2 - fx * scale * sx + q["dx"] * scale)
    oy = round(out * FOOT - fy * scale * sy + q["dy"] * scale)
    cell.paste(small, (ox, oy), small)
    if action == "corpse":
        lying = cell.rotate(90, resample=Image.BICUBIC)
        cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
        cell.alpha_composite(lying, (0, int(out * 0.18)))
    return cell
