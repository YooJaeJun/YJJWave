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
	UPROPERTY() float CampEdgeX = 0.f;   // 진지 오른쪽 끝 x (유성은 이보다 오른쪽에만)
	UPROPERTY() float HeroHomeX = 0.f;   // 영웅이 지키는 자리 x (세로는 가운데)
	UPROPERTY() float HeroLeash = 0.f;   // 영웅이 자리에서 이만큼까지만 쫓아간다
	UPROPERTY() float HeroArrive = 0.f;   // 영웅이 자리로 돌아왔다고 보는 거리
	UPROPERTY() float HeroRegen = 0.f;   // 영웅 초당 체력 회복
	UPROPERTY() int32 StartGold = 0;   // 시작 금화
	UPROPERTY() float PassiveGold = 0.f;   // 초당 기본 금화
	UPROPERTY() float LevelHPGrowth = 0.f;   // 웨이브마다 적 체력 배율
	UPROPERTY() float LevelDamageGrowth = 0.f;   // 웨이브마다 적 공격력 배율
	UPROPERTY() float EnemySpawnInset = 0.f;   // 적이 나오는 곳: 오른쪽 끝에서 이만큼 안
	UPROPERTY() float SpawnScatter = 0.f;   // 생산 위치 흩뿌림
	UPROPERTY() float BuySpread = 0.f;   // 즉시 구매 병사가 진지 끝 가운데에서 위아래로 퍼지는 폭
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
	UPROPERTY() int32 ReorderTicks = 0;   // 몇 틱마다 유닛 슬롯을 격자 칸 순서로 다시 모으나 (캐시 적중, 0이면 끔)
	UPROPERTY() int32 MaxNeighbors = 0;   // 밀어내기 검사 최대 이웃 수
	UPROPERTY() float SeparationStiffness = 0.f;   // 겹친 만큼 밀어내는 비율 (틱마다)
	UPROPERTY() float MaxPushPerTick = 0.f;   // 한 틱에 밀려나는 최대 거리
	UPROPERTY() float UnitAcceleration = 0.f;   // 원하는 속도로 붙는 빠르기 (/초)
	UPROPERTY() float ImpulseDamping = 0.f;   // 밀쳐진 속도가 줄어드는 빠르기 (/초)
	UPROPERTY() float LateralDrift = 0.f;   // 행군할 때 위아래로 흔들리는 정도 (속도 비율)
	UPROPERTY() float BigUnitRadius = 0.f;   // 이 반지름 이상은 큰 유닛(보스): 모두가 따로 밀어내기 검사
	UPROPERTY() float ArrowHitRadius = 0.f;   // 대상이 먼저 죽은 화살이 다른 적을 맞히는 반경
	UPROPERTY() float MultishotSpread = 0.f;   // 추가 화살이 노리는 대상 주변 반경
	UPROPERTY() float HitFlashTime = 0.f;   // 맞으면 붉게 깜빡이는 시간
	UPROPERTY() float AttackAnimTime = 0.f;   // 공격 동작 최소 길이
	UPROPERTY() float AttackAnimShare = 0.f;   // 공격 동작 길이 = 공격 간격 x 이 비율 (큰 유닛은 느릿하게 휘두른다)
	UPROPERTY() float AttackAnimMax = 0.f;   // 공격 동작 최대 길이
	UPROPERTY() float WalkStrideScale = 0.f;   // 걸음 한 바퀴 거리 = 그림 크기 x 이 비율
	UPROPERTY() float WalkStrideMin = 0.f;   // 걸음 한 바퀴 최소 거리
	UPROPERTY() float WalkStrideMax = 0.f;   // 걸음 한 바퀴 최대 거리 (큰 보스도 다리가 보이게 움직인다)
	UPROPERTY() float DeathFling = 0.f;   // 죽을 때 맞은 방향으로 날아가는 속도
	// ---- 스펠 ----
	UPROPERTY() float MeteorKnockback = 0.f;   // 유성 밀쳐내기 (질량으로 나눈다)
	UPROPERTY() float MeteorFallTime = 0.f;   // 유성이 떨어지는 시간
	UPROPERTY() float RallyMoveSpeed = 0.f;   // 함성 중 이동 속도 배율 (공격 속도·지속·대기는 Spells.csv Lv별)
	UPROPERTY() float BurnTick = 0.f;   // 불바다 피해 간격
	UPROPERTY() float ShowerSpread = 0.f;   // 유성우: 작은 운석이 떨어지는 범위 (반경 배율)
	UPROPERTY() float ShowerDelay = 0.f;   // 유성우: 운석 사이 시간
	UPROPERTY() float ShowerRadiusScale = 0.f;   // 유성우: 작은 운석 반경 배율
	UPROPERTY() float MinCooldownScale = 0.f;   // 재사용 대기 감소의 바닥
	UPROPERTY() float BlastEffectTime = 0.f;   // 유성 폭발 연출 시간
	UPROPERTY() float RallyEffectTime = 0.f;   // 함성 고리 연출 시간
	UPROPERTY() float BuildEffectTime = 0.f;   // 건설 먼지 연출 시간
	UPROPERTY() float RescueEffectTime = 0.f;   // 최후의 저항·부활 고리 연출 시간
	UPROPERTY() float RescueEffectSize = 0.f;   // 최후의 저항·부활 고리 크기
	// ---- 재미: 카드·전선·강령술·물량 연출 ----
	UPROPERTY() int32 CardChoices = 0;   // 웨이브가 바뀔 때 고르는 카드 수
	UPROPERTY() float FrontBonusMax = 0.f;   // 아군 앞줄이 적 끝까지 밀면 금화·영혼 +이 비율 (영웅 자리 0 ~ 오른쪽 끝 최대)
	UPROPERTY() int32 MaxCorpses = 0;   // 강령술사가 일으킬 수 있는 시체 기억 수
	UPROPERTY() float NecroRaiseInterval = 0.f;   // 강령술사가 시체를 일으키는 간격
	UPROPERTY() int32 NecroRaiseCount = 0;   // 한 번에 일으키는 시체 수
	UPROPERTY() float NecroRaiseRadius = 0.f;   // 강령술사 둘레 이 안의 시체만
	UPROPERTY() float RaiseEffectTime = 0.f;   // 되살아나는 연기 연출 시간
	UPROPERTY() float RaiseEffectSize = 0.f;   // 되살아나는 연기 크기
	// ---- 진급 (졸병 → 베테랑 → 장군 → 영웅, 처치 수는 Units.csv PromoteKills) ----
	UPROPERTY() float PromoteEffectTime = 0.f;   // 진급 빛줄기 연출 시간
	UPROPERTY() float PromoteEffectScale = 0.f;   // 진급 빛줄기 크기 (진급한 유닛 DrawSize 배율)
	// ---- 왕국의 마지막 냄비 (국물 단계는 Broth.csv, 적이 주는 뼈는 Units.csv Bones) ----
	UPROPERTY() float PotOverflowTime = 0.f;   // 마지막 단계에서 이 시간 안에 안 열면 끓어 넘쳐 뼈를 다 잃는다
	UPROPERTY() float PotSteamTime = 0.f;   // 뚜껑 열 때 김 연출 시간
	UPROPERTY() float SlashTime = 0.f;   // 영웅·보스 휘두르기 붓 초승달 시간
	UPROPERTY() float PotSteamSize = 0.f;   // 뚜껑 열 때 김 크기 (화면 UI)
	UPROPERTY() float PotOverflowSteamScale = 0.f;   // 끓어 넘칠 때 김 크기 배율
	// ---- 말풍선 대화 (Dialogues.csv) ----
	UPROPERTY() float HeroLowShare = 0.f;   // 영웅 체력이 이 비율 아래면 HeroLow 대화
	// ---- 국자 내려치기 (전장 클릭) ----
	UPROPERTY() float SmackKnockback = 0.f;   // 밀쳐내기
	UPROPERTY() float SmackDelay = 0.f;   // 국자가 내려오는 시간
	UPROPERTY() float SmackDustScale = 0.f;   // 쾅 먼지 크기 (범위 배율)
	UPROPERTY() float SmackRingTime = 0.f;   // 쾅 고리 시간
	// ---- 성벽·성문 (적이 성벽을 때려 0이 되면 패배, 체력은 Stages.csv WallHP) ----
	UPROPERTY() float GateHalf = 0.f;   // 성문 반 높이 (가운데 위아래로)
	UPROPERTY() float GateExit = 0.f;   // 성문을 나와 모이는 거리
	UPROPERTY() float WallReach = 0.f;   // 적이 성벽을 때리는 거리 여유
	UPROPERTY() float WallFlashTime = 0.f;   // 성벽이 맞으면 붉게 깜빡이는 시간
	UPROPERTY() float WallBreakPuffs = 0.f;   // 성벽이 무너질 때 먼지 수
	UPROPERTY() float HeroRespawnTime = 0.f;   // 영웅이 쓰러지면 다시 나오기까지
	UPROPERTY() float AttackStrikeShare = 0.f;   // 공격 동작 중 이 비율에서 맞는다 (휘두르기 전에 맞지 않게)
	// ---- 마무리·별 ----
	UPROPERTY() float VictoryDelay = 0.f;   // 적을 다 쓰러뜨린 뒤 결과 창까지 (쓰러지는 연출을 보여 준다)
	UPROPERTY() float StarThreeHealth = 0.f;   // 이겼을 때 성벽 체력이 이 비율 이상이면 별 3개
	UPROPERTY() float StarTwoHealth = 0.f;   // 이 비율 이상이면 별 2개 (아래면 1개)
	UPROPERTY() float BossSlowMo = 0.f;   // 보스가 쓰러질 때 느려지는 배율
	UPROPERTY() float BossSlowMoTime = 0.f;   // 느려지는 시간 (게임 시간)
	UPROPERTY() float BossBlastScale = 0.f;   // 보스 쓰러짐 폭발 크기 (보스 그림 크기 배율)
	UPROPERTY() float BossBlastTimeScale = 0.f;   // 보스 쓰러짐 폭발 연출 길이 배율
	// ---- 전선 ----
	UPROPERTY() float PlayerAdvanceShare = 0.f;   // 아군이 나아가는 끝 (전장 너비 비율). 그 뒤는 적 진영: 앞에서 밀고 당긴다
	UPROPERTY() float PlayerChaseMargin = 0.f;   // 전선 끝에서 이만큼 더 안쪽 적은 쫓지 않는다
	UPROPERTY() int32 MaxPendingEvents = 0;   // HUD가 가져가지 않은 판 사건은 이만큼까지만 쌓는다
	// ---- 소리 ----
	UPROPERTY() int32 VoicesPerSecond = 0;   // 보이스는 전체 합쳐 초당 이만큼까지 (수천 마리가 한꺼번에 떠들지 않게)
	UPROPERTY() float MusicFade = 0.f;   // 배경음을 바꿀 때 서서히 줄이고 키우는 시간
	// ---- 특성 성좌 ----
	UPROPERTY() float PerkCostBase = 0.f;   // 첫 별 영혼 비용
	UPROPERTY() float PerkCostGrowth = 0.f;   // 별을 찍을 때마다 비용 배율
	// ---- 오토파일럿·벤치마크 ----
	UPROPERTY() float BotThinkInterval = 0.f;   // 봇 판단 간격 (게임 시간)
	UPROPERTY() float BotCameraFollow = 0.f;   // 봇 카메라가 전선을 따라가는 빠르기
	UPROPERTY() float BotCameraZoom = 0.f;   // 봇 카메라 확대
	UPROPERTY() float BotMeteorLead = 0.f;   // 봇 유성: 적 앞줄보다 이만큼 뒤
	UPROPERTY() float BotRallyRatio = 0.f;   // 적 수가 아군의 이 배를 넘으면 함성
	UPROPERTY() float BotBuyGold = 0.f;   // 봇: 금화가 이보다 많으면 병사 x10 즉시 구매
	UPROPERTY() int32 BotPotStage = 0;   // 봇: 냄비가 이 단계(0부터) 이상이면 뚜껑을 연다
	UPROPERTY() float BotSmackSpread = 0.f;   // 봇 국자: 전장 가운데에서 위아래로 이만큼 안
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

	// ---- 냄비 (Buildings.png 6 솥, 7 뚜껑 / Fx.png 12 국물 면, 13 뼈, 14 김, 15 진급 빛) ----
	UPROPERTY() float PotCell = 0.f;   // Buildings 칸: 솥과 장작불
	UPROPERTY() float PotLidCell = 0.f;   // Buildings 칸: 뚜껑
	UPROPERTY() float FxBroth = 0.f;   // Fx 칸: 흰 타원 (국물 면·거품)
	UPROPERTY() float FxSteam = 0.f;   // Fx 칸: 김
	UPROPERTY() float FxPromote = 0.f;   // Fx 칸: 진급 빛줄기
	UPROPERTY() float UiBadge = 0.f;   // UI 칸: 중간·주문 별 바탕 원
	UPROPERTY() float UiBone = 0.f;   // UI 칸: 뼈
	UPROPERTY() float UiPot = 0.f;   // UI 칸: 냄비
	UPROPERTY() float PotAnchorY = 0.f;   // 냄비 그림에서 바닥 자리 (칸 높이 비율)
	UPROPERTY() float PotRimY = 0.f;   // 솥 아가리 가운데 높이 (칸 비율, generate_art POT_RIM_Y/256)
	UPROPERTY() float PotRimW = 0.f;   // 솥 아가리 너비 (칸 비율)
	UPROPERTY() float PotRimH = 0.f;   // 솥 아가리 높이 (칸 비율)
	UPROPERTY() float PotFillMin = 0.f;   // 국물이 막 생겼을 때 아가리를 채운 비율
	UPROPERTY() float PotBubbles = 0.f;   // 끓는 거품 수 (단계가 오를수록 다 보인다)
	UPROPERTY() float PotBubbleSize = 0.f;   // 거품 크기 (아가리 너비 비율)
	UPROPERTY() float PotBubbleSpeed = 0.f;   // 거품 박자
	UPROPERTY() float PotSteamPuffs = 0.f;   // 늘 피어오르는 김 수
	UPROPERTY() float PotSteamRise = 0.f;   // 김이 올라가는 높이 (화면 UI)
	UPROPERTY() float PotSteamPuffSize = 0.f;   // 김 하나 크기 (화면 UI)
	UPROPERTY() float PotSteamAlpha = 0.f;   // 김 진하기
	UPROPERTY() float PotLidLift = 0.f;   // 뚜껑 열 때 들리는 높이 (화면 UI)
	UPROPERTY() float PotLidTime = 0.f;   // 뚜껑이 들렸다 내려오는 시간
	UPROPERTY() float PotRattle = 0.f;   // 넘치기 직전 뚜껑 덜컹 크기 (화면 UI)
	UPROPERTY() float PotRattleSpeed = 0.f;   // 뚜껑 덜컹 빠르기
	UPROPERTY() float PotLabelGap = 0.f;   // 냄비 위 글자 간격
	UPROPERTY() float PotHoverScale = 0.f;   // 냄비에 마우스를 올리면 커지는 배율
	UPROPERTY() float BoneSpin = 0.f;   // 진급 빛 회전 빠르기
	UPROPERTY() float FxSlash = 0.f;   // Fx 칸: 붓 초승달 (영웅·보스 휘두르기)
	UPROPERTY() float FxImpact = 0.f;   // Fx 칸: 먹 튀김 (맞은 자리)
	UPROPERTY() float SlashScale = 0.f;   // 초승달 크기 (휘두르기 반경 배율)
	UPROPERTY() float SlashStartScale = 0.f;   // 초승달이 펼쳐지기 시작하는 크기
	UPROPERTY() float SlashLift = 0.f;   // 초승달 높이 (반경 배율, 몸통께)
	UPROPERTY() float SlashSweep = 0.f;   // 초승달이 휘둘러지며 도는 각도 (라디안)
	UPROPERTY() float ImpactScale = 0.f;   // 먹 튀김 크기 (반경 배율)
	UPROPERTY() float ImpactSpin = 0.f;   // 먹 튀김 도는 각도
	UPROPERTY() float PromoteRise = 0.f;   // 진급 빛이 떠오르는 높이 (월드)
	UPROPERTY() float PromoteBannerTier = 0.f;   // 이 진급 단계 이상이면 화면 배너 (2 = 장군)
	// ---- 국자 · 연속 처치 ----
	UPROPERTY() float FxLadle = 0.f;   // Fx 칸: 큰 국자
	UPROPERTY() float UiLadle = 0.f;   // UI 칸: 국자 (남은 횟수)
	UPROPERTY() float SmackDrawSize = 0.f;   // 내려오는 국자 그림 크기 (월드)
	UPROPERTY() float SmackStartAngle = 0.f;   // 국자가 휘두르기 시작하는 각도 (라디안)
	UPROPERTY() float ComboWindow = 0.f;   // 이 시간 안에 또 쓰러뜨리면 연속 처치가 이어진다
	UPROPERTY() float ComboShowAt = 0.f;   // 이만큼부터 연속 처치 숫자를 보인다
	UPROPERTY() float ComboHot = 0.f;   // 이만큼이면 가장 크고 붉게
	UPROPERTY() float ComboText = 0.f;   // 연속 처치 숫자 크기
	UPROPERTY() float ComboGrow = 0.f;   // 가장 뜨거울 때 커지는 비율
	UPROPERTY() float ComboPopScale = 0.f;   // 하나 늘 때 툭 커지는 비율
	UPROPERTY() float ComboPopSpeed = 0.f;   // 툭 커진 것이 돌아오는 빠르기
	UPROPERTY() float ComboFade = 0.f;   // 끊기기 전 흐려지는 시간
	UPROPERTY() float ComboRight = 0.f;   // 화면 오른쪽에서 숫자 가운데까지
	UPROPERTY() float ComboY = 0.f;   // 숫자 높이
	UPROPERTY() float ComboLabelGap = 0.f;   // 숫자와 글자 사이
	// ---- 성벽·금화·웨이브 예고 ----
	UPROPERTY() float BuildingTiers = 0.f;   // Buildings.png 강화 단계 줄 수
	UPROPERTY() float WallRow = 0.f;   // Buildings 줄: 성벽 조각·성문 탑
	UPROPERTY() float WallCol = 0.f;   // Buildings 칸: 성벽 조각
	UPROPERTY() float GateCol = 0.f;   // Buildings 칸: 성문 탑
	UPROPERTY() float WallSegment = 0.f;   // 성벽 조각 간격 (월드)
	UPROPERTY() float WallDrawSize = 0.f;   // 성벽 조각 그림 크기 (월드)
	UPROPERTY() float GateTowerSize = 0.f;   // 성문 탑 그림 크기 (월드)
	UPROPERTY() float WallAnchor = 0.f;   // 성벽 그림 바닥 자리 (칸 높이 비율)
	UPROPERTY() float WallBarW = 0.f;   // 성문 위 체력 너비
	UPROPERTY() float WallBarGap = 0.f;   // 성문 탑과 체력 사이
	UPROPERTY() float WallLabelW = 0.f;   // 좌측 패널 '성벽' 글자 칸
	UPROPERTY() float CoinPopsPerSecond = 0.f;   // 처치 금화 튀기는 초당 이만큼까지
	UPROPERTY() float CoinPopTime = 0.f;   // 금화가 튀었다 사라지는 시간
	UPROPERTY() float CoinPopJump = 0.f;   // 금화가 튀는 높이 (월드)
	UPROPERTY() float CoinPopFloat = 0.f;   // 금화가 끝으로 갈수록 더 오르는 높이
	UPROPERTY() float CoinPopDrift = 0.f;   // 금화가 옆으로 퍼지는 거리
	UPROPERTY() float CoinPopFade = 0.f;   // 끝에서 흐려지는 비율
	UPROPERTY() float CoinPopSize = 0.f;   // 튀는 금화 그림 크기
	UPROPERTY() float CoinPopGap = 0.f;   // 금화와 숫자 사이
	UPROPERTY() float PreviewY = 0.f;   // 웨이브 예고 판 높이
	UPROPERTY() float PreviewIcon = 0.f;   // 웨이브 예고 적 그림 크기
	UPROPERTY() float PreviewCountW = 0.f;   // 웨이브 예고 마리 수 칸
	UPROPERTY() float PreviewGap = 0.f;   // 예고 제목과 그림 사이
	// ---- 결과 창 ----
	UPROPERTY() float ResultStepTime = 0.f;   // 결과 창 한 칸이 나타나는 시간 (위에서부터 차례로)
	UPROPERTY() float ResultSlide = 0.f;   // 나타나며 미끄러지는 거리
	UPROPERTY() float ResultPopTime = 0.f;   // 결과 창이 커지며 나타나는 시간
	UPROPERTY() float ResultPopStart = 0.f;   // 결과 창 처음 크기 배율
	UPROPERTY() float ResultPopOvershoot = 0.f;   // 결과 창이 살짝 넘쳤다 돌아오는 정도
	UPROPERTY() float ResultWinH = 0.f;   // 이겼을 때 결과 창 높이 (별 줄 포함)
	UPROPERTY() float ResultStarCount = 0.f;   // 별 수
	UPROPERTY() float ResultStarSize = 0.f;   // 결과 별 크기
	UPROPERTY() float ResultStarGap = 0.f;   // 결과 별 사이
	UPROPERTY() float ResultStarDelay = 0.f;   // 별이 하나씩 나오는 간격
	UPROPERTY() float ResultStarPopTime = 0.f;   // 별이 커졌다 자리 잡는 시간
	UPROPERTY() float ResultStarPopScale = 0.f;   // 별이 처음 나올 때 크기 배율
	UPROPERTY() float ResultStarEmptyAlpha = 0.f;   // 얻지 못한 별 자리 진하기
	// ---- 타이틀 ----
	UPROPERTY() float TitleShade = 0.f;   // 타이틀 바닥 어둡게
	UPROPERTY() float TitleLogoY = 0.f;   // 제목 높이 (화면 비율)
	UPROPERTY() float TitleLogoSize = 0.f;   // 제목 글자 크기
	UPROPERTY() float TitleTaglineGap = 0.f;   // 제목과 한 줄 사이
	UPROPERTY() float TitleButtonsY = 0.f;   // 버튼 시작 높이 (화면 비율)
	UPROPERTY() float TitleButtonW = 0.f;   // 타이틀·월드맵 버튼 너비
	UPROPERTY() float TitleButtonH = 0.f;   // 타이틀·월드맵 버튼 높이
	UPROPERTY() float TitleButtonGap = 0.f;   // 버튼 사이
	UPROPERTY() float TitlePotSize = 0.f;   // 타이틀 냄비 크기
	UPROPERTY() float TitlePotY = 0.f;   // 타이틀 냄비 발 높이 (화면 비율)
	UPROPERTY() float TitleSteamScale = 0.f;   // 타이틀 김 크기 (냄비 크기 배율)
	UPROPERTY() float TitleSteamRise = 0.f;   // 타이틀 김 오르는 높이 (냄비 크기 배율)
	UPROPERTY() float TitleArmyRows = 0.f;   // 양옆 무리 줄 수
	UPROPERTY() float TitleArmyCols = 0.f;   // 양옆 무리 한 줄 수
	UPROPERTY() float TitleArmyY = 0.f;   // 무리 첫 줄 발 높이 (화면 비율)
	UPROPERTY() float TitleArmyRowGap = 0.f;   // 무리 줄 사이
	UPROPERTY() float TitleArmyGap = 0.f;   // 무리 사이 간격
	UPROPERTY() float TitleUnitSize = 0.f;   // 타이틀 병사 크기
	// ---- 월드맵 ----
	UPROPERTY() float RegionCloudPuffs = 0.f;   // 잠긴 지역을 덮는 구름 덩이 수 (가운데 하나 + 둘레)
	UPROPERTY() float RegionCloudSize = 0.f;   // 구름 덩이 크기
	UPROPERTY() float RegionCloudRadius = 0.f;   // 둘레 구름 반지름
	UPROPERTY() float RegionCloudWide = 0.f;   // 둘레 구름 가로 늘림
	UPROPERTY() float RegionCloudSpeed = 0.f;   // 구름 일렁이는 빠르기
	UPROPERTY() float RegionCloudBreath = 0.f;   // 구름 일렁이는 크기
	UPROPERTY() float RegionLabelLift = 0.f;   // 지역 이름을 자리보다 이만큼 위에
	UPROPERTY() float StagePathAlpha = 0.f;   // 스테이지 길 진하기
	UPROPERTY() float StagePathWidth = 0.f;   // 스테이지 길 굵기
	UPROPERTY() float StageNodeSize = 0.f;   // 스테이지 자리 크기
	UPROPERTY() float StageNodeHover = 0.f;   // 마우스를 올리면 커지는 배율
	UPROPERTY() float StageNameGap = 0.f;   // 자리와 이름 사이
	UPROPERTY() float StageStarSize = 0.f;   // 월드맵 별 크기
	UPROPERTY() float MapHeaderW = 0.f;   // 월드맵 머리글 판 너비
	// ---- 솥 판 (전장 왼쪽 아래 화면 UI) ----
	UPROPERTY() float PotUiMargin = 0.f;   // 좌측 패널·화면 아래와 솥 판 사이
	UPROPERTY() float PotUiW = 0.f;   // 솥 판 너비
	UPROPERTY() float PotUiH = 0.f;   // 솥 판 높이 (솥 그림은 위로 삐져나온다)
	UPROPERTY() float PotUiPad = 0.f;   // 솥 판 안쪽 여백
	UPROPERTY() float PotUiSize = 0.f;   // 솥 그림 크기
	UPROPERTY() float PotUiCol = 0.f;   // 솥 그림 칸 너비 (글자는 그 오른쪽)
	UPROPERTY() float PotUiFeet = 0.f;   // 솥 발이 칸 바닥에서 떨어진 거리
	UPROPERTY() float PotUiGlow = 0.f;   // 열 수 있을 때 판이 반짝이는 세기
	UPROPERTY() float PotUiLineGap = 0.f;   // 단계 이름과 숫자 사이
	UPROPERTY() float PotUiBoneIcon = 0.f;   // 숫자 앞 뼈 그림 크기
	UPROPERTY() float PotUiNextGap = 0.f;   // 숫자와 '/ 다음 단계' 사이
	UPROPERTY() float PotNumberText = 0.f;   // 뼈 숫자 글자 크기
	UPROPERTY() float PotNumberPopScale = 0.f;   // 뼈가 들어오면 숫자가 툭 커지는 비율
	UPROPERTY() float PotNumberPopSpeed = 0.f;   // 툭 커진 숫자가 돌아오는 빠르기
	UPROPERTY() float PotPlusInterval = 0.f;   // +뼈를 이만큼 모아서 한 번에 튀긴다 (초)
	UPROPERTY() float PotPlusMax = 0.f;   // 한꺼번에 떠 있는 +뼈 수
	UPROPERTY() float PotPlusTime = 0.f;   // +뼈가 떠올라 사라지는 시간
	UPROPERTY() float PotPlusRise = 0.f;   // +뼈가 떠오르는 높이
	UPROPERTY() float PotPlusW = 0.f;   // +뼈 글자 자리 (칸 오른쪽 끝에서)
	UPROPERTY() float PotTickW = 0.f;   // 단계 눈금 두께
	UPROPERTY() float PotTickOver = 0.f;   // 눈금이 게이지 위아래로 삐져나오는 길이
	UPROPERTY() float PotPromptSpeed = 0.f;   // 냄비 위 '뚜껑 열기!' 튀는 빠르기
	UPROPERTY() float PotPromptBounce = 0.f;   // 냄비 위 '뚜껑 열기!' 튀는 높이
	// ---- 말풍선 (Dialogues.csv) ----
	UPROPERTY() float BubbleMaxW = 0.f;   // 말풍선 최대 너비 (화면)
	UPROPERTY() float BubblePad = 0.f;   // 말풍선 안쪽 가로 여백
	UPROPERTY() float BubblePadY = 0.f;   // 말풍선 안쪽 세로 여백
	UPROPERTY() float BubbleText = 0.f;   // 말풍선 글자 크기
	UPROPERTY() float BubbleNameText = 0.f;   // 말풍선 이름 글자 크기
	UPROPERTY() float BubbleNameGap = 0.f;   // 이름과 대사 사이
	UPROPERTY() float BubbleLineH = 0.f;   // 말풍선 줄 높이
	UPROPERTY() float BubbleGap = 0.f;   // 머리 위 말풍선까지 간격 (화면)
	UPROPERTY() float BubbleFade = 0.f;   // 말풍선이 나타나고 사라지는 시간
	UPROPERTY() float BubbleTypeSpeed = 0.f;   // 말풍선 글자가 나오는 빠르기 (초당 글자)
	UPROPERTY() float BubbleScreenMargin = 0.f;   // 말풍선이 화면 밖으로 나가지 않게 띄우는 여백
	UPROPERTY() float BubbleHeadShare = 0.f;   // 말풍선 꼬리 끝: 유닛 발에서 그림 크기의 이 비율만큼 위
	UPROPERTY() float PerkIconScale = 0.f;   // 중간·주문 별 효과 그림 크기 (바탕 원 지름 배율)
	UPROPERTY() float PerkIconDim = 0.f;   // 아직 못 찍는 별 효과 그림 밝기
	// ---- 아틀라스 (Tools/Art/generate_art.py 와 같아야 한다) ----
	UPROPERTY() float UnitCell = 0.f;   // Units.png 칸 크기
	UPROPERTY() float BigUnitCell = 0.f;   // BigUnits.png 칸 크기 (보스·영웅)
	UPROPERTY() float FxCell = 0.f;   // Fx.png 칸 크기
	UPROPERTY() float BuildingCell = 0.f;   // Buildings.png 칸 크기
	UPROPERTY() float UiCell = 0.f;   // UI.png 칸 크기
	UPROPERTY() float ProjectileRow = 0.f;   // Units.png 투사체 줄
	UPROPERTY() float AnimWalkFirst = 0.f;   // Units.png 걷기 첫 칸
	UPROPERTY() float AnimWalkFrames = 0.f;   // 걷기 칸 수 (그림 크기만큼 걸을 때 한 바퀴)
	UPROPERTY() float AnimAttackFirst = 0.f;   // 공격 첫 칸
	UPROPERTY() float AnimAttackFrames = 0.f;   // 공격 칸 수 (들어 올림 → 때림 → 돌아옴)
	UPROPERTY() float AnimHitFirst = 0.f;   // 맞음 첫 칸
	UPROPERTY() float AnimHitFrames = 0.f;   // 맞음 칸 수
	UPROPERTY() float AnimCorpse = 0.f;   // 시체 칸
	UPROPERTY() float AnimIdleFirst = 0.f;   // 숨쉬기 첫 칸
	UPROPERTY() float AnimIdleFrames = 0.f;   // 숨쉬기 칸 수
	UPROPERTY() float AnimIdleSpeed = 0.f;   // 이 속력보다 느리면 숨쉬기 그림
	UPROPERTY() float AnimIdleFps = 0.f;   // 숨쉬기 초당 칸
	UPROPERTY() float AnimDazedFirst = 0.f;   // 헤벌레(세게 밀려 휘청) 첫 칸
	UPROPERTY() float AnimDazedFrames = 0.f;   // 헤벌레 칸 수
	UPROPERTY() float AnimDazedImpulse = 0.f;   // 밀쳐진 속력이 이보다 크면 헤벌레 그림
	UPROPERTY() float AnimDazedFps = 0.f;   // 헤벌레 초당 칸
	UPROPERTY() float EmptyPlotCol = 0.f;   // Buildings.png 빈 칸
	UPROPERTY() float FxShadow = 0.f;   // Fx 칸: 그림자
	UPROPERTY() float FxBloodFirst = 0.f;   // Fx 칸: 첫 피 웅덩이
	UPROPERTY() float FxBloodCount = 0.f;   // 피 웅덩이 종류 수
	UPROPERTY() float FxScorch = 0.f;   // Fx 칸: 그을음
	UPROPERTY() float FxDust = 0.f;   // Fx 칸: 먼지
	UPROPERTY() float FxRing = 0.f;   // Fx 칸: 고리
	UPROPERTY() float FxMeteor = 0.f;   // Fx 칸: 운석
	UPROPERTY() float FxBlast = 0.f;   // Fx 칸: 폭발
	UPROPERTY() float FxFlame = 0.f;   // Fx 칸: 불꽃
	UPROPERTY() float UiCoin = 0.f;   // UI 칸: 금화
	UPROPERTY() float UiSoul = 0.f;   // UI 칸: 영혼
	UPROPERTY() float UiSword = 0.f;   // UI 칸: 아군 수
	UPROPERTY() float UiSkull = 0.f;   // UI 칸: 적 수
	UPROPERTY() float UiMeteor = 0.f;   // UI 칸: 유성
	UPROPERTY() float UiRally = 0.f;   // UI 칸: 함성
	UPROPERTY() float UiStar = 0.f;   // UI 칸: 작은 별
	UPROPERTY() float UiBigStar = 0.f;   // UI 칸: 큰 별
	UPROPERTY() float UiHero = 0.f;   // UI 칸: 영웅 얼굴
	UPROPERTY() float UiPanel = 0.f;   // UI 칸: 패널 (9칸 늘리기)
	UPROPERTY() float UiPanelDark = 0.f;   // UI 칸: 어두운 패널
	UPROPERTY() float UiCard = 0.f;   // UI 칸: 카드
	UPROPERTY() float PanelCornerFraction = 0.f;   // 패널 그림에서 모서리가 차지하는 비율
	UPROPERTY() float PanelCorner = 0.f;   // 화면에 그리는 패널 모서리 크기
	// ---- 카메라 ----
	UPROPERTY() float ZoomDefault = 0.f;   // 시작 확대
	UPROPERTY() float ZoomMin = 0.f;   // 최소 확대 (실제로는 전장이 화면을 꽉 채우는 값 아래로는 안 간다)
	UPROPERTY() float ZoomMax = 0.f;   // 최대 확대
	UPROPERTY() float ZoomStep = 0.f;   // 휠 한 칸 확대 배율
	UPROPERTY() float CameraStartX = 0.f;   // 시작 시점 x
	UPROPERTY() float PanSpeed = 0.f;   // 키보드·가장자리 이동 속도 (화면 픽셀/초)
	UPROPERTY() float EdgePan = 0.f;   // 1이면 화면 가장자리에 마우스를 대면 이동
	UPROPERTY() float EdgePanPixels = 0.f;   // 가장자리 폭
	// ---- 전장 그리기 ----
	UPROPERTY() float GroundTargetWidth = 0.f;   // 피·시체가 쌓이는 바닥 렌더 타깃 너비 (높이는 전장 비율)
	UPROPERTY() float FootAnchor = 0.f;   // 유닛 그림에서 발 위치 (위=0 아래=1)
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
	UPROPERTY() float AimAlpha = 0.f;   // 주문 조준 표시 진하기
	UPROPERTY() float BurnFlames = 0.f;   // 불바다 불꽃 수
	UPROPERTY() float BurnSpread = 0.f;   // 불꽃이 퍼지는 반경 배율
	UPROPERTY() float BurnFlameScale = 0.f;   // 불꽃 크기 (반경 배율)
	UPROPERTY() float BurnFlicker = 0.f;   // 불꽃 일렁임 크기
	UPROPERTY() float BurnFlickerSpeed = 0.f;   // 불꽃 일렁임 빠르기
	UPROPERTY() float BurnFlickerPhase = 0.f;   // 불꽃마다 일렁임 어긋남
	UPROPERTY() float BurnFadeShare = 0.f;   // 불바다 끝 무렵 이 비율 동안 사그라든다
	UPROPERTY() float GroundSquash = 0.f;   // 바닥 원을 눌러 보이는 세로 비율
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
	UPROPERTY() float TextCapPerPoint = 0.f;   // 글자 몸통 높이(픽셀) = 폰트 포인트 x 이 값 (세로 가운데 맞춤)
	UPROPERTY() float TitleText = 0.f;   // 제목 글자
	UPROPERTY() float BigText = 0.f;   // 큰 글자
	UPROPERTY() float MidText = 0.f;   // 중간 글자
	UPROPERTY() float SmallText = 0.f;   // 작은 글자
	UPROPERTY() float TinyText = 0.f;   // 아주 작은 글자
	UPROPERTY() float PanelPad = 0.f;   // 패널 안쪽 여백 (배치용)
	UPROPERTY() float TextPad = 0.f;   // 글 상자 안쪽 여백: 카드 고르기·설명창 (테두리 두께 포함, 글자가 테두리에 붙지 않게)
	UPROPERTY() float SidePanelW = 0.f;   // 좌측 패널 너비 (모든 UI가 여기. 전장은 나머지 전부)
	UPROPERTY() float SectionGap = 0.f;   // 패널 칸 사이 간격
	UPROPERTY() float HeaderH = 0.f;   // 스테이지·웨이브 칸 높이
	UPROPERTY() float ResRowH = 0.f;   // 금화·영혼 줄 아이콘 높이
	UPROPERTY() float SpellH = 0.f;   // 스펠·특성 칸 높이
	UPROPERTY() float BuyH = 0.f;   // 병사 즉시 구매 버튼 높이
	UPROPERTY() float BuyPopupGap = 0.f;   // 구매 팝업: 카드 아래 간격
	UPROPERTY() float BuyCountA = 0.f;   // 구매 버튼 1: 마리 수
	UPROPERTY() float BuyCountB = 0.f;   // 구매 버튼 2: 마리 수
	UPROPERTY() float BuyCountC = 0.f;   // 구매 버튼 3: 마리 수
	UPROPERTY() float BuyTopRow = 0.f;   // 구매 버튼 위 줄(x3) 가운데 높이 (반 칸 비율)
	UPROPERTY() float BuyBottomRow = 0.f;   // 구매 버튼 아래 줄(금화) 가운데 높이 (아래에서 반 칸 비율)
	UPROPERTY() float DebugLineGap = 0.f;   // 성능 줄 사이
	UPROPERTY() float IconSize = 0.f;   // 아이콘 크기
	UPROPERTY() float IconGap = 0.f;   // 아이콘과 글자 사이
	UPROPERTY() float SmallIconScale = 0.f;   // 작은 아이콘 배율
	UPROPERTY() float BarH = 0.f;   // 숫자 없는 게이지 높이 (웨이브 시간)
	UPROPERTY() float BarBorder = 0.f;   // 막대 테두리
	UPROPERTY() float BarShineHeight = 0.f;   // 막대 반짝 띠 높이 비율
	UPROPERTY() float BarShineAlpha = 0.f;   // 막대 반짝 띠 진하기
	UPROPERTY() float TextBarH = 0.f;   // 숫자가 들어가는 게이지 높이 (영웅·보스)
	UPROPERTY() float BarTextScale = 0.f;   // 게이지 안 글자 크기 = 게이지 높이 x 이 비율
	UPROPERTY() float SpeedButtonW = 0.f;   // 배속 버튼 너비 (화면 오른쪽 위)
	UPROPERTY() float SpeedButtonH = 0.f;   // 배속 버튼 높이
	UPROPERTY() float OptionsButtonTopW = 0.f;   // 화면 오른쪽 위 옵션 버튼 너비 (배속 왼쪽)
	UPROPERTY() float SkillChargeBarH = 0.f;   // 국자 칸: 다음 한 번이 차오르는 막대 두께
	UPROPERTY() float DragStartPixels = 0.f;   // 왼쪽 버튼을 이만큼 끌면 클릭이 아니라 화면 이동
	UPROPERTY() float UnitBarLift = 0.f;   // 머리 위 체력 게이지: 발에서 그림 크기의 이 비율 위
	UPROPERTY() float UnitBarWidth = 0.f;   // 머리 위 체력 게이지 너비 (그림 크기 비율)
	UPROPERTY() float UnitBarMinW = 0.f;   // 머리 위 체력 게이지 최소 너비 (화면)
	UPROPERTY() float UnitBarH = 0.f;   // 머리 위 체력 게이지 두께
	UPROPERTY() float UnitBarNameGap = 0.f;   // 보스 이름과 게이지 사이
	UPROPERTY() float CardH = 0.f;   // 카드 높이
	UPROPERTY() float CardGap = 0.f;   // 카드 간격
	UPROPERTY() float CardIcon = 0.f;   // 카드 아이콘 크기
	UPROPERTY() float TipW = 0.f;   // 설명창 너비
	UPROPERTY() float TipLineH = 0.f;   // 설명창 줄 높이
	UPROPERTY() float TipOffset = 0.f;   // 설명창: 마우스 오른쪽으로
	UPROPERTY() float GameOverW = 0.f;   // 패배 패널 너비
	UPROPERTY() float GameOverH = 0.f;   // 결과 창 높이
	UPROPERTY() float ResultButtonH = 0.f;   // 결과 창 버튼 높이
	UPROPERTY() float GlowSpeed = 0.f;   // 반짝임 속도
	UPROPERTY() float NumberMan = 0.f;   // 이 이상은 만 단위로
	UPROPERTY() float NumberEok = 0.f;   // 이 이상은 억 단위로
	// ---- 카드·배너·흔들림 ----
	UPROPERTY() float CardW = 0.f;   // 양념 카드 너비
	UPROPERTY() float CardPickH = 0.f;   // 양념 카드 높이
	UPROPERTY() float CardIconSize = 0.f;   // 양념 카드 그림 크기
	UPROPERTY() float CardSubGap = 0.f;   // 양념 고르기 제목과 설명 사이
	UPROPERTY() float CardLineGap = 0.f;   // 맛 설명 줄 사이
	UPROPERTY() float SeasoningIcon = 0.f;   // 좌측 패널 넣은 양념 그림 크기
	UPROPERTY() float SeasoningGap = 0.f;   // 넣은 양념 그림 사이
	UPROPERTY() float CardPickGap = 0.f;   // 카드 사이
	UPROPERTY() float CardTitleY = 0.f;   // 카드 고르기 제목 y
	UPROPERTY() float BannerY = 0.f;   // 물량 배너 y
	UPROPERTY() float BannerTime = 0.f;   // 배너가 떠 있는 시간
	UPROPERTY() float BannerText = 0.f;   // 배너 글자 크기
	UPROPERTY() float BannerPop = 0.f;   // 배너가 처음 튀어나오는 크기 (+비율)
	UPROPERTY() float BannerPopSpeed = 0.f;   // 튀어나온 크기가 줄어드는 빠르기
	UPROPERTY() float BannerFadeSharpness = 0.f;   // 끝에 사라지는 빠르기 (클수록 늦게까지 또렷)
	UPROPERTY() float MilestoneFirst = 0.f;   // 첫 물량 배너: 아군이 이 수를 넘을 때
	UPROPERTY() float MilestoneFactor = 0.f;   // 다음 물량 배너는 이 배수
	UPROPERTY() float MassacreWindow = 0.f;   // 학살 판정 시간 창 (초)
	UPROPERTY() float MassacreKills = 0.f;   // 이 시간 안에 이만큼 처치하면 학살 배너
	UPROPERTY() float ShakeTime = 0.f;   // 화면 흔들림 시간
	UPROPERTY() float ShakePixels = 0.f;   // 화면 흔들림 세기 (픽셀)
	UPROPERTY() float DeathRiseFromFling = 0.f;   // 세게 맞아 날아갈수록 더 높이 튄다 (밀쳐진 속도 비율)
	// ---- 컷씬 ----
	UPROPERTY() float CutBarH = 0.f;   // 위아래 검은 띠 높이
	UPROPERTY() float CutBoxW = 0.f;   // 대사 상자 너비
	UPROPERTY() float CutBoxH = 0.f;   // 대사 상자 높이
	UPROPERTY() float CutPortrait = 0.f;   // 초상화 크기
	UPROPERTY() float CutTitleFade = 0.f;   // 장 제목 카드: 서서히 나타나는 시간
	UPROPERTY() float CutSubDelay = 0.f;   // 장 제목 카드: 아래 줄이 늦게 나오는 시간
	UPROPERTY() float CutTitleShade = 0.f;   // 장 제목 카드: 전장 어둡게
	UPROPERTY() float CutTitleY = 0.f;   // 장 제목 카드: 제목 높이 (화면 높이 비율)
	UPROPERTY() float CutTitleScale = 0.f;   // 장 제목 카드: 제목 글자 배율 (TitleText 기준)
	UPROPERTY() float CutRuleW = 0.f;   // 장 제목 카드: 제목 아래 줄 너비 (전장 너비 비율)
	UPROPERTY() float CutRuleH = 0.f;   // 장 제목 카드: 제목 아래 줄 두께
	UPROPERTY() float CutRuleAlpha = 0.f;   // 장 제목 카드: 제목 아래 줄 진하기
	UPROPERTY() float CutSubGap = 0.f;   // 장 제목 카드: 제목·줄·아래 글 사이
	UPROPERTY() float CutSubW = 0.f;   // 장 제목 카드: 아래 글 줄바꿈 너비 (전장 너비 비율)
	UPROPERTY() float CutTypeSpeed = 0.f;   // 대사 글자가 초당 나오는 수
	UPROPERTY() float CutCameraFollow = 0.f;   // 컷씬 카메라가 목표로 가는 빠르기
	// ---- 옵션 화면 ----
	UPROPERTY() float OptionsW = 0.f;   // 옵션 창 너비
	UPROPERTY() float OptionsRowH = 0.f;   // 옵션 한 줄 높이
	UPROPERTY() float OptionsLabelW = 0.f;   // 옵션 이름 칸 너비
	UPROPERTY() float OptionsButtonW = 0.f;   // 옵션 작은 버튼 너비 (- + ◀ ▶)
	UPROPERTY() float OptionsCloseW = 0.f;   // 닫기 버튼 너비
	UPROPERTY() float OptionsVolumeStep = 0.f;   // 볼륨 버튼 한 번에 바뀌는 양
	UPROPERTY() float OptionsOpenH = 0.f;   // 좌측 패널 옵션 버튼 높이
	UPROPERTY() float TreeBarH = 0.f;   // 좌측 패널 특성 막대 높이
	UPROPERTY() float TreeBarIcon = 0.f;   // 특성 막대 별 그림 크기
	// ---- 특성 성좌 화면 ----
	UPROPERTY() float TreeTitleY = 0.f;   // 성좌 제목 y
	UPROPERTY() float TreeCenterY = 0.f;   // 성좌 중심 y
	UPROPERTY() float TreeScale = 0.f;   // 성좌 좌표 → 화면 배율
	UPROPERTY() float PerkSmall = 0.f;   // 작은 별 반지름
	UPROPERTY() float PerkBig = 0.f;   // 중간 별 반지름
	UPROPERTY() float PerkKeystone = 0.f;   // 쐐기 반지름
	UPROPERTY() float TipKindW = 0.f;   // 설명창 오른쪽 위 별 종류 글자 너비
	UPROPERTY() float PerkHoverScale = 0.f;   // 마우스 올린 별 확대
	UPROPERTY() float SpellNodeLvGap = 0.f;   // 주문 별 아래 Lv 글자 간격
	UPROPERTY() float LinkWidth = 0.f;   // 선 굵기
	UPROPERTY() float LinkTakenWidth = 0.f;   // 찍은 선 굵기
};
