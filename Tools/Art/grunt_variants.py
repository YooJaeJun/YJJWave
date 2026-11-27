"""승인 졸병 원화(AI/Approved/Grunt.png)에서 투구·국자·냄비뚜껑만 지우고 장비를 직접 그려 궁수·방패병·망치 대장 원화를 만든다
→ AI/Approved/Archer.png, Shield.png, Brute.png (흰 바탕, generate_art 가 바탕을 지운다).
몸·얼굴·손은 원화 그대로 (손은 새 장비 위에 다시 덮는다). 좌표는 바탕 지운 졸병 그림(685×606) 기준.
리그(rig.py GRUNT_VARIANTS)는 여기 PAD·STRETCH 와 잘린 위치(VARIANT_BOX)를 쓴다.
실행: python grunt_variants.py (generate_art.py 보다 먼저)"""
import colorsys, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_art as ga
from PIL import Image, ImageDraw, ImageChops

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "AI", "Approved")
PAD = 140
STRETCH = {"Brute": 1.14}   # 가로로 넓히는 배율 (뚱뚱한 망치 대장)
MARGIN = 24               # 저장할 때 그림 둘레 흰 여백
SS = 4                    # 매끈한 선을 위한 배율
INK = (22, 18, 18, 255)
LINE = 7

base = ga.art_source("Grunt")
W, H = base.size[0] + PAD * 2, base.size[1] + PAD * 2


def P(pts):
    return [((x + PAD) * SS, (y + PAD) * SS) for x, y in pts]


def layer():
    return Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))


def done(big):
    return big.resize((W, H), Image.LANCZOS)


def poly(img, pts, fill, line=LINE):
    big = layer()
    d = ImageDraw.Draw(big)
    d.polygon(P(pts), fill=INK)
    # 안쪽 채우기: 먹선 두께만큼 줄인 다각형 대신, 같은 다각형을 선으로 덧그려 테두리를 만든다
    d.polygon(P(pts), fill=fill)
    d.line(P(pts + pts[:1]), fill=INK, width=line * SS, joint="curve")
    img.alpha_composite(done(big))


def stroke(img, pts, color, width, line=LINE):
    big = layer()
    d = ImageDraw.Draw(big)
    d.line(P(pts), fill=INK, width=(width + line * 2) * SS, joint="curve")
    for x, y in (pts[0], pts[-1]):
        r = (width / 2 + line) * SS
        cx, cy = (x + PAD) * SS, (y + PAD) * SS
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)
    d.line(P(pts), fill=color, width=width * SS, joint="curve")
    for x, y in (pts[0], pts[-1]):
        r = width / 2 * SS
        cx, cy = (x + PAD) * SS, (y + PAD) * SS
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=color)
    img.alpha_composite(done(big))


def thin(img, pts, color, width):
    big = layer()
    ImageDraw.Draw(big).line(P(pts), fill=color, width=width * SS, joint="curve")
    img.alpha_composite(done(big))


def mask(pts):
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).polygon([(x + PAD, y + PAD) for x, y in pts], fill=255)
    return m


def erase_box(img, x0, y0, x1, y1):
    erase(img, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)])


def erase(img, pts):
    a = img.getchannel("A")
    img.putalpha(ImageChops.subtract(a, mask(pts)))


def cut_part(img, pts):
    part = img.copy()
    part.putalpha(ImageChops.multiply(img.getchannel("A"), mask(pts)))
    return part


def recolor(img, pts, hue, sat, light=1.0):
    """다각형 안 회색 쇠 부분만 색을 입힌다 (검은 먹선은 그대로)"""
    m = mask(pts).load()
    px = img.load()
    for y in range(H):
        for x in range(W):
            if not m[x, y]:
                continue
            r, g, b, a = px[x, y]
            if not a:
                continue
            h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
            if l < 0.16 or l > 0.76 or s > 0.25:
                continue
            r2, g2, b2 = colorsys.hls_to_rgb(hue, min(1.0, l * light), sat)
            px[x, y] = (round(r2 * 255), round(g2 * 255), round(b2 * 255), a)


def front_fist(img, x=568, y=398):
    """냄비뚜껑을 지운 앞손: 막대 팔 + 동그란 주먹을 원화 결로 다시 그린다"""
    stroke(img, [(500, y), (x - 8, y)], (250, 246, 238, 255), 14, line=5)
    big = layer()
    d = ImageDraw.Draw(big)
    for r, c in ((24, INK), (18, (250, 246, 238, 255))):
        cx, cy, rr = (x + PAD) * SS, (y + PAD) * SS, r * SS
        d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=c)
    img.alpha_composite(done(big))


def feather(img, root, tip, color):
    ang = math.atan2(tip[1] - root[1], tip[0] - root[0])
    nx, ny = -math.sin(ang), math.cos(ang)
    pts = []
    for k in range(9):
        t = k / 8
        w = 26 * math.sin(math.pi * t) ** 0.7
        pts.append((root[0] + (tip[0] - root[0]) * t + nx * w, root[1] + (tip[1] - root[1]) * t + ny * w))
    for k in range(8, -1, -1):
        t = k / 8
        w = 26 * math.sin(math.pi * t) ** 0.7
        pts.append((root[0] + (tip[0] - root[0]) * t - nx * w, root[1] + (tip[1] - root[1]) * t - ny * w))
    poly(img, pts, color, line=5)
    thin(img, [root, tip], INK, 3)


def padded():
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(base, (PAD, PAD))
    return img


HELMET = [(150, 140), (160, 80), (200, 25), (260, 2), (330, 8), (420, 38), (482, 88), (505, 125), (480, 142), (390, 150),
          (300, 190), (225, 262), (195, 305), (160, 255)]
LADLE = [(0, 25), (48, 25), (190, 262), (200, 300), (150, 305), (0, 110)]
BACK_HAND = [(122, 268), (178, 258), (200, 296), (232, 340), (200, 356), (160, 352), (122, 322)]
LID = [(520 + 85 + 88 * math.cos(a / 20 * math.tau), 400 + 112 * math.sin(a / 20 * math.tau)) for a in range(20)]
FRONT_HAND = [(515, 372), (552, 378), (586, 380), (590, 422), (552, 424), (515, 412)]


def archer():
    img = padded()
    back_hand = cut_part(img, BACK_HAND)
    erase(img, LADLE)
    erase(img, LID)
    erase_box(img, 597, 270, 700, 530)
    # 초록 칠한 냄비 투구 + 꽂은 깃털
    recolor(img, HELMET, 0.30, 0.42, 0.95)
    feather(img, (330, 20), (250, -110), (214, 64, 52, 255))
    # 뒷손: 화살 한 대를 치켜든다
    stroke(img, [(212, 352), (34, 70)], (176, 120, 66, 255), 11)
    poly(img, [(34, 70), (10, 18), (60, 46)], (190, 196, 204, 255), line=5)
    poly(img, [(196, 326), (226, 336), (236, 376), (214, 366)], (214, 64, 52, 255), line=4)
    poly(img, [(196, 326), (176, 344), (184, 384), (204, 362)], (214, 64, 52, 255), line=4)
    img.alpha_composite(back_hand)
    # 앞손: 몸 밖으로 활을 내민다 (배를 안 가린다)
    bow = [(530 + 70 * math.sin(math.pi * t) ** 0.8, 236 + 330 * t) for t in [k / 24 for k in range(25)]]
    thin(img, [(530, 236), (530, 566)], INK, 3)
    stroke(img, bow, (150, 96, 50, 255), 14)
    front_fist(img, 592, 400)
    return img


def shield():
    img = padded()
    erase(img, LID)
    erase_box(img, 597, 270, 700, 530)
    # 냄비 투구 → 거꾸로 쓴 파란 양동이
    recolor(img, HELMET, 0.58, 0.55, 1.05)
    front_fist(img)
    # 앞손: 큰 나무 문짝 방패 (손을 가리고 앞에 든다)
    door = [(520, 214), (676, 226), (668, 590), (512, 578)]
    poly(img, door, (196, 146, 92, 255), line=8)
    for t in (0.33, 0.66):
        thin(img, [(520 + 156 * t, 220 + 12 * t), (512 + 156 * t, 582 + 12 * t)], (120, 82, 46, 255), 4)
    for y0 in (282, 500):
        band = [(516, y0), (674, y0 + 12), (673, y0 + 40), (515, y0 + 28)]
        poly(img, band, (110, 112, 120, 255), line=5)
        for x in (538, 596, 652):
            yy = y0 + 20 + (x - 516) * 12 / 158
            big = layer()
            r = 6 * SS
            cx, cy = (x + PAD) * SS, (yy + PAD) * SS
            ImageDraw.Draw(big).ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)
            img.alpha_composite(done(big))
    return img


def brute():
    img = padded()
    back_hand = cut_part(img, BACK_HAND)
    erase(img, LADLE)
    erase(img, LID)
    erase_box(img, 597, 270, 700, 530)
    recolor(img, HELMET, 0.07, 0.35, 0.85)
    # 투구에 뿔 두 개
    poly(img, [(262, 34), (222, 4), (200, -40), (234, -8), (282, 22)], (236, 226, 204, 255), line=6)
    poly(img, [(404, 32), (448, 0), (480, -44), (470, 6), (430, 52)], (236, 226, 204, 255), line=6)
    # 가죽 앞치마 (몸통 일부, 움직이지 않음)
    poly(img, [(256, 396), (482, 406), (496, 470), (470, 528), (372, 552), (270, 528), (244, 470)], (128, 80, 48, 255))
    front_fist(img)
    thin(img, [(250, 400), (300, 330)], INK, 6)
    thin(img, [(480, 410), (470, 330)], INK, 6)
    # 뒷손: 큰 쇠망치를 치켜든다
    stroke(img, [(214, 356), (30, 40)], (150, 100, 58, 255), 18)
    ang = math.atan2(40 - 356, 30 - 214)
    ux, uy = math.cos(ang), math.sin(ang)
    nx, ny = -uy, ux
    cx, cy = 30 + ux * 10, 40 + uy * 10
    hw, hl = 50, 95
    head = [(cx + nx * hl + ux * hw, cy + ny * hl + uy * hw), (cx - nx * hl + ux * hw, cy - ny * hl + uy * hw),
            (cx - nx * hl - ux * hw, cy - ny * hl - uy * hw), (cx + nx * hl - ux * hw, cy + ny * hl - uy * hw)]
    poly(img, head, (104, 108, 118, 255), line=8)
    img.alpha_composite(back_hand)
    # 뚱뚱하게: 가로로 넓힌다
    img = img.resize((round(W * STRETCH["Brute"]), H), Image.LANCZOS)
    return img


if __name__ == "__main__":
    for name, fn in (("Archer", archer), ("Shield", shield), ("Brute", brute)):
        im = fn()
        box = im.getbbox()
        im = im.crop(box)
        sheet = Image.new("RGBA", (im.width + MARGIN * 2, im.height + MARGIN * 2), (255, 255, 255, 255))
        sheet.alpha_composite(im, (MARGIN, MARGIN))
        sheet.convert("RGB").save(os.path.join(OUT, f"{name}.png"))
        print(name, "잘린 위치", box[:2])
