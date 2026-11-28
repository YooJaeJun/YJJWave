"""YJJWave 그림을 Pillow로 만든다 → Content/YJJWave/Art/*.png (게임이 실행 중에 바로 읽는다).

  Units.png      128px 칸, 32열. 줄 = Units.csv AtlasRow, 열 0~7 걷기 8~13 공격 14~15 맞음 16~17 헤벌레(밀려 넘어짐) 18 시체 19~22 숨쉬기. 15번 줄 = 투사체
  Fx.png         128px 칸: 0 그림자 1~4 피 5 그을음 6 먼지 7 고리 8 운석 9 폭발
  Buildings.png  256px 칸: 0 졸병 막사 1 궁수 초소 2 방패 막사 3 대장간 4 빈 칸 5 성벽 조각
  UI.png         128px 칸: 0 금화 1 영혼 2 칼 3 해골 4 유성 5 뿔나팔 6 별 7 큰 별 8 성벽 9~12 병영 아이콘 13 패널 14 어두운 패널 15 카드
  Ground.png     전장 바닥 (피·시체는 게임이 그 위에 굽는다)

칸 번호는 Content/YJJWave/Data/UILayout.csv 와 약속이다.
그림 방향: 냥코 대전쟁 + 졸라맨. 굵은 고동 먹선, 흰 몸, 동그란 머리, 점 눈, 멍한 얼굴.
실행: Tools/Art/BuildArt.ps1
"""
import math
import os
import random
import sys

from PIL import ImageChops, Image, ImageDraw, ImageFilter

import rig   # AI 원화 유닛 애니메이션 (영웅·베테랑·해골왕)

SS = 4                       # 크게 그린 뒤 줄인다
INK = (46, 28, 22, 255)      # 먹선
# 인자: .png 이름은 그것만 다시 만들기, 그 밖의 것은 저장 폴더
_OUT_ARGS = [a for a in sys.argv[1:] if not a.lower().endswith(".png")]
OUT = _OUT_ARGS[0] if _OUT_ARGS else os.path.join(os.path.dirname(__file__), "..", "..", "Content", "YJJWave", "Art")

UNIT_CELL = 128
UNIT_COLS = 32
UNIT_ROWS = 20
PROJECTILE_ROW = 19
FOOT = 0.92


def rgba(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def shade(c, k):
    return (int(c[0] * k), int(c[1] * k), int(c[2] * k), c[3])


class Pen:
    """완성 크기 좌표로 그리고 SS배 캔버스에 칠한다. 모든 도형에 먹선."""

    def __init__(self, w, h, lw=3.0):
        self.img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)
        self.lw = lw

    def s(self, v):
        return v * SS

    def ellipse(self, cx, cy, rx, ry, fill, line=True):
        box = [self.s(cx - rx), self.s(cy - ry), self.s(cx + rx), self.s(cy + ry)]
        if line:
            o = self.s(self.lw)
            self.d.ellipse([box[0] - o, box[1] - o, box[2] + o, box[3] + o], fill=INK)
        self.d.ellipse(box, fill=fill)

    def poly(self, pts, fill, line=True):
        p = [(self.s(x), self.s(y)) for x, y in pts]
        if line:
            self.d.line(p + [p[0]], fill=INK, width=int(self.s(self.lw) * 2), joint="curve")
            for x, y in p:
                r = self.s(self.lw)
                self.d.ellipse([x - r, y - r, x + r, y + r], fill=INK)
        self.d.polygon(p, fill=fill)

    def rect(self, x0, y0, x1, y1, fill, radius=0, line=True):
        if line:
            o = self.s(self.lw)
            self.d.rounded_rectangle([self.s(x0) - o, self.s(y0) - o, self.s(x1) + o, self.s(y1) + o], radius=self.s(radius) + o, fill=INK)
        self.d.rounded_rectangle([self.s(x0), self.s(y0), self.s(x1), self.s(y1)], radius=self.s(radius), fill=fill)

    def stick(self, x0, y0, x1, y1, width, fill=INK, line=True):
        if line and fill != INK:
            self.d.line([self.s(x0), self.s(y0), self.s(x1), self.s(y1)], fill=INK, width=int(self.s(width + self.lw * 2)))
            for x, y in ((x0, y0), (x1, y1)):
                r = self.s(width / 2 + self.lw)
                self.d.ellipse([self.s(x) - r, self.s(y) - r, self.s(x) + r, self.s(y) + r], fill=INK)
        self.d.line([self.s(x0), self.s(y0), self.s(x1), self.s(y1)], fill=fill, width=int(self.s(width)))
        for x, y in ((x0, y0), (x1, y1)):
            r = self.s(width / 2)
            self.d.ellipse([self.s(x) - r, self.s(y) - r, self.s(x) + r, self.s(y) + r], fill=fill)

    def arc(self, cx, cy, r, a0, a1, width, fill):
        box = [self.s(cx - r), self.s(cy - r), self.s(cx + r), self.s(cy + r)]
        self.d.arc(box, a0, a1, fill=INK, width=int(self.s(width + self.lw * 2)))
        self.d.arc(box, a0, a1, fill=fill, width=int(self.s(width)))

    def dot(self, x, y, r, fill=INK):
        self.d.ellipse([self.s(x - r), self.s(y - r), self.s(x + r), self.s(y + r)], fill=fill)

    def done(self, w, h):
        return self.img.resize((w, h), Image.LANCZOS)


# ------------------------------------------------------------------------------------------------
# 유닛

# 병영 병사: 고양이 입 (맞으면 울먹, 디버프면 헤벌레). 상위 병사·영웅: 열혈 (입을 크게 벌린 기합)
CAT_FACES = dict(walk="cat", idle="cat", attack="cat", hit="cry", dazed="derp")
HOTBLOOD_FACES = dict(walk="hotblood", idle="hotblood", attack="shout", hit="cry", dazed="derp")

UNITS = [
    # 줄, 몸색, 머리색, 모자, 무기, 크기, 특징
    dict(name="Grunt", body="#FFFFFF", head="#FFFFFF", hat="pot", weapon="club", scale=1.0, faces=CAT_FACES),
    dict(name="Archer", body="#FFFFFF", head="#FFFFFF", hat="hood", weapon="bow", scale=1.0, faces=CAT_FACES),
    dict(name="Shield", body="#FFFFFF", head="#FFFFFF", hat="helm", weapon="shield", scale=1.05, faces=CAT_FACES),
    dict(name="Brute", body="#F3D7B0", head="#F6DDBA", hat="beard", weapon="hammer", scale=1.15, faces=CAT_FACES),
    dict(name="Skeleton", body="#E9E4D4", head="#F1ECDC", hat="skull", weapon="bone", scale=1.0),
    dict(name="Imp", body="#E2483A", head="#EA5444", hat="horns", weapon="claw", scale=0.9),
    dict(name="Fatty", body="#8DBF5A", head="#9CCB68", hat="zombie", weapon="none", scale=1.15, fat=True),
    dict(name="BoneArcher", body="#E9E4D4", head="#F1ECDC", hat="skull", weapon="bonebow", scale=1.0),
    dict(name="BoneKing", body="#E9E4D4", head="#F1ECDC", hat="crown", weapon="greatsword", scale=1.0, cape="#6B3FA0"),
    # 9: 내 영웅 (중심). 졸병 출신 대장: 금빛 투구 + 깃털, 빨간 망토, 큰 칼
    dict(name="Hero", body="#FFFFFF", head="#FFFFFF", hat="knight", weapon="greatsword", scale=1.0, cape="#D9453A", faces=HOTBLOOD_FACES),
]
HERO_ROW = 9
# 10: 강령술사 (적). 보라 두건 + 해골 지팡이
UNITS.append(dict(name="Necro", body="#6B3FA0", head="#E9E4D4", hat="necro", weapon="staff", scale=1.0))
# 11~13: 졸병 진급 (졸병 → 베테랑 → 장군 → 영웅). 갈수록 웅장하고 황당하게
#   베테랑: 찌그러진 냄비 투구 + 반창고, 안대, 콧수염, 빨간 목도리, 큰 국자
#   장군: 몸이 통째로 가마솥, 솥뚜껑 투구 + 붉은 깃털, 흰 수염, 금 견장, 냄비 문장 군기
#   영웅: 머리는 졸병 그대로인데 몸은 근육 산, 불타는 냄비 왕관, 뒤로 금빛 후광, 쇠사슬에 매단 가마솥
UNITS.append(dict(name="Veteran", body="#FFFFFF", head="#FFFFFF", hat="dentpot", weapon="ladle", scale=1.05, faces=HOTBLOOD_FACES,
                  extras=("eyepatch", "mustache", "scarf")))
UNITS.append(dict(name="General", body="#5E636E", head="#FFFFFF", hat="lidhelm", weapon="banner", scale=1.0, faces=HOTBLOOD_FACES,
                  build="cauldron", cape="#8E2A22", extras=("longbeard", "epaulets")))
UNITS.append(dict(name="GruntHero", body="#FFFFFF", head="#FFFFFF", hat="flamecrown", weapon="potflail", scale=1.0, faces=HOTBLOOD_FACES,
                  build="muscle", cape="#F2C230", extras=("halo",)))
# 14~15: 적 진지 (AI 원화: 뼈 천막 = 일반, 해골 망루 = 큰 진지). 움직이지 않고, 부서지면 납작한 잔해
UNITS.append(dict(name="BoneTent", body="#FFFFFF", head="#FFFFFF", hat="none", weapon="none", scale=1.0))
UNITS.append(dict(name="BoneTower", body="#FFFFFF", head="#FFFFFF", hat="none", weapon="none", scale=1.0))
# 16·17: 영웅 둘 (Gemini 원화, 리그로 움직인다)
UNITS.append(dict(name="HeroCook", body="#FFFFFF", head="#FFFFFF", hat="none", weapon="none", scale=1.0, faces=HOTBLOOD_FACES))
UNITS.append(dict(name="HeroPot", body="#FFFFFF", head="#FFFFFF", hat="none", weapon="none", scale=1.0, faces=HOTBLOOD_FACES))


# 한 유닛의 그림 칸 (UILayout.csv Anim* 와 같아야 한다). kind는 표정 고르기에 쓴다 (UNITS faces)
def walk_pose(i, n):
    t = i / n * math.tau
    s = math.sin(t)
    return dict(kind="walk", step=(-8 * s, 8 * s), lean=1.5, swing=0.1 * s, bob=abs(math.cos(t)) * 3)


ATTACK_POSES = [
    dict(kind="attack", step=(-2, 2), lean=-2, swing=-0.35),                 # 준비
    dict(kind="attack", step=(-3, 3), lean=-4, swing=-0.6),                  # 크게 들어 올림
    dict(kind="attack", step=(-2, 4), lean=2, swing=0.2, mouth="open"),      # 내리치는 중
    dict(kind="attack", step=(-4, 7), lean=7, swing=1.0, mouth="open"),      # 때림
    dict(kind="attack", step=(-4, 6), lean=5, swing=0.85, mouth="open"),     # 여운
    dict(kind="attack", step=(-3, 4), lean=2, swing=0.4),                    # 돌아옴
]
HIT_POSES = [
    dict(kind="hit", step=(3, -2), lean=-7, swing=-0.2, mouth="open"),
    dict(kind="hit", step=(2, -1), lean=-4, swing=-0.1),
]
DAZED_POSES = [   # 세게 밀려 휘청: 헤벌레
    dict(kind="dazed", step=(5, -4), lean=-9, swing=-0.4, bob=1),
    dict(kind="dazed", step=(-3, 5), lean=-5, swing=-0.25, bob=0),
]
IDLE_POSES = [    # 숨쉬기 4칸
    dict(kind="idle", step=(-2, 2), lean=0, swing=0, bob=0),
    dict(kind="idle", step=(-2, 2), lean=0, swing=0.03, bob=0.7),
    dict(kind="idle", step=(-2, 2), lean=0.5, swing=0.05, bob=1.3),
    dict(kind="idle", step=(-2, 2), lean=0, swing=0.03, bob=0.7),
]
WALK_FRAMES = 8
FRAME_LAYOUT = ([walk_pose(i, WALK_FRAMES) for i in range(WALK_FRAMES)] + ATTACK_POSES + HIT_POSES + DAZED_POSES)   # 0~17
CORPSE_COL = 18
IDLE_COL = 19


def draw_unit(u, pose, dead=False, out=UNIT_CELL, face=None):
    """오른쪽을 보는 졸라맨. pose: step(뒷발,앞발 x), lean(앞으로 기울기), swing(-0.5 들어 올림 ~ 1 때림), bob, eyes, mouth"""
    if u["name"] in ART_UNITS:   # AI 원화 유닛 (영웅·해골왕·베테랑)
        return draw_art_unit(u, pose, dead, out, face)
    p = Pen(UNIT_CELL, UNIT_CELL, lw=3.2)
    k = u["scale"]
    cx = 64
    foot = UNIT_CELL * FOOT
    body = rgba(u["body"])
    head = rgba(u["head"])
    fat = u.get("fat", False)

    # 크기: 머리 크게, 몸 작게 (머리 반지름 ~ 셀의 1/5)
    hr = 24 * k if not fat else 22 * k
    bw = (13 if not fat else 24) * k
    bh = 20 * k
    leg = 14 * k
    build = u.get("build", "")
    if build == "cauldron":     # 장군: 가마솥 몸통
        hr, bw, bh, leg = 21 * k, 30 * k, 26 * k, 12 * k
    elif build == "muscle":     # 영웅: 작은 머리 + 근육 산
        hr, bw, bh, leg = 15 * k, 40 * k, 34 * k, 15 * k
    extras = u.get("extras", ())
    bob = pose.get("bob", 0)
    hip_y = foot - leg - bob
    body_cy = hip_y - bh * 0.45
    head_cy = body_cy - bh * 0.5 - hr * 0.78

    step = pose["step"]
    lean = pose["lean"]

    if "halo" in extras and not dead:   # 뒤로 퍼지는 금빛 후광
        hcx, hcy = cx + lean * 0.8, head_cy + hr * 0.4
        for i in range(12):
            a = math.radians(i * 30 + pose.get("bob", 0) * 4)
            a2 = a + math.radians(9)
            p.poly([(hcx, hcy), (hcx + math.cos(a) * 58, hcy + math.sin(a) * 58), (hcx + math.cos(a2) * 58, hcy + math.sin(a2) * 58)], rgba("#FFE27A", 110), line=False)
        p.d.ellipse([p.s(hcx - 30), p.s(hcy - 30), p.s(hcx + 30), p.s(hcy + 30)], outline=rgba("#FFD23A", 200), width=int(p.s(3)))

    if u.get("cape"):
        p.poly([(cx - 4 + lean, body_cy - bh * 0.4), (cx - 26 * k, foot - 2), (cx + 6, foot - 4)], rgba(u["cape"]))

    # 다리 (막대)
    p.stick(cx - 4 + lean * 0.3, hip_y, cx - 4 + step[0], foot - 2, 5 * k)
    p.stick(cx + 4 + lean * 0.3, hip_y, cx + 4 + step[1], foot - 2, 5 * k)

    # 무기 (뒤쪽 팔)
    wx = cx + bw * 0.6 + lean
    wy = body_cy
    weapon = u["weapon"]
    swing = pose["swing"]

    def club(color, length, thick, head_r):
        ang = math.radians(-100 + 80 * swing)
        ex, ey = wx + math.cos(ang) * length, wy + math.sin(ang) * length
        p.stick(wx, wy, ex, ey, thick, rgba(color))
        if head_r:
            p.ellipse(ex, ey, head_r, head_r, rgba(color))

    # 몸
    if build == "cauldron":
        p.ellipse(cx - bw * 0.95 + lean, body_cy - bh * 0.15, 5 * k, 6 * k, rgba("#4A4E57"))   # 솥 손잡이
        p.ellipse(cx + bw * 0.95 + lean, body_cy - bh * 0.15, 5 * k, 6 * k, rgba("#4A4E57"))
        p.ellipse(cx + lean, body_cy + bh * 0.08, bw, bh * 0.62, body)
        p.rect(cx - bw * 0.92 + lean, body_cy - bh * 0.5, cx + bw * 0.92 + lean, body_cy - bh * 0.3, rgba("#7A808C"), radius=3)   # 솥 테
        p.ellipse(cx + lean - bw * 0.35, body_cy + bh * 0.05, bw * 0.18, bh * 0.2, rgba("#8A909C"), line=False)   # 쇠 광택
        p.stick(cx + lean - bw * 0.1, body_cy + bh * 0.1, cx + lean + bw * 0.25, body_cy + bh * 0.3, 1.6)            # 찌그러진 자국
    elif build == "muscle":
        sy = body_cy - bh * 0.5
        p.stick(cx - bw * 0.4 + lean, sy + 4, cx - bw * 0.55 + lean - 6, body_cy + bh * 0.3, 8 * k, body)   # 뒤팔
        p.poly([(cx - bw * 0.5 + lean, sy), (cx + bw * 0.5 + lean, sy), (cx + bw * 0.22 + lean, body_cy + bh * 0.5),
                (cx - bw * 0.22 + lean, body_cy + bh * 0.5)], body)
        for ox in (-1, 1):   # 가슴
            p.d.arc([p.s(cx + lean + ox * bw * 0.22 - bw * 0.2), p.s(sy + 2), p.s(cx + lean + ox * bw * 0.22 + bw * 0.2), p.s(sy + bh * 0.4)], 20, 160, fill=INK, width=int(p.s(1.8)))
        for row in range(3):   # 복근
            y = body_cy + row * bh * 0.14
            p.stick(cx + lean - bw * 0.12, y, cx + lean - bw * 0.03, y, 1.5)
            p.stick(cx + lean + bw * 0.03, y, cx + lean + bw * 0.12, y, 1.5)
    elif fat:
        p.ellipse(cx + lean, body_cy, bw, bh * 0.85, body)
        p.stick(cx + lean - 8, body_cy - 4, cx + lean - 2, body_cy + 2, 1.5)   # 꿰맨 자국
        p.stick(cx + lean - 2, body_cy - 4, cx + lean - 8, body_cy + 2, 1.5)
    else:
        p.rect(cx - bw * 0.5 + lean, body_cy - bh * 0.5, cx + bw * 0.5 + lean, body_cy + bh * 0.5, body, radius=5 * k)
    if u["hat"] == "skull" or u["hat"] == "crown":
        for i in range(3):   # 갈비뼈
            y = body_cy - bh * 0.25 + i * 5 * k
            p.stick(cx - bw * 0.35 + lean, y, cx + bw * 0.35 + lean, y, 1.6)

    if weapon == "club":
        club("#B07A45", 24 * k, 6 * k, 7 * k)
    elif weapon == "ladle":   # 큰 국자
        ang = math.radians(-100 + 85 * swing)
        ex, ey = wx + math.cos(ang) * 30 * k, wy + math.sin(ang) * 30 * k
        p.stick(wx, wy, ex, ey, 4 * k, rgba("#8B5A2B"))
        p.ellipse(ex + math.cos(ang) * 6, ey + math.sin(ang) * 6, 10 * k, 8 * k, rgba("#AEB5C0"))
        p.ellipse(ex + math.cos(ang) * 6, ey + math.sin(ang) * 6 - 2, 6 * k, 4 * k, rgba("#7E8590"), line=False)
    elif weapon == "banner":   # 냄비 문장 군기 + 금빛 창날
        ang = math.radians(-92 + 55 * swing)
        bx, by = wx - 2, wy + 12
        tx, ty = bx + math.cos(ang) * 64, by + math.sin(ang) * 64
        ux, uy = math.cos(ang), math.sin(ang)
        nx, ny = -uy, ux
        fx0, fy0 = bx + ux * 36, by + uy * 36
        flag = [(fx0, fy0), (tx - ux * 4, ty - uy * 4), (tx - ux * 4 - nx * 30, ty - uy * 4 - ny * 30),
                (fx0 - nx * 24 + ux * 6, fy0 - ny * 24 + uy * 6), (fx0 - nx * 30, fy0 - ny * 30)]
        p.poly(flag, rgba("#C0392B"))
        mx, my2 = (fx0 + tx) * 0.5 - nx * 14, (fy0 + ty) * 0.5 - ny * 14
        p.ellipse(mx, my2 + 2, 7, 5, rgba("#AEB5C0"))   # 냄비 문장
        p.rect(mx - 9, my2 - 3, mx + 9, my2 - 1, rgba("#AEB5C0"), radius=1)
        p.stick(bx, by, tx, ty, 3.6, rgba("#5B3A1E"))
        p.poly([(tx + nx * 4, ty + ny * 4), (tx + ux * 12, ty + uy * 12), (tx - nx * 4, ty - ny * 4)], rgba("#F2C230"))
    elif weapon == "potflail":   # 쇠사슬 끝 가마솥
        hx0, hy0 = cx + bw * 0.42 + lean, body_cy - bh * 0.32
        ang = math.radians(-125 + 115 * swing)
        ex, ey = hx0 + math.cos(ang) * 28, hy0 + math.sin(ang) * 28
        for i in range(1, 6):
            t = i / 6
            p.ellipse(hx0 + (ex - hx0) * t, hy0 + (ey - hy0) * t, 2.2, 2.2, rgba("#8A8F99"))
        p.ellipse(ex, ey + 2, 13, 11, rgba("#3A3D44"))
        p.rect(ex - 13, ey - 9, ex + 13, ey - 5, rgba("#5E636E"), radius=2)
        p.ellipse(ex - 4, ey + 1, 3, 3, rgba("#5E636E"), line=False)
        p.stick(cx + bw * 0.3 + lean, body_cy - bh * 0.42, hx0, hy0, 8 * k, body)   # 앞팔 (주먹 = 사슬 손잡이)
        p.ellipse(hx0, hy0, 6, 6, body)
    elif weapon == "bone":
        club("#F1ECDC", 22 * k, 5 * k, 5 * k)
    elif weapon == "hammer":
        ang = math.radians(-95 + 85 * swing)
        ex, ey = wx + math.cos(ang) * 30 * k, wy + math.sin(ang) * 30 * k
        p.stick(wx, wy, ex, ey, 5 * k, rgba("#9C6B3D"))
        hx, hy = math.cos(ang + math.pi / 2), math.sin(ang + math.pi / 2)
        p.poly([(ex - hx * 14 * k - math.cos(ang) * 6, ey - hy * 14 * k - math.sin(ang) * 6),
                (ex + hx * 14 * k - math.cos(ang) * 6, ey + hy * 14 * k - math.sin(ang) * 6),
                (ex + hx * 14 * k + math.cos(ang) * 8, ey + hy * 14 * k + math.sin(ang) * 8),
                (ex - hx * 14 * k + math.cos(ang) * 8, ey - hy * 14 * k + math.sin(ang) * 8)], rgba("#8A8F99"))
    elif weapon == "greatsword":
        ang = math.radians(-80 + 65 * swing)
        ex, ey = wx + math.cos(ang) * 42, wy + math.sin(ang) * 42
        p.stick(wx, wy, ex, ey, 7, rgba("#C9CED8"))
        p.stick(wx - math.sin(ang) * 9, wy + math.cos(ang) * 9, wx + math.sin(ang) * 9, wy - math.cos(ang) * 9, 4, rgba("#D9A93A"))
    elif weapon in ("bow", "bonebow"):
        color = "#A0683A" if weapon == "bow" else "#F1ECDC"
        bx = wx + 2 + 4 * max(swing, 0)
        p.arc(bx - 8, wy, 16 * k, -70, 70, 3.5, rgba(color))
        p.stick(bx - 8 + 16 * k * math.cos(math.radians(-70)), wy + 16 * k * math.sin(math.radians(-70)),
                bx - 8 + 16 * k * math.cos(math.radians(70)), wy + 16 * k * math.sin(math.radians(70)), 1.2, rgba("#EEEEEE"), line=False)
        if swing > 0.3:
            p.stick(bx - 14, wy, bx + 14, wy, 2, rgba("#7A5230"))
    elif weapon == "shield":
        sx = cx + bw * 0.55 + lean + 5 * max(swing, 0)
        p.ellipse(sx, body_cy + 2, 12 * k, 16 * k, rgba("#B8834C"))
        p.ellipse(sx, body_cy + 2, 4 * k, 4 * k, rgba("#8A8F99"))
    elif weapon == "staff":
        top = wy - 34 * k - 10 * max(swing, 0)
        p.stick(wx, wy + 16, wx + 4 * swing, top, 3.5, rgba("#5B3A1E"))
        p.ellipse(wx + 4 * swing, top - 4, 7 * k, 6 * k, rgba("#F1ECDC"))
        p.dot(wx + 4 * swing - 2, top - 4, 1.4)
        p.dot(wx + 4 * swing + 2, top - 4, 1.4)
        if swing > 0.5:
            p.ellipse(wx + 4 * swing + 10, top - 6, 6, 6, rgba("#C77DFF", 200), line=False)
    elif weapon == "claw":
        for i in range(3):
            a = math.radians(-30 + i * 25 + 20 * swing)
            p.stick(wx, wy, wx + math.cos(a) * 12, wy + math.sin(a) * 12, 2.5, rgba("#FFF4D6"))

    # 머리
    hx = cx + lean * 0.8
    p.ellipse(hx, head_cy, hr, hr * 0.92, head)
    hat = u["hat"]
    if hat == "pot":
        p.poly([(hx - hr * 0.95, head_cy - hr * 0.25), (hx + hr * 0.95, head_cy - hr * 0.25),
                (hx + hr * 0.8, head_cy - hr * 0.95), (hx - hr * 0.8, head_cy - hr * 0.95)], rgba("#9AA2AE"))
        p.stick(hx + hr * 0.9, head_cy - hr * 0.55, hx + hr * 1.35, head_cy - hr * 0.6, 3, rgba("#6E7681"))
    elif hat == "hood":
        p.poly([(hx - hr * 1.05, head_cy + hr * 0.2), (hx - hr * 0.7, head_cy - hr * 0.9), (hx + hr * 0.1, head_cy - hr * 1.25),
                (hx + hr * 0.9, head_cy - hr * 0.7), (hx + hr * 0.5, head_cy - hr * 0.35), (hx - hr * 0.6, head_cy - hr * 0.3)], rgba("#4E9A3E"))
    elif hat == "helm":
        p.poly([(hx - hr, head_cy - hr * 0.1), (hx - hr * 0.85, head_cy - hr * 0.85), (hx, head_cy - hr * 1.1),
                (hx + hr * 0.85, head_cy - hr * 0.85), (hx + hr, head_cy - hr * 0.1)], rgba("#3F74C9"))
        p.stick(hx, head_cy - hr * 1.1, hx, head_cy - hr * 1.45, 3, rgba("#E04A3A"))
    elif hat == "beard":
        p.poly([(hx - hr * 0.45, head_cy + hr * 0.84), (hx + hr * 0.95, head_cy + hr * 0.8), (hx + hr * 0.35, head_cy + hr * 1.4)], rgba("#8B5A2B"))   # 턱수염 (입 아래)
        p.poly([(hx - hr * 0.9, head_cy - hr * 0.5), (hx + hr * 0.9, head_cy - hr * 0.5), (hx, head_cy - hr * 1.0)], rgba("#C0392B"))
    elif hat == "horns":
        p.poly([(hx - hr * 0.6, head_cy - hr * 0.7), (hx - hr * 0.9, head_cy - hr * 1.5), (hx - hr * 0.2, head_cy - hr * 0.9)], rgba("#FFF4D6"))
        p.poly([(hx + hr * 0.3, head_cy - hr * 0.85), (hx + hr * 0.7, head_cy - hr * 1.55), (hx + hr * 0.8, head_cy - hr * 0.6)], rgba("#FFF4D6"))
    elif hat == "zombie":
        p.stick(hx - hr * 0.4, head_cy - hr * 0.9, hx - hr * 0.1, head_cy - hr * 1.3, 2.5, rgba("#5B3A1E"))
        p.stick(hx + hr * 0.1, head_cy - hr * 0.95, hx + hr * 0.3, head_cy - hr * 1.35, 2.5, rgba("#5B3A1E"))
    elif hat == "necro":
        p.poly([(hx - hr * 1.05, head_cy + hr * 0.35), (hx - hr * 0.8, head_cy - hr * 0.9), (hx + hr * 0.05, head_cy - hr * 1.45),
                (hx + hr * 0.95, head_cy - hr * 0.75), (hx + hr * 0.75, head_cy - hr * 0.3), (hx - hr * 0.55, head_cy - hr * 0.35)], rgba("#4B2A78"))
    elif hat == "knight":
        p.poly([(hx - hr, head_cy - hr * 0.05), (hx - hr * 0.9, head_cy - hr * 0.8), (hx, head_cy - hr * 1.12),
                (hx + hr * 0.9, head_cy - hr * 0.8), (hx + hr, head_cy - hr * 0.05)], rgba("#F2C230"))
        p.rect(hx - hr * 0.12, head_cy - hr * 1.1, hx + hr * 0.12, head_cy - hr * 0.3, rgba("#D9A93A"), radius=2)
        p.poly([(hx - hr * 0.1, head_cy - hr * 1.1), (hx - hr * 0.9, head_cy - hr * 1.7), (hx + hr * 0.2, head_cy - hr * 1.35)], rgba("#E04A3A"))
    elif hat == "dentpot":   # 찌그러진 냄비 + 반창고
        p.poly([(hx - hr * 0.95, head_cy - hr * 0.25), (hx + hr * 0.95, head_cy - hr * 0.25), (hx + hr * 0.8, head_cy - hr * 0.95),
                (hx + hr * 0.15, head_cy - hr * 0.95), (hx - hr * 0.05, head_cy - hr * 0.72), (hx - hr * 0.3, head_cy - hr * 0.95),
                (hx - hr * 0.8, head_cy - hr * 0.95)], rgba("#8C939E"))
        p.stick(hx + hr * 0.9, head_cy - hr * 0.55, hx + hr * 1.35, head_cy - hr * 0.6, 3, rgba("#6E7681"))
        p.rect(hx + hr * 0.3, head_cy - hr * 0.82, hx + hr * 0.7, head_cy - hr * 0.62, rgba("#F3D7B0"), radius=2)
        p.rect(hx + hr * 0.42, head_cy - hr * 0.94, hx + hr * 0.58, head_cy - hr * 0.5, rgba("#F3D7B0"), radius=2)
    elif hat == "lidhelm":   # 솥뚜껑 투구 + 붉은 깃털 셋
        for i, (dx, tip) in enumerate(((-0.25, -2.1), (0.05, -2.35), (0.35, -2.05))):
            p.poly([(hx + hr * dx - 3, head_cy - hr * 1.0), (hx + hr * (dx - 0.5), head_cy + hr * (tip - 0.1)),
                    (hx + hr * (dx + 0.15), head_cy + hr * (tip + 0.25)), (hx + hr * dx + 3, head_cy - hr * 1.0)], rgba("#E04A3A" if i != 1 else "#FF6A4A"))
        p.ellipse(hx, head_cy - hr * 0.66, hr * 1.3, hr * 0.42, rgba("#9AA2AE"))
        p.rect(hx - hr * 1.3, head_cy - hr * 0.66, hx + hr * 1.3, head_cy - hr * 0.5, rgba("#D9A93A"), radius=3)
        p.ellipse(hx, head_cy - hr * 1.06, hr * 0.2, hr * 0.14, rgba("#F2C230"))
    elif hat == "flamecrown":   # 불타는 냄비 왕관
        flick = pose.get("bob", 0) * 0.08 + pose.get("swing", 0) * 0.05
        for i, (dx, h) in enumerate(((-0.6, 1.5), (-0.15, 2.1 + flick), (0.35, 1.8 - flick), (0.75, 1.3))):
            p.poly([(hx + hr * (dx - 0.32), head_cy - hr * 0.8), (hx + hr * (dx - 0.1), head_cy - hr * (0.8 + h * 0.6)),
                    (hx + hr * dx, head_cy - hr * (0.8 + h)), (hx + hr * (dx + 0.32), head_cy - hr * 0.8)], rgba("#FF7A2A"))
            p.poly([(hx + hr * (dx - 0.15), head_cy - hr * 0.8), (hx + hr * dx, head_cy - hr * (0.8 + h * 0.6)),
                    (hx + hr * (dx + 0.15), head_cy - hr * 0.8)], rgba("#FFE46A"), line=False)
        p.rect(hx - hr * 1.0, head_cy - hr * 0.95, hx + hr * 1.0, head_cy - hr * 0.4, rgba("#F2C230"), radius=3)
        for i in range(3):
            p.ellipse(hx - hr * 0.55 + i * hr * 0.55, head_cy - hr * 0.68, hr * 0.12, hr * 0.12, rgba("#E04A3A"))
    elif hat == "crown":
        pts = [(hx - hr * 0.8, head_cy - hr * 0.55)]
        for i in range(5):
            x = hx - hr * 0.8 + i * hr * 0.4
            pts += [(x + hr * 0.2, head_cy - hr * 1.35), (x + hr * 0.4, head_cy - hr * 0.85)]
        pts[-1] = (hx + hr * 0.8, head_cy - hr * 0.55)
        p.poly(pts, rgba("#F2C230"))

    # 얼굴: 점 눈 (해골은 큰 구멍)
    ex = hx + hr * 0.25
    ey = head_cy + hr * 0.05
    if pose.get("kind") == "hit" and not u.get("faces") and not face and hat not in ("skull", "crown", "horns") and not dead:
        for ox, d in ((-hr * 0.28, 1), (hr * 0.28, -1)):
            p.stick(ex + ox - 3 * d, ey - 3, ex + ox + 3 * d, ey, 2)
            p.stick(ex + ox + 3 * d, ey, ex + ox - 3 * d, ey + 3, 2)
        p.ellipse(ex, ey + hr * 0.45, 3.5, 3, rgba("#7A2A20"), line=False)
    elif dead:
        for ox in (-hr * 0.28, hr * 0.28):
            p.stick(ex + ox - 3, ey - 3, ex + ox + 3, ey + 3, 2)
            p.stick(ex + ox - 3, ey + 3, ex + ox + 3, ey - 3, 2)
    elif hat == "crown":
        draw_boss_face(p, hx, head_cy, hr, pose)
    elif hat in ("skull", "crown"):
        p.dot(ex - hr * 0.3, ey, hr * 0.2)
        p.dot(ex + hr * 0.3, ey, hr * 0.2)
        p.stick(ex - hr * 0.2, ey + hr * 0.45, ex + hr * 0.3, ey + hr * 0.45, 1.6)
    elif hat == "horns":
        p.dot(ex - hr * 0.25, ey, 3.2, rgba("#FFF27A"))
        p.dot(ex + hr * 0.3, ey, 3.2, rgba("#FFF27A"))
        p.dot(ex - hr * 0.25, ey, 1.6)
        p.dot(ex + hr * 0.3, ey, 1.6)
    elif (face or u.get("faces")) and not dead:
        draw_face(p, hx, head_cy, hr, face or u["faces"].get(pose.get("kind", "idle"), "cat"), pose)
    else:
        p.dot(ex - hr * 0.28, ey, 2.6)
        p.dot(ex + hr * 0.28, ey, 2.6)
        if pose.get("mouth") == "open":
            p.ellipse(ex, ey + hr * 0.42, 3.5, 3, rgba("#7A2A20"), line=False)   # 기합 입
        else:
            p.stick(ex - 3, ey + hr * 0.42, ex + 3, ey + hr * 0.42, 1.6)

    # 덧붙이는 것 (얼굴 위)
    fx = hx + hr * 0.18
    fy = head_cy + hr * 0.18
    gap = hr * 0.46
    if "eyepatch" in extras and not dead:
        p.stick(fx + gap, fy, hx - hr * 0.9, head_cy - hr * 0.35, 1.8)
        p.ellipse(fx + gap, fy, hr * 0.2, hr * 0.17, INK, line=False)
    if "mustache" in extras:
        for d in (-1, 1):
            p.poly([(fx, fy + hr * 0.2), (fx + d * hr * 0.36, fy + hr * 0.13), (fx + d * hr * 0.56, fy - hr * 0.02),
                    (fx + d * hr * 0.5, fy + hr * 0.2), (fx + d * hr * 0.14, fy + hr * 0.28)], rgba("#4A2E1E"))
    if "longbeard" in extras:
        p.poly([(fx - hr * 0.5, head_cy + hr * 0.86), (fx + hr * 0.55, head_cy + hr * 0.84), (fx + hr * 0.2, head_cy + hr * 1.9),
                (fx, head_cy + hr * 1.6), (fx - hr * 0.2, head_cy + hr * 1.95)], rgba("#F4F1E8"))
        for d in (-1, 1):   # 팔자 콧수염 (입 위)
            p.poly([(fx, fy + hr * 0.26), (fx + d * hr * 0.55, fy + hr * 0.2), (fx + d * hr * 0.78, fy + hr * 0.04),
                    (fx + d * hr * 0.6, fy + hr * 0.34), (fx + d * hr * 0.2, fy + hr * 0.36)], rgba("#F4F1E8"))
    if "epaulets" in extras:
        for d in (-1, 1):
            p.ellipse(cx + lean + d * bw * 0.72, body_cy - bh * 0.45, 7 * k, 4.5 * k, rgba("#F2C230"))
    if "scarf" in extras:
        ny = body_cy - bh * 0.45
        p.poly([(cx - bw * 0.55 + lean, ny - 2), (cx + bw * 0.55 + lean, ny - 2), (cx + bw * 0.5 + lean, ny + 4), (cx - bw * 0.5 + lean, ny + 4)], rgba("#D9453A"))
        p.poly([(cx - bw * 0.4 + lean, ny + 2), (cx - bw * 1.6 + lean - pose.get("bob", 0), ny + 6), (cx - bw * 1.3 + lean, ny + 12), (cx - bw * 0.2 + lean, ny + 6)], rgba("#D9453A"))

    return p.done(out, out)


MOUTH = rgba("#5A1E18")
TONGUE = rgba("#F27A7A")
BLUSH = rgba("#FF9A9A", 150)
SWEAT = rgba("#8FD3FF")


def draw_face(p, hx, hy, hr, style, pose):
    """냥코풍 톤 얼굴: 머리 아래쪽에 멀리 떨어진 작은 점 눈, 표정은 입이 다 한다.
    style: blank 멍 / shout 함성 / smug 뻔뻔 / derp 헤벌레 / hotblood 열혈 / cry 울먹 / shock 벙찜 / cat 오메가 입"""
    ex = hx + hr * 0.18          # 오른쪽을 보므로 얼굴이 살짝 오른쪽
    ey = hy + hr * 0.18          # 눈은 머리 가운데보다 아래
    gap = hr * 0.46              # 눈 사이가 멀다
    my = ey + hr * 0.40          # 입 높이
    eye = hr * 0.15
    attack = pose.get("mouth") == "open"

    def dots(r=eye):
        p.dot(ex - gap, ey, r)
        p.dot(ex + gap, ey, r)

    def omega(y, w):
        # ω 입: 작은 반원 두 개
        for ox in (-w, w):
            p.d.arc([p.s(ex + ox - w), p.s(y - w), p.s(ex + ox + w), p.s(y + w)], 0, 180, fill=INK, width=int(p.s(2.4)))

    def open_mouth(w, h, tongue=True):
        p.ellipse(ex, my + h * 0.3, w, h, MOUTH, line=False)
        if tongue:
            p.ellipse(ex, my + h * 0.75, w * 0.6, h * 0.38, TONGUE, line=False)

    if style == "blank":
        dots()
        if attack:
            open_mouth(hr * 0.28, hr * 0.3)
        else:
            omega(my, hr * 0.14)
    elif style == "shout":
        dots()
        # 얼굴 절반을 차지하는 함성 입 (냥코 기본 고양이의 '와아' 느낌)
        open_mouth(hr * (0.42 if attack else 0.34), hr * (0.42 if attack else 0.32))
    elif style == "smug":
        for ox in (-gap, gap):   # 반쯤 감은 눈
            p.d.arc([p.s(ex + ox - eye * 2.2), p.s(ey - eye * 1.4), p.s(ex + ox + eye * 2.2), p.s(ey + eye * 1.8)], 200, 340, fill=INK, width=int(p.s(1.8)))
        p.d.arc([p.s(ex - hr * 0.16), p.s(my - hr * 0.18), p.s(ex + hr * 0.16), p.s(my + hr * 0.06)], 20, 160, fill=INK, width=int(p.s(1.8)))
        for ox in (-gap * 1.25, gap * 1.25):
            p.ellipse(ex + ox, ey + hr * 0.22, hr * 0.13, hr * 0.07, BLUSH, line=False)
        if attack:
            open_mouth(hr * 0.16, hr * 0.14, tongue=False)
    elif style == "derp":
        p.dot(ex - gap, ey, eye * 1.5)
        p.dot(ex + gap, ey + eye * 0.6, eye * 0.7)
        open_mouth(hr * 0.12, hr * (0.2 if attack else 0.12))
        p.stick(ex + hr * 0.06, my + hr * 0.2, ex + hr * 0.08, my + hr * 0.42, 2.2, SWEAT, line=False)   # 침
    elif style == "hotblood":
        dots(eye * 1.1)
        for ox, d in ((-gap, 1), (gap, -1)):   # 치켜 올린 굵은 눈썹
            p.stick(ex + ox - hr * 0.2 * d, ey - hr * 0.32, ex + ox + hr * 0.16 * d, ey - hr * 0.18, 3.4)
        # 윗변이 평평한 D자 입을 크게 벌린 기합 (공격 때 더 크게)
        w = hr * (0.34 if attack else 0.27)
        h = hr * (0.42 if attack else 0.3)
        top = my - hr * 0.06
        p.d.chord([p.s(ex - w), p.s(top - h), p.s(ex + w), p.s(top + h)], 0, 180, fill=MOUTH, outline=INK, width=int(p.s(1.8)))
        p.d.chord([p.s(ex - w * 0.6), p.s(top + h * 0.35), p.s(ex + w * 0.6), p.s(top + h * 1.05)], 180, 360, fill=TONGUE)
    elif style == "cry":
        for ox, d in ((-gap, 1), (gap, -1)):   # > <
            p.stick(ex + ox - 3 * d, ey - 3, ex + ox + 3 * d, ey, 1.9)
            p.stick(ex + ox + 3 * d, ey, ex + ox - 3 * d, ey + 3, 1.9)
        zig = [(ex - hr * 0.18 + i * hr * 0.09, my + (hr * 0.05 if i % 2 else -hr * 0.03)) for i in range(5)]
        for a, b in zip(zig, zig[1:]):
            p.stick(a[0], a[1], b[0], b[1], 1.6)
        p.ellipse(hx - hr * 0.7, hy - hr * 0.2, hr * 0.09, hr * 0.14, SWEAT, line=False)
    elif style == "shock":
        for ox in (-gap, gap):   # 흰자 큰 눈 + 작은 눈동자
            p.ellipse(ex + ox, ey, eye * 2.4, eye * 2.6, rgba("#FFFFFF"))
            p.dot(ex + ox + eye * 0.4, ey, eye * 0.7)
        p.ellipse(ex, my + hr * 0.04, hr * 0.08, hr * 0.11, MOUTH, line=False)
        for i in range(3):
            p.stick(hx - hr * 0.95 + i * 3, hy - hr * 0.55 - i * 2, hx - hr * 1.05 + i * 3, hy - hr * 0.35 - i * 2, 1.2)
    elif style == "cat":
        dots(eye * 0.9)
        for ox in (-gap, gap):   # 위로 올라간 짧은 눈썹
            p.stick(ex + ox - eye * 1.3, ey - hr * 0.28, ex + ox + eye * 1.3, ey - hr * 0.34, 2.4)
        if attack:
            open_mouth(hr * 0.3, hr * 0.32)
        else:
            omega(my - hr * 0.02, hr * 0.15)


BOSS_GLOW = rgba("#B44DFF")
BOSS_GLOW_HOT = rgba("#FF4D6A")
BOSS_SOCKET = rgba("#2A1830")
BONE_SHADE = rgba("#CFC6AE")


def draw_boss_face(p, hx, hy, hr, pose):
    """해골왕: 깊은 눈구멍 속 보랏빛 눈, 노려보는 눈썹뼈, 코 구멍, 이빨 턱, 이마의 금.
    공격하면 턱이 떨어지고 눈이 붉게 타오른다, 맞으면 눈이 작아진다"""
    kind = pose.get("kind", "idle")
    attack = kind == "attack" and pose.get("mouth") == "open"
    ex = hx + hr * 0.2
    ey = hy + hr * 0.02
    gap = hr * 0.36
    # 광대·턱 그늘
    p.ellipse(ex, hy + hr * 0.55, hr * 0.62, hr * 0.32, BONE_SHADE, line=False)
    # 눈구멍 (안쪽으로 기운 눈물방울) + 빛나는 눈
    glow = BOSS_GLOW_HOT if attack else BOSS_GLOW
    pupil = hr * (0.13 if attack else 0.06 if kind == "hit" else 0.1)
    for ox, d in ((-gap, 1), (gap, -1)):
        cx = ex + ox
        p.poly([(cx - hr * 0.24, ey - hr * 0.06), (cx + hr * 0.02 * d, ey - hr * 0.2), (cx + hr * 0.24, ey - hr * 0.04),
                (cx + hr * 0.16, ey + hr * 0.2), (cx - hr * 0.16, ey + hr * 0.2)], BOSS_SOCKET)
        p.ellipse(cx + hr * 0.03, ey + hr * 0.03, pupil * 1.9, pupil * 1.9, rgba("#B44DFF", 90), line=False)
        p.ellipse(cx + hr * 0.03, ey + hr * 0.03, pupil, pupil, glow, line=False)
        p.ellipse(cx + hr * 0.01, ey, pupil * 0.4, pupil * 0.4, rgba("#FFFFFF"), line=False)
        # 미간으로 내려 꽂히는 눈썹뼈
        p.stick(cx - hr * 0.26 * d, ey - hr * 0.3, cx + hr * 0.2 * d, ey - hr * 0.16, 3.6)
    # 코 구멍 (거꾸로 된 하트)
    ny = ey + hr * 0.3
    p.poly([(ex - hr * 0.09, ny), (ex, ny + hr * 0.15), (ex + hr * 0.09, ny), (ex, ny + hr * 0.04)], BOSS_SOCKET)
    # 턱: 이빨 줄 (공격하면 아래턱이 떨어져 벌어진다)
    jy = ey + hr * 0.52
    jaw = hr * (0.3 if attack else 0.0)
    tw = hr * 0.44
    if attack:
        p.rect(ex - tw * 0.9, jy, ex + tw * 0.9, jy + jaw, BOSS_SOCKET, radius=3)
    for row, y in enumerate((jy - hr * 0.08, jy + jaw)):
        p.rect(ex - tw, y, ex + tw, y + hr * 0.13, rgba("#FFFDF2"), radius=2)
        for i in range(1, 6):
            x = ex - tw + i * tw * 2 / 6
            p.stick(x, y + hr * 0.01, x, y + hr * 0.12, 1.4)
    # 이마의 금 + 해묵은 이 빠짐
    p.stick(hx - hr * 0.35, hy - hr * 0.5, hx - hr * 0.2, hy - hr * 0.32, 1.8)
    p.stick(hx - hr * 0.2, hy - hr * 0.32, hx - hr * 0.3, hy - hr * 0.18, 1.8)
    p.stick(hx - hr * 0.2, hy - hr * 0.32, hx - hr * 0.06, hy - hr * 0.26, 1.4)



# ------------------------------------------------------------------------------------------------
# AI 원화 유닛 (Tools/Art/AI/Approved, 사용자 승인 시안): 흰 바탕을 지우고, 동작 칸마다 기울기·들썩임만 준다
#   flip: 원화가 왼쪽을 보면 True (아틀라스는 모두 오른쪽을 본다)

ART_UNITS = {
    "Hero": dict(file="Hero.png", flip=False),
    "BoneKing": dict(file="BossBlonde.png", flip=True),
    "Veteran": dict(file="Veteran1.png", flip=False),
    "Grunt": dict(file="Grunt.png", flip=False, shadow=True),
    "Skeleton": dict(file="Skeleton.png", flip=True, shadow=True, holes=0.55),
    # 졸병 변형 (grunt_variants.py 로 만든 원화)
    "Archer": dict(file="Archer.png", flip=False, shadow=True),
    "Shield": dict(file="Shield.png", flip=False, shadow=True),
    "Brute": dict(file="Brute.png", flip=False, shadow=True),
    # Gemini 로 승인 원화를 고쳐 만든 원화 (적은 왼쪽을 보고 그려져 뒤집는다)
    "Imp": dict(file="Imp.png", flip=True, shadow=True),
    "Fatty": dict(file="Fatty.png", flip=True, shadow=True),
    "BoneArcher": dict(file="BoneArcher.png", flip=True, shadow=True),
    "Necro": dict(file="Necro.png", flip=True, shadow=True),
    "General": dict(file="General.png", flip=False, shadow=True),
    "GruntHero": dict(file="GruntHero.png", flip=False, shadow=True),
    "HeroCook": dict(file="HeroCook.png", flip=False, shadow=True),
    "HeroPot": dict(file="HeroPot.png", flip=False, shadow=True),
    "BoneTent": dict(file="CampTent.png", flip=False, building=True),
    "BoneTower": dict(file="CampTower.png", flip=False, building=True),
}
RUBBLE_PIECES = (5, 5)   # 무너진 진지: 원화를 가로 × 세로 몇 조각으로 쪼개나
RUBBLE_JITTER = 0.35     # 조각 경계 흔들림 (칸 비율)
RUBBLE_MIN_PIECE = 0.004 # 이보다 작은 조각은 버린다 (원화 넓이 비율)
RUBBLE_TILT = 35         # 조각 기울기 (도, ±)
RUBBLE_SPREAD = 1.6      # 가운데에서 바깥으로 퍼지는 배율
RUBBLE_SCATTER = 0.22    # 조각마다 좌우로 흩어지는 정도 (원화 긴 변 비율)
RUBBLE_SINK = 0.55       # 떨어진 조각이 아래 무더기에 파묻히는 정도 (조각 높이 비율)
RUBBLE_MAX_H = 0.38      # 무더기 최대 높이 (원화 높이 비율)
RUBBLE_TINT = (175, 165, 158)   # 잔해 색 (곱하기, 조금 어둡게)
ART_FILL = 0.84          # 칸 높이 중 그림이 차지하는 비율 (발은 FOOT)
ART_LEAN = 0.8           # 자세 lean 1당 기울기 (도)
ART_SWING = 6.0          # 공격 swing 1당 앞으로 숙이는 각도 (도)
ART_BG_MIN = 200         # 이보다 밝고
ART_BG_CHROMA = 24       # 채도가 이보다 낮으면 바탕 (가장자리에서 이어진 곳만)
ART_SHADOW_MIN = 150     # shadow 원화: 이만큼 밝은 회색까지 바탕 (그려진 바닥 그림자 지우기)
ART_FRINGE = 3           # 바탕 둘레 몇 픽셀까지 흰 섞임을 뺀다
ART_HOLE_MIN = 1500      # holes 원화: 이보다 큰 갇힌 흰 덩어리는 바탕 (holes 높이 비율 아래만)
_art_cache = {}


def _defringe(img, cleared):
    """바탕을 지운 자리 둘레 몇 픽셀: 흰 바탕이 반쯤 섞인 윤곽 픽셀에서 흰색을 투명으로 뺀다 (흰 테두리 없애기).
    색 c = 원래색 × a + 흰색 × (1 - a) 로 보고, 가장 어두운 채널로 a 를 구한다"""
    w, h = img.size
    px = img.load()
    near = bytearray(w * h)
    edge = [i for i in range(w * h) if cleared[i] and px[i % w, i // w][3] == 0]
    for _ in range(ART_FRINGE):
        grow = []
        for i in edge:
            x, y = i % w, i // w
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                j = ny * w + nx
                if 0 <= nx < w and 0 <= ny < h and not near[j] and px[nx, ny][3]:
                    near[j] = 1
                    grow.append(j)
        for j in grow:
            x, y = j % w, j // w
            r, g, b, a = px[x, y]
            alpha = (255 - min(r, g, b)) / 255.0
            if alpha <= 0.02:
                px[x, y] = (0, 0, 0, 0)
                continue
            un = lambda c: max(0, min(255, round((c - 255 * (1 - alpha)) / alpha)))
            px[x, y] = (un(r), un(g), un(b), round(a * alpha))
        edge = grow


def art_source(name):
    """원화를 읽어 바탕을 지우고 그림 둘레로 잘라 둔다"""
    if name in _art_cache:
        return _art_cache[name]
    from collections import deque
    spec = ART_UNITS[name]
    img = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "AI", "Approved", spec["file"])).convert("RGBA")
    w, h = img.size
    px = img.load()
    seen = bytearray(w * h)
    # shadow: 원화에 그려진 회색 바닥 그림자도 바탕으로 (게임이 그림자를 따로 그린다)
    bg_min = ART_SHADOW_MIN if spec.get("shadow") else ART_BG_MIN
    q = deque([(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)] + [(0, y) for y in range(h)] + [(w - 1, y) for y in range(h)])
    while q:
        x, y = q.popleft()
        if x < 0 or y < 0 or x >= w or y >= h or seen[y * w + x]:
            continue
        seen[y * w + x] = 1
        r, g, b, _ = px[x, y]
        if min(r, g, b) < bg_min or max(r, g, b) - min(r, g, b) > ART_BG_CHROMA:
            continue
        px[x, y] = (0, 0, 0, 0)
        q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    if spec.get("clear_all_white"):
        # 흰 부분이 없는 그림(바위 등): 가시 사이에 갇힌 흰 바탕까지 지운다
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                if a and min(r, g, b) >= ART_BG_MIN and max(r, g, b) - min(r, g, b) <= ART_BG_CHROMA:
                    px[x, y] = (0, 0, 0, 0)
    if spec.get("holes"):
        # 몸 사이에 갇힌 흰 바탕 (가장자리와 안 이어진 큰 흰 덩어리) 도 지운다
        # holes = 이 높이 비율 아래만 (머리의 흰 하이라이트는 남긴다)
        for y0 in range(int(h * spec["holes"]), h):
            for x0 in range(w):
                if seen[y0 * w + x0] or not px[x0, y0][3]:
                    continue
                r, g, b, _ = px[x0, y0]
                if min(r, g, b) < ART_BG_MIN or max(r, g, b) - min(r, g, b) > ART_BG_CHROMA:
                    continue
                blob = []
                q = deque([(x0, y0)])
                seen[y0 * w + x0] = 1
                while q:
                    x, y = q.popleft()
                    blob.append((x, y))
                    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                        if 0 <= nx < w and 0 <= ny < h and not seen[ny * w + nx]:
                            r, g, b, a = px[nx, ny]
                            if a and min(r, g, b) >= ART_BG_MIN and max(r, g, b) - min(r, g, b) <= ART_BG_CHROMA:
                                seen[ny * w + nx] = 1
                                q.append((nx, ny))
                if len(blob) >= ART_HOLE_MIN:
                    for x, y in blob:
                        px[x, y] = (0, 0, 0, 0)
    _defringe(img, seen)
    img = img.crop(img.getbbox())
    if spec["flip"]:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    _art_cache[name] = img
    return img


def draw_art_unit(u, pose, dead=False, out=UNIT_CELL, face=None):
    src = art_source(u["name"])
    scale = min(out * ART_FILL / src.size[1], out * 0.96 / src.size[0])
    fig = src.resize((max(1, round(src.size[0] * scale)), max(1, round(src.size[1] * scale))), Image.LANCZOS)
    tilt = pose.get("lean", 0) * ART_LEAN + max(pose.get("swing", 0), 0) * ART_SWING
    cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
    foot = out * FOOT - pose.get("bob", 0) * out / UNIT_CELL
    cell.paste(fig, (round((out - fig.size[0]) / 2), round(foot - fig.size[1])), fig)
    if tilt:
        cell = cell.rotate(-tilt, resample=Image.BICUBIC, center=(out / 2, out * FOOT))
    return cell




def rubble(u, out):
    """무너진 진지: 원화를 조각내 아래 조각부터 바닥에 떨어뜨려 쌓는다 (기울고 흩어진 무더기)"""
    whole = draw_art_unit(u, IDLE_POSES[0], out=out)
    box = whole.getbbox()
    src = whole.crop(box)
    w, h = src.size
    rnd = random.Random(u["name"])
    cols, rows = RUBBLE_PIECES

    def jitter(i, n, size):
        return size * i / n + (rnd.uniform(-RUBBLE_JITTER, RUBBLE_JITTER) * size / n if 0 < i < n else 0)

    grid = [[(jitter(c, cols, w), jitter(r, rows, h)) for c in range(cols + 1)] for r in range(rows + 1)]
    pieces = []
    for r in range(rows):
        for c in range(cols):
            mask = Image.new("L", src.size, 0)
            ImageDraw.Draw(mask).polygon([grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]], fill=255)
            piece = Image.new("RGBA", src.size, (0, 0, 0, 0))
            piece.paste(src, (0, 0), mask)
            bb = piece.getbbox()
            if bb and (bb[2] - bb[0]) * (bb[3] - bb[1]) > w * h * RUBBLE_MIN_PIECE:
                pieces.append((bb, piece.crop(bb)))
    cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
    ground = int(out * FOOT)
    top = [ground] * out          # 칸마다 지금 무더기 위끝 (y)
    center = box[0] + w / 2
    for bb, chunk in sorted(pieces, key=lambda p: -p[0][3]):   # 아래 조각부터
        # 세로로 긴 조각은 옆으로 눕힌다 (무너진 느낌), 그리고 조금씩 기울인다
        angle = rnd.uniform(-RUBBLE_TILT, RUBBLE_TILT) + (rnd.choice((-90, 90)) if chunk.size[1] > chunk.size[0] else 0)
        rot = chunk.rotate(angle, expand=True, resample=Image.BICUBIC)
        rw, rh = rot.size
        piece_x = box[0] + (bb[0] + bb[2]) / 2
        # 높은 진지(망루)도 옆으로 넓게 무너지게: 흩어지는 폭은 원화 긴 변 기준
        x0 = int(center + (piece_x - center) * RUBBLE_SPREAD + rnd.uniform(-1, 1) * max(w, h) * RUBBLE_SCATTER - rw / 2)
        x0 = min(max(x0, 0), out - rw)
        alpha = rot.getchannel("A").load()
        lows, highs = {}, {}
        for i in range(rw):
            ys = [y for y in range(rh) if alpha[i, y] > 40]
            if ys:
                lows[i], highs[i] = ys[-1], ys[0]
        if not lows:
            continue
        # 떨어져서 처음 닿는 자리 + 조금 파묻힘
        y0 = min(top[x0 + i] - low for i, low in lows.items()) + int(rh * RUBBLE_SINK)
        # 무더기는 원화 높이의 RUBBLE_MAX_H 를 넘지 않는다 (넘치면 더 파묻는다)
        y0 = max(y0, int(ground - h * RUBBLE_MAX_H) - min(highs.values()))
        y0 = min(y0, ground - max(lows.values()))
        cell.alpha_composite(rot, (x0, y0))
        for i, high in highs.items():
            top[x0 + i] = min(top[x0 + i], y0 + high)
    return ImageChops.multiply(cell, Image.new("RGBA", cell.size, RUBBLE_TINT + (255,)))


def corpse(u, out=UNIT_CELL):
    """시체: 쓰러진 모습 (눈 X) + 바닥에 붙게 아래로. 진지는 무너진 무더기"""
    if ART_UNITS.get(u["name"], {}).get("building"):
        return rubble(u, out)
    img = draw_unit(u, IDLE_POSES[0], dead=True, out=out)
    img = img.rotate(90, resample=Image.BICUBIC, expand=False)
    lying = Image.new("RGBA", img.size, (0, 0, 0, 0))
    lying.alpha_composite(img, (0, int(img.size[1] * 0.18)))
    return lying


def projectile(kind):
    p = Pen(UNIT_CELL, UNIT_CELL, lw=2.4)
    y = 64
    shaft = "#8B5A2B" if kind == 0 else "#F1ECDC"
    tip = "#9AA2AE" if kind == 0 else "#D8D0BA"
    p.stick(24, y, 96, y, 4, rgba(shaft))
    p.poly([(96, y - 9), (114, y), (96, y + 9)], rgba(tip))
    p.poly([(20, y - 10), (34, y), (20, y + 10), (28, y)], rgba("#E04A3A" if kind == 0 else "#B0A890"))
    return p.done(UNIT_CELL, UNIT_CELL)


def orb():
    """강령술사 투사체: 보라 구슬 (오른쪽으로 꼬리)"""
    p = Pen(UNIT_CELL, UNIT_CELL, lw=2.4)
    p.poly([(30, 64), (70, 52), (70, 76)], rgba("#C77DFF", 150), line=False)
    p.ellipse(78, 64, 16, 16, rgba("#9B4DFF"))
    p.ellipse(73, 59, 5, 5, rgba("#E6C8FF"), line=False)
    return p.done(UNIT_CELL, UNIT_CELL)


BIG_SUB = 4          # 큰 유닛은 한 칸을 4칸으로 쪼갠 중간 프레임까지 (UILayout BigFrameMul)
BIG_LOGICAL = 24     # 큰 유닛 한 줄의 논리 칸 수 (0~17 동작, 18 시체, 19~22 숨쉬기) (UILayout BigLogicalCols)
ACTIONS = [("walk", 0, 8), ("attack", 8, 6), ("hit", 14, 2), ("dazed", 16, 2), ("corpse", 18, 1), ("idle", 19, 4)]


def action_of(frame):
    for name, first, count in ACTIONS:
        if first <= frame < first + count:
            return name, first, count
    return None, 0, 1


def lerp_pose(a, b, t):
    q = dict(a)
    for key in ("lean", "swing", "bob"):
        q[key] = a.get(key, 0) + (b.get(key, 0) - a.get(key, 0)) * t
    q["step"] = tuple(x + (y - x) * t for x, y in zip(a["step"], b["step"]))
    return q


def old_pose(name, phase):
    """코드 그림 유닛: 동작 칸 사이를 섞어 중간 프레임"""
    keys = {"walk": None, "attack": ATTACK_POSES, "hit": HIT_POSES, "dazed": DAZED_POSES, "idle": IDLE_POSES}[name]
    if keys is None:
        return walk_pose(phase * WALK_FRAMES, WALK_FRAMES)
    x = phase * len(keys)
    i = min(int(x), len(keys) - 1)
    loop = name in ("idle", "dazed")
    j = (i + 1) % len(keys) if loop else min(i + 1, len(keys) - 1)
    return lerp_pose(keys[i], keys[j], x - int(x))


BIG_SHEET_COLS = 12  # 큰 유닛 그림 한 줄 칸 수 (칸은 논리 칸 순서 × 중간 프레임으로 줄줄이)


def big_specs():
    """Units.csv 의 큰 유닛: 이름 → (칸 크기, 중간 프레임 수). 칸은 최대 확대에서 화면 크기에 맞춘다"""
    import csv
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Content", "YJJWave", "Data", "Units.csv")
    with open(path, encoding="utf-8-sig", newline="") as f:
        return {r["Name"]: (int(float(r["BigCell"])), int(r["BigSub"])) for r in csv.DictReader(f) if int(r["BigRow"]) >= 0}


def big_atlas(name):
    """큰 유닛 하나 (Big_이름.png): 논리 칸마다 중간 프레임. 칸 번호 = 논리 칸 × 중간 수 + 중간"""
    cell, sub_count = big_specs()[name]
    total = BIG_LOGICAL * sub_count
    atlas = Image.new("RGBA", (cell * BIG_SHEET_COLS, cell * ((total + BIG_SHEET_COLS - 1) // BIG_SHEET_COLS)), (0, 0, 0, 0))
    u = next(x for x in UNITS if x["name"] == name)
    for frame in range(BIG_LOGICAL):
        action, first, count = action_of(frame)
        if action is None:
            continue
        for sub in range(sub_count):
            phase = (frame - first + sub / sub_count) / count
            if name in rig.RIGS:
                img = rig.frame(name, action, phase, cell)
            elif action == "corpse":
                img = corpse(u, cell)
            else:
                img = draw_unit(u, old_pose(action, phase), out=cell)
            index = frame * sub_count + sub
            atlas.alpha_composite(img, ((index % BIG_SHEET_COLS) * cell, (index // BIG_SHEET_COLS) * cell))
    return atlas


PROP_W, PROP_H = 1024, 256   # Props.png 한 칸 (길목 바위: 가로로 긴 무더기)


def props_atlas():
    """길목 지형 그림 (AI 원화 Approved/Rocks.png, 바탕 지움): 0 = 바위 덤불"""
    ART_UNITS.setdefault("Rocks", dict(file="Rocks.png", flip=False, clear_all_white=True))
    rocks = art_source("Rocks")
    scale = min(PROP_W / rocks.size[0], PROP_H / rocks.size[1])
    fig = rocks.resize((round(rocks.size[0] * scale), round(rocks.size[1] * scale)), Image.LANCZOS)
    atlas = Image.new("RGBA", (PROP_W, PROP_H), (0, 0, 0, 0))
    atlas.paste(fig, ((PROP_W - fig.size[0]) // 2, PROP_H - fig.size[1]), fig)
    return atlas


def units_atlas():
    atlas = Image.new("RGBA", (UNIT_CELL * UNIT_COLS, UNIT_CELL * UNIT_ROWS), (0, 0, 0, 0))
    for row, u in enumerate(UNITS):
        if u["name"] in rig.RIGS:
            # AI 원화 리그 유닛: 칸마다 동작·위상으로 (작은 칸은 중간 프레임 없음)
            for frame in range(BIG_LOGICAL):
                action, first, count = action_of(frame)
                if action is not None:
                    img = rig.frame(u["name"], action, (frame - first) / count, UNIT_CELL)
                    atlas.alpha_composite(img, (frame * UNIT_CELL, row * UNIT_CELL))
            continue
        for col, pose in enumerate(FRAME_LAYOUT):
            atlas.alpha_composite(draw_unit(u, pose), (col * UNIT_CELL, row * UNIT_CELL))
        atlas.alpha_composite(corpse(u), (CORPSE_COL * UNIT_CELL, row * UNIT_CELL))
        for i, pose in enumerate(IDLE_POSES):
            atlas.alpha_composite(draw_unit(u, pose), ((IDLE_COL + i) * UNIT_CELL, row * UNIT_CELL))
    for kind in range(2):
        atlas.alpha_composite(projectile(kind), (kind * UNIT_CELL, PROJECTILE_ROW * UNIT_CELL))
    atlas.alpha_composite(orb(), (2 * UNIT_CELL, PROJECTILE_ROW * UNIT_CELL))
    return atlas


# ------------------------------------------------------------------------------------------------
# 이펙트

def _fx_atlas_base():
    """예전 128px 칸 효과들 (fx_atlas 가 2배로 키워 위쪽 3줄에 둔다)"""
    C = 128
    atlas = Image.new("RGBA", (C * 8, C * 3), (0, 0, 0, 0))
    rnd = random.Random(7)

    def put(i, img):
        atlas.alpha_composite(img, ((i % 8) * C, (i // 8) * C))

    # 0 그림자
    sh = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([10, 30, C - 10, C - 30], fill=(0, 0, 0, 200))
    put(0, sh.filter(ImageFilter.GaussianBlur(8)))

    # 1~4 피 웅덩이
    for i in range(4):
        b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
        d = ImageDraw.Draw(b)
        color = (120 + rnd.randint(0, 30), 10, 14, 230)
        for _ in range(9 + i * 2):
            r = rnd.uniform(10, 26) * SS
            x = C * SS / 2 + rnd.gauss(0, 16) * SS
            y = C * SS / 2 + rnd.gauss(0, 12) * SS
            d.ellipse([x - r, y - r * 0.8, x + r, y + r * 0.8], fill=color)
        for _ in range(6):
            r = rnd.uniform(3, 7) * SS
            a = rnd.uniform(0, math.tau)
            dist = rnd.uniform(36, 54) * SS
            x, y = C * SS / 2 + math.cos(a) * dist, C * SS / 2 + math.sin(a) * dist * 0.8
            d.ellipse([x - r, y - r, x + r, y + r], fill=color)
        put(1 + i, b.resize((C, C), Image.LANCZOS))

    # 5 그을음
    sc = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    d = ImageDraw.Draw(sc)
    for r in range(60, 0, -4):
        d.ellipse([64 - r, 64 - r * 0.75, 64 + r, 64 + r * 0.75], fill=(30, 20, 15, int(30 + (60 - r) * 3)))
    put(5, sc.filter(ImageFilter.GaussianBlur(3)))

    # 6 먼지
    p = Pen(C, C, lw=2.5)
    for x, y, r in ((44, 80, 22), (80, 78, 24), (62, 58, 26), (30, 64, 14), (98, 60, 15)):
        p.ellipse(x, y, r, r * 0.85, rgba("#E8D7B4"))
    put(6, p.done(C, C))

    # 7 고리
    ring = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    d.ellipse([6 * SS, 6 * SS, (C - 6) * SS, (C - 6) * SS], outline=(255, 255, 255, 255), width=7 * SS)
    d.ellipse([16 * SS, 16 * SS, (C - 16) * SS, (C - 16) * SS], outline=(255, 255, 255, 90), width=3 * SS)
    put(7, ring.resize((C, C), Image.LANCZOS))

    # 8 운석 (오른쪽이 앞, 꼬리는 왼쪽)
    p = Pen(C, C, lw=3)
    p.poly([(8, 64), (60, 40), (70, 64), (60, 88)], rgba("#FFB43A"), line=False)
    p.poly([(26, 64), (64, 50), (70, 64), (64, 78)], rgba("#FFF07A"), line=False)
    p.ellipse(86, 64, 24, 22, rgba("#6E4A36"))
    p.ellipse(80, 58, 7, 6, rgba("#8F6A50"), line=False)
    p.ellipse(94, 72, 5, 4, rgba("#523424"), line=False)
    put(8, p.done(C, C))

    # 9 폭발
    p = Pen(C, C, lw=3)
    pts = []
    for i in range(16):
        a = i / 16 * math.tau
        r = 58 if i % 2 == 0 else 34
        pts.append((64 + math.cos(a) * r, 64 + math.sin(a) * r))
    p.poly(pts, rgba("#FF8A2A"))
    pts = [(64 + (x - 64) * 0.6, 64 + (y - 64) * 0.6) for x, y in pts]
    p.poly(pts, rgba("#FFE46A"), line=False)
    put(9, p.done(C, C))

    # 10 갈라진 땅 (세로로 이어 붙인다: 위아래 끝이 가운데에서 만난다)
    p = Pen(C, C, lw=3)
    left = [(64, 0), (54, 18), (62, 34), (48, 52), (58, 70), (46, 88), (56, 106), (64, 128)]
    right = [(64, 128), (72, 106), (66, 88), (80, 70), (68, 52), (78, 34), (66, 18), (64, 0)]
    p.poly(left + right, rgba("#3A2418"))
    p.poly([(62, 8), (58, 30), (54, 52), (52, 76), (54, 100), (62, 120), (64, 100), (62, 76), (62, 52), (64, 30)], rgba("#FF7A2A"), line=False)
    for x0, y0, x1, y1 in ((52, 52, 30, 44), (78, 34, 100, 26), (70, 88, 96, 98), (50, 88, 28, 100)):
        p.stick(x0, y0, x1, y1, 3)
    for x, y, r in ((24, 30, 7), (104, 52, 6), (20, 82, 5), (106, 110, 7)):
        p.ellipse(x, y, r, r * 0.8, rgba("#8C6B45"))
    put(10, p.done(C, C))

    # 11 불꽃 (아래가 바닥)
    p = Pen(C, C, lw=3)
    p.poly([(64, 6), (88, 44), (100, 82), (92, 112), (64, 124), (36, 112), (28, 82), (44, 56), (52, 70), (56, 36)], rgba("#FF7A2A"))
    p.poly([(64, 46), (80, 78), (84, 104), (64, 116), (44, 104), (48, 80), (58, 90)], rgba("#FFE46A"), line=False)
    put(11, p.done(C, C))

    # 12 흰 타원 (칠해 쓰는 국물 면·거품)
    b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    ImageDraw.Draw(b).ellipse([3 * SS, 3 * SS, (C - 3) * SS, (C - 3) * SS], fill=(255, 255, 255, 255))
    put(12, b.resize((C, C), Image.LANCZOS))

    # 13 뼈 (가로)
    p = Pen(C, C, lw=3)
    p.stick(34, 64, 94, 64, 12, rgba("#F1ECDC"))
    for x in (30, 98):
        p.ellipse(x, 56, 11, 10, rgba("#F1ECDC"))
        p.ellipse(x, 72, 11, 10, rgba("#F1ECDC"))
    put(13, p.done(C, C))

    # 14 김 (흰 뭉게, 칠해 쓴다)
    b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(b)
    for x, y, r in ((50, 76, 30), (80, 70, 28), (64, 48, 30), (40, 54, 18), (90, 46, 18)):
        d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS], fill=(255, 255, 255, 170))
    put(14, b.resize((C, C), Image.LANCZOS).filter(ImageFilter.GaussianBlur(2)))

    # 15 진급 빛줄기 (칠해 쓴다)
    b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(b)
    for i in range(10):
        a = i / 10 * math.tau
        a2 = a + 0.16
        d.polygon([(64 * SS, 64 * SS), ((64 + math.cos(a) * 62) * SS, (64 + math.sin(a) * 62) * SS),
                   ((64 + math.cos(a2) * 62) * SS, (64 + math.sin(a2) * 62) * SS)], fill=(255, 255, 255, 200))
    d.polygon([(x * SS, y * SS) for x, y in star_points(64, 64, 30, 12)], fill=(255, 255, 255, 255))
    put(15, b.resize((C, C), Image.LANCZOS))

    # 16 큰 국자 (자루는 위, 국자 머리는 아래: 내려치기)
    put(16, ladle_image(C))

    # 17 붓 초승달: 가운데 굵고 양끝 가늘게, 마른 붓 결 (가는 호를 끊어 가며 겹친다)
    b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(b)
    r0, r1 = 40, 58
    for n in range(26):
        t = n / 25
        r = (r0 + (r1 - r0) * t) * SS
        span = 150 * math.sin(math.pi * (0.15 + 0.7 * t)) ** 0.6
        a = -span / 2
        while a < span / 2:
            seg = rnd.uniform(8, 30)
            if rnd.random() > 0.12 + 0.25 * abs(t - 0.5):
                d.arc([C * SS / 2 - r, C * SS / 2 - r, C * SS / 2 + r, C * SS / 2 + r], a, min(a + seg, span / 2),
                      fill=(255, 248, 228, rnd.randint(170, 245)), width=int(SS * 1.6))
            a += seg + rnd.uniform(0, 4)
    for n in range(3):   # 바깥쪽 먹 테
        r = (r1 + 1.5 + n) * SS
        d.arc([C * SS / 2 - r, C * SS / 2 - r, C * SS / 2 + r, C * SS / 2 + r], -60 + n * 12, 60 - n * 15, fill=(46, 28, 22, 150), width=int(SS * 1.2))
    put(17, b.resize((C, C), Image.LANCZOS).filter(ImageFilter.GaussianBlur(0.4)))

    # 18 먹 튀김: 삐죽한 별 + 먹선 + 튄 방울
    b = Image.new("RGBA", (C * SS, C * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(b)
    pts = []
    for k in range(14):
        a = k / 14 * math.tau + rnd.uniform(-0.12, 0.12)
        rr = (rnd.uniform(40, 58) if k % 2 == 0 else rnd.uniform(16, 24)) * SS
        pts.append((C * SS / 2 + math.cos(a) * rr, C * SS / 2 + math.sin(a) * rr))
    d.line(pts + [pts[0]], fill=(46, 28, 22, 255), width=int(SS * 5), joint="curve")
    d.polygon(pts, fill=(255, 244, 214, 255))
    inner = [(C * SS / 2 + (x - C * SS / 2) * 0.5, C * SS / 2 + (y - C * SS / 2) * 0.5) for x, y in pts]
    d.polygon(inner, fill=(255, 214, 90, 255))
    for k in range(9):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(52, 62) * SS
        s = rnd.uniform(2, 4.5) * SS
        x, y = C * SS / 2 + math.cos(a) * rr, C * SS / 2 + math.sin(a) * rr
        d.ellipse([x - s, y - s, x + s, y + s], fill=(46, 28, 22, 230))
    put(18, b.resize((C, C), Image.LANCZOS))
    return atlas


# ------------------------------------------------------------------------------------------------
# 건물

# 움직이는 효과 (사용자 승인 시안): 먼지 구름 · 타격 터짐 · 붓 휘두르기, 각 FX_ANIM_FRAMES 프레임. 굵은 먹선 + 2톤
FX_CELL = 256                 # Fx.png 칸 (UILayout FxCell)
FX_ANIM_FRAMES = 6            # UILayout FxAnimFrames
FX_ANIM_ROWS = {"dust": 3, "impact": 4, "slash": 5}   # 줄 (칸 번호 = 줄 × 8 + 프레임, UILayout FxDustFirst·FxImpactFirst·FxSlashFirst)
FX_INK = (34, 26, 24, 255)
FX_SS = 3


def _fx_ease_out(t):
    return 1 - (1 - t) ** 3


def _fx_canvas():
    return Image.new("RGBA", (FX_CELL * FX_SS, FX_CELL * FX_SS), (0, 0, 0, 0))


def _fx_done(img, alpha=1.0):
    img = img.resize((FX_CELL, FX_CELL), Image.LANCZOS)
    if alpha < 1.0:
        img.putalpha(img.getchannel("A").point(lambda v: int(v * max(alpha, 0.0))))
    return img


def _fx_circ(d, x, y, r, fill):
    s = FX_SS
    d.ellipse([(x - r) * s, (y - r) * s, (x + r) * s, (y + r) * s], fill=fill)


def fx_dust_frame(f):
    """먼지: 작게 피어올라 부풀고 흙 알갱이가 튄 뒤 옅어진다 (발 자리 = 칸 아래쪽)"""
    C, N = FX_CELL, FX_ANIM_FRAMES
    t = f / (N - 1)
    rnd = random.Random(11)
    img = _fx_canvas()
    d = ImageDraw.Draw(img)
    cx, cy = C / 2, C * 0.62
    blobs = [(rnd.uniform(math.pi * 0.9, math.pi * 2.1), rnd.uniform(10, 34), rnd.uniform(16, 26), rnd.uniform(0.7, 1.3)) for _ in range(8)]
    grow = _fx_ease_out(min(t * 1.6, 1.0))
    fade = max(0.0, (t - 0.55) / 0.45)
    pts = []
    for a, dist, r0, sp in blobs:
        spread = dist * (0.4 + 1.5 * grow) * sp
        pts.append((cx + math.cos(a) * spread * 1.3, cy + math.sin(a) * spread * 0.8 - 26 * grow, r0 * (0.45 + 0.8 * grow) * (1 - 0.55 * fade)))
    lw = 4.5 * (1 - 0.6 * fade)
    for x, y, r in pts:
        _fx_circ(d, x, y, r + lw, FX_INK)
    for x, y, r in pts:
        _fx_circ(d, x, y, r, (196, 170, 132, 255))
    for x, y, r in pts:
        _fx_circ(d, x - r * 0.18, y - r * 0.22, r * 0.78, (240, 226, 200, 255))
    for x, y, r in pts:
        _fx_circ(d, x - r * 0.38, y - r * 0.42, r * 0.22, (255, 250, 238, 255))
    if t < 0.75:
        for k in range(7):
            a = math.pi * (1.05 + 0.9 * k / 6)
            dist = 30 + 120 * _fx_ease_out(t)
            x, y = cx + math.cos(a) * dist, cy + math.sin(a) * dist * 0.7 + 90 * t * t
            r = 6 * (1 - t)
            _fx_circ(d, x, y, r + 2.5, FX_INK)
            _fx_circ(d, x, y, r, (122, 92, 64, 255))
    return _fx_done(img, 1 - fade * 0.85)


FX_BURST_SCALES = (0.35, 0.7, 0.95, 1.05, 1.1, 1.12)   # 터짐 프레임마다 크기 (칸 비율)
FX_BURST_ALPHAS = (1.0, 1.0, 1.0, 0.8, 0.45, 0.15)    # 프레임마다 진하기
FX_BURST_FIT = 0.82


def fx_burst_frame(f):
    """타격 터짐 (Gemini 폭발 구름 그림): 작게 번쩍 → 부풀어 오름 → 옅어지며 사라짐"""
    C = FX_CELL
    cloud = gemini_icons("FxIcons")[2]
    s = C * FX_BURST_FIT * FX_BURST_SCALES[f] / max(cloud.size)
    img = cloud.resize((max(1, round(cloud.size[0] * s)), max(1, round(cloud.size[1] * s))), Image.LANCZOS)
    if FX_BURST_ALPHAS[f] < 1.0:
        img.putalpha(img.getchannel("A").point(lambda v: int(v * FX_BURST_ALPHAS[f])))
    cell = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    cell.alpha_composite(img, ((C - img.size[0]) // 2, (C - img.size[1]) // 2))
    return cell


def fx_slash_frame(f):
    """붓 휘두르기: 오른쪽으로 쓸고 가는 초승달 (왼쪽은 게임이 뒤집는다), 꼬리부터 사라진다"""
    C, N, s = FX_CELL, FX_ANIM_FRAMES, FX_SS
    t = f / (N - 1)
    img = _fx_canvas()
    d = ImageDraw.Draw(img)
    cx = cy = C / 2
    R = 96
    sweep = math.radians(20 + 170 * _fx_ease_out(min(t * 1.4, 1)))
    start = math.radians(-120)
    tail = max(0.0, t - 0.45) / 0.55
    a0, a1 = start + sweep * tail, start + sweep
    steps = 40

    def band(width, out_k, in_k):
        outer, inner = [], []
        for i in range(steps + 1):
            u = i / steps
            a = a0 + (a1 - a0) * u
            w = width * math.sin(math.pi * min(u * 1.1, 1)) ** 0.7 * (1 - 0.6 * tail)
            outer.append(((cx + math.cos(a) * (R + w * out_k)) * s, (cy + math.sin(a) * (R + w * out_k)) * s))
            inner.append(((cx + math.cos(a) * (R - w * in_k)) * s, (cy + math.sin(a) * (R - w * in_k)) * s))
        return outer + inner[::-1]

    d.polygon(band(34, 0.5, 0.5), fill=FX_INK)
    d.polygon(band(22, 0.35, 0.45), fill=(255, 250, 235, 255))
    rnd = random.Random(3)
    for _ in range(5):
        off = rnd.uniform(-10, 10)
        pts = [((cx + math.cos(a0 + (a1 - a0) * i / steps) * (R + off)) * s, (cy + math.sin(a0 + (a1 - a0) * i / steps) * (R + off)) * s) for i in range(steps + 1)]
        d.line(pts[int(steps * 0.25):], fill=(200, 190, 170, 255), width=2 * s)
    if t > 0.2:
        for j in range(4):
            a = a1 + 0.12 + j * 0.08
            dist = R + rnd.uniform(-12, 16) + 30 * t
            _fx_circ(d, cx + math.cos(a) * dist, cy + math.sin(a) * dist, 5 - j, FX_INK)
    return _fx_done(img, 1 - max(0, t - 0.7) / 0.3 * 0.9)


def fx_atlas():
    """Fx.png: 256px 칸 8열. 위 3줄 = 예전 효과(128px 그림을 2배), 아래 3줄 = 움직이는 효과 프레임"""
    base = _fx_atlas_base()
    big = base.resize((base.width * 2, base.height * 2), Image.LANCZOS)
    rows = max(FX_ANIM_ROWS.values()) + 1
    atlas = Image.new("RGBA", (FX_CELL * 8, FX_CELL * rows), (0, 0, 0, 0))
    atlas.alpha_composite(big, (0, 0))
    # Gemini 이펙트 그림 (불꽃·불덩이·폭발·김·빛기둥·그을음)
    fx_icons = gemini_icons("FxIcons")
    for index, cell in GEMINI_FX_CELLS.items():
        cx, cy = (cell % 8) * FX_CELL, (cell // 8) * FX_CELL
        atlas.paste(Image.new("RGBA", (FX_CELL, FX_CELL), (0, 0, 0, 0)), (cx, cy))
        atlas.alpha_composite(_place(fx_icons[index], FX_CELL, GEMINI_FX_FIT, GEMINI_FX_FIT, (1 + GEMINI_FX_FIT) / 2), (cx, cy))
    ladle_cell = 16   # UILayout FxLadle: 국자 내려치기
    lx, ly = (ladle_cell % 8) * FX_CELL, (ladle_cell // 8) * FX_CELL
    atlas.paste(Image.new("RGBA", (FX_CELL, FX_CELL), (0, 0, 0, 0)), (lx, ly))
    atlas.alpha_composite(_place(gemini_icons("Fort")[3], FX_CELL, 0.95, 0.95, 0.97), (lx, ly))
    for name, make in (("dust", fx_dust_frame), ("impact", fx_burst_frame), ("slash", fx_slash_frame)):
        for f in range(FX_ANIM_FRAMES):
            atlas.alpha_composite(make(f), (f * FX_CELL, FX_ANIM_ROWS[name] * FX_CELL))
    return atlas

def building(kind, tier=0):
    """병영 그림. tier 0~3 = 강화 Lv1~4: 돌 기단·깃발 → 옆 망루·문장 → 금 지붕 장식·왕관"""
    C = 256
    p = Pen(C, C, lw=4.5)
    base = C * 0.9
    if tier >= 2 and kind < 4:   # 옆 망루 (건물 뒤)
        p.rect(14, base - 150, 62, base - 6, rgba("#B9B2A6"), radius=4)
        for i in range(3):
            p.rect(10 + i * 18, base - 166, 24 + i * 18, base - 146, rgba("#C9C2B6"), radius=2)
        p.rect(30, base - 120, 46, base - 96, rgba("#3A2418"), radius=6)
    if kind == 4:   # 빈 칸: 흙 + 말뚝 + 망치 표지
        p.ellipse(128, base - 18, 92, 34, rgba("#9B7A52"))
        p.ellipse(128, base - 22, 76, 24, rgba("#B0905F"), line=False)
        for x in (60, 196):
            p.rect(x - 5, base - 70, x + 5, base - 14, rgba("#8B5A2B"), radius=2)
        p.rect(104, base - 96, 152, base - 62, rgba("#E8D7B4"), radius=5)
        p.stick(118, base - 70, 138, base - 88, 4, rgba("#8B5A2B"))
        p.rect(130, base - 94, 146, base - 82, rgba("#8A8F99"), radius=2)
        return p.done(C, C)
    if kind == 5:   # 성벽 조각 (돌 탑)
        p.rect(78, base - 150, 178, base, rgba("#B9B2A6"), radius=6)
        for i in range(3):
            p.rect(74 + i * 38, base - 176, 102 + i * 38, base - 146, rgba("#C9C2B6"), radius=3)
        for row in range(5):
            y = base - 130 + row * 26
            p.stick(82, y, 174, y, 1.8)
            off = 0 if row % 2 == 0 else 22
            for x in range(96 + off, 170, 44):
                p.stick(x, y, x, y + 26 if row < 4 else y + 20, 1.8)
        return p.done(C, C)

    p.ellipse(128, base - 6, 100, 18, rgba("#7E6A48"))  # 바닥 그림자 흙
    if kind == 0:   # 졸병 막사: 나무 오두막, 빨간 지붕
        p.rect(52, base - 110, 204, base - 6, rgba("#C89A62"), radius=6)
        for x in range(64, 200, 24):
            p.stick(x, base - 104, x, base - 10, 1.6)
        p.poly([(36, base - 104), (128, base - 190), (220, base - 104)], rgba("#D9453A"))
        p.rect(110, base - 62, 146, base - 6, rgba("#5E3A1E"), radius=10)
        p.rect(68, base - 90, 96, base - 66, rgba("#F6E7B0"), radius=3)
    elif kind == 1:  # 궁수 초소: 나무 탑 + 초록 깃발
        p.rect(84, base - 150, 172, base - 6, rgba("#B98852"), radius=4)
        p.stick(84, base - 6, 172, base - 150, 3, rgba("#8B5A2B"))
        p.stick(172, base - 6, 84, base - 150, 3, rgba("#8B5A2B"))
        p.rect(66, base - 180, 190, base - 146, rgba("#C89A62"), radius=4)
        p.stick(128, base - 180, 128, base - 236, 4, rgba("#5E3A1E"))
        p.poly([(130, base - 236), (176, base - 222), (130, base - 206)], rgba("#4E9A3E"))
    elif kind == 2:  # 방패 막사: 돌 + 파란 지붕 + 방패 문장
        p.rect(48, base - 116, 208, base - 6, rgba("#B9B2A6"), radius=8)
        p.poly([(34, base - 110), (128, base - 176), (222, base - 110)], rgba("#3F74C9"))
        p.ellipse(128, base - 60, 30, 36, rgba("#B8834C"))
        p.ellipse(128, base - 60, 10, 10, rgba("#8A8F99"))
    elif kind == 3:  # 대장간: 돌, 굴뚝, 모루
        p.rect(44, base - 104, 196, base - 6, rgba("#A39C90"), radius=8)
        p.rect(150, base - 170, 182, base - 96, rgba("#8F887C"), radius=3)
        p.ellipse(172, base - 190, 18, 14, rgba("#BDBDBD"))
        p.ellipse(186, base - 214, 14, 11, rgba("#D6D6D6"))
        p.poly([(30, base - 100), (128, base - 150), (210, base - 100)], rgba("#6E4A36"))
        p.rect(80, base - 64, 140, base - 6, rgba("#3A2418"), radius=8)
        p.ellipse(110, base - 34, 18, 12, rgba("#FF8A2A"), line=False)
        p.rect(186, base - 40, 236, base - 24, rgba("#5A5F69"), radius=3)
        p.rect(200, base - 24, 222, base - 6, rgba("#5A5F69"), radius=2)
    tier_extras(p, tier, base)
    return p.done(C, C)


TIER_BANNER = ["#3F74C9", "#3F74C9", "#D9453A", "#F2C230"]


def tier_extras(p, tier, base):
    """강화 단계 표시: 1 돌 기단 + 깃발, 2 문장 방패, 3 금 장식 + 왕관 (단계가 오를수록 눈에 띄게)"""
    if tier >= 1:
        p.rect(40, base - 14, 216, base + 2, rgba("#9A938A"), radius=3)
        for x in range(52, 212, 26):
            p.stick(x, base - 14, x, base + 2, 1.4)
        p.stick(232, base - 4, 232, base - 120, 4, rgba("#5E3A1E"))
        p.poly([(234, base - 120), (252, base - 110), (234, base - 98)], rgba(TIER_BANNER[tier]))
    if tier >= 2:
        p.poly([(108, base - 132), (148, base - 132), (148, base - 108), (128, base - 92), (108, base - 108)], rgba("#C0392B"))
        p.poly(star_points(128, base - 116, 9, 4), rgba("#F2C230"), line=False)
    if tier >= 3:
        cx, cy = 128, 26
        p.poly([(cx - 34, cy + 22), (cx - 34, cy - 4), (cx - 18, cy + 8), (cx, cy - 14), (cx + 18, cy + 8), (cx + 34, cy - 4), (cx + 34, cy + 22)], rgba("#F2C230"))
        for dx in (-18, 0, 18):
            p.ellipse(cx + dx, cy + 14, 4, 4, rgba("#E04A3A"))
        for x, y, r in ((40, 40, 9), (220, 30, 8), (190, 70, 6)):
            p.poly(star_points(x, y, r, r * 0.4, 4), rgba("#FFE46A"), line=False)


def wall_segment():
    """성벽 한 조각 (세로로 이어 붙인다): 위에서 내려다본 돌 성벽 + 톱니"""
    C = 256
    p = Pen(C, C, lw=4)
    base = C * 0.92
    p.rect(88, base - 170, 168, base, rgba("#A8A196"), radius=4)
    p.rect(96, base - 170, 160, base - 150, rgba("#BDB6AA"), radius=2, line=False)
    for i in range(3):
        p.rect(84 + i * 30, base - 196, 110 + i * 30, base - 166, rgba("#B9B2A6"), radius=3)
    for row in range(6):
        y = base - 140 + row * 24
        p.stick(90, y, 166, y, 1.6)
        off = 0 if row % 2 == 0 else 18
        for x in range(104 + off, 164, 36):
            p.stick(x, y, x, y + 24 if row < 5 else y + 18, 1.6)
    return p.done(C, C)


def gate_tower():
    """성문 옆 둥근 탑: 돌 몸통 + 파란 뾰족 지붕 + 깃발"""
    C = 256
    p = Pen(C, C, lw=4.5)
    base = C * 0.92
    p.rect(78, base - 150, 178, base, rgba("#B9B2A6"), radius=10)
    for row in range(5):
        y = base - 130 + row * 26
        p.stick(80, y, 176, y, 1.8)
    p.rect(114, base - 110, 142, base - 78, rgba("#3A2418"), radius=10)
    p.poly([(64, base - 146), (128, base - 230), (192, base - 146)], rgba("#3F74C9"))
    p.stick(128, base - 230, 128, base - 252, 4, rgba("#5E3A1E"))
    p.poly([(130, base - 252), (162, base - 244), (130, base - 236)], rgba("#D9453A"))
    return p.done(C, C)


# 왕국의 마지막 냄비 (Buildings.png 6 = 솥과 장작불, 7 = 뚜껑). 국물 면은 HUD가 그 위에 칠한다
# 솥 아가리: 가운데 (128, POT_RIM_Y), 반지름 (POT_RIM_RX, POT_RIM_RY) → UILayout.csv PotRim*
POT_RIM_Y = 100
POT_RIM_RX = 86
POT_RIM_RY = 24


def pot_body():
    C = 256
    p = Pen(C, C, lw=4.5)
    base = C * 0.94
    # 다리 (뒤)
    for x, d in ((78, -1), (178, 1)):
        p.stick(x, POT_RIM_Y + 80, x + d * 14, base - 4, 9, rgba("#3A3D44"))
    # 솥 몸통 + 손잡이 + 테 + 아가리 안쪽
    for x in (30, 226):
        p.ellipse(x, POT_RIM_Y + 20, 12, 16, rgba("#3A3D44"))
    p.ellipse(128, POT_RIM_Y + 48, 98, 66, rgba("#4A4E57"))
    p.ellipse(94, POT_RIM_Y + 44, 18, 26, rgba("#626772"), line=False)
    p.ellipse(128, POT_RIM_Y, POT_RIM_RX + 10, POT_RIM_RY + 8, rgba("#6E7480"))
    p.ellipse(128, POT_RIM_Y, POT_RIM_RX, POT_RIM_RY, rgba("#241C18"), line=False)
    p.rect(64, POT_RIM_Y + 40, 192, POT_RIM_Y + 52, rgba("#D9A93A"), radius=4)
    # 장작과 불 (솥 아래, 앞)
    for x0, x1 in ((64, 192), (74, 200)):
        p.stick(x0, base - 4, x1, base - 12, 11, rgba("#7A4A26"))
    p.poly([(88, base - 8), (100, base - 40), (114, base - 24), (128, base - 52), (142, base - 24), (156, base - 42), (168, base - 8)], rgba("#FF7A2A"))
    p.poly([(108, base - 8), (128, base - 36), (148, base - 8)], rgba("#FFE46A"), line=False)
    return p.done(C, C)


def pot_lid():
    C = 256
    p = Pen(C, C, lw=4.5)
    p.ellipse(128, POT_RIM_Y - 4, POT_RIM_RX + 14, POT_RIM_RY + 22, rgba("#8C939E"))
    p.ellipse(110, POT_RIM_Y - 14, 34, 10, rgba("#AEB5C0"), line=False)
    p.rect(108, POT_RIM_Y - 44, 148, POT_RIM_Y - 26, rgba("#5A3A20"), radius=6)
    return p.done(C, C)


def gemini_pot():
    """솥(장작불 포함)과 뚜껑 (Gemini UI 그림). 솥은 바닥을 칸 POT_FOOT 에 맞추고,
    아가리는 GEMINI_POT_MOUTH (= UILayout PotRimY/W/H). 뚜껑은 그 아가리에 맞춘다"""
    C = 256
    icons = {name: gemini_icons(name) for name in ("UIIconsA",)}
    body = icons[GEMINI_POT[0]][GEMINI_POT[1]]
    scale = C * GEMINI_POT_WIDTH / body.size[0]
    body = body.resize((round(body.size[0] * scale), round(body.size[1] * scale)), Image.LANCZOS)
    pot = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    ox, oy = (C - body.size[0]) // 2, round(C * 0.94) - body.size[1]
    pot.alpha_composite(body, (ox, oy))
    # 아가리 (안쪽 어두운 타원): 그림을 보고 잰 값 = UILayout PotRimY·PotRimW·PotRimH
    my, mw, mh = GEMINI_POT_MOUTH
    x0, x1 = C * (0.5 - mw / 2), C * (0.5 + mw / 2)
    y0, y1 = C * (my - mh / 2), C * (my + mh / 2)
    lid_src = icons[GEMINI_LID[0]][GEMINI_LID[1]]
    lw = (x1 - x0) * GEMINI_LID_OVER
    ls = lw / lid_src.size[0]
    lid_img = lid_src.resize((round(lid_src.size[0] * ls), round(lid_src.size[1] * ls)), Image.LANCZOS)
    lid = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    lid.alpha_composite(lid_img, (round((x0 + x1) / 2 - lid_img.size[0] / 2), round((y0 + y1) / 2 - lid_img.size[1] * GEMINI_LID_SEAT)))
    return pot, lid


BUILDING_FIT = 0.86         # Gemini 병영 그림 너비·높이 상한 (칸 비율)
BUILDING_BASE = 0.9         # 병영 바닥 자리 (칸 높이 비율, UILayout PlotAnchor)
BUILDING_TIER_STAR = 0.17   # 강화 단계 별 크기 (칸 비율)
FORT_FIT = 0.9              # 성벽·성문·빈 터 그림 상한 (칸 비율)
FORT_BASE = 0.92            # 성벽·성문 바닥 (UILayout WallAnchor)
PLOT_FIT = 0.78             # 빈 터 너비 (칸 비율)


def _place(img, C, fit_w, fit_h, base):
    """그림을 칸에 맞춰 줄이고 가운데 아래(base)에 발을 붙인다"""
    s = min(C * fit_w / img.size[0], C * fit_h / img.size[1])
    img = img.resize((max(1, round(img.size[0] * s)), max(1, round(img.size[1] * s))), Image.LANCZOS)
    cell = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    cell.alpha_composite(img, ((C - img.size[0]) // 2, round(C * base) - img.size[1]))
    return cell


def _tier_stars(cell, tier, C):
    """강화 단계: 지붕 위에 금별 tier 개 (UI 아이콘 결의 금별)"""
    star = gemini_icons("UIIconsA")[6]
    sz = round(C * BUILDING_TIER_STAR)
    star = star.resize((sz, round(sz * star.size[1] / star.size[0])), Image.LANCZOS)
    box = cell.getbbox()
    y = max(0, box[1] - sz // 3)
    total = tier * sz * 0.8
    for k in range(tier):
        cell.alpha_composite(star, (round(C / 2 - total / 2 + k * sz * 0.8), y))
    return cell


def buildings_atlas():
    """줄 = 강화 단계 (병영 0~3 칸). 0줄: 4 빈 칸, 5 성벽 탑(옛), 6 냄비, 7 뚜껑 / 1줄: 4 성벽 조각, 5 성문 탑 (UILayout.csv Wall*·Gate*)"""
    C = 256
    atlas = Image.new("RGBA", (C * 8, C * 4), (0, 0, 0, 0))
    pot, lid = gemini_pot()
    atlas.alpha_composite(pot, (6 * C, 0))
    atlas.alpha_composite(lid, (7 * C, 0))
    # Gemini 그림 (승인 화풍): 병영 4종 × 강화 단계(금별), 빈 건설 터, 성벽 조각, 성문 탑
    barracks = gemini_icons("Barracks")
    fort = gemini_icons("Fort")
    for k in range(4):
        base = _place(barracks[k], C, BUILDING_FIT, BUILDING_FIT, BUILDING_BASE)
        for tier in range(4):
            cell = base.copy()
            if tier:
                cell = _tier_stars(cell, tier, C)
            atlas.alpha_composite(cell, (k * C, tier * C))
    atlas.alpha_composite(_place(fort[2], C, PLOT_FIT, FORT_FIT, BUILDING_BASE), (4 * C, 0))
    atlas.alpha_composite(_place(fort[0], C, FORT_FIT, FORT_FIT, FORT_BASE), (4 * C, C))
    atlas.alpha_composite(_place(fort[1], C, FORT_FIT, FORT_FIT, FORT_BASE), (5 * C, C))
    return atlas


# ------------------------------------------------------------------------------------------------
# UI

def star_points(cx, cy, r0, r1, n=5, rot=-90):
    pts = []
    for i in range(n * 2):
        a = math.radians(rot + i * 180 / n)
        r = r0 if i % 2 == 0 else r1
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return pts


def ui_icon(i):
    C = 128
    p = Pen(C, C, lw=5)
    if i == 0:   # 금화
        p.ellipse(64, 64, 46, 46, rgba("#F2C230"))
        p.ellipse(64, 64, 32, 32, rgba("#FFDA4A"), line=False)
        p.rect(58, 40, 70, 88, rgba("#E0A820"), radius=4, line=False)
    elif i == 1:  # 영혼 (초록 불꽃 혼)
        p.poly([(64, 12), (96, 60), (92, 96), (64, 112), (36, 96), (32, 60), (50, 44), (58, 66)], rgba("#5AF2B8"))
        p.dot(52, 80, 5)
        p.dot(76, 80, 5)
    elif i == 2:  # 칼 (아군)
        p.poly([(64, 8), (78, 24), (74, 84), (54, 84), (50, 24)], rgba("#DDE3EC"))
        p.rect(32, 82, 96, 94, rgba("#3F74C9"), radius=4)
        p.rect(56, 94, 72, 120, rgba("#8B5A2B"), radius=4)
    elif i == 3:  # 해골 (적)
        p.ellipse(64, 56, 42, 38, rgba("#F1ECDC"))
        p.rect(44, 80, 84, 108, rgba("#F1ECDC"), radius=8)
        p.dot(48, 56, 11)
        p.dot(80, 56, 11)
        for x in (54, 64, 74):
            p.stick(x, 92, x, 106, 3)
    elif i == 4:  # 유성
        p.poly([(10, 118), (60, 52), (76, 68)], rgba("#FFB43A"))
        p.ellipse(80, 48, 32, 30, rgba("#6E4A36"))
    elif i == 5:  # 뿔나팔
        p.poly([(14, 78), (84, 30), (114, 22), (110, 66), (84, 64), (22, 96)], rgba("#E8C27A"))
        p.ellipse(108, 44, 10, 22, rgba("#C9A050"))
    elif i in (6, 7):  # 별, 큰 별
        if i == 7:
            p.ellipse(64, 64, 58, 58, rgba("#FFFFFF", 90), line=False)
        p.poly(star_points(64, 66, 54 if i == 7 else 50, 22 if i == 7 else 20), rgba("#FFFFFF"))
    elif i == 8:  # 영웅 얼굴
        face = draw_unit(UNITS[HERO_ROW], IDLE_POSES[0]).crop((8, 0, 120, 112)).resize((C, C), Image.LANCZOS)
        return face
    elif 9 <= i <= 12:   # 병영 아이콘 = 병사 얼굴
        u = UNITS[i - 9]
        face = draw_unit(u, IDLE_POSES[0]).crop((16, 0, 112, 96)).resize((C, C), Image.LANCZOS)
        return face
    elif i == 16:  # 대지 가르기: 쪼개진 땅덩이와 튀는 돌
        p.poly([(8, 84), (58, 70), (64, 118), (12, 112)], rgba("#8C6B45"))
        p.poly([(70, 70), (120, 84), (116, 112), (64, 118)], rgba("#8C6B45"))
        p.poly([(58, 70), (66, 48), (72, 70), (64, 118)], rgba("#FF7A2A"))
        for x, y, r in ((36, 40, 9), (92, 30, 11), (64, 18, 7)):
            p.ellipse(x, y, r, r * 0.85, rgba("#6E5236"))
    elif i == 17:  # 특성 별 바탕 원 (상태 색으로 칠한다)
        p.ellipse(64, 64, 56, 56, rgba("#FFFFFF"))
    elif i == 18:  # 응급 처치: 붕대 감은 하트
        heart(p, 64, 66, 44, rgba("#E04A5A"))
        p.rect(30, 56, 98, 74, rgba("#F6F0E0"), radius=4)
        p.rect(55, 46, 73, 84, rgba("#F6F0E0"), radius=4)
    elif i == 19:  # 끈질긴 졸병: 비석 + 위로 화살
        p.rect(30, 50, 86, 116, rgba("#A39C90"), radius=24)
        p.stick(46, 78, 70, 78, 3)
        p.stick(58, 66, 58, 98, 3)
        p.poly([(96, 14), (120, 44), (106, 44), (106, 84), (86, 84), (86, 44), (72, 44)], rgba("#5AD27A"))
    elif i == 20:  # 영웅의 숨결: 초록 하트 + 바람
        heart(p, 58, 70, 40, rgba("#5AD27A"))
        for y, w in ((26, 40), (40, 30)):
            p.arc(98, y + 10, 14, 180, 360, 4, rgba("#BFF5D0"))
            p.stick(70, y + 10, 70 + w, y + 10, 4, rgba("#BFF5D0"))
    elif i == 21:  # 분노: 빨간 핏대
        for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            cx, cy = 64 + sx * 24, 64 + sy * 24
            p.poly([(cx - sx * 4, cy - sy * 22), (cx + sx * 18, cy + sy * 4), (cx + sx * 6, cy + sy * 10), (cx - sx * 10, cy - sy * 6)], rgba("#E8333A"))
    elif i == 22:  # 폭발 화살
        pts = [(84 + math.cos(a / 12 * math.tau) * (38 if a % 2 == 0 else 20), 50 + math.sin(a / 12 * math.tau) * (38 if a % 2 == 0 else 20)) for a in range(12)]
        p.poly(pts, rgba("#FF8A2A"))
        p.stick(12, 116, 70, 62, 6, rgba("#8B5A2B"))
        p.poly([(62, 56), (84, 46), (74, 70)], rgba("#DDE3EC"))
    elif i == 23:  # 두 발 쏘기: 나란한 화살 둘
        for oy in (-22, 22):
            p.stick(14, 64 + oy, 96, 64 + oy, 5, rgba("#8B5A2B"))
            p.poly([(96, 52 + oy), (120, 64 + oy), (96, 76 + oy)], rgba("#DDE3EC"))
            p.poly([(10, 54 + oy), (26, 64 + oy), (10, 74 + oy)], rgba("#E04A3A"))
    elif i == 24:  # 황금 해골
        p.ellipse(64, 56, 42, 38, rgba("#F2C230"))
        p.rect(44, 80, 84, 108, rgba("#F2C230"), radius=8)
        p.dot(48, 56, 11)
        p.dot(80, 56, 11)
        for x in (54, 64, 74):
            p.stick(x, 92, x, 106, 3)
        p.poly(star_points(104, 22, 16, 6), rgba("#FFF7B0"), line=False)
    elif i == 25:  # 영혼 폭발: 혼 + 빛
        for a in range(8):
            ang = a / 8 * math.tau
            p.stick(64 + math.cos(ang) * 44, 66 + math.sin(ang) * 44, 64 + math.cos(ang) * 58, 66 + math.sin(ang) * 58, 5, rgba("#FFE46A"))
        p.poly([(64, 22), (88, 60), (84, 90), (64, 104), (44, 90), (40, 60), (54, 48), (60, 66)], rgba("#5AF2B8"))
    elif i == 26:  # 충격파: 망치 + 퍼지는 물결
        for r in (36, 52):
            p.arc(64, 96, r, 200, 340, 4, rgba("#FFE46A"))
        p.stick(84, 20, 58, 76, 8, rgba("#9C6B3D"))
        p.rect(30, 66, 82, 96, rgba("#8A8F99"), radius=5)
    elif i == 27:  # 대장장이 근성: 모루 + 초록 십자
        p.poly([(14, 52), (100, 52), (100, 70), (78, 76), (84, 100), (40, 100), (46, 76), (26, 70)], rgba("#5A5F69"))
        p.rect(30, 100, 96, 114, rgba("#4A4E57"), radius=3)
        p.rect(94, 14, 108, 54, rgba("#5AD27A"), radius=3)
        p.rect(81, 27, 121, 41, rgba("#5AD27A"), radius=3)
    elif i == 28:  # 유성 연쇄: 운석 둘
        for ox, oy, k in ((-18, 18, 0.75), (16, -14, 1.0)):
            p.poly([(64 + ox - 52 * k, 64 + oy + 46 * k), (64 + ox - 6 * k, 64 + oy - 6 * k), (64 + ox + 6 * k, 64 + oy + 10 * k)], rgba("#FFB43A"))
            p.ellipse(64 + ox + 8 * k, 64 + oy - 2 * k, 22 * k, 20 * k, rgba("#6E4A36"))
    elif i == 29:  # 치유의 함성: 뿔나팔 + 초록 십자
        p.poly([(14, 86), (74, 50), (100, 44), (96, 82), (74, 78), (22, 104)], rgba("#E8C27A"))
        p.ellipse(96, 62, 9, 19, rgba("#C9A050"))
        p.rect(96, 8, 110, 46, rgba("#5AD27A"), radius=3)
        p.rect(84, 20, 122, 34, rgba("#5AD27A"), radius=3)
    elif i == 30:  # 가시 방패
        for a in range(10):
            ang = a / 10 * math.tau
            p.poly([(64 + math.cos(ang - 0.18) * 40, 64 + math.sin(ang - 0.18) * 44), (64 + math.cos(ang) * 60, 64 + math.sin(ang) * 62),
                    (64 + math.cos(ang + 0.18) * 40, 64 + math.sin(ang + 0.18) * 44)], rgba("#DDE3EC"))
        p.ellipse(64, 64, 40, 44, rgba("#3F74C9"))
        p.ellipse(64, 64, 12, 12, rgba("#8A8F99"))
    elif i == 31:  # 철벽 의지: 방패 + 금 하트
        p.poly([(24, 20), (104, 20), (104, 64), (64, 116), (24, 64)], rgba("#3F74C9"))
        p.poly([(32, 28), (96, 28), (96, 62), (64, 104), (32, 62)], rgba("#5A8BDB"), line=False)
        heart(p, 64, 62, 26, rgba("#F2C230"))
    elif i == 32:  # 흡혈 군단: 핏방울 + 송곳니
        p.poly([(64, 10), (96, 66), (96, 84), (80, 112), (48, 112), (32, 84), (32, 66)], rgba("#C0262E"))
        p.poly([(40, 60), (52, 92), (60, 60)], rgba("#FFFFFF"))
        p.poly([(68, 60), (76, 92), (88, 60)], rgba("#FFFFFF"))
    elif i == 33:  # 뼈 (냄비 재료)
        p.stick(36, 64, 92, 64, 14, rgba("#F1ECDC"))
        for x in (30, 98):
            p.ellipse(x, 54, 13, 12, rgba("#F1ECDC"))
            p.ellipse(x, 74, 13, 12, rgba("#F1ECDC"))
    elif i == 35:  # 국자 (내려치기 횟수)
        return ladle_image(C)
    elif i == 36:  # 쌀 한 가마니
        p.poly([(30, 50), (98, 50), (106, 112), (22, 112)], rgba("#E8D9B0"))
        p.rect(40, 34, 88, 52, rgba("#D8C89A"), radius=6)
        p.stick(42, 50, 86, 50, 4, rgba("#8B5A2B"))
        for x, y in ((50, 80), (64, 90), (78, 78), (58, 100), (74, 98)):
            p.ellipse(x, y, 4, 2.5, rgba("#FFFFFF"), line=False)
        p.ellipse(64, 20, 14, 8, rgba("#FFFFFF"))
    elif i == 37:  # 고춧가루 단지
        p.rect(30, 44, 98, 112, rgba("#C8A06A"), radius=16)
        p.ellipse(64, 46, 34, 10, rgba("#D9453A"))
        for x in (44, 58, 72, 86):
            p.ellipse(x, 40, 6, 5, rgba("#E8333A"), line=False)
        p.rect(40, 70, 88, 86, rgba("#F6E7B0"), radius=4)
    elif i == 38:  # 사골 뼈
        p.stick(30, 90, 98, 40, 16, rgba("#F1ECDC"))
        for x, y in ((24, 96), (36, 102), (92, 34), (104, 40)):
            p.ellipse(x, y, 12, 11, rgba("#F1ECDC"))
        p.ellipse(64, 64, 14, 12, rgba("#E8C27A"))
        p.ellipse(64, 64, 7, 6, rgba("#C0392B"), line=False)
    elif i == 39:  # 마늘
        for dx in (-22, 0, 22):
            p.ellipse(64 + dx, 76, 22, 32, rgba("#F6F0E0"))
        p.poly([(56, 46), (64, 14), (72, 46)], rgba("#E8E0C8"))
        p.stick(46, 104, 82, 104, 2, rgba("#C9B890"), line=False)
    elif i == 40:  # 후추 (통후추 갈이)
        p.rect(44, 40, 84, 112, rgba("#3A2A22"), radius=12)
        p.rect(48, 22, 80, 44, rgba("#5A4030"), radius=6)
        p.ellipse(64, 16, 8, 8, rgba("#8B5A2B"))
        for x, y in ((24, 100), (100, 96), (30, 80), (104, 76)):
            p.ellipse(x, y, 6, 6, rgba("#2A2420"))
    elif i == 41:  # 선지 (그릇에 붉은 덩어리)
        p.ellipse(64, 88, 50, 22, rgba("#E8E0D0"))
        p.rect(38, 50, 90, 86, rgba("#7A1E24"), radius=8)
        p.ellipse(52, 60, 8, 5, rgba("#B8323A"), line=False)
    elif i == 42:  # 인삼
        p.poly([(64, 34), (76, 60), (72, 92), (88, 116), (74, 112), (64, 96), (52, 116), (40, 114), (56, 92), (52, 60)], rgba("#E8C890"))
        for dx in (-14, 0, 14):
            p.poly([(64, 36), (64 + dx - 8, 8), (64 + dx + 8, 8)], rgba("#4E9A3E"))
        p.ellipse(56, 70, 3, 3, INK, line=False)
        p.ellipse(68, 70, 3, 3, INK, line=False)
    elif i == 43:  # 금가루 주머니
        p.poly([(34, 64), (94, 64), (104, 112), (24, 112)], rgba("#8B5A2B"))
        p.poly([(44, 64), (54, 40), (74, 40), (84, 64)], rgba("#A6703A"))
        p.stick(46, 64, 82, 64, 4, rgba("#F2C230"))
        for x, y, r in ((30, 30, 10), (96, 24, 8), (64, 18, 6), (104, 50, 6)):
            p.poly(star_points(x, y, r, r * 0.4, 4), rgba("#FFE46A"), line=False)
    elif i == 44:  # 청양고추
        p.poly([(30, 30), (44, 26), (90, 76), (104, 112), (80, 100), (40, 52)], rgba("#3FA040"))
        p.stick(30, 30, 18, 14, 5, rgba("#2E7A30"))
        p.stick(46, 40, 84, 82, 3, rgba("#7AD070"), line=False)
    elif i == 45:  # 대파 한 단
        for dx, c in ((-16, "#3FA040"), (0, "#4FB050"), (16, "#3FA040")):
            p.rect(56 + dx, 14, 72 + dx, 70, rgba(c), radius=6)
            p.rect(56 + dx, 66, 72 + dx, 112, rgba("#F4F4E8"), radius=6)
        p.rect(36, 74, 92, 84, rgba("#C0392B"), radius=3)
    elif i == 34:  # 냄비 (끓는 국)
        for x, y, r in ((48, 26, 12), (70, 16, 14), (88, 30, 10)):
            p.ellipse(x, y, r, r, rgba("#FFFFFF", 200), line=False)
        p.ellipse(64, 84, 50, 36, rgba("#4A4E57"))
        p.ellipse(64, 56, 50, 14, rgba("#6E7480"))
        p.ellipse(64, 56, 42, 9, rgba("#F2E2B8"), line=False)
        p.rect(20, 92, 108, 100, rgba("#D9A93A"), radius=3)
    elif i in (13, 14, 15):   # 패널 (9칸 늘리기용. 모서리 = 칸의 1/4)
        fill = {13: rgba("#5C3B28"), 14: rgba("#2E1E16", 235), 15: rgba("#6E4A30")}[i]
        p = Pen(C, C, lw=0)
        p.rect(4, 4, C - 4, C - 4, INK, radius=22, line=False)
        p.rect(10, 10, C - 10, C - 10, fill, radius=17, line=False)
        return p.done(C, C)
    return p.done(C, C)


UI_ICONS = 48   # 0~15 + 16 (안 씀) + 17 특성 바탕 원 + 18~32 중간 별 효과 + 33 뼈 + 34 냄비 + 35 국자 + 36~45 양념 (Cards.csv Icon) (UILayout.csv Ui*, Perks.csv Icon)


def ladle_image(C):
    """큰 국자: 나무 자루 + 쇠 국자 머리 (머리가 아래)"""
    p = Pen(C, C, lw=4)
    p.stick(64, 8, 64, 84, 9, rgba("#8B5A2B"))
    p.rect(58, 70, 70, 86, rgba("#6E7681"), radius=3)
    p.ellipse(64, 100, 30, 20, rgba("#AEB5C0"))
    p.ellipse(64, 96, 22, 11, rgba("#7E8590"), line=False)
    p.ellipse(54, 104, 8, 4, rgba("#DDE3EC"), line=False)
    return p.done(C, C)


def heart(p, cx, cy, r, fill):
    pts = []
    for i in range(40):
        t = i / 40 * math.tau
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        pts.append((cx + x * r / 17, cy + y * r / 17))
    p.poly(pts, fill)


# AI 원화 UI (Approved/UIKit.png, 사용자 승인 시안): 나무 틀 패널 3종 + 아이콘 몇 개. 좌표는 바탕 지우고 자른 그림 기준
UI_KIT_FRAME = dict(tl=(0, 48), tr=(855, 48), bl=(0, 675), br=(855, 675), top=(120, 48, 190, 148), bottom=(120, 675, 220, 775),
                    left=(0, 250, 100, 350), right=(855, 250, 955, 350), patch=100, band=60)
UI_KIT_ICON_TOP = 795      # 아이콘 줄이 시작하는 높이
UI_KIT_ICONS = {2: 0, 3: 1, 4: 2, 5: 3, 6: 34}   # 시안 아이콘 순서 (0·1 나무 단추) → UI.png 칸 (금화·영혼·칼·해골·냄비)
UI_PANEL_FILL = {13: "#4A3424", 14: "#2E211A", 15: "#5A3A26"}   # 패널·어두운 패널·카드: 양피지 대신 나무판 (밝은 글자가 읽히게)
UI_BARRACKS_ICONS = {9: "Grunt", 10: "Archer", 11: "Shield", 12: "Brute"}   # 병영 아이콘 칸: 그 병사 원화 윗몸
UI_BARRACKS_ICON_TOP = 0.72   # 원화 위에서 이만큼만 (윗몸)
UI_HERO_PORTRAITS = {8: "Hero", 46: "HeroCook", 47: "HeroPot"}   # 영웅 얼굴 칸 (Heroes.csv Portrait)
UI_HERO_PORTRAIT_TOP = 0.6
UI_ICON_FILL = 0.88        # 아이콘이 칸에서 차지하는 비율


def _ui_kit():
    ART_UNITS.setdefault("UIKit", dict(file="UIKit.png", flip=False))
    return art_source("UIKit")


def _parchment_to_wood(patch, inner, fill):
    """틀 안쪽(inner 영역)의 양피지 색 픽셀만 나무판 색으로 (금 장식·선은 남긴다)"""
    px = patch.load()
    x0, y0, x1, y1 = inner
    for y in range(max(y0, 0), min(y1, patch.size[1])):
        for x in range(max(x0, 0), min(x1, patch.size[0])):
            r, g, b, a = px[x, y]
            if a and (r + g + b) / 3 > 150 and 20 < r - b < 110:
                px[x, y] = fill
    return patch


def ui_panel(cell, size=128):
    """9칸 늘리기 패널: 모서리·변은 시안 나무 틀, 가운데는 나무판"""
    kit = _ui_kit()
    F = UI_KIT_FRAME
    k, band = F["patch"], F["band"]
    corner = round(size * 0.25)      # UILayout PanelCornerFraction
    fill = rgba(UI_PANEL_FILL[cell])
    out = Image.new("RGBA", (size, size), fill)
    # 가운데 나무결
    d = ImageDraw.Draw(out)
    rnd = random.Random(cell)
    for _ in range(6):
        y = rnd.randint(corner, size - corner)
        d.line([(corner, y), (size - corner, y + rnd.randint(-2, 2))], fill=tuple(max(c - 14, 0) for c in fill[:3]) + (255,), width=1)

    def piece(box, inner, to_size):
        patch = kit.crop(box).copy()
        return _parchment_to_wood(patch, inner, fill).resize(to_size, Image.LANCZOS)

    tl, tr, bl, br = F["tl"], F["tr"], F["bl"], F["br"]
    out.paste(piece((tl[0], tl[1], tl[0] + k, tl[1] + k), (band, band, k, k), (corner, corner)), (0, 0))
    out.paste(piece((tr[0], tr[1], tr[0] + k, tr[1] + k), (0, band, k - band, k), (corner, corner)), (size - corner, 0))
    out.paste(piece((bl[0], bl[1], bl[0] + k, bl[1] + k), (band, 0, k, k - band), (corner, corner)), (0, size - corner))
    out.paste(piece((br[0], br[1], br[0] + k, br[1] + k), (0, 0, k - band, k - band), (corner, corner)), (size - corner, size - corner))
    top, bottom, left, right = F["top"], F["bottom"], F["left"], F["right"]
    out.paste(piece(top, (0, band, top[2] - top[0], k), (size - corner * 2, corner)), (corner, 0))
    out.paste(piece(bottom, (0, 0, bottom[2] - bottom[0], k - band), (size - corner * 2, corner)), (corner, size - corner))
    out.paste(piece(left, (band, 0, k, left[3] - left[1]), (corner, size - corner * 2)), (0, corner))
    out.paste(piece(right, (0, 0, k - band, right[3] - right[1]), (corner, size - corner * 2)), (size - corner, corner))
    # 바깥 모서리 둥근 부분은 시안 그대로 투명
    return out


def _fit_icon(img, size=128):
    img = img.crop(img.getbbox())
    scale = size * UI_ICON_FILL / max(img.size)
    img = img.resize((max(1, round(img.size[0] * scale)), max(1, round(img.size[1] * scale))), Image.LANCZOS)
    cell = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cell.alpha_composite(img, ((size - img.size[0]) // 2, (size - img.size[1]) // 2))
    return cell


def ui_kit_icons():
    """시안 아래 줄 아이콘들을 왼쪽부터 떼어 낸다 (빈 세로줄로 나눔)"""
    kit = _ui_kit()
    row = kit.crop((0, UI_KIT_ICON_TOP, kit.size[0], kit.size[1]))
    a = row.getchannel("A").load()
    filled = [any(a[x, y] > 40 for y in range(row.size[1])) for x in range(row.size[0])]
    icons, start = [], None
    for x, on in enumerate(filled + [False]):
        if on and start is None:
            start = x
        elif not on and start is not None:
            if x - start > 20:
                icons.append(row.crop((start, 0, x, row.size[1])))
            start = None
    return icons


# Gemini UI 그림 (승인 시안, UIKit 화풍): 판마다 열×줄로 고르게 놓인 아이콘
GEMINI_ICON_SHEETS = {
    "UIIconsA": (4, 2),   # 국자, 솥+장작불, 솥뚜껑, 유성, 뿔나팔, 뼈, 별, 큰 별
    "UIIconsB": (5, 2),   # 쌀, 고춧가루, 사골, 마늘, 후추, 선지, 인삼, 금가루, 청양고추, 대파
    "Barracks": (4, 1),   # 졸병 막사, 궁수 초소, 방패 막사, 망치 막사
    "Fort": (4, 1),       # 성벽 조각, 성문 탑, 빈 건설 터, 큰 국자
    "PerkIcons": (4, 4),  # 특성 별 효과 16종 (UI 16, 18~32)
    "FxIcons": (3, 2),    # 불꽃, 불덩이, 폭발 구름, 김, 빛기둥, 그을음
}
GEMINI_UI_CELLS = {("UIIconsA", 0): 35, ("UIIconsA", 3): 4, ("UIIconsA", 4): 5, ("UIIconsA", 5): 33}
GEMINI_UI_CELLS.update({("UIIconsB", k): 36 + k for k in range(10)})
GEMINI_UI_CELLS.update({("PerkIcons", k): (16 if k == 0 else 17 + k) for k in range(16)})
GEMINI_FX_CELLS = {0: 11, 1: 8, 2: 9, 3: 14, 4: 15}   # FxIcons 순서 → Fx.png 칸 (UILayout FxFlame·FxMeteor·FxBlast·FxSteam·FxPromote). 5 그을음은 바닥에 구워져 너무 진해서 예전 옅은 그림을 쓴다
GEMINI_FX_FIT = 0.9
GEMINI_STAR_CELLS = {("UIIconsA", 6): 6, ("UIIconsA", 7): 7}   # 별: 게임이 색을 곱하므로 흰 별로 바꾼다
GEMINI_POT = ("UIIconsA", 1)       # Buildings.png 6 = 솥과 장작불
GEMINI_LID = ("UIIconsA", 2)       # Buildings.png 7 = 뚜껑
GEMINI_POT_WIDTH = 0.82            # 솥 그림 너비 (칸 비율)
GEMINI_LID_OVER = 1.12             # 뚜껑 너비 = 아가리 너비 × 이 배
GEMINI_LID_SEAT = 0.62             # 뚜껑 그림에서 아가리 가운데에 걸리는 높이 (뚜껑 높이 비율)
GEMINI_POT_MOUTH = (0.231, 0.539, 0.122)   # 솥 아가리 가운데 y, 너비, 높이 (칸 비율, 솥 너비 GEMINI_POT_WIDTH 기준)
STAR_WHITE_LIFT = 70               # 흰 별: 밝기를 이만큼 올린다


def gemini_icons(sheet):
    """판 하나를 열×줄 칸으로 나눠 칸마다 그림 둘레로 자른다 (바탕은 art_source 가 지운다)"""
    ART_UNITS.setdefault(sheet, dict(file=sheet + ".png", flip=False))
    img = art_source(sheet)
    cols, rows = GEMINI_ICON_SHEETS[sheet]
    w, h = img.size
    icons = []
    for r in range(rows):
        for c in range(cols):
            cell = img.crop((c * w // cols, r * h // rows, (c + 1) * w // cols, (r + 1) * h // rows))
            box = cell.getbbox()
            icons.append(cell.crop(box) if box else cell)
    return icons


def _white_star(img):
    """금별 → 먹선은 그대로, 안쪽은 밝은 회색 (게임이 상태 색을 곱한다)"""
    gray = img.convert("LA")
    out = Image.new("RGBA", img.size)
    px, gp = out.load(), gray.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            l, a = gp[x, y]
            v = l if l < 80 else min(255, l + STAR_WHITE_LIFT)
            px[x, y] = (v, v, v, a)
    return out


def ui_atlas():
    C = 128
    atlas = Image.new("RGBA", (C * 8, C * ((UI_ICONS + 7) // 8)), (0, 0, 0, 0))
    for i in range(UI_ICONS):
        atlas.alpha_composite(ui_icon(i), ((i % 8) * C, (i // 8) * C))

    def put(i, img):
        atlas.paste(Image.new("RGBA", (C, C), (0, 0, 0, 0)), ((i % 8) * C, (i // 8) * C))
        atlas.alpha_composite(img, ((i % 8) * C, (i // 8) * C))

    # 새 화풍 (승인 시안): 나무 틀 패널, 금화·영혼·칼·해골·냄비, 졸병 막사
    for cell in UI_PANEL_FILL:
        put(cell, ui_panel(cell, C))
    icons = ui_kit_icons()
    for index, cell in UI_KIT_ICONS.items():
        put(cell, _fit_icon(icons[index], C))
    for cell, name in UI_HERO_PORTRAITS.items():
        art = art_source(name)
        put(cell, _fit_icon(art.crop((0, 0, art.size[0], int(art.size[1] * UI_HERO_PORTRAIT_TOP))), C))
    for cell, name in UI_BARRACKS_ICONS.items():
        art = art_source(name)
        put(cell, _fit_icon(art.crop((0, 0, art.size[0], int(art.size[1] * UI_BARRACKS_ICON_TOP))), C))
    # Gemini UI 그림: 국자·유성·뿔나팔·뼈·국 재료, 흰 별
    sheets = {name: gemini_icons(name) for name in GEMINI_ICON_SHEETS}
    for (sheet, index), cell in GEMINI_UI_CELLS.items():
        put(cell, _fit_icon(sheets[sheet][index], C))
    for (sheet, index), cell in GEMINI_STAR_CELLS.items():
        put(cell, _fit_icon(_white_star(sheets[sheet][index]), C))
    return atlas


# ------------------------------------------------------------------------------------------------
# 바닥

GROUND_FOREST = dict(grass="#5E8F3A", patches=["#58873A", "#67993F", "#527F34", "#6CA144"], dirt="#8C6B45",
                     specks=["#7E6040", "#977552", "#856544"], blade="#3F6B26", camp="#7A5E3C", camp_specks=["#6E5436", "#856845", "#735838"], graves=False)
# 공동묘지: 시든 풀, 잿빛 흙, 풀밭에 비석과 십자가
GROUND_GRAVEYARD = dict(grass="#4A5640", patches=["#435038", "#525E46", "#3E4934", "#57634A"], dirt="#6B5E58",
                        specks=["#5E524C", "#776A63", "#665A54"], blade="#2F3A28", camp="#5E5248", camp_specks=["#544840", "#6A5D52", "#5A4E46"], graves=True)


GROUND_BASE = (2048, 740)    # 바닥 무늬를 짠 기준 크기 (전장 하나, 무늬 크기·밀도는 이 기준)
GROUND_FIELD_PX = 4096       # Ground.png 너비 (= 렌더 타깃 GroundTargetWidth, 최대 확대에서도 또렷하게)
OUTER_SCALE = 0.5            # Ground_Outer.png = 전장+둘레 한 장을 이 배율로 줄인 것
GROUND_SOFT = 6              # 풀 무늬를 번지게 하는 폭 (기준 픽셀)
GROUND_RIM = 6               # 흙길 가장자리 어두운 띠 폭 (기준 픽셀)
GROUND_RIM_COLOR = (62, 48, 30, 255)
GROUND_RIM_ALPHA = 0.55
GROUND_INK = (40, 32, 26, 255)   # 먹선 색
GROUND_INKED_TUFTS = 0.12   # 먹선 두른 풀 포기 비율
GROUND_PEBBLES = 140         # 흙길 조약돌 수 (기준 넓이당)
OUTER_VIGNETTE = 0.45        # 둘레 바깥 끝으로 갈수록 어두워지는 정도
_ground_cache = {}


def _table_values(file, names):
    """Tuning.csv / UILayout.csv 의 Name,Value 표에서 값 몇 개"""
    import csv
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Content", "YJJWave", "Data", file)
    with open(path, encoding="utf-8-sig", newline="") as f:
        rows = {r["Name"]: r["Value"] for r in csv.DictReader(f)}
    return [float(rows[n]) for n in names]


def _ground_whole(P):
    """전장 + 둘레를 한 장으로 (무늬가 끊기지 않게). 반환: 그림, 전장 자리 (x0, y0, x1, y1)"""
    key = P["grass"]
    if key in _ground_cache:
        return _ground_cache[key]
    FW, FH = _table_values("Tuning.csv", ["FieldWidth", "FieldHeight"])
    MX, MY = _table_values("UILayout.csv", ["ViewMarginX", "ViewMarginY"])
    BW, BH = GROUND_BASE
    t = GROUND_FIELD_PX / FW              # 월드 1 당 픽셀
    k = GROUND_FIELD_PX / BW              # 기준 무늬 대비 배율
    W, H = round((FW + MX * 2) * t), round((FH + MY * 2) * t)
    field = (round(MX * t), round(MY * t), round(MX * t) + GROUND_FIELD_PX, round(MY * t) + round(FH * t))
    density = (W * H) / (BW * BH * k * k)   # 기준 넓이 대비 몇 배 넓은가
    rnd = random.Random(3)
    img = Image.new("RGBA", (W, H), rgba(P["grass"]))
    d = ImageDraw.Draw(img)
    for _ in range(int(900 * density)):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        r = rnd.uniform(10, 46) * k
        d.ellipse([x - r, y - r * 0.6, x + r, y + r * 0.6], fill=rgba(rnd.choice(P["patches"])))
    # 동그라미 무늬를 붓 칠처럼 번지게 (AI 원화 결의 부드러운 바탕)
    img = img.filter(ImageFilter.GaussianBlur(GROUND_SOFT * k))
    d = ImageDraw.Draw(img)
    # 가운데 흙길: 전장 기준 식 그대로 좌우 둘레까지 잇는다
    band = Image.new("L", (W, H), 0)
    bd = ImageDraw.Draw(band)
    step = 8 * k
    px = 0.0
    while px < W:
        world_x = px / t - MX
        xb = world_x * BW / FW
        top = BH * 0.3 + math.sin(xb * 0.006) * 24 + rnd.uniform(-6, 6)
        bot = BH * 0.72 + math.sin(xb * 0.005 + 1.3) * 26 + rnd.uniform(-6, 6)
        bd.rectangle([px, (top * FH / BH + MY) * t, px + step, (bot * FH / BH + MY) * t], fill=255)
        px += step
    band = band.filter(ImageFilter.GaussianBlur(10 * k))
    dirt = Image.new("RGBA", (W, H), rgba(P["dirt"]))
    dd = ImageDraw.Draw(dirt)
    for _ in range(int(1400 * density)):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        r = rnd.uniform(3, 14) * k
        dd.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=rgba(rnd.choice(P["specks"])))
    dirt = dirt.filter(ImageFilter.GaussianBlur(GROUND_SOFT * k * 0.5))
    img = Image.composite(dirt, img, band)
    # 흙길 가장자리: 풀이 흙에 닿는 곳을 조금 어둡게 (먹선 결)
    rim = ImageChops.subtract(band.filter(ImageFilter.MaxFilter(2 * round(GROUND_RIM * k) + 1)), band).filter(ImageFilter.GaussianBlur(GROUND_RIM * k * 0.5))
    img = Image.composite(Image.new("RGBA", (W, H), GROUND_RIM_COLOR), img, rim.point(lambda v: int(v * GROUND_RIM_ALPHA)))
    d = ImageDraw.Draw(img)
    bp = band.load()
    # 풀 포기: 먹선 두른 작은 풀 (몇 개만, 나머지는 옅은 선)
    for _ in range(int(1600 * density)):
        x, y = rnd.uniform(0, W - 1), rnd.uniform(0, H - 1)
        if bp[int(x), int(y)] > 80:
            continue
        inked = rnd.random() < GROUND_INKED_TUFTS
        for j in range(3):
            tip = (x + j * 3 * k + rnd.uniform(-3, 3) * k, y - rnd.uniform(6, 12) * k * (1.6 if inked else 1.0))
            if inked:
                d.line([x + j * 3 * k, y, tip[0], tip[1]], fill=GROUND_INK, width=round(4 * k))
            d.line([x + j * 3 * k, y, tip[0], tip[1]], fill=rgba(P["blade"]) if not inked else rgba(P["patches"][-1]), width=round(2 * k))
    # 흙길 위 돌멩이: 먹선 두른 회색 조약돌
    for _ in range(int(GROUND_PEBBLES * density)):
        x, y = rnd.uniform(0, W - 1), rnd.uniform(0, H - 1)
        if bp[int(x), int(y)] < 200:
            continue
        r = rnd.uniform(3, 7) * k
        d.ellipse([x - r - k, y - r * 0.7 - k, x + r + k, y + r * 0.7 + k], fill=GROUND_INK)
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=rgba("#A79C8E"))
        d.ellipse([x - r * 0.5, y - r * 0.6, x + r * 0.1, y - r * 0.1], fill=rgba("#C9C0B3"))
    # 진지 (성벽 왼쪽): 다져진 흙, 둘레 배경 위아래 끝까지 (멀리 볼 때 진지가 잘려 보이지 않게)
    camp_x = field[0] + round(520 * t)
    cy0, cy1 = 0, H
    d.rectangle([0, cy0, camp_x, cy1], fill=rgba(P["camp"]))
    for _ in range(int(500 * density * (camp_x * (cy1 - cy0)) / (W * H))):
        x, y = rnd.uniform(0, camp_x), rnd.uniform(cy0, cy1)
        r = rnd.uniform(4, 16) * k
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=rgba(rnd.choice(P["camp_specks"])))
    if P["graves"]:
        gp = Pen(W, H, lw=2.2 * k)
        for _ in range(int(70 * k * density / 2)):
            x, y = rnd.uniform(camp_x + 40 * k, W - 20 * k), rnd.uniform(20 * k, H - 20 * k)
            if bp[int(x), int(y)] > 40:
                continue
            if rnd.random() < 0.6:
                gp.rect(x - 9 * k, y - 22 * k, x + 9 * k, y, rgba("#8E8A86"), radius=8 * k)
                gp.stick(x - 4 * k, y - 12 * k, x + 4 * k, y - 12 * k, 1.4 * k)
            else:
                gp.stick(x, y - 26 * k, x, y, 3 * k, rgba("#6E5A44"))
                gp.stick(x - 8 * k, y - 18 * k, x + 8 * k, y - 18 * k, 3 * k, rgba("#6E5A44"))
        img.alpha_composite(gp.done(W, H))
    _ground_cache[key] = (img, field)
    return _ground_cache[key]


def ground(P=GROUND_FOREST):
    """전장 바닥 (피·시체는 게임이 그 위에 굽는다) = 전장+둘레 한 장에서 전장 자리만 잘라 낸 것"""
    img, field = _ground_whole(P)
    return img.crop(field)


def ground_outer(P=GROUND_FOREST):
    """전장 바깥 둘레 배경 (멀리 볼 때): 전장+둘레 한 장을 줄이고, 끝으로 갈수록 어둡게.
    가운데 전장 자리는 Ground.png 가 덮는다 (같은 그림이라 이음매가 없다)"""
    whole, field = _ground_whole(P)
    s = OUTER_SCALE
    W, H = round(whole.width * s), round(whole.height * s)
    img = whole.resize((W, H), Image.LANCZOS)
    f = [round(v * s) for v in field]
    MX, MY = f[0], f[1]
    # 전장에서 멀어질수록 어둡게 (작게 계산해 키운다)
    q = 8
    shade = Image.new("L", (W // q + 1, H // q + 1), 0)
    sp = shade.load()
    for y in range(shade.height):
        for x in range(shade.width):
            dx = max(f[0] - x * q, x * q - f[2], 0) / max(MX, 1)
            dy = max(f[1] - y * q, y * q - f[3], 0) / max(MY, 1)
            sp[x, y] = int(255 * OUTER_VIGNETTE * min(1.0, math.hypot(dx, dy)))
    shade = shade.resize((W, H), Image.BILINEAR).filter(ImageFilter.GaussianBlur(12))
    return Image.composite(Image.new("RGBA", (W, H), (20, 16, 12, 255)), img, shade)


WORLD_MAP_SIZE = (1920, 1080)


def ai_world_map():
    """월드맵: AI 원화 (Approved/WorldMapWide.png, 사용자 승인 시안 화풍) 를 16:9 로 잘라 키운다.
    지역·스테이지 자리는 Regions.csv / Stages.csv MapX·MapY (이 그림 기준)"""
    img = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "AI", "Approved", "WorldMapWide.png")).convert("RGBA")
    w, h = img.size
    target = WORLD_MAP_SIZE[0] / WORLD_MAP_SIZE[1]
    if w / h > target:
        cw = round(h * target)
        img = img.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h))
    else:
        ch = round(w / target)
        img = img.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
    return img.resize(WORLD_MAP_SIZE, Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    jobs = [("Props.png", props_atlas), ("Units.png", units_atlas), ("Fx.png", fx_atlas), ("Buildings.png", buildings_atlas), ("UI.png", ui_atlas), ("Ground.png", ground),
            ("Ground_Graveyard.png", lambda: ground(GROUND_GRAVEYARD)), ("WorldMap.png", ai_world_map),
            ("Ground_Outer.png", ground_outer), ("Ground_Graveyard_Outer.png", lambda: ground_outer(GROUND_GRAVEYARD))]
    jobs += [("Big_%s.png" % name, (lambda n: lambda: big_atlas(n))(name)) for name in big_specs()]
    only = [a for a in sys.argv[1:] if a.lower().endswith(".png")]   # 파일 이름을 주면 그것만 다시 만든다
    for name, make in jobs:
        if only and name not in only:
            continue
        make().save(os.path.join(OUT, name))
        print("wrote", name)


if __name__ == "__main__":
    main()
