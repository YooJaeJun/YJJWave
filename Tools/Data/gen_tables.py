"""Tuning.csv / UILayout.csv 와 그 USTRUCT(Source/YJJWave/Data/YJJTuning.h)를 한 곳에서 만든다.

값을 바꿀 때는 CSV를 직접 고쳐도 된다 (이 스크립트는 이름을 늘릴 때 쓴다).
이미 CSV에 있는 값은 덮어쓰지 않는다 → 새 이름만 기본값으로 붙는다.
"""
import csv
import io
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "Content", "YJJWave", "Data")
HEADER = os.path.join(ROOT, "Source", "YJJWave", "Data", "YJJTuning.h")

# (이름, 기본값, 정수?, 설명). 값이 None이면 구역 제목
TUNING = [
    ("# 판", None, False, ""),
    ("SimStep", 0.0333333, False, "시뮬 고정 틱 (초)"),
    ("MaxStepsPerFrame", 8, True, "한 프레임에 도는 최대 틱 (배속 포함)"),
    ("SpeedSteps", 3, True, "배속 단계 수 (x1 x2 x4)"),
    ("SpeedFactor", 2, False, "단계마다 곱하는 배속"),
    ("UnitCapacity", 40000, True, "유닛 슬롯 수 (양 팀 합)"),
    ("MaxUnitsPerTeam", 18000, True, "편마다 최대 유닛 (넘으면 생산을 쉰다)"),
    ("FieldWidth", 3600, False, "전장 너비"),
    ("FieldHeight", 1300, False, "전장 높이"),
    ("FieldMargin", 30, False, "유닛이 못 가는 위아래·양 끝 여백"),
    ("CampEdgeX", 520, False, "진지 오른쪽 끝 x (유성은 이보다 오른쪽에만)"),
    ("HeroHomeX", 640, False, "영웅이 지키는 자리 x (세로는 가운데)"),
    ("HeroLeash", 380, False, "영웅이 자리에서 이만큼까지만 쫓아간다"),
    ("HeroArrive", 16, False, "영웅이 자리로 돌아왔다고 보는 거리"),
    ("HeroRegen", 12, False, "영웅 초당 체력 회복"),
    ("StartGold", 120, True, "시작 금화"),
    ("PassiveGold", 2, False, "초당 기본 금화"),
    ("LevelHPGrowth", 1.10, False, "웨이브마다 적 체력 배율"),
    ("LevelDamageGrowth", 1.06, False, "웨이브마다 적 공격력 배율"),
    ("EnemySpawnInset", 40, False, "적이 나오는 곳: 오른쪽 끝에서 이만큼 안"),
    ("SpawnScatter", 30, False, "생산 위치 흩뿌림"),
    ("EnemySpawnBand", 0.6, False, "적이 나오는 띠 높이 (전장 높이 비율, 가운데 기준)"),
    ("MaxPendingDeaths", 20000, True, "HUD가 안 가져간 죽음 기록 최대"),
    ("# 진지", None, False, ""),
    ("PlotColumns", 4, True, "건설 칸 가로 수"),
    ("PlotRows", 6, True, "건설 칸 세로 수"),
    ("PlotStartX", 70, False, "첫 칸 x"),
    ("PlotSpacingX", 120, False, "칸 가로 간격"),
    ("PlotSpacingY", 190, False, "칸 세로 간격"),
    ("PlotSize", 150, False, "건설 먼지 연출 크기"),
    ("PlotPulseTime", 0.3, False, "병사를 내보낼 때 들썩이는 시간"),
    ("MaxBuildingLevel", 20, True, "병영 최대 레벨"),
    ("# 유닛 움직임", None, False, ""),
    ("GridCellSize", 24, False, "이웃 찾기 격자 칸 크기 (작은 유닛 지름의 1.5~2배)"),
    ("RetargetTicks", 8, True, "몇 틱마다 대상을 다시 찾나 (유닛마다 엇갈림)"),
    ("ReorderTicks", 30, True, "몇 틱마다 유닛 슬롯을 격자 칸 순서로 다시 모으나 (캐시 적중, 0이면 끔)"),
    ("MaxNeighbors", 28, True, "밀어내기 검사 최대 이웃 수"),
    ("SeparationStiffness", 0.5, False, "겹친 만큼 밀어내는 비율 (틱마다)"),
    ("MaxPushPerTick", 3, False, "한 틱에 밀려나는 최대 거리"),
    ("UnitAcceleration", 8, False, "원하는 속도로 붙는 빠르기 (/초)"),
    ("ImpulseDamping", 5, False, "밀쳐진 속도가 줄어드는 빠르기 (/초)"),
    ("LateralDrift", 0.25, False, "행군할 때 위아래로 흔들리는 정도 (속도 비율)"),
    ("BigUnitRadius", 20, False, "이 반지름 이상은 큰 유닛(보스): 모두가 따로 밀어내기 검사"),
    ("ArrowHitRadius", 18, False, "대상이 먼저 죽은 화살이 다른 적을 맞히는 반경"),
    ("MultishotSpread", 80, False, "추가 화살이 노리는 대상 주변 반경"),
    ("HitFlashTime", 0.12, False, "맞으면 붉게 깜빡이는 시간"),
    ("AttackAnimTime", 0.2, False, "공격 그림을 보여 주는 시간"),
    ("DeathFling", 140, False, "죽을 때 맞은 방향으로 날아가는 속도"),
    ("# 스펠", None, False, ""),
    ("MeteorDamage", 400, False, "유성 피해"),
    ("MeteorRadius", 140, False, "유성 반경"),
    ("MeteorKnockback", 1800, False, "유성 밀쳐내기 (질량으로 나눈다)"),
    ("MeteorFallTime", 0.6, False, "유성이 떨어지는 시간"),
    ("MeteorCooldown", 12, False, "유성 재사용 대기"),
    ("RallyDuration", 6, False, "돌격 함성 지속"),
    ("RallyMoveSpeed", 1.5, False, "함성 중 이동 속도 배율"),
    ("RallyAttackSpeed", 1.6, False, "함성 중 공격 속도 배율"),
    ("RallyCooldown", 25, False, "함성 재사용 대기"),
    ("MinCooldownScale", 0.3, False, "재사용 대기 감소의 바닥"),
    ("BlastEffectTime", 0.5, False, "유성 폭발 연출 시간"),
    ("RallyEffectTime", 0.8, False, "함성 고리 연출 시간"),
    ("BuildEffectTime", 0.5, False, "건설 먼지 연출 시간"),
    ("RescueEffectTime", 0.6, False, "최후의 저항·부활 고리 연출 시간"),
    ("RescueEffectSize", 70, False, "최후의 저항·부활 고리 크기"),
    ("# 특성 성좌", None, False, ""),
    ("PerkCostBase", 15, False, "첫 별 영혼 비용"),
    ("PerkCostGrowth", 1.12, False, "별을 찍을 때마다 비용 배율"),
    ("# 오토파일럿·벤치마크", None, False, ""),
    ("BotThinkInterval", 0.5, False, "봇 판단 간격 (게임 시간)"),
    ("BotCameraFollow", 1.5, False, "봇 카메라가 전선을 따라가는 빠르기"),
    ("BotCameraZoom", 0.55, False, "봇 카메라 확대"),
    ("BotMeteorLead", 60, False, "봇 유성: 적 앞줄보다 이만큼 뒤"),
    ("BotRallyRatio", 1.2, False, "적 수가 아군의 이 배를 넘으면 함성"),
    ("BotExitDelay", 2, False, "끝나고 종료까지 기다리는 실제 시간"),
    ("BenchDepth", 0.25, False, "벤치마크: 양 끝에서 전장 너비의 이 비율 안에 깐다"),
    ("BenchCameraZoom", 0.5, False, "벤치마크 카메라 확대 (전장 전체가 보이게 = 그리기 최악)"),
]

LAYOUT = [
    ("# 아틀라스 (Tools/Art/generate_art.py 와 같아야 한다)", None, False, ""),
    ("UnitCell", 128, False, "Units.png 칸 크기"),
    ("FxCell", 128, False, "Fx.png 칸 크기"),
    ("BuildingCell", 256, False, "Buildings.png 칸 크기"),
    ("UiCell", 128, False, "UI.png 칸 크기"),
    ("ProjectileRow", 15, False, "Units.png 투사체 줄"),
    ("AnimWalkFirst", 0, False, "Units.png 걷기 첫 칸"),
    ("AnimWalkFrames", 6, False, "걷기 칸 수 (그림 크기만큼 걸을 때 한 바퀴)"),
    ("AnimAttackFirst", 6, False, "공격 첫 칸"),
    ("AnimAttackFrames", 4, False, "공격 칸 수 (들어 올림 → 때림 → 돌아옴)"),
    ("AnimHitFirst", 10, False, "맞음 첫 칸"),
    ("AnimHitFrames", 2, False, "맞음 칸 수"),
    ("AnimCorpse", 12, False, "시체 칸"),
    ("AnimIdleFirst", 13, False, "숨쉬기 첫 칸"),
    ("AnimIdleFrames", 2, False, "숨쉬기 칸 수"),
    ("AnimIdleSpeed", 6, False, "이 속력보다 느리면 숨쉬기 그림"),
    ("AnimIdleFps", 2.5, False, "숨쉬기 초당 칸"),
    ("EmptyPlotCol", 4, False, "Buildings.png 빈 칸"),
    ("FxShadow", 0, False, "Fx 칸: 그림자"),
    ("FxBloodFirst", 1, False, "Fx 칸: 첫 피 웅덩이"),
    ("FxBloodCount", 4, False, "피 웅덩이 종류 수"),
    ("FxScorch", 5, False, "Fx 칸: 그을음"),
    ("FxDust", 6, False, "Fx 칸: 먼지"),
    ("FxRing", 7, False, "Fx 칸: 고리"),
    ("FxMeteor", 8, False, "Fx 칸: 운석"),
    ("FxBlast", 9, False, "Fx 칸: 폭발"),
    ("UiCoin", 0, False, "UI 칸: 금화"),
    ("UiSoul", 1, False, "UI 칸: 영혼"),
    ("UiSword", 2, False, "UI 칸: 아군 수"),
    ("UiSkull", 3, False, "UI 칸: 적 수"),
    ("UiMeteor", 4, False, "UI 칸: 유성"),
    ("UiRally", 5, False, "UI 칸: 함성"),
    ("UiStar", 6, False, "UI 칸: 작은 별"),
    ("UiBigStar", 7, False, "UI 칸: 큰 별"),
    ("UiHero", 8, False, "UI 칸: 영웅 얼굴"),
    ("UiPanel", 13, False, "UI 칸: 패널 (9칸 늘리기)"),
    ("UiPanelDark", 14, False, "UI 칸: 어두운 패널"),
    ("UiCard", 15, False, "UI 칸: 카드"),
    ("PanelCornerFraction", 0.25, False, "패널 그림에서 모서리가 차지하는 비율"),
    ("PanelCorner", 22, False, "화면에 그리는 패널 모서리 크기"),
    ("# 카메라", None, False, ""),
    ("ZoomDefault", 0.7, False, "시작 확대"),
    ("ZoomMin", 0.3, False, "최소 확대 (실제로는 전장이 화면을 꽉 채우는 값 아래로는 안 간다)"),
    ("ZoomMax", 2.0, False, "최대 확대"),
    ("ZoomStep", 1.15, False, "휠 한 칸 확대 배율"),
    ("CameraStartX", 1100, False, "시작 시점 x"),
    ("PanSpeed", 900, False, "키보드·가장자리 이동 속도 (화면 픽셀/초)"),
    ("EdgePan", 1, False, "1이면 화면 가장자리에 마우스를 대면 이동"),
    ("EdgePanPixels", 12, False, "가장자리 폭"),
    ("# 전장 그리기", None, False, ""),
    ("GroundTargetWidth", 4096, False, "피·시체가 쌓이는 바닥 렌더 타깃 너비 (높이는 전장 비율)"),
    ("FootAnchor", 0.92, False, "유닛 그림에서 발 위치 (위=0 아래=1)"),
    ("CullMargin", 120, False, "화면 밖 여유 (월드)"),
    ("SortBuckets", 512, False, "앞뒤 정렬 버킷 수"),
    ("ShadowMinZoom", 0.45, False, "이보다 멀리 보면 그림자 생략"),
    ("ShadowAlpha", 0.45, False, "그림자 진하기"),
    ("ShadowWidth", 0.7, False, "그림자 너비 (그림 크기 비율)"),
    ("ShadowHeight", 0.28, False, "그림자 높이"),
    ("ProjectileSize", 30, False, "화살 그림 크기"),
    ("ArrowArcPerDistance", 0.25, False, "화살 포물선 높이 (거리 비율)"),
    ("ArrowArcMax", 90, False, "화살 포물선 최대 높이"),
    ("PlotDrawSize", 120, False, "병영 그림 크기"),
    ("PlotHoverScale", 1.08, False, "마우스를 올린 병영 확대"),
    ("PlotBounce", 0.12, False, "생산할 때 들썩임"),
    ("PlotAnchor", 0.9, False, "병영 그림에서 바닥 위치"),
    ("PlotLabelOffset", 14, False, "레벨 글자 위치 (발밑에서 아래로)"),
    ("PlotLabelSize", 20, False, "레벨 글자 크기"),
    ("PlotLabelZoomMin", 0.6, False, "멀리 볼 때 레벨 글자 최소 배율"),
    ("PlotPickRadius", 55, False, "병영 클릭 반경"),
    ("PlotPickLift", 50, False, "클릭 판정 중심: 발밑에서 위로"),
    ("MaxDying", 1500, False, "날아가는 시체 최대 (넘으면 바로 눕는다)"),
    ("DeathJitter", 40, False, "죽을 때 흩어지는 속도"),
    ("DeathRise", 260, False, "죽을 때 튀어 오르는 속도 (질량 제곱근으로 나눈다)"),
    ("DeathRiseMin", 0.5, False, "튀어 오르는 속도 최소 비율"),
    ("DeathSpin", 9, False, "날아가며 도는 속도 (라디안/초)"),
    ("DeathGravity", 1100, False, "시체 중력"),
    ("DeathDrag", 2.5, False, "시체 감속"),
    ("CorpseShade", 0.8, False, "바닥에 구운 시체 밝기"),
    ("BloodScatter", 0.2, False, "피 웅덩이 흩어짐 (그림 크기 비율)"),
    ("BloodSizeMin", 0.8, False, "피 웅덩이 최소 크기 비율"),
    ("BloodSizeMax", 1.4, False, "피 웅덩이 최대 크기 비율"),
    ("BloodAlphaMin", 0.5, False, "피 웅덩이 최소 진하기"),
    ("CraterScale", 2.2, False, "유성 그을음 크기 (반경 배율)"),
    ("AimRingStart", 1.4, False, "유성 표시 고리 시작 배율"),
    ("AimAlpha", 0.7, False, "유성 조준 고리 진하기"),
    ("MeteorOffsetX", 260, False, "운석이 떨어지기 시작하는 x 차이"),
    ("MeteorFallHeight", 700, False, "운석이 떨어지기 시작하는 높이"),
    ("MeteorDrawSize", 120, False, "운석 그림 크기"),
    ("BlastStartScale", 0.6, False, "폭발 시작 크기"),
    ("BlastEndScale", 1.3, False, "폭발 끝 크기"),
    ("DustStartScale", 0.6, False, "먼지 시작 크기"),
    ("DustEndScale", 1.4, False, "먼지 끝 크기"),
    ("# UI (1920x1080 가상 좌표)", None, False, ""),
    ("FrameMsSmoothing", 0.05, False, "프레임 시간 표시 부드럽게"),
    ("FontPointScale", 0.75, False, "글자 크기 → 폰트 포인트"),
    ("MinFontSize", 9, False, "최소 폰트 포인트"),
    ("TextOutline", 0.08, False, "글자 외곽선 (글자 크기 비율)"),
    ("TitleText", 54, False, "제목 글자"),
    ("BigText", 34, False, "큰 글자"),
    ("MidText", 26, False, "중간 글자"),
    ("SmallText", 20, False, "작은 글자"),
    ("TinyText", 16, False, "아주 작은 글자"),
    ("PanelPad", 14, False, "패널 안쪽 여백"),
    ("SidePanelW", 420, False, "좌측 패널 너비 (모든 UI가 여기. 전장은 나머지 전부)"),
    ("SectionGap", 12, False, "패널 칸 사이 간격"),
    ("HeaderH", 120, False, "스테이지·웨이브 칸 높이"),
    ("VsH", 100, False, "아군 VS 적 칸 높이"),
    ("BossH", 54, False, "보스 칸 높이"),
    ("ResH", 76, False, "금화·영혼 칸 높이"),
    ("SpellH", 120, False, "스펠·특성 칸 높이"),
    ("DebugLineGap", 4, False, "성능 줄 사이"),
    ("IconSize", 48, False, "아이콘 크기"),
    ("IconGap", 8, False, "아이콘과 글자 사이"),
    ("SmallIconScale", 0.5, False, "작은 아이콘 배율"),
    ("BarH", 18, False, "막대 높이"),
    ("BarBorder", 3, False, "막대 테두리"),
    ("BarShineHeight", 0.35, False, "막대 반짝 띠 높이 비율"),
    ("BarShineAlpha", 0.25, False, "막대 반짝 띠 진하기"),
    ("BarTextLift", -2, False, "막대 위 글자 y 보정"),
    ("SpeedButtonW", 110, False, "배속 버튼 너비"),
    ("BossBarSplit", 0.35, False, "보스 이름 / 막대 나누는 비율"),
    ("CardH", 136, False, "카드 높이"),
    ("CardGap", 10, False, "카드 간격"),
    ("CardIcon", 72, False, "카드 아이콘 크기"),
    ("TipW", 380, False, "설명창 너비"),
    ("TipLineH", 30, False, "설명창 줄 높이"),
    ("TipOffset", 24, False, "설명창: 마우스 오른쪽으로"),
    ("GameOverW", 760, False, "패배 패널 너비"),
    ("GameOverH", 300, False, "결과 창 높이"),
    ("ResultButtonH", 64, False, "결과 창 버튼 높이"),
    ("GlowSpeed", 5, False, "반짝임 속도"),
    ("NumberMan", 10000, False, "이 이상은 만 단위로"),
    ("NumberEok", 100000000, False, "이 이상은 억 단위로"),
    ("# 특성 성좌 화면", None, False, ""),
    ("TreeTitleY", 24, False, "성좌 제목 y"),
    ("TreeCenterY", 570, False, "성좌 중심 y"),
    ("TreeScale", 0.42, False, "성좌 좌표 → 화면 배율"),
    ("PerkSmall", 12, False, "작은 별 반지름"),
    ("PerkBig", 22, False, "중간 별 반지름"),
    ("PerkKeystone", 30, False, "쐐기 반지름"),
    ("TipKindW", 110, False, "설명창 오른쪽 위 별 종류 글자 너비"),
    ("PerkHoverScale", 1.3, False, "마우스 올린 별 확대"),
    ("LinkWidth", 2, False, "선 굵기"),
    ("LinkTakenWidth", 4, False, "찍은 선 굵기"),
]


def read_existing(path):
    values = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8-sig", newline="") as f:
            for row in csv.reader(f):
                if len(row) >= 2 and row[0] and row[0] != "Name":
                    values[row[0]] = row[1]
    return values


def fmt(value, integer):
    if integer:
        return str(int(value))
    text = ("%.7f" % value).rstrip("0").rstrip(".")
    return text if text not in ("", "-0") else "0"


def write_csv(path, items):
    existing = read_existing(path)
    out = io.StringIO()
    w = csv.writer(out, lineterminator="\n")
    w.writerow(["Name", "Value", "Note"])
    for name, value, integer, note in items:
        if value is not None:
            w.writerow([name, existing.get(name, fmt(value, integer)), note])
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        f.write(out.getvalue())


def struct(name, doc, items):
    lines = ["/**", " * " + doc, " * 기본값은 두지 않는다 (CSV에 빠진 이름은 [Data] 오류 로그로 드러난다).", " */",
             "USTRUCT()", "struct " + name, "{", "\tGENERATED_BODY()", ""]
    for item_name, value, integer, note in items:
        if value is None:
            lines.append("\t// ---- " + item_name[2:] + " ----")
            continue
        kind, zero = ("int32", "0") if integer else ("float", "0.f")
        lines.append("\tUPROPERTY() %s %s = %s;   // %s" % (kind, item_name, zero, note))
    lines.append("};")
    return "\n".join(lines)


def main():
    os.makedirs(DATA, exist_ok=True)
    write_csv(os.path.join(DATA, "Tuning.csv"), TUNING)
    write_csv(os.path.join(DATA, "UILayout.csv"), LAYOUT)
    header = "\n\n".join([
        "#pragma once\n\n#include \"CoreMinimal.h\"\n#include \"YJJTuning.generated.h\"\n\n"
        "// Tools/Data/gen_tables.py 가 만든다. 이름을 늘릴 때는 스크립트에 추가하고 다시 돌린다.",
        struct("FYJJTuning", "게임 규칙의 낱개 수치. 값은 Content/YJJWave/Data/Tuning.csv (행 이름 = 변수 이름).", TUNING),
        struct("FYJJUILayout", "화면 배치·연출·아틀라스 약속 수치. 값은 Content/YJJWave/Data/UILayout.csv.", LAYOUT),
    ]) + "\n"
    with open(HEADER, "w", encoding="utf-8-sig", newline="\n") as f:
        f.write(header)
    print("Tuning %d, UILayout %d" % (sum(1 for t in TUNING if t[1] is not None), sum(1 for t in LAYOUT if t[1] is not None)))


main()
