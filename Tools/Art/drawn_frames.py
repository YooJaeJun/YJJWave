"""손그림 프레임 애니 (곰 족장): AI/Frames/_src/<이름> 의 Gemini 시트에서 캐릭터를 떼어 내 AI/Frames/<이름>/<동작>_<번호>.png 로. 이후 generate_art.py Big_<이름>.png Units.png
- 칸으로 자르지 않는다 (몽둥이가 칸 경계를 넘어가면 잘리므로). 큰 덩어리 = 캐릭터, 작은 조각(먼지·떨어진 왕관)은 가까운 캐릭터에 붙인다
- 크기는 시트마다 하나: 서 있는 기준 프레임의 키를 대기 1번 키에 맞춘다 (시체는 누운 몸 길이로)
결과: AI/Frames/<이름>/*.png, _src 에 동작별 미리보기 GIF·한눈 띠. 필요: pip install opencv-python numpy"""
import os

import cv2
import numpy as np
from PIL import Image, ImageDraw

ART = os.path.dirname(os.path.abspath(__file__))
NAME = "BearChief"
HERE = os.path.join(ART, "AI", "Frames", "_src", NAME)      # Gemini 원본 시트
DEST = os.path.join(ART, "AI", "Frames", NAME)              # generate_art 가 읽는 프레임
OUT = 512
FOOT = 0.93
BG = (235, 232, 220, 255)
TARGET_H = OUT * 0.66      # 서 있는 키 (칸 비율)
CORPSE_LEN = 1.15          # 누운 몸 길이 = 서 있는 키 x 이 배
ANCHOR_X = 0.43            # 몸 가운데 칸 비율 (공격 몽둥이가 앞으로 뻗을 자리)

# 시트: 캐릭터 수, 줄 수, 크기 기준 프레임 번호
SHEETS = {
    'walk3_sheet.png':   (8, 2, 0),
    'frames_sheet.png':  (6, 2, 0),
    'idlehit_sheet.png': (8, 2, 0),
    'dazed_edit.png':    (1, 1, 0),
    'corpse3_sheet.png': (1, 1, None),
}
# 동작: (시트, 쓸 캐릭터 번호들)
ACTIONS = {
    'walk':   ('walk3_sheet.png', [0, 1, 2, 3, 4, 5, 6, 7]),
    'attack': ('frames_sheet.png', [0, 1, 2, 3, 4, 5]),
    'idle':   ('idlehit_sheet.png', [0, 1, 2, 3]),
    'hit':    ('idlehit_sheet.png', [4, 5]),
    'dazed':  ('dazed_edit.png', [0, 0]),
    'corpse': ('corpse3_sheet.png', [0]),
}
DAZED_TILT = (4, -4)   # 어질: 같은 그림을 발 축으로 좌우 기우뚱


def alpha_of(rgb):
    h, w = rgb.shape[:2]
    light = ((rgb.min(axis=2) > 222) & (rgb.max(axis=2).astype(int) - rgb.min(axis=2) < 28)).astype(np.uint8) * 255
    mask = np.zeros((h + 2, w + 2), np.uint8)
    for x, y in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]:
        if light[y, x] == 255:
            cv2.floodFill(light, mask, (x, y), 128)
    return np.where(light == 128, 0, 255).astype(np.uint8)


def characters(sheet, count, rows):
    rgb = np.array(Image.open(os.path.join(HERE, sheet)).convert('RGB'))
    H, W = rgb.shape[:2]
    a = alpha_of(rgb)
    n, lab, st, cen = cv2.connectedComponentsWithStats((a > 0).astype(np.uint8), 8)
    comps = [i for i in range(1, n) if st[i, 4] >= 60]
    comps.sort(key=lambda i: -st[i, 4])
    bodies = comps[:count]
    groups = {b: [b] for b in bodies}
    for i in comps[count:]:
        # 가장 가까운 캐릭터 (테두리 상자 사이 거리)
        x, y, w, h = st[i, :4]
        def gap(b):
            bx, by, bw, bh = st[b, :4]
            dx = max(bx - (x + w), x - (bx + bw), 0)
            dy = max(by - (y + h), y - (by + bh), 0)
            return dx * dx + dy * dy
        best = min(bodies, key=gap)
        if gap(best) < (min(W, H) * 0.08) ** 2:
            groups[best].append(i)
    order = sorted(bodies, key=lambda b: (int(cen[b][1] // (H / rows)), cen[b][0]))
    soft = cv2.GaussianBlur(a, (3, 3), 0)
    out = []
    for b in order:
        m = np.isin(lab, groups[b])
        m = cv2.dilate(m.astype(np.uint8), np.ones((3, 3), np.uint8)) > 0
        ys, xs = np.nonzero(m)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        alpha = np.where(m[y0:y1, x0:x1], soft[y0:y1, x0:x1], 0)
        out.append(np.dstack([rgb[y0:y1, x0:x1], alpha]))
    return out


def metrics(f):
    a = f[:, :, 3] > 128
    ys, xs = np.nonzero(a)
    foot = ys.max()
    low = ys > foot - (foot - ys.min()) * 0.45
    return foot, int(np.median(xs[low])), foot - ys.min()


def body_length(f):
    """누운 몸 길이: 가장 큰 덩어리 (몽둥이·왕관이 떨어져 있으면 빠진다)의 너비"""
    a = (f[:, :, 3] > 128).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(a, 8)
    big = 1 + int(np.argmax(st[1:, 4]))
    return st[big, 2]


chars = {s: characters(s, c, r) for s, (c, r, _) in SHEETS.items()}
scale = {}
for s, (c, r, ref) in SHEETS.items():
    scale[s] = TARGET_H / metrics(chars[s][ref])[2] if ref is not None else TARGET_H * CORPSE_LEN / body_length(chars[s][0])

os.makedirs(DEST, exist_ok=True)
aligned = {}
for name, (s, idx) in ACTIONS.items():
    aligned[name] = []
    for k, i in enumerate(idx):
        img = Image.fromarray(chars[s][i])
        img = img.resize((round(img.width * scale[s]), round(img.height * scale[s])), Image.LANCZOS)
        foot, cx, _ = metrics(np.array(img))
        if name == 'corpse':
            cx = img.width // 2
        canvas = Image.new('RGBA', (OUT, OUT), (0, 0, 0, 0))
        ax = round(OUT * (0.5 if name == 'corpse' else ANCHOR_X))
        canvas.paste(img, (ax - cx, round(OUT * FOOT) - foot), img)
        if name == 'dazed':
            canvas = canvas.rotate(DAZED_TILT[k], resample=Image.BICUBIC, center=(ax, round(OUT * FOOT)))
        canvas.save(os.path.join(DEST, '%s_%d.png' % (name, k)))
        aligned[name].append(canvas)
        # 칸 밖으로 잘린 곳 검사
        x0, y0 = ax - cx, round(OUT * FOOT) - foot
        if x0 < 0 or y0 < 0 or x0 + img.width > OUT or y0 + img.height > OUT:
            print('CLIP', name, k, x0, y0, x0 + img.width, y0 + img.height)

DUR = {'walk': 110, 'attack': 120, 'idle': 220, 'hit': 160, 'dazed': 260, 'corpse': 600}
for name, fr in aligned.items():
    gif = []
    for c in fr:
        bg = Image.new('RGBA', (OUT, OUT), BG)
        bg.alpha_composite(c)
        gif.append(bg.convert('RGB').resize((320, 320), Image.LANCZOS))
    gif[0].save(os.path.join(HERE, 'preview_%s.gif' % name), save_all=True, append_images=gif[1:] or [gif[0]], duration=DUR[name], loop=0)

CELL = 200
rows = list(aligned.items())
sheet = Image.new('RGBA', (CELL * 8 + 120, CELL * len(rows)), (255, 255, 255, 255))
d = ImageDraw.Draw(sheet)
for r, (name, fr) in enumerate(rows):
    d.text((8, r * CELL + CELL // 2), name, fill=(0, 0, 0, 255))
    for k, c in enumerate(fr):
        bg = Image.new('RGBA', (OUT, OUT), BG)
        bg.alpha_composite(c)
        sheet.paste(bg.resize((CELL, CELL), Image.LANCZOS), (120 + k * CELL, r * CELL))
        d.line([(120 + k * CELL, r * CELL + round(CELL * FOOT)), (120 + (k + 1) * CELL, r * CELL + round(CELL * FOOT))], fill=(255, 0, 0, 90))
        d.rectangle([120 + k * CELL, r * CELL, 120 + (k + 1) * CELL - 1, (r + 1) * CELL - 1], outline=(180, 180, 180, 255))
sheet.save(os.path.join(HERE, 'preview_all.png'))
print('ok', {s: round(v, 3) for s, v in scale.items()})
