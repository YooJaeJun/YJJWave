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

from PIL import Image, ImageDraw, ImageFilter

SS = 4                       # 크게 그린 뒤 줄인다
INK = (46, 28, 22, 255)      # 먹선
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "Content", "YJJWave", "Art")

UNIT_CELL = 128
UNIT_COLS = 32
UNIT_ROWS = 16
PROJECTILE_ROW = 15
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

UNITS = [
    # 줄, 몸색, 머리색, 모자, 무기, 크기, 특징
    dict(name="Grunt", body="#FFFFFF", head="#FFFFFF", hat="pot", weapon="club", scale=1.0, faces=dict(walk="cat", idle="cat", attack="cat", hit="cry", dazed="derp")),
    dict(name="Archer", body="#FFFFFF", head="#FFFFFF", hat="hood", weapon="bow", scale=1.0, faces=dict(walk="hotblood", idle="hotblood", attack="shout", hit="cry", dazed="derp")),
    dict(name="Shield", body="#FFFFFF", head="#FFFFFF", hat="helm", weapon="shield", scale=1.05, faces=dict(walk="hotblood", idle="hotblood", attack="shout", hit="cry", dazed="derp")),
    dict(name="Brute", body="#F3D7B0", head="#F6DDBA", hat="beard", weapon="hammer", scale=1.15, faces=dict(walk="hotblood", idle="hotblood", attack="shout", hit="cry", dazed="derp")),
    dict(name="Skeleton", body="#E9E4D4", head="#F1ECDC", hat="skull", weapon="bone", scale=1.0),
    dict(name="Imp", body="#E2483A", head="#EA5444", hat="horns", weapon="claw", scale=0.9),
    dict(name="Fatty", body="#8DBF5A", head="#9CCB68", hat="zombie", weapon="none", scale=1.15, fat=True),
    dict(name="BoneArcher", body="#E9E4D4", head="#F1ECDC", hat="skull", weapon="bonebow", scale=1.0),
    dict(name="BoneKing", body="#E9E4D4", head="#F1ECDC", hat="crown", weapon="greatsword", scale=1.0, cape="#6B3FA0"),
    # 9: 내 영웅 (중심). 졸병 출신 대장: 금빛 투구 + 깃털, 빨간 망토, 큰 칼
    dict(name="Hero", body="#FFFFFF", head="#FFFFFF", hat="knight", weapon="greatsword", scale=1.0, cape="#D9453A", faces=dict(walk="hotblood", idle="hotblood", attack="shout", hit="cry", dazed="derp")),
]
HERO_ROW = 9
# 10: 강령술사 (적). 보라 두건 + 해골 지팡이
UNITS.append(dict(name="Necro", body="#6B3FA0", head="#E9E4D4", hat="necro", weapon="staff", scale=1.0))


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
    bob = pose.get("bob", 0)
    hip_y = foot - leg - bob
    body_cy = hip_y - bh * 0.45
    head_cy = body_cy - bh * 0.5 - hr * 0.78

    step = pose["step"]
    lean = pose["lean"]

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
    if fat:
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
        p.poly([(hx - hr * 0.6, head_cy + hr * 0.2), (hx + hr * 0.9, head_cy + hr * 0.15), (hx + hr * 0.3, head_cy + hr * 1.05)], rgba("#8B5A2B"))
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
        if attack:
            open_mouth(hr * 0.32, hr * 0.28)
        else:
            p.rect(ex - hr * 0.26, my - hr * 0.07, ex + hr * 0.26, my + hr * 0.12, rgba("#FFFFFF"), radius=2)   # 앙다문 이
            p.stick(ex, my - hr * 0.07, ex, my + hr * 0.12, 1.4)
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


def corpse(u):
    """시체: 쓰러진 모습 (눈 X) + 바닥에 붙게 아래로"""
    img = draw_unit(u, IDLE_POSES[0], dead=True)
    img = img.rotate(90, resample=Image.BICUBIC, expand=False)
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.alpha_composite(img, (0, int(UNIT_CELL * 0.18)))
    return out


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


def units_atlas():
    atlas = Image.new("RGBA", (UNIT_CELL * UNIT_COLS, UNIT_CELL * UNIT_ROWS), (0, 0, 0, 0))
    for row, u in enumerate(UNITS):
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

def fx_atlas():
    C = 128
    atlas = Image.new("RGBA", (C * 8, C * 2), (0, 0, 0, 0))
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
    return atlas


# ------------------------------------------------------------------------------------------------
# 건물

def building(kind):
    C = 256
    p = Pen(C, C, lw=4.5)
    base = C * 0.9
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
    return p.done(C, C)


def buildings_atlas():
    C = 256
    atlas = Image.new("RGBA", (C * 8, C), (0, 0, 0, 0))
    for k in range(6):
        atlas.alpha_composite(building(k), (k * C, 0))
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
    elif i in (13, 14, 15):   # 패널 (9칸 늘리기용. 모서리 = 칸의 1/4)
        fill = {13: rgba("#5C3B28"), 14: rgba("#2E1E16", 235), 15: rgba("#6E4A30")}[i]
        p = Pen(C, C, lw=0)
        p.rect(4, 4, C - 4, C - 4, INK, radius=22, line=False)
        p.rect(10, 10, C - 10, C - 10, fill, radius=17, line=False)
        hl = shade(fill, 1.35)
        p.rect(14, 14, C - 14, 26, (hl[0], hl[1], hl[2], 140), radius=6, line=False)
        return p.done(C, C)
    return p.done(C, C)


def ui_atlas():
    C = 128
    atlas = Image.new("RGBA", (C * 8, C * 2), (0, 0, 0, 0))
    for i in range(16):
        atlas.alpha_composite(ui_icon(i), ((i % 8) * C, (i // 8) * C))
    return atlas


# ------------------------------------------------------------------------------------------------
# 바닥

def ground():
    W, H = 2048, 740
    rnd = random.Random(3)
    img = Image.new("RGBA", (W, H), rgba("#5E8F3A"))
    d = ImageDraw.Draw(img)
    # 풀 얼룩
    for _ in range(900):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        r = rnd.uniform(10, 46)
        c = rnd.choice(["#58873A", "#67993F", "#527F34", "#6CA144"])
        d.ellipse([x - r, y - r * 0.6, x + r, y + r * 0.6], fill=rgba(c))
    # 가운데 흙길
    band = Image.new("L", (W, H), 0)
    bd = ImageDraw.Draw(band)
    for x in range(0, W, 8):
        top = H * 0.3 + math.sin(x * 0.006) * 24 + rnd.uniform(-6, 6)
        bot = H * 0.72 + math.sin(x * 0.005 + 1.3) * 26 + rnd.uniform(-6, 6)
        bd.rectangle([x, top, x + 8, bot], fill=255)
    band = band.filter(ImageFilter.GaussianBlur(10))
    dirt = Image.new("RGBA", (W, H), rgba("#8C6B45"))
    dd = ImageDraw.Draw(dirt)
    for _ in range(1400):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        r = rnd.uniform(3, 14)
        dd.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=rgba(rnd.choice(["#7E6040", "#977552", "#856544"])))
    img = Image.composite(dirt, img, band)
    d = ImageDraw.Draw(img)
    # 풀 포기
    for _ in range(1600):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        if band.getpixel((int(x), int(y))) > 80:
            continue
        for k in range(3):
            d.line([x + k * 3, y, x + k * 3 + rnd.uniform(-3, 3), y - rnd.uniform(6, 12)], fill=rgba("#3F6B26"), width=2)
    # 진지 (성벽 왼쪽): 다져진 흙
    camp = int(W * 520 / 3600)
    d.rectangle([0, 0, camp, H], fill=rgba("#7A5E3C"))
    for _ in range(500):
        x, y = rnd.uniform(0, camp), rnd.uniform(0, H)
        r = rnd.uniform(4, 16)
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=rgba(rnd.choice(["#6E5436", "#856845", "#735838"])))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    jobs = [("Units.png", units_atlas), ("Fx.png", fx_atlas), ("Buildings.png", buildings_atlas), ("UI.png", ui_atlas), ("Ground.png", ground)]
    for name, make in jobs:
        make().save(os.path.join(OUT, name))
        print("wrote", name)


if __name__ == "__main__":
    main()
