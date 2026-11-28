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
    # 요리사 대장: 국자 든 팔(앞), 등에 진 솥(뒤), 요리사 모자, 두 다리
    "HeroCook": dict(
        feet=(340, 862),
        parts={
            "cape": dict(poly=[(30, 455), (200, 455), (215, 655), (30, 655)], pivot=(190, 560), blur=14),
            "back_leg": dict(poly=[(140, 760), (275, 760), (275, 865), (140, 865)], pivot=(220, 770)),
            "front_leg": dict(poly=[(360, 760), (525, 760), (525, 865), (360, 865)], pivot=(420, 770)),
            "crown": dict(poly=[(60, 55), (425, 55), (425, 330), (190, 335), (60, 300)], pivot=(300, 340), blur=24),
            "sword": dict(poly=[(455, 470), (530, 410), (560, 300), (590, 0), (765, 0), (765, 260), (650, 300), (610, 470), (575, 590), (520, 590), (470, 530)], pivot=(470, 470), blur=8),
        },
        windup=-30, strike=85,
    ),
    # 냄비 투척꾼: 뚜껑 든 팔(치켜듦), 폭탄 든 팔(망토 자리), 냄비 투구, 두 다리
    "HeroPot": dict(
        feet=(450, 845),
        parts={
            "cape": dict(poly=[(600, 330), (904, 330), (904, 615), (600, 615), (585, 500)], pivot=(600, 525), blur=12),
            "back_leg": dict(poly=[(265, 735), (425, 735), (425, 847), (265, 847)], pivot=(350, 745)),
            "front_leg": dict(poly=[(470, 735), (645, 735), (645, 847), (470, 847)], pivot=(540, 745)),
            "crown": dict(poly=[(280, 150), (705, 150), (705, 375), (600, 335), (320, 355), (280, 300)], pivot=(460, 365), blur=24),
            "sword": dict(poly=[(0, 0), (345, 0), (345, 300), (285, 420), (335, 530), (280, 565), (215, 425), (0, 405)], pivot=(300, 525), blur=8),
        },
        windup=-35, strike=90,
    ),
    # 멧돼지: 큰 머리, 몽둥이 든 팔(앞), 꼬리(망토 자리), 두 다리
    "Boar": dict(
        feet=(255, 840),
        parts={
            "cape": dict(poly=[(0, 595), (125, 595), (130, 700), (0, 700)], pivot=(115, 640), blur=12),
            "back_leg": dict(poly=[(105, 690), (225, 690), (225, 844), (105, 844)], pivot=(165, 700)),
            "front_leg": dict(poly=[(285, 690), (410, 690), (410, 844), (285, 844)], pivot=(345, 700)),
            "crown": dict(poly=[(50, 0), (480, 0), (500, 250), (470, 400), (380, 445), (180, 445), (90, 400), (40, 260)], pivot=(270, 445), blur=24),
            "sword": dict(poly=[(405, 470), (480, 440), (545, 160), (695, 160), (695, 270), (575, 600), (470, 625), (420, 560)], pivot=(410, 500), blur=8),
        },
        windup=-28, strike=58, center=True,
    ),
    # 미친 다람쥐: 큰 꼬리(망토 자리), 솔방울 든 팔, 머리, 두 다리
    "Squirrel": dict(
        feet=(420, 835),
        parts={
            "cape": dict(poly=[(0, 0), (330, 0), (300, 250), (245, 450), (300, 700), (0, 700)], pivot=(280, 450), blur=22),
            "back_leg": dict(poly=[(275, 680), (385, 680), (385, 838), (275, 838)], pivot=(330, 690)),
            "front_leg": dict(poly=[(445, 680), (595, 680), (595, 838), (445, 838)], pivot=(510, 690)),
            "crown": dict(poly=[(265, 50), (600, 40), (670, 300), (640, 430), (500, 460), (320, 460), (262, 330)], pivot=(450, 455), blur=24),
            "sword": dict(poly=[(525, 450), (640, 270), (830, 270), (830, 485), (700, 560), (565, 525)], pivot=(530, 470), blur=8),
        },
        windup=-25, strike=55,
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
RIGS.update({
    # 궁수: 깃털 꽂은 초록 투구, 활 든 팔(앞), 화살 든 팔·화살통(망토 자리), 두 다리
    "Archer": dict(
        feet=(330, 668),
        parts={
            "cape": dict(poly=[(0, 100), (165, 100), (160, 170), (140, 260), (200, 340), (210, 445), (125, 455), (0, 425)], pivot=(190, 400), blur=14),
            "back_leg": dict(poly=[(222, 605), (300, 605), (300, 675), (222, 675)], pivot=(262, 610)),
            "front_leg": dict(poly=[(380, 605), (452, 605), (452, 675), (380, 675)], pivot=(412, 610)),
            "crown": dict(poly=[(120, 0), (285, 0), (275, 75), (400, 75), (475, 125), (505, 215), (420, 200), (300, 245), (205, 320), (175, 380), (168, 170), (165, 100), (120, 60)], pivot=(320, 240), blur=20),
            "sword": dict(poly=[(500, 372), (535, 65), (678, 65), (678, 675), (528, 675), (555, 425), (500, 442)], pivot=(505, 405), blur=8),
        },
        windup=-8, strike=12,
    ),
    # 방패병: 양동이 투구, 몽둥이 든 팔(뒤), 나무 방패(망토 자리로 흔든다), 두 다리
    "Shield": dict(
        feet=(312, 596),
        parts={
            "cape": dict(poly=[(458, 122), (692, 122), (692, 601), (448, 601), (452, 400)], pivot=(470, 380), blur=12),
            "back_leg": dict(poly=[(215, 545), (292, 545), (292, 601), (215, 601)], pivot=(255, 548)),
            "front_leg": dict(poly=[(345, 545), (422, 545), (422, 601), (345, 601)], pivot=(385, 548)),
            "crown": dict(poly=[(148, 8), (422, 8), (470, 212), (412, 248), (300, 232), (200, 248), (162, 182)], pivot=(310, 240), blur=20),
            "sword": dict(poly=[(0, 82), (112, 82), (192, 318), (208, 388), (160, 398), (100, 348), (0, 172)], pivot=(182, 372), blur=8),
        },
        windup=-30, strike=65, center=True, fit=0.9,
    ),
    # 망치꾼: 뿔 투구, 쇠망치 든 팔(뒤), 앞 팔(망토 자리), 두 다리
    "Brute": dict(
        feet=(385, 596),
        parts={
            "cape": dict(poly=[(555, 280), (633, 280), (633, 432), (555, 432)], pivot=(565, 300), blur=12),
            "back_leg": dict(poly=[(260, 540), (336, 540), (336, 600), (260, 600)], pivot=(300, 545)),
            "front_leg": dict(poly=[(440, 540), (516, 540), (516, 600), (440, 600)], pivot=(476, 545)),
            "crown": dict(poly=[(238, 12), (562, 12), (562, 192), (450, 182), (330, 186), (248, 198)], pivot=(400, 190), blur=20),
            "sword": dict(poly=[(0, 0), (215, 0), (208, 88), (168, 118), (218, 258), (242, 372), (208, 388), (150, 300), (122, 168), (0, 168)], pivot=(192, 292), blur=8),
        },
        windup=-4, strike=62, center=True, fit=0.84,
    ),
    # 곰 족장 (산지 보스): 나뭇가지 관 쓴 머리, 통나무 몽둥이 든 팔(앞), 털 망토. 망토·굵은 다리는 떼지 않고 몸째 들썩인다
    "BearChief": dict(
        feet=(350, 892),
        parts={
            "crown": dict(poly=[(150, 0), (520, 0), (532, 150), (500, 300), (440, 372), (260, 372), (200, 300), (150, 150)], pivot=(345, 372), blur=24),
            "sword": dict(poly=[(590, 420), (660, 38), (821, 38), (821, 300), (740, 618), (640, 618), (600, 540)], pivot=(612, 470), blur=8),
        },
        windup=-22, strike=34, center=True,
    ),
})

CENTER_FIT = 0.96  # 칸 너비 중 그림 너비 상한
FILL = 0.84        # 칸 높이 중 그림 비율 (generate_art.ART_FILL과 같게)
FOOT = 0.92        # 칸에서 발 높이 (generate_art.FOOT)
PAD = 160          # 휘어 나간 칼·망토가 잘리지 않게 둘레에 두는 여백 (원화 픽셀)
BLUR = {"cape": 26, "back_leg": 14, "front_leg": 14, "crown": 30, "sword": 8}   # 부위 경계를 흐리는 폭 (원화 픽셀)
STILL_PARTS = ("crown",)   # 몸에 붙여 같이 움직이는 부위 (머리·모자를 따로 돌리면 이마에 틈이 갈라진다)
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
        if part in STILL_PARTS:
            continue
        mask = Image.new("L", padded.size, 0)
        # 원화 끝에 닿은 꼭짓점은 여백 끝까지 밀어 낸다 (끝에 걸친 무기 조각이 몸에 남지 않게)
        sw, sh = source.size
        edge = lambda v, size: -PAD if v <= 0 else (size + PAD if v >= size - 1 else v)
        ImageDraw.Draw(mask).polygon([(edge(x, sw) + PAD, edge(y, sh) + PAD) for x, y in spec["poly"]], fill=255)
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
    scale = min(out * FILL / source.size[1], out * rig.get("fit", CENTER_FIT) / source.size[0])
    small = full.resize((max(1, round(full.size[0] * scale * sx)), max(1, round(full.size[1] * scale * sy))), Image.LANCZOS)
    cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
    # center: 무기가 한쪽으로 크게 뻗은 원화는 발 대신 그림 가운데를 칸 가운데에 (휘두를 때 칸 밖으로 안 잘리게)
    ax = source.size[0] / 2 + PAD if rig.get("center") else fx
    ox = round(out / 2 - ax * scale * sx + q["dx"] * scale)
    oy = round(out * FOOT - fy * scale * sy + q["dy"] * scale)
    cell.paste(small, (ox, oy), small)
    if action == "corpse":
        lying = cell.rotate(90, resample=Image.BICUBIC)
        cell = Image.new("RGBA", (out, out), (0, 0, 0, 0))
        cell.alpha_composite(lying, (0, int(out * 0.18)))
    return cell
