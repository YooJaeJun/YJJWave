#pragma once

#include "CoreMinimal.h"
#include "YJJTuning.generated.h"

// Tools/Data/gen_tables.py 가 만든다. 이름을 늘릴 때는 스크립트에 추가하고 다시 돌린다.

/**
 * 게임 규칙의 낱개 수치. 값은 Content/YJJWave/Data/Tuning.csv (행 이름 = 변수 이름).
 * 기본값은 두지 않는다 (CSV에 빠진 이름은 [Data] 오류 로그로 드러난다).
 */
USTRUCT()
struct FYJJTuning
{
	GENERATED_BODY()

	// ---- 판 ----
	UPROPERTY() float SimStep = 0.f;   // 시뮬 고정 틱 (초)
	UPROPERTY() int32 MaxStepsPerFrame = 0;   // 한 프레임에 도는 최대 틱 (배속 포함)
	UPROPERTY() int32 SpeedSteps = 0;   // 배속 단계 수 (x1 x2 x4)
	UPROPERTY() float SpeedFactor = 0.f;   // 단계마다 곱하는 배속
	UPROPERTY() int32 UnitCapacity = 0;   // 유닛 슬롯 수 (양 팀 합)
	UPROPERTY() int32 MaxUnitsPerTeam = 0;   // 편마다 최대 유닛 (넘으면 생산을 쉰다)
	UPROPERTY() float FieldWidth = 0.f;   // 전장 너비
	UPROPERTY() float FieldHeight = 0.f;   // 전장 높이
	UPROPERTY() float FieldMargin = 0.f;   // 유닛이 못 가는 위아래·양 끝 여백
	UPROPERTY() float WallX = 0.f;   // 성벽 x
	UPROPERTY() float WallHP = 0.f;   // 성벽 체력
	UPROPERTY() int32 StartGold = 0;   // 시작 금화
	UPROPERTY() float PassiveGold = 0.f;   // 초당 기본 금화
	UPROPERTY() float EndlessLevelDuration = 0.f;   // Waves.csv가 비었을 때 레벨 길이
	UPROPERTY() float EndlessRateGrowth = 0.f;   // 정의된 마지막 레벨 뒤 레벨마다 생산 배율
	UPROPERTY() float LevelHPGrowth = 0.f;   // 물결 레벨마다 적 체력 배율
	UPROPERTY() float LevelDamageGrowth = 0.f;   // 물결 레벨마다 적 공격력 배율
	UPROPERTY() float EnemySpawnInset = 0.f;   // 적이 나오는 곳: 오른쪽 끝에서 이만큼 안
	UPROPERTY() float SpawnScatter = 0.f;   // 생산 위치 흩뿌림
	UPROPERTY() float EnemySpawnBand = 0.f;   // 적이 나오는 띠 높이 (전장 높이 비율, 가운데 기준)
	UPROPERTY() int32 MaxPendingDeaths = 0;   // HUD가 안 가져간 죽음 기록 최대
	// ---- 진지 ----
	UPROPERTY() int32 PlotColumns = 0;   // 건설 칸 가로 수
	UPROPERTY() int32 PlotRows = 0;   // 건설 칸 세로 수
	UPROPERTY() float PlotStartX = 0.f;   // 첫 칸 x
	UPROPERTY() float PlotSpacingX = 0.f;   // 칸 가로 간격
	UPROPERTY() float PlotSpacingY = 0.f;   // 칸 세로 간격
	UPROPERTY() float PlotSize = 0.f;   // 건설 먼지 연출 크기
	UPROPERTY() float PlotPulseTime = 0.f;   // 병사를 내보낼 때 들썩이는 시간
	UPROPERTY() int32 MaxBuildingLevel = 0;   // 병영 최대 레벨
	// ---- 유닛 움직임 ----
	UPROPERTY() float GridCellSize = 0.f;   // 이웃 찾기 격자 칸 크기 (작은 유닛 지름의 1.5~2배)
	UPROPERTY() int32 RetargetTicks = 0;   // 몇 틱마다 대상을 다시 찾나 (유닛마다 엇갈림)
	UPROPERTY() int32 MaxNeighbors = 0;   // 밀어내기 검사 최대 이웃 수
	UPROPERTY() float SeparationStiffness = 0.f;   // 겹친 만큼 밀어내는 비율 (틱마다)
	UPROPERTY() float MaxPushPerTick = 0.f;   // 한 틱에 밀려나는 최대 거리
	UPROPERTY() float UnitAcceleration = 0.f;   // 원하는 속도로 붙는 빠르기 (/초)
	UPROPERTY() float ImpulseDamping = 0.f;   // 밀쳐진 속도가 줄어드는 빠르기 (/초)
	UPROPERTY() float LateralDrift = 0.f;   // 행군할 때 위아래로 흔들리는 정도 (속도 비율)
	UPROPERTY() float BigUnitRadius = 0.f;   // 이 반지름 이상은 큰 유닛(보스): 모두가 따로 밀어내기 검사
	UPROPERTY() float WallAttackReach = 0.f;   // 적이 성벽을 때리는 거리
	UPROPERTY() float ArrowHitRadius = 0.f;   // 대상이 먼저 죽은 화살이 다른 적을 맞히는 반경
	UPROPERTY() float MultishotSpread = 0.f;   // 추가 화살이 노리는 대상 주변 반경
	UPROPERTY() float HitFlashTime = 0.f;   // 맞으면 붉게 깜빡이는 시간
	UPROPERTY() float AttackAnimTime = 0.f;   // 공격 그림을 보여 주는 시간
	UPROPERTY() float DeathFling = 0.f;   // 죽을 때 맞은 방향으로 날아가는 속도
	// ---- 스펠 ----
	UPROPERTY() float MeteorDamage = 0.f;   // 유성 피해
	UPROPERTY() float MeteorRadius = 0.f;   // 유성 반경
	UPROPERTY() float MeteorKnockback = 0.f;   // 유성 밀쳐내기 (질량으로 나눈다)
	UPROPERTY() float MeteorFallTime = 0.f;   // 유성이 떨어지는 시간
	UPROPERTY() float MeteorCooldown = 0.f;   // 유성 재사용 대기
	UPROPERTY() float RallyDuration = 0.f;   // 돌격 함성 지속
	UPROPERTY() float RallyMoveSpeed = 0.f;   // 함성 중 이동 속도 배율
	UPROPERTY() float RallyAttackSpeed = 0.f;   // 함성 중 공격 속도 배율
	UPROPERTY() float RallyCooldown = 0.f;   // 함성 재사용 대기
	UPROPERTY() float MinCooldownScale = 0.f;   // 재사용 대기 감소의 바닥
	UPROPERTY() float BlastEffectTime = 0.f;   // 유성 폭발 연출 시간
	UPROPERTY() float RallyEffectTime = 0.f;   // 함성 고리 연출 시간
	UPROPERTY() float BuildEffectTime = 0.f;   // 건설 먼지 연출 시간
	// ---- 특성 성좌 ----
	UPROPERTY() float PerkCostBase = 0.f;   // 첫 별 영혼 비용
	UPROPERTY() float PerkCostGrowth = 0.f;   // 별을 찍을 때마다 비용 배율
	// ---- 오토파일럿·벤치마크 ----
	UPROPERTY() float BotThinkInterval = 0.f;   // 봇 판단 간격 (게임 시간)
	UPROPERTY() float BotCameraFollow = 0.f;   // 봇 카메라가 전선을 따라가는 빠르기
	UPROPERTY() float BotCameraZoom = 0.f;   // 봇 카메라 확대
	UPROPERTY() float BotMeteorLead = 0.f;   // 봇 유성: 적 앞줄보다 이만큼 뒤
	UPROPERTY() float BotRallyRatio = 0.f;   // 적 수가 아군의 이 배를 넘으면 함성
	UPROPERTY() float BotExitDelay = 0.f;   // 끝나고 종료까지 기다리는 실제 시간
	UPROPERTY() float BenchDepth = 0.f;   // 벤치마크: 양 끝에서 전장 너비의 이 비율 안에 깐다
	UPROPERTY() float BenchCameraZoom = 0.f;   // 벤치마크 카메라 확대 (전장 전체가 보이게 = 그리기 최악)
};

/**
 * 화면 배치·연출·아틀라스 약속 수치. 값은 Content/YJJWave/Data/UILayout.csv.
 * 기본값은 두지 않는다 (CSV에 빠진 이름은 [Data] 오류 로그로 드러난다).
 */
USTRUCT()
struct FYJJUILayout
{
	GENERATED_BODY()

	// ---- 아틀라스 (Tools/Art/generate_art.py 와 같아야 한다) ----
	UPROPERTY() float UnitCell = 0.f;   // Units.png 칸 크기
	UPROPERTY() float FxCell = 0.f;   // Fx.png 칸 크기
	UPROPERTY() float BuildingCell = 0.f;   // Buildings.png 칸 크기
	UPROPERTY() float UiCell = 0.f;   // UI.png 칸 크기
	UPROPERTY() float ProjectileRow = 0.f;   // Units.png 투사체 줄
	UPROPERTY() float EmptyPlotCol = 0.f;   // Buildings.png 빈 칸
	UPROPERTY() float WallCol = 0.f;   // Buildings.png 성벽 조각
	UPROPERTY() float FxShadow = 0.f;   // Fx 칸: 그림자
	UPROPERTY() float FxBloodFirst = 0.f;   // Fx 칸: 첫 피 웅덩이
	UPROPERTY() float FxBloodCount = 0.f;   // 피 웅덩이 종류 수
	UPROPERTY() float FxScorch = 0.f;   // Fx 칸: 그을음
	UPROPERTY() float FxDust = 0.f;   // Fx 칸: 먼지
	UPROPERTY() float FxRing = 0.f;   // Fx 칸: 고리
	UPROPERTY() float FxMeteor = 0.f;   // Fx 칸: 운석
	UPROPERTY() float FxBlast = 0.f;   // Fx 칸: 폭발
	UPROPERTY() float UiCoin = 0.f;   // UI 칸: 금화
	UPROPERTY() float UiSoul = 0.f;   // UI 칸: 영혼
	UPROPERTY() float UiSword = 0.f;   // UI 칸: 아군 수
	UPROPERTY() float UiSkull = 0.f;   // UI 칸: 적 수
	UPROPERTY() float UiMeteor = 0.f;   // UI 칸: 유성
	UPROPERTY() float UiRally = 0.f;   // UI 칸: 함성
	UPROPERTY() float UiStar = 0.f;   // UI 칸: 작은 별
	UPROPERTY() float UiBigStar = 0.f;   // UI 칸: 큰 별
	UPROPERTY() float UiWall = 0.f;   // UI 칸: 성벽
	UPROPERTY() float UiPanel = 0.f;   // UI 칸: 패널 (9칸 늘리기)
	UPROPERTY() float UiPanelDark = 0.f;   // UI 칸: 어두운 패널
	UPROPERTY() float UiCard = 0.f;   // UI 칸: 카드
	UPROPERTY() float PanelCornerFraction = 0.f;   // 패널 그림에서 모서리가 차지하는 비율
	UPROPERTY() float PanelCorner = 0.f;   // 화면에 그리는 패널 모서리 크기
	// ---- 카메라 ----
	UPROPERTY() float ZoomDefault = 0.f;   // 시작 확대
	UPROPERTY() float ZoomMin = 0.f;   // 최소 확대 (전장 전체)
	UPROPERTY() float ZoomMax = 0.f;   // 최대 확대
	UPROPERTY() float ZoomStep = 0.f;   // 휠 한 칸 확대 배율
	UPROPERTY() float CameraStartX = 0.f;   // 시작 시점 x
	UPROPERTY() float CameraMargin = 0.f;   // 전장 밖으로 보이는 여유
	UPROPERTY() float PanSpeed = 0.f;   // 키보드·가장자리 이동 속도 (화면 픽셀/초)
	UPROPERTY() float EdgePan = 0.f;   // 1이면 화면 가장자리에 마우스를 대면 이동
	UPROPERTY() float EdgePanPixels = 0.f;   // 가장자리 폭
	// ---- 전장 그리기 ----
	UPROPERTY() float GroundTargetWidth = 0.f;   // 피·시체가 쌓이는 바닥 렌더 타깃 너비 (높이는 전장 비율)
	UPROPERTY() float FootAnchor = 0.f;   // 유닛 그림에서 발 위치 (위=0 아래=1)
	UPROPERTY() float WalkBob = 0.f;   // 걸을 때 통통 튀는 높이 (그림 크기 비율)
	UPROPERTY() float CullMargin = 0.f;   // 화면 밖 여유 (월드)
	UPROPERTY() float SortBuckets = 0.f;   // 앞뒤 정렬 버킷 수
	UPROPERTY() float ShadowMinZoom = 0.f;   // 이보다 멀리 보면 그림자 생략
	UPROPERTY() float ShadowAlpha = 0.f;   // 그림자 진하기
	UPROPERTY() float ShadowWidth = 0.f;   // 그림자 너비 (그림 크기 비율)
	UPROPERTY() float ShadowHeight = 0.f;   // 그림자 높이
	UPROPERTY() float ProjectileSize = 0.f;   // 화살 그림 크기
	UPROPERTY() float ArrowArcPerDistance = 0.f;   // 화살 포물선 높이 (거리 비율)
	UPROPERTY() float ArrowArcMax = 0.f;   // 화살 포물선 최대 높이
	UPROPERTY() float PlotDrawSize = 0.f;   // 병영 그림 크기
	UPROPERTY() float PlotHoverScale = 0.f;   // 마우스를 올린 병영 확대
	UPROPERTY() float PlotBounce = 0.f;   // 생산할 때 들썩임
	UPROPERTY() float PlotAnchor = 0.f;   // 병영 그림에서 바닥 위치
	UPROPERTY() float PlotLabelOffset = 0.f;   // 레벨 글자 위치 (발밑에서 아래로)
	UPROPERTY() float PlotLabelSize = 0.f;   // 레벨 글자 크기
	UPROPERTY() float PlotLabelZoomMin = 0.f;   // 멀리 볼 때 레벨 글자 최소 배율
	UPROPERTY() float PlotPickRadius = 0.f;   // 병영 클릭 반경
	UPROPERTY() float PlotPickLift = 0.f;   // 클릭 판정 중심: 발밑에서 위로
	UPROPERTY() float WallDrawSize = 0.f;   // 성벽 조각 그림 크기
	UPROPERTY() float WallSegment = 0.f;   // 성벽 조각 간격
	UPROPERTY() float MaxDying = 0.f;   // 날아가는 시체 최대 (넘으면 바로 눕는다)
	UPROPERTY() float DeathJitter = 0.f;   // 죽을 때 흩어지는 속도
	UPROPERTY() float DeathRise = 0.f;   // 죽을 때 튀어 오르는 속도 (질량 제곱근으로 나눈다)
	UPROPERTY() float DeathRiseMin = 0.f;   // 튀어 오르는 속도 최소 비율
	UPROPERTY() float DeathSpin = 0.f;   // 날아가며 도는 속도 (라디안/초)
	UPROPERTY() float DeathGravity = 0.f;   // 시체 중력
	UPROPERTY() float DeathDrag = 0.f;   // 시체 감속
	UPROPERTY() float CorpseShade = 0.f;   // 바닥에 구운 시체 밝기
	UPROPERTY() float BloodScatter = 0.f;   // 피 웅덩이 흩어짐 (그림 크기 비율)
	UPROPERTY() float BloodSizeMin = 0.f;   // 피 웅덩이 최소 크기 비율
	UPROPERTY() float BloodSizeMax = 0.f;   // 피 웅덩이 최대 크기 비율
	UPROPERTY() float BloodAlphaMin = 0.f;   // 피 웅덩이 최소 진하기
	UPROPERTY() float CraterScale = 0.f;   // 유성 그을음 크기 (반경 배율)
	UPROPERTY() float AimRingStart = 0.f;   // 유성 표시 고리 시작 배율
	UPROPERTY() float AimAlpha = 0.f;   // 유성 조준 고리 진하기
	UPROPERTY() float MeteorOffsetX = 0.f;   // 운석이 떨어지기 시작하는 x 차이
	UPROPERTY() float MeteorFallHeight = 0.f;   // 운석이 떨어지기 시작하는 높이
	UPROPERTY() float MeteorDrawSize = 0.f;   // 운석 그림 크기
	UPROPERTY() float BlastStartScale = 0.f;   // 폭발 시작 크기
	UPROPERTY() float BlastEndScale = 0.f;   // 폭발 끝 크기
	UPROPERTY() float DustStartScale = 0.f;   // 먼지 시작 크기
	UPROPERTY() float DustEndScale = 0.f;   // 먼지 끝 크기
	// ---- UI (1920x1080 가상 좌표) ----
	UPROPERTY() float FrameMsSmoothing = 0.f;   // 프레임 시간 표시 부드럽게
	UPROPERTY() float FontPointScale = 0.f;   // 글자 크기 → 폰트 포인트
	UPROPERTY() float MinFontSize = 0.f;   // 최소 폰트 포인트
	UPROPERTY() float TextOutline = 0.f;   // 글자 외곽선 (글자 크기 비율)
	UPROPERTY() float TitleText = 0.f;   // 제목 글자
	UPROPERTY() float BigText = 0.f;   // 큰 글자
	UPROPERTY() float MidText = 0.f;   // 중간 글자
	UPROPERTY() float SmallText = 0.f;   // 작은 글자
	UPROPERTY() float TinyText = 0.f;   // 아주 작은 글자
	UPROPERTY() float TopY = 0.f;   // 위 패널 y
	UPROPERTY() float TopH = 0.f;   // 위 패널 높이
	UPROPERTY() float PanelPad = 0.f;   // 패널 안쪽 여백
	UPROPERTY() float IconSize = 0.f;   // 아이콘 크기
	UPROPERTY() float IconGap = 0.f;   // 아이콘과 글자 사이
	UPROPERTY() float SmallIconScale = 0.f;   // 작은 아이콘 배율
	UPROPERTY() float BarH = 0.f;   // 막대 높이
	UPROPERTY() float BarBorder = 0.f;   // 막대 테두리
	UPROPERTY() float BarShineHeight = 0.f;   // 막대 반짝 띠 높이 비율
	UPROPERTY() float BarShineAlpha = 0.f;   // 막대 반짝 띠 진하기
	UPROPERTY() float BarTextLift = 0.f;   // 막대 위 글자 y 보정
	UPROPERTY() float LevelPanelX = 0.f;   // 레벨 패널 x
	UPROPERTY() float LevelPanelW = 0.f;   // 레벨 패널 너비
	UPROPERTY() float VsPanelX = 0.f;   // VS 패널 x
	UPROPERTY() float VsPanelW = 0.f;   // VS 패널 너비
	UPROPERTY() float ResPanelX = 0.f;   // 자원 패널 x
	UPROPERTY() float ResPanelW = 0.f;   // 자원 패널 너비
	UPROPERTY() float SpeedButtonW = 0.f;   // 배속 버튼 너비
	UPROPERTY() float BossBarGap = 0.f;   // 보스 막대 위 간격
	UPROPERTY() float BossBarH = 0.f;   // 보스 막대 패널 높이
	UPROPERTY() float BossBarSplit = 0.f;   // 보스 이름 / 막대 나누는 비율
	UPROPERTY() float CardY = 0.f;   // 아래 카드 y
	UPROPERTY() float CardW = 0.f;   // 병영 카드 너비
	UPROPERTY() float CardH = 0.f;   // 카드 높이
	UPROPERTY() float CardGap = 0.f;   // 카드 간격
	UPROPERTY() float CardGroupGap = 0.f;   // 병영 카드와 스펠 사이
	UPROPERTY() float CardIcon = 0.f;   // 카드 아이콘 크기
	UPROPERTY() float SpellW = 0.f;   // 스펠·특성 버튼 너비
	UPROPERTY() float TipW = 0.f;   // 설명창 너비
	UPROPERTY() float TipLineH = 0.f;   // 설명창 줄 높이
	UPROPERTY() float TipOffset = 0.f;   // 설명창: 마우스 오른쪽으로
	UPROPERTY() float GameOverW = 0.f;   // 패배 패널 너비
	UPROPERTY() float GameOverH = 0.f;   // 패배 패널 높이
	UPROPERTY() float GlowSpeed = 0.f;   // 반짝임 속도
	UPROPERTY() float NumberMan = 0.f;   // 이 이상은 만 단위로
	UPROPERTY() float NumberEok = 0.f;   // 이 이상은 억 단위로
	// ---- 특성 성좌 화면 ----
	UPROPERTY() float TreeCenterX = 0.f;   // 성좌 중심 x
	UPROPERTY() float TreeCenterY = 0.f;   // 성좌 중심 y
	UPROPERTY() float TreeScale = 0.f;   // 성좌 좌표 → 화면 배율
	UPROPERTY() float PerkSmall = 0.f;   // 작은 별 반지름
	UPROPERTY() float PerkBig = 0.f;   // 큰 별 반지름
	UPROPERTY() float PerkHoverScale = 0.f;   // 마우스 올린 별 확대
	UPROPERTY() float LinkWidth = 0.f;   // 선 굵기
	UPROPERTY() float LinkTakenWidth = 0.f;   // 찍은 선 굵기
};
