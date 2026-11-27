"""AI 원화 유닛 애니메이션: 승인된 원화(generate_art.art_source: 바탕 지우고 자른 그림)를 조각으로 나눠
축 둘레로 돌리고 옮겨 동작 프레임을 만든다. 좌표는 잘라 둔 원화 기준 픽셀.
  frame(이름, 동작, 위상 0~1, 칸 크기) → RGBA 칸
동작: idle(숨쉬기) walk(걷기) attack(휘두르기) hit(맞음) dazed(휘청) corpse(쓰러짐)"""
import math

from PIL import Image, ImageChops, ImageDraw, ImageMath

RIGS = {
    # 영웅급: 왕관 투구·깃털, 치켜든 금빛 대검, 금테 망토, 두 다리
    "Hero": dict(
        feet=(430, 900),
        parts={
            "cape": dict(poly=[(-40, 880), (0, 690), (150, 580), (268, 525), (308, 550), (296, 630), (256, 640), (250, 720), (304, 760), (304, 880)], pivot=(300, 560), blur=26),
            "back_leg": dict(poly=[(225, 822), (348, 822), (348, 905), (225, 905)], pivot=(300, 822)),
            "front_leg": dict(poly=[(468, 822), (632, 822), (632, 905), (468, 905)], pivot=(520, 822)),
            "crown": dict(poly=[(150, 180), (260, 60), (400, 170), (445, 255), (535, 360), (572, 430), (572, 548), (292, 548), (248, 470), (222, 352), (150, 250)], pivot=(420, 548), blur=30),
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
            "sword": dict(poly=[(236, 285), (300, 312), (325, 290), (430, 175), (465, 148), (492, 158), (492, 208), (376, 346), (362, 368), (326, 386), (290, 386), (282, 356), (236, 328)], pivot=(245, 300)),
        },
        windup=-70, strike=55,
    ),
    # 졸병: 냄비 투구, 국자 든 팔(뒤), 냄비뚜껑 방패 든 팔(앞, 망토 자리로 흔든다), 짧은 다리
    "Grunt": dict(
        feet=(345, 598),
        parts={
            "cape": dict(poly=[(500, 300), (685, 290), (685, 510), (500, 510), (490, 420)], pivot=(505, 405), blur=12),
            "back_leg": dict(poly=[(235, 535), (312, 535), (312, 606), (235, 606)], pivot=(275, 540)),
            "front_leg": dict(poly=[(392, 535), (462, 535), (462, 606), (392, 606)], pivot=(425, 540)),
            "crown": dict(poly=[(140, 60), (250, 0), (420, 20), (505, 95), (500, 145), (400, 160), (300, 200), (215, 262), (165, 268), (140, 200)], pivot=(330, 175), blur=20),
            "sword": dict(poly=[(0, 35), (45, 35), (170, 245), (205, 275), (238, 345), (212, 365), (150, 345), (118, 300), (95, 250), (0, 85)], pivot=(212, 340), blur=8),
        },
        windup=-30, strike=65,
    ),
    # 해골 졸병: 큰 해골 머리, 포크 든 팔, 뒤쪽 팔(망토 자리), 두 다리
    "Skeleton": dict(
        feet=(220, 832),
        parts={
            "cape": dict(poly=[(40, 380), (135, 380), (165, 470), (165, 655), (75, 655), (35, 560)], pivot=(120, 400), blur=12),
            "back_leg": dict(poly=[(90, 640), (232, 640), (232, 850), (25, 850), (25, 795), (90, 760)], pivot=(170, 652)),
            "front_leg": dict(poly=[(238, 640), (342, 640), (395, 850), (232, 850)], pivot=(290, 652)),
            "crown": dict(poly=[(0, 0), (432, 0), (442, 250), (402, 350), (382, 440), (262, 448), (202, 402), (120, 362), (0, 300)], pivot=(232, 425), blur=24),
            "sword": dict(poly=[(280, 448), (330, 478), (420, 468), (470, 120), (631, 120), (631, 330), (532, 352), (502, 480), (502, 562), (422, 712), (358, 702), (400, 562), (320, 542), (280, 512)], pivot=(292, 482), blur=8),
        },
        windup=-25, strike=40,
    ),
    # ---- Gemini 원화 (기존 원화를 고쳐 화풍을 맞춘 것) ----
    # 도깨비: 큰 빨간 머리·뿔, 몽둥이 든 팔(앞), 뒤쪽 팔, 두 다리
    "Imp": dict(
        feet=(230, 866),
        parts={
            "cape": dict(poly=[(30, 450), (150, 450), (165, 580), (150, 710), (30, 710), (20, 580)], pivot=(130, 480), blur=12),
            "back_leg": dict(poly=[(60, 700), (215, 700), (215, 869), (60, 869)], pivot=(150, 705)),
            "front_leg": dict(poly=[(235, 700), (395, 700), (395, 869), (235, 869)], pivot=(300, 705)),
            "crown": dict(poly=[(0, 0), (450, 0), (450, 330), (420, 410), (330, 440), (160, 440), (60, 390), (0, 300)], pivot=(240, 430), blur=24),
            "sword": dict(poly=[(300, 480), (390, 470), (470, 330), (490, 150), (570, 130), (628, 150), (628, 320), (560, 430), (510, 490), (500, 610), (440, 650), (380, 650), (385, 580), (300, 580)], pivot=(295, 520), blur=8),
        },
        windup=-30, strike=70,
    ),
    # 뚱보 좀비: 머리는 몸통 위쪽, 포크 든 팔, 앞으로 뻗은 팔, 짧은 다리 (포크가 배 앞이라 휘두름은 작게)
    "Fatty": dict(
        feet=(300, 836),
        parts={
            "cape": dict(poly=[(110, 390), (355, 390), (355, 545), (110, 545)], pivot=(150, 420), blur=14),
            "back_leg": dict(poly=[(110, 770), (260, 770), (260, 839), (110, 839)], pivot=(185, 775)),
            "front_leg": dict(poly=[(330, 770), (480, 770), (480, 839), (330, 839)], pivot=(400, 775)),
            "crown": dict(poly=[(60, 0), (580, 0), (590, 300), (500, 390), (300, 400), (130, 380), (60, 300)], pivot=(330, 400), blur=30),
            "sword": dict(poly=[(470, 140), (732, 140), (732, 520), (600, 520), (530, 720), (450, 720), (470, 520), (500, 420)], pivot=(530, 440), blur=8),
        },
        windup=-10, strike=25,
    ),
    # 해골 궁수: 두건 쓴 해골 머리, 활 든 두 손(앞), 뒤쪽 팔, 두 다리 (쏘는 동작이라 작게)
    "BoneArcher": dict(
        feet=(260, 850),
        parts={
            "cape": dict(poly=[(40, 470), (145, 470), (155, 600), (140, 670), (40, 670)], pivot=(115, 490), blur=12),
            "back_leg": dict(poly=[(80, 690), (255, 690), (255, 852), (80, 852)], pivot=(180, 695)),
            "front_leg": dict(poly=[(265, 690), (430, 690), (430, 852), (265, 852)], pivot=(330, 695)),
            "crown": dict(poly=[(20, 0), (445, 0), (450, 300), (425, 420), (330, 480), (200, 480), (140, 440), (30, 340)], pivot=(300, 480), blur=24),
            "sword": dict(poly=[(290, 470), (360, 260), (430, 250), (520, 330), (583, 440), (583, 520), (520, 560), (450, 730), (370, 720), (380, 560), (290, 560)], pivot=(300, 510), blur=8),
        },
        windup=-8, strike=12,
    ),
    # 강령술사: 두건 해골 머리, 해골 지팡이 든 팔, 펄럭이는 왼쪽 누더기, 두 다리
    "Necro": dict(
        feet=(300, 668),
        parts={
            "cape": dict(poly=[(0, 370), (190, 340), (200, 600), (0, 600)], pivot=(190, 380), blur=20),
            "back_leg": dict(poly=[(180, 560), (290, 560), (290, 670), (180, 670)], pivot=(240, 565)),
            "front_leg": dict(poly=[(300, 540), (430, 540), (430, 670), (300, 670)], pivot=(340, 545)),
            "crown": dict(poly=[(120, 20), (510, 20), (510, 300), (450, 350), (300, 360), (200, 330), (120, 250)], pivot=(330, 350), blur=24),
            "sword": dict(poly=[(350, 380), (450, 370), (500, 320), (510, 0), (640, 0), (640, 140), (560, 200), (530, 340), (525, 450), (480, 470), (470, 640), (420, 640), (440, 470), (350, 450)], pivot=(360, 410), blur=8),
        },
        windup=-20, strike=35,
    ),
    # 장군 (진급 2): 깃털 투구, 치켜든 칼, 둥근 방패(망토 자리로 흔든다), 두 다리
    "General": dict(
        feet=(360, 736),
        parts={
            "cape": dict(poly=[(530, 390), (743, 390), (743, 690), (530, 690), (515, 540)], pivot=(545, 540), blur=10),
            "back_leg": dict(poly=[(250, 670), (335, 670), (335, 738), (250, 738)], pivot=(295, 675)),
            "front_leg": dict(poly=[(390, 670), (480, 670), (480, 738), (390, 738)], pivot=(435, 675)),
            "crown": dict(poly=[(150, 0), (500, 0), (560, 160), (560, 380), (530, 450), (320, 460), (210, 420), (160, 300)], pivot=(360, 470), blur=24),
            "sword": dict(poly=[(0, 95), (70, 95), (215, 345), (240, 430), (260, 500), (205, 520), (140, 480), (0, 190)], pivot=(230, 490), blur=8),
        },
        windup=-25, strike=70,
    ),
    # 영웅 (진급 3): 날개 금투구, 빛나는 칼, 금 냄비뚜껑 방패, 두 다리
    "GruntHero": dict(
        feet=(390, 642),
        parts={
            "cape": dict(poly=[(530, 280), (749, 280), (749, 600), (530, 600), (520, 440)], pivot=(540, 440), blur=10),
            "back_leg": dict(poly=[(270, 595), (370, 595), (370, 644), (270, 644)], pivot=(320, 600)),
            "front_leg": dict(poly=[(410, 595), (510, 595), (510, 644), (410, 644)], pivot=(455, 600)),
            "crown": dict(poly=[(150, 0), (540, 0), (560, 200), (540, 330), (420, 380), (260, 360), (200, 280), (150, 150)], pivot=(370, 370), blur=24),
            "sword": dict(poly=[(40, 30), (110, 30), (220, 280), (230, 360), (260, 400), (220, 430), (170, 390), (110, 320), (0, 100)], pivot=(215, 400), blur=8),
        },
        windup=-25, strike=75,
    ),
    # 금발 해골 보스급: 머리(해골·금발·왕관)·망토·대검. 다리는 망토 앞이라 떼지 않고 몸째 들썩인다
    "BoneKing": dict(
        feet=(420, 930),
        parts={
            "cape": dict(poly=[(0, 880), (80, 780), (150, 620), (200, 560), (262, 600), (302, 700), (312, 900), (232, 941), (0, 941)], pivot=(250, 560)),
            "crown": dict(poly=[(0, 330), (60, 250), (200, 160), (210, 0), (470, 0), (528, 160), (528, 330), (482, 372), (362, 378), (330, 352), (250, 420), (160, 582), (60, 562), (0, 470)], pivot=(400, 372)),
            "sword": dict(poly=[(490, 372), (625, 360), (642, 470), (658, 560), (614, 604), (598, 952), (494, 952), (490, 560), (506, 520), (546, 470), (546, 440)], pivot=(585, 460)),
        },
        windup=-200, strike=-40,
    ),
}
# 졸병 변형 (grunt_variants.py: 졸병 원화에 장비만 바꿔 그림): 졸병 리그를 옮겨 쓴다
GRUNT_VARIANT_PAD = 140   # grunt_variants.PAD
GRUNT_VARIANTS = {        # 이름: (잘린 위치 x, y, 가로 배율)  ← grunt_variants.py 실행 때 찍히는 값
    "Archer": (145, 25, 1.0),
    "Shield": (140, 140, 1.0),
    "Brute": (58, 74, 1.14),
}
# 위로 솟은 깃털·뿔과 커진 망치 머리까지 덮게 넓힌 투구·무기 다각형 (졸병 원화 좌표)
GRUNT_VARIANT_CROWN = [(140, 60), (180, -140), (520, -140), (505, 95), (500, 145), (400, 160), (300, 200), (215, 262), (165, 268), (140, 200)]
GRUNT_VARIANT_SWORD = [(-80, -80), (140, -80), (150, 120), (170, 245), (205, 275), (238, 345), (212, 365), (150, 345), (118, 300), (95, 250), (-80, 130)]


def _grunt_variant(bx, by, sx):
    base = RIGS["Grunt"]
    move = lambda p: ((p[0] + GRUNT_VARIANT_PAD) * sx - bx, p[1] + GRUNT_VARIANT_PAD - by)
    parts = {}
    for part, spec in base["parts"].items():
        poly = {"crown": GRUNT_VARIANT_CROWN, "sword": GRUNT_VARIANT_SWORD}.get(part, spec["poly"])
        parts[part] = dict(spec, poly=[move(p) for p in poly], pivot=move(spec["pivot"]))
    return dict(base, feet=move(base["feet"]), parts=parts)


for _name, (_bx, _by, _sx) in GRUNT_VARIANTS.items():
    RIGS[_name] = _grunt_variant(_bx, _by, _sx)

FILL = 0.84        # 칸 높이 중 그림 비율 (generate_art.ART_FILL과 같게)
FOOT = 0.92        # 칸에서 발 높이 (generate_art.FOOT)
PAD = 160          # 휘어 나간 칼·망토가 잘리지 않게 둘레에 두는 여백 (원화 픽셀)
BLUR = {"cape": 26, "back_leg": 14, "front_leg": 14, "crown": 30, "sword": 8}   # 부위 경계를 흐리는 폭 (원화 픽셀)
ORDER = ["cape", "back_leg", "body", "front_leg", "crown", "sword"]   # 뒤 → 앞
CORE_SOFT = 1.5    # 부위 속 경계를 살짝 흐리는 폭 (원화 픽셀)
SPARKLE_KEEP = ("Hero", "GruntHero")   # 몸에서 떨어진 반짝이를 지우는 원화 (칼과 따로 놀아서)
_cache = {}


def _main_only(img):
    """몸에 붙은 가장 큰 덩어리만 남긴다 (떨어진 반짝이 지우기)"""
    from collections import deque
    w, h = img.size
    a = img.getchannel("A").load()
    label = [0] * (w * h)
    sizes = [0]
    for sy in range(h):
        for sx in range(w):
            if a[sx, sy] < 24 or label[sy * w + sx]:
                continue
            n = len(sizes)
            sizes.append(0)
            q = deque([(sx, sy)])
            label[sy * w + sx] = n
            while q:
                x, y = q.popleft()
                sizes[n] += 1
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not label[ny * w + nx] and a[nx, ny] >= 24:
                        label[ny * w + nx] = n
                        q.append((nx, ny))
    big = max(range(1, len(sizes)), key=lambda i: sizes[i])
    out = img.copy()
    px = out.load()
    for y in range(h):
        for x in range(w):
            if label[y * w + x] not in (0, big):
                px[x, y] = (0, 0, 0, 0)
            elif label[y * w + x] == 0 and a[x, y]:
                px[x, y] = (0, 0, 0, 0)
    return out


def _prepared(name):
    """여백 두른 원화와 부위별 흐린 가중치 (원화 좌표 + PAD)"""
    if name in _cache:
        return _cache[name]
    from PIL import ImageFilter
    from generate_art import art_source
    source = art_source(name)
    if name in SPARKLE_KEEP:
        source = _main_only(source)
    rig = RIGS[name]
    padded = Image.new("RGBA", (source.size[0] + PAD * 2, source.size[1] + PAD * 2), (0, 0, 0, 0))
    padded.paste(source, (PAD, PAD))
    weights = {}
    used = Image.new("L", padded.size, 0)
    for part, spec in rig["parts"].items():
        mask = Image.new("L", padded.size, 0)
        ImageDraw.Draw(mask).polygon([(x + PAD, y + PAD) for x, y in spec["poly"]], fill=255)
        blur = spec.get("blur", BLUR[part])
        weights[part] = mask.filter(ImageFilter.GaussianBlur(blur / 2))
        # 부위 속(경계 띠를 뺀 곳)만 몸에서 덜어 낸다 → 경계 띠는 몸에도 남아 있어 부위가 움직여도 틈이 안 생긴다
        core = mask.filter(ImageFilter.MinFilter(2 * (blur // 2) + 1)).filter(ImageFilter.GaussianBlur(CORE_SOFT))
        used = ImageChops.lighter(used, core)
    weights["body"] = ImageChops.invert(used)
    _cache[name] = (source, padded, weights)
    return _cache[name]


def _warp(name, q):
    """부위마다 흐린 몫만큼 떼어 각자 돌린 뒤 뒤에서 앞 순서로 덮는다.
    몸은 부위 경계 띠까지 꽉 차 있어 틈이 안 생기고, 앞 부위가 뒤를 제대로 가린다"""
    source, padded, weights = _prepared(name)
    rig = RIGS[name]
    full = Image.new("RGBA", padded.size, (0, 0, 0, 0))
    for part in ORDER:
        if part not in weights:
            continue
        layer = padded.copy()
        layer.putalpha(ImageChops.multiply(padded.getchannel("A"), weights[part]))
        if part != "body":
            spec = rig["parts"][part]
            center = (spec["pivot"][0] + PAD, spec["pivot"][1] + PAD)
            shift = (0, q["crown_dy"]) if part == "crown" else (0, 0)
            layer = layer.rotate(-q[part], resample=Image.BICUBIC, center=center, translate=shift)
        full.alpha_composite(layer)
    return full, source


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
    rig = RIGS[name]
    q = pose(name, action, p)
    full, source = _warp(name, q)
    fx, fy = rig["feet"][0] + PAD, rig["feet"][1] + PAD
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
