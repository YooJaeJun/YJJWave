"""졸병 얼굴 표정 샘플 시트 (고르기용). generate_art.py 의 draw_face 스타일을 나란히 보여 준다.

  실행: Tools/Art/.venv/Scripts/python.exe Tools/Art/face_samples.py
  결과: Tools/Art/Samples/grunt_faces.png
    위 줄  : 크게 (기본·공격 순간)
    아래 줄: 게임 속 크기 (확대 0.85에서 졸병 약 47px) — 작게 봐도 표정이 읽히는지
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(__file__))
import generate_art as art  # noqa: E402

CONCEPT_OUT = os.path.join(os.path.dirname(__file__), "Samples", "concept_frames.png")

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONT = os.path.join(ROOT, "Content", "YJJWave", "Fonts", "DoHyeon-Regular.ttf")
OUT = os.path.join(os.path.dirname(__file__), "Samples", "grunt_faces.png")

STYLES = [
    ("plain", "지금 (비교)"),
    ("blank", "A 멍 (w 입)"),
    ("shout", "B 함성"),
    ("cat", "C 눈썹+w 입"),
    ("hotblood", "D 열혈"),
]
BIG = 300
SMALL = 56
LABEL_H = 44
BG = (94, 143, 58, 255)


def main():
    grunt = art.UNITS[0]
    idle = art.IDLE_POSES[0]
    strike = art.ATTACK_POSES[2]
    cols = len(STYLES)
    cell_w = BIG
    sheet = Image.new("RGBA", (cell_w * cols, LABEL_H + BIG * 2 + SMALL * 2 + 40), BG)
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(FONT, 30)
    for i, (style, label) in enumerate(STYLES):
        face = None if style == "plain" else style
        x = i * cell_w
        draw.text((x + 14, 8), label, font=font, fill=(255, 245, 220, 255), stroke_width=3, stroke_fill=(46, 28, 22, 255))
        sheet.alpha_composite(art.draw_unit(grunt, idle, out=BIG, face=face), (x, LABEL_H))
        sheet.alpha_composite(art.draw_unit(grunt, strike, out=BIG, face=face), (x, LABEL_H + BIG))
        # 게임 속 크기 (작게, 무리 지어)
        small_idle = art.draw_unit(grunt, idle, face=face).resize((SMALL, SMALL), Image.LANCZOS)
        small_hit = art.draw_unit(grunt, strike, face=face).resize((SMALL, SMALL), Image.LANCZOS)
        y = LABEL_H + BIG * 2 + 20
        for k in range(4):
            sheet.alpha_composite(small_idle if k % 2 == 0 else small_hit, (x + 20 + k * 62, y + (k % 2) * 12))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    sheet.save(OUT)
    print(OUT)



def concept():
    """확정 컨셉: 졸병(고양이 입·울먹·헤벌레)과 특수 병졸(열혈·함성)의 칸 줄"""
    names = ["Grunt", "Archer", "Shield", "Brute", "Hero"]
    cols = [0, 2, 4, 6, 8, 11, 14, 16, 19]   # 걷기·걷기·걷기·걷기·준비·때림·맞음·헤벌레·숨쉬기
    size = 170
    font = ImageFont.truetype(FONT, 26)
    sheet = Image.new("RGBA", (size * len(cols), LABEL_H + size * len(names)), BG)
    draw = ImageDraw.Draw(sheet)
    labels = ["걷기", "", "", "", "공격 준비", "때림 (함성)", "맞음 (울먹)", "밀림 (헤벌레)", "숨쉬기"]
    for i, label in enumerate(labels):
        draw.text((i * size + 10, 8), label, font=font, fill=(255, 245, 220, 255), stroke_width=3, stroke_fill=(46, 28, 22, 255))
    layout = art.FRAME_LAYOUT
    for r, name in enumerate(names):
        u = next(x for x in art.UNITS if x["name"] == name)
        for i, col in enumerate(cols):
            pose = layout[col] if col < len(layout) else art.IDLE_POSES[col - art.IDLE_COL]
            sheet.alpha_composite(art.draw_unit(u, pose, out=size), (i * size, LABEL_H + r * size))
    sheet.save(CONCEPT_OUT)
    print(CONCEPT_OUT)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "concept":
        concept()
    else:
        main()
