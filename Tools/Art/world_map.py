"""월드맵 그림 (Content/YJJWave/Art/WorldMap.png). 지역 자리·색은 Content/YJJWave/Data/Regions.csv.

한 대륙 위에 지역이 맞붙는다. 경계는 칼로 자르지 않고 색·높이·장식이 서서히 섞인다:
  1) 낮은 해상도에서 칸마다 지역 무게 (물결진 거리의 소프트맥스) → 색·높이를 섞는다 + 잔무늬 노이즈
  2) 높이로 빛과 그늘 (왼쪽 위에서 비추는 언덕 음영)
  3) 바다 (깊은 곳 → 얕은 곳) · 파도 거품 · 모래 해안
  4) 강 (설산·산지에서 바다로, 굽이치게) · 길 (지역을 잇는 흙길)
  5) 장식: 촘촘히 흩고, 그 자리의 지역 무게로 종류를 뽑는다 → 경계에서는 두 지역 장식이 섞인다. 그림자, 위에서 아래 순서
  6) 가장자리 구름 테두리
잠긴 지역을 덮는 구름은 HUD가 그린다 (열리면 걷힌다).
"""
import csv
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

import generate_art as g

W, H = 1920, 1080
S = 2                       # 색·높이는 절반 해상도에서 계산해 키운다
BLEND = 70.0                # 경계에서 섞이는 폭 (화면 px, 클수록 부드럽다)
WARP = (46, 26)             # 경계를 물결지게 흔드는 크기

# 지역별: 땅 색 두 가지(낮은 곳·높은 곳), 높이, 장식 밀도
BIOMES = {
    "Forest":    dict(low="#4C9A3A", high="#3B7E2E", height=0.35, density=1.0),
    "Mountain":  dict(low="#9A8A70", high="#7A6A54", height=0.85, density=0.8),
    "Graveyard": dict(low="#5E5A72", high="#4A4660", height=0.3, density=0.8),
    "Canyon":    dict(low="#46324C", high="#2C1E34", height=0.15, density=0.7),
    "Desert":    dict(low="#E8C980", high="#D2AE62", height=0.25, density=0.6),
    "SnowPeak":  dict(low="#E6EEF5", high="#C4D2DF", height=0.95, density=0.8),
    "Volcano":   dict(low="#7A4A3A", high="#4E2E26", height=0.8, density=0.7),
}
OCEAN_DEEP = (46, 128, 160)
OCEAN_SHALLOW = (92, 196, 205)
SAND = "#EAD39A"


def read_regions():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Content", "YJJWave", "Data", "Regions.csv")
    with open(path, encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    return sorted(rows, key=lambda r: int(r["Order"]))


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def value_noise(w, h, cell, seed):
    """부드러운 0~1 노이즈 (작은 무작위 그림을 키운다)"""
    rnd = random.Random(seed)
    small = Image.new("L", (w // cell + 2, h // cell + 2))
    small.putdata([rnd.randint(0, 255) for _ in range(small.size[0] * small.size[1])])
    return small.resize((w, h), Image.BICUBIC)


def octaves(w, h, seed):
    a = value_noise(w, h, 64, seed)
    b = value_noise(w, h, 24, seed + 1)
    c = value_noise(w, h, 8, seed + 2)
    return Image.blend(Image.blend(a, b, 0.35), c, 0.2)


class Field:
    """지역 무게: 화면 좌표 → 지역별 무게 (경계에서 섞인다)"""

    def __init__(self, regions):
        self.names = [r["Name"] for r in regions]
        self.centers = [(float(r["MapX"]) * W, float(r["MapY"]) * H) for r in regions]

    def weights(self, x, y):
        wx = x + WARP[0] * math.sin(y * 0.0105) + WARP[1] * math.sin(x * 0.021 + 2)
        wy = y + WARP[0] * math.cos(x * 0.0093) + WARP[1] * math.sin(y * 0.026 + 1)
        d = [math.hypot(wx - cx, wy - cy) for cx, cy in self.centers]
        m = min(d)
        e = [math.exp(-(v - m) / BLEND) for v in d]
        t = sum(e)
        return [v / t for v in e]

    def pick(self, x, y, rnd):
        ws = self.weights(x, y)
        r = rnd.random()
        for name, wgt in zip(self.names, ws):
            r -= wgt
            if r <= 0:
                return name
        return self.names[-1]


def land_mask(rnd):
    w, h = W // S, H // S
    mask = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(mask)
    pts = []
    for i in range(90):
        a = i / 90 * math.tau
        k = 1 + 0.07 * math.sin(a * 4 + 1) + 0.05 * math.sin(a * 9 + 3) + 0.025 * math.sin(a * 17) + rnd.uniform(-0.015, 0.015)
        pts.append((w * 0.5 + math.cos(a) * w * 0.455 * k, h * 0.5 + math.sin(a) * h * 0.415 * k))
    d.polygon(pts, fill=255)
    for cx, cy, r in ((0.08, 0.86, 26), (0.94, 0.13, 20), (0.6, 0.95, 16), (0.04, 0.2, 14)):   # 작은 섬
        d.ellipse([cx * w - r * 1.4, cy * h - r, cx * w + r * 1.4, cy * h + r], fill=255)
    return mask.filter(ImageFilter.GaussianBlur(4)).point(lambda v: 255 if v > 128 else 0)


def terrain(field, mask, seed):
    """땅 색 + 언덕 음영 (절반 해상도)"""
    w, h = W // S, H // S
    noise = octaves(w, h, seed).load()
    m = mask.load()
    biomes = [BIOMES[n] for n in field.names]
    lows = [hex_rgb(b["low"]) for b in biomes]
    highs = [hex_rgb(b["high"]) for b in biomes]
    heights = [b["height"] for b in biomes]
    color = Image.new("RGB", (w, h))
    height = [[0.0] * w for _ in range(h)]
    cp = color.load()
    for y in range(h):
        for x in range(w):
            if not m[x, y]:
                continue
            ws = field.weights(x * S, y * S)
            n = noise[x, y] / 255.0
            hh = sum(wg * hg for wg, hg in zip(ws, heights)) * (0.6 + 0.8 * n)
            height[y][x] = hh
            t = min(max((n - 0.3) * 1.6, 0.0), 1.0)
            r = gg = b = 0.0
            for wg, lo, hi in zip(ws, lows, highs):
                r += wg * (lo[0] + (hi[0] - lo[0]) * t)
                gg += wg * (lo[1] + (hi[1] - lo[1]) * t)
                b += wg * (lo[2] + (hi[2] - lo[2]) * t)
            cp[x, y] = (int(r), int(gg), int(b))
    # 왼쪽 위에서 비추는 빛: 높이 차이로 밝게·어둡게
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            if not m[x, y]:
                continue
            slope = (height[y - 1][x - 1] - height[y + 1][x + 1]) * 1.6
            k = 1 + max(min(slope, 0.35), -0.35)
            r, gg, b = cp[x, y]
            cp[x, y] = (min(int(r * k), 255), min(int(gg * k), 255), min(int(b * k), 255))
    return color


def ocean(mask):
    """깊은 바다 → 얕은 바다 (땅 가까이) + 물결"""
    near = mask.filter(ImageFilter.GaussianBlur(26)).resize((W, H), Image.BILINEAR)
    deep = Image.new("RGB", (W, H), OCEAN_DEEP)
    shallow = Image.new("RGB", (W, H), OCEAN_SHALLOW)
    sea = Image.composite(shallow, deep, near.point(lambda v: min(255, v * 3)))
    d = ImageDraw.Draw(sea)
    rnd = random.Random(5)
    for _ in range(420):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        d.arc([x - 16, y - 5, x + 16, y + 5], 200, 340, fill=(150, 220, 228), width=2)
    return sea.convert("RGBA")


def spline(points, steps=14):
    """Catmull-Rom 곡선 점들"""
    out = []
    pts = [points[0]] + points + [points[-1]]
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2 + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in (0, 1)))
    out.append(points[-1])
    return out


def draw_river(p, pts, w0, w1):
    curve = spline(pts)
    n = len(curve)
    for i, (a, b) in enumerate(zip(curve, curve[1:])):
        width = w0 + (w1 - w0) * i / n
        p.stick(a[0], a[1], b[0], b[1], width, g.rgba("#4FB4C8"))
    for i, (a, b) in enumerate(zip(curve, curve[1:])):
        width = w0 + (w1 - w0) * i / n
        p.stick(a[0], a[1], b[0], b[1], width * 0.35, g.rgba("#9FE2EA"), line=False)
    return curve


def draw_road(p, pts, rnd):
    curve = spline(pts, steps=18)
    for a, b in zip(curve, curve[1:]):
        p.stick(a[0], a[1], b[0], b[1], 9, g.rgba("#C9A46A"))
    for i, (a, b) in enumerate(zip(curve, curve[1:])):
        if i % 3 == 0:
            p.stick(a[0], a[1], b[0], b[1], 2.2, g.rgba("#A6824C"), line=False)
    return curve


# ---------------------------------------------------------------------------------------------
# 장식 하나 (x, y = 발밑, k = 크기)

SHADOWS = []   # 그림자는 따로 모아 장식 밑에 깐다 (펜은 겹쳐 칠하지 않고 덮어쓰므로)


def shadow(p, x, y, rx):
    SHADOWS.append((x + rx * 0.15, y, rx, rx * 0.32))


def peak(p, x, y, k, body, light, cap=None):
    h, w = 70 * k, 46 * k
    shadow(p, x, y, w)
    p.poly([(x - w, y), (x, y - h), (x + w, y)], g.rgba(body))
    p.poly([(x - w + 3, y - 2), (x, y - h + 4), (x - 2, y - 2)], g.rgba(light), line=False)   # 빛 받는 왼쪽
    if cap:
        p.poly([(x - w * 0.36, y - h * 0.64), (x, y - h + 2), (x + w * 0.36, y - h * 0.64), (x + w * 0.12, y - h * 0.56), (x, y - h * 0.66), (x - w * 0.14, y - h * 0.56)], g.rgba(cap), line=False)


def decor(p, biome, x, y, k, rnd):
    if biome == "Forest":
        shadow(p, x, y, 16 * k)
        if rnd.random() < 0.5:   # 둥근 나무
            p.stick(x, y, x, y - 14 * k, 5 * k, g.rgba("#6E4A2A"))
            p.ellipse(x, y - 26 * k, 18 * k, 18 * k, g.rgba(rnd.choice(["#3E8A34", "#4FA040", "#357A2C"])))
            p.ellipse(x - 5 * k, y - 31 * k, 7 * k, 6 * k, g.rgba("#6CC055"), line=False)
        else:                    # 뾰족 나무
            p.stick(x, y, x, y - 10 * k, 4 * k, g.rgba("#6E4A2A"))
            p.poly([(x - 16 * k, y - 8 * k), (x, y - 46 * k), (x + 16 * k, y - 8 * k)], g.rgba(rnd.choice(["#2F7A3A", "#3A8A44"])))
    elif biome == "Mountain":
        if rnd.random() < 0.75:
            peak(p, x, y, k * rnd.uniform(0.8, 1.2), "#8C7A64", "#B09A7E", "#D8CDB8" if rnd.random() < 0.3 else None)
        else:
            shadow(p, x, y, 14 * k)
            p.ellipse(x, y - 8 * k, 14 * k, 10 * k, g.rgba("#7E6E58"))
    elif biome == "SnowPeak":
        if rnd.random() < 0.7:
            peak(p, x, y, k * rnd.uniform(0.9, 1.3), "#A9BDCF", "#D5E2EE", "#FFFFFF")
        else:
            shadow(p, x, y, 14 * k)
            p.stick(x, y, x, y - 8 * k, 4 * k, g.rgba("#6E4A2A"))
            p.poly([(x - 15 * k, y - 6 * k), (x, y - 42 * k), (x + 15 * k, y - 6 * k)], g.rgba("#2F6A4A"))
            p.poly([(x - 8 * k, y - 26 * k), (x, y - 42 * k), (x + 8 * k, y - 26 * k)], g.rgba("#FFFFFF"), line=False)
    elif biome == "Graveyard":
        shadow(p, x, y, 12 * k)
        r = rnd.random()
        if r < 0.45:
            p.rect(x - 9 * k, y - 24 * k, x + 9 * k, y, g.rgba("#A39E98"), radius=8 * k)
            p.stick(x - 4 * k, y - 14 * k, x + 4 * k, y - 14 * k, 1.4)
        elif r < 0.75:
            p.stick(x, y, x, y - 28 * k, 3.5 * k, g.rgba("#7A6650"))
            p.stick(x - 9 * k, y - 20 * k, x + 9 * k, y - 20 * k, 3.5 * k, g.rgba("#7A6650"))
        else:   # 마른 나무
            p.stick(x, y, x, y - 34 * k, 4 * k, g.rgba("#4A3A30"))
            p.stick(x, y - 22 * k, x - 12 * k, y - 32 * k, 2.5 * k, g.rgba("#4A3A30"))
            p.stick(x, y - 26 * k, x + 10 * k, y - 38 * k, 2.5 * k, g.rgba("#4A3A30"))
    elif biome == "Canyon":
        if rnd.random() < 0.55:   # 갈라진 틈
            pts = [(x - 20 * k, y - 4 * k), (x - 6 * k, y - 10 * k), (x + 4 * k, y - 2 * k), (x + 22 * k, y - 8 * k), (x + 20 * k, y + 2 * k), (x + 2 * k, y + 6 * k), (x - 8 * k, y + 2 * k)]
            p.poly(pts, g.rgba("#140C1A"))
        else:                     # 보라 수정
            shadow(p, x, y, 12 * k)
            for dx, hgt in ((-7, 26), (0, 36), (8, 22)):
                p.poly([(x + (dx - 5) * k, y), (x + dx * k, y - hgt * k), (x + (dx + 5) * k, y)], g.rgba("#9B4DFF"))
            p.ellipse(x, y - 30 * k, 3 * k, 3 * k, g.rgba("#F0D8FF"), line=False)
    elif biome == "Desert":
        r = rnd.random()
        if r < 0.45:   # 선인장
            shadow(p, x, y, 10 * k)
            p.stick(x, y, x, y - 34 * k, 9 * k, g.rgba("#5FA048"))
            p.stick(x - 10 * k, y - 18 * k, x - 10 * k, y - 28 * k, 6 * k, g.rgba("#5FA048"))
            p.stick(x - 10 * k, y - 18 * k, x - 2 * k, y - 18 * k, 6 * k, g.rgba("#5FA048"))
        elif r < 0.8:  # 모래 언덕 줄
            p.arc(x, y + 10 * k, 30 * k, 200, 340, 3 * k, g.rgba("#C29A52"))
        else:          # 바위
            shadow(p, x, y, 14 * k)
            p.ellipse(x, y - 8 * k, 15 * k, 10 * k, g.rgba("#B0855A"))
    elif biome == "Volcano":
        r = rnd.random()
        if r < 0.35:   # 작은 화산
            shadow(p, x, y, 40 * k)
            p.poly([(x - 40 * k, y), (x - 10 * k, y - 46 * k), (x + 10 * k, y - 46 * k), (x + 40 * k, y)], g.rgba("#4E3630"))
            p.poly([(x - 10 * k, y - 46 * k), (x - 3 * k, y - 26 * k), (x + 4 * k, y - 40 * k), (x + 10 * k, y - 46 * k)], g.rgba("#FF7A2A"))
            for i in range(3):
                p.ellipse(x + (i - 1) * 6 * k, y - (60 + i * 14) * k, (9 + i * 3) * k, (7 + i * 2) * k, g.rgba("#6E6A6A", 190), line=False)
        elif r < 0.7:  # 용암 줄기
            p.stick(x - 18 * k, y - 4 * k, x + 2 * k, y + 2 * k, 5 * k, g.rgba("#FF8A2A"))
            p.stick(x + 2 * k, y + 2 * k, x + 18 * k, y - 2 * k, 4 * k, g.rgba("#FFB43A"))
        else:          # 검은 바위
            shadow(p, x, y, 14 * k)
            p.poly([(x - 14 * k, y), (x - 6 * k, y - 18 * k), (x + 8 * k, y - 14 * k), (x + 14 * k, y)], g.rgba("#2E2422"))


def landmark(p, biome, x, y, rnd):
    """지역 가운데 큰 볼거리"""
    if biome == "Forest":   # 나무 오두막 마을
        for dx, dy in ((-40, 10), (30, -6), (0, 30)):
            hx, hy = x + dx, y + dy
            shadow(p, hx, hy, 22)
            p.rect(hx - 16, hy - 22, hx + 16, hy, g.rgba("#C89A62"), radius=3)
            p.poly([(hx - 22, hy - 20), (hx, hy - 40), (hx + 22, hy - 20)], g.rgba("#D9453A"))
    elif biome == "Graveyard":   # 큰 무덤 + 달
        shadow(p, x, y, 40)
        p.rect(x - 34, y - 50, x + 34, y, g.rgba("#8E8A86"), radius=14)
        p.rect(x - 8, y - 70, x + 8, y - 40, g.rgba("#8E8A86"), radius=3)
        p.rect(x - 22, y - 62, x + 22, y - 52, g.rgba("#8E8A86"), radius=3)
        p.ellipse(x + 90, y - 110, 26, 26, g.rgba("#F4F0D8"))
    elif biome == "Volcano":
        decor(p, "Volcano", x, y, 2.4, random.Random(1))
    elif biome in ("Mountain", "SnowPeak"):
        cap = "#FFFFFF" if biome == "SnowPeak" else "#D8CDB8"
        body = "#A9BDCF" if biome == "SnowPeak" else "#8C7A64"
        peak(p, x - 40, y + 10, 1.6, body, "#D5E2EE" if biome == "SnowPeak" else "#B09A7E", cap)
        peak(p, x + 30, y, 2.1, body, "#D5E2EE" if biome == "SnowPeak" else "#B09A7E", cap)
    elif biome == "Canyon":   # 어둠의 탑
        shadow(p, x, y, 24)
        p.poly([(x - 20, y), (x - 10, y - 90), (x + 10, y - 90), (x + 20, y)], g.rgba("#2A1E30"))
        p.poly([(x - 14, y - 90), (x, y - 120), (x + 14, y - 90)], g.rgba("#3A2A44"))
        p.ellipse(x, y - 70, 6, 8, g.rgba("#C77DFF"), line=False)
    elif biome == "Desert":   # 피라미드
        shadow(p, x, y, 60)
        p.poly([(x - 60, y), (x, y - 66), (x + 60, y)], g.rgba("#D9B26A"))
        p.poly([(x, y - 66), (x + 60, y), (x + 12, y)], g.rgba("#B8904E"), line=False)


def clouds_frame(img, rnd):
    """지도 가장자리 구름 테두리"""
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    spots = []
    for i in range(70):
        t = i / 70
        spots += [(t * W, rnd.uniform(-30, 40)), (t * W, H - rnd.uniform(-30, 40))]
    for i in range(40):
        t = i / 40
        spots += [(rnd.uniform(-30, 40), t * H), (W - rnd.uniform(-30, 40), t * H)]
    for x, y in spots:
        r = rnd.uniform(40, 80)
        d.ellipse([x - r, y - r * 0.8 + 8, x + r, y + r * 0.8 + 8], fill=(150, 170, 190, 120))
    for x, y in spots:
        r = rnd.uniform(38, 76)
        d.ellipse([x - r, y - r * 0.8, x + r, y + r * 0.8], fill=(250, 252, 255, 255))
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(3)))


def world_map():
    rnd = random.Random(11)
    regions = read_regions()
    field = Field(regions)
    mask = land_mask(rnd)

    img = ocean(mask)
    # 파도 거품 + 모래 해안 + 땅
    big = mask.resize((W, H), Image.BILINEAR).point(lambda v: 255 if v > 128 else 0)
    foam = big.filter(ImageFilter.MaxFilter(15))
    img.paste(g.rgba("#D8F4F4"), (0, 0), foam)
    img.paste(g.rgba("#5CC4CD"), (0, 0), big.filter(ImageFilter.MaxFilter(11)))
    img.paste(g.rgba(SAND), (0, 0), big.filter(ImageFilter.MaxFilter(7)))
    land = terrain(field, mask, 3).resize((W, H), Image.BICUBIC).convert("RGBA")
    img.paste(land, (0, 0), big)

    p = g.Pen(W, H, lw=2.6)
    centers = dict(zip(field.names, field.centers))
    # 강: 설산·산지에서 바다로
    rivers = []
    if "SnowPeak" in centers:
        sx, sy = centers["SnowPeak"]
        rivers.append(draw_river(p, [(sx - 60, sy + 40), (sx - 140, sy + 160), (sx - 90, sy + 300), (sx - 160, H * 0.98)], 5, 16))
    if "Mountain" in centers:
        mx, my = centers["Mountain"]
        rivers.append(draw_river(p, [(mx + 40, my + 30), (mx + 150, my + 170), (mx + 120, my + 330), (mx + 190, H * 0.99)], 5, 14))
    # 길: 지역 중심을 차례로, 굽이치게
    order = [centers[n] for n in field.names]
    waypoints = [order[0]]
    for a, b in zip(order, order[1:]):
        waypoints += [((a[0] + b[0]) / 2 + rnd.uniform(-50, 50), (a[1] + b[1]) / 2 + rnd.uniform(-50, 50)), b]
    roads = draw_road(p, waypoints, rnd)
    keep_out = rivers + [roads]

    # 장식: 촘촘히 흩고 (강·길·가운데 볼거리 자리는 비운다), 그 자리 지역 무게로 종류를 뽑는다
    bm = big.load()
    spots = []
    grid = {}
    cell = 34
    for _ in range(26000):
        x, y = rnd.uniform(20, W - 20), rnd.uniform(20, H - 20)
        if not bm[int(x), int(y)]:
            continue
        key = (int(x // cell), int(y // cell))
        if key in grid:
            continue
        if any(math.hypot(x - cx, y - cy) < 90 for cx, cy in field.centers):
            continue
        if any(min(abs(x - a) + abs(y - b) for a, b in line[::3]) < 22 for line in keep_out):
            continue
        biome = field.pick(x, y, rnd)
        if rnd.random() > BIOMES[biome]["density"]:
            continue
        grid[key] = True
        spots.append((x + rnd.uniform(-6, 6), y + rnd.uniform(-6, 6), biome))
    items = [(y, x, b, None) for x, y, b in spots] + [(cy + 30, cx, n, "landmark") for n, (cx, cy) in centers.items()]
    for y, x, biome, kind in sorted(items):
        if kind:
            landmark(p, biome, x, y, rnd)
        else:
            decor(p, biome, x, y, rnd.uniform(0.55, 0.85), rnd)
    shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for x, y, rx, ry in SHADOWS:
        sd.ellipse([x - rx, y - ry, x + rx, y + ry], fill=(0, 0, 0, 70))
    img.alpha_composite(shade.filter(ImageFilter.GaussianBlur(2)))
    img.alpha_composite(p.done(W, H))
    clouds_frame(img, rnd)
    return img
