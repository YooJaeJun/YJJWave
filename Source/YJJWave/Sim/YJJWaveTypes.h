#pragma once

#include "CoreMinimal.h"

/** 편. 아군은 왼쪽 진지에서 오른쪽으로, 적은 오른쪽 끝에서 왼쪽 성벽으로 */
enum class EYJJTeam : uint8
{
	Player,
	Enemy,
	Count
};

/** 특성 성좌·병영 레벨이 올리는 능력치. Perks.csv 의 Stat 열 이름과 같다 */
enum class EYJJStat : uint8
{
	HP,            // 병사 체력 %
	Damage,        // 병사 공격력 %
	AttackSpeed,   // 병사 공격 속도 %
	MoveSpeed,     // 병사 이동 속도 %
	SpawnRate,     // 병영 생산 속도 %
	SpawnCount,    // 병영이 한 번에 내보내는 수 (+정수)
	Multishot,     // 원거리 병사 화살 수 (+정수)
	Splash,        // 범위 공격 반경 %
	Gold,          // 처치 금화 %
	Souls,         // 처치 영혼 %
	HeroHP,        // 영웅 최대 체력 %
	SpellPower,    // 스펠 위력 %
	SpellCooldown, // 스펠 재사용 대기 감소 %
	Count
};

/** 특성 별 종류 */
enum class EYJJPerkKind : uint8
{
	Start,     // 처음부터 찍혀 있는 중심 별
	Small,     // 작은 별: 능력치 숫자만
	Notable,   // 중간 별: 특수 효과 하나 (EYJJSpecial)
	Keystone,  // 쐐기: 대가가 있는 큰 규칙 변경 (능력치 + 와 -)
	Spell,     // 주문 별: 여러 번 찍어 주문 Lv을 올린다 (Spells.csv)
};

/**
 * 중간 별 특수 효과. Perks.csv Special 열 이름과 같다. A·B 값의 뜻:
 *   LastStand  체력이 A 비율 아래로 떨어지면 한 번 최대 체력의 B만큼 회복
 *   Revive     처음 쓰러질 때 최대 체력 A로 한 번 일어난다
 *   HeroRegen  영웅 초당 회복 +A
 *   Fury       체력이 A 비율 아래면 공격력 +B
 *   ArrowBurst 화살이 반경 A 범위 피해
 *   ExtraShot  화살 +A발
 *   GoldBurst  적 처치 시 A 확률로 금화 B배
 *   SoulBurst  적 처치 시 A 확률로 영혼 B배
 *   Shockwave  밀쳐내기 A배
 *   MeteorTwice 유성이 A초 뒤 한 번 더 떨어진다
 *   RallyHeal  돌격 함성 때 아군 전체 최대 체력의 A만큼 회복
 *   Thorns     근접으로 맞은 피해의 A를 때린 적에게 돌려준다
 *   Lifesteal  준 피해의 A만큼 회복
 */
enum class EYJJSpecial : uint8
{
	None,
	LastStand,
	Revive,
	HeroRegen,
	Fury,
	ArrowBurst,
	ExtraShot,
	GoldBurst,
	SoulBurst,
	Shockwave,
	MeteorTwice,
	RallyHeal,
	Thorns,
	Lifesteal,
	Count
};

/** 유닛 종류별 특수 효과 값 (특성이 정하고 시뮬이 읽는다) */
struct FYJJUnitSpecial
{
	float LastStandAt = 0.f;
	float LastStandHeal = 0.f;
	float ReviveHP = 0.f;
	float FuryAt = 0.f;
	float FuryBonus = 0.f;
	float ArrowSplash = 0.f;
	float KnockbackScale = 1.f;
	float Thorns = 0.f;
	float Lifesteal = 0.f;
};

/** 유닛 한 마리가 한 번 쓰는 효과 표시 (FYJJHorde::Flags 비트) */
namespace YJJUnitFlag
{
	inline constexpr uint8 LastStandUsed = 1 << 0;
	inline constexpr uint8 Revived = 1 << 1;
}

/** 주문 (Spells.csv Spell 열 이름과 같다) */
enum class EYJJSpell : uint8
{
	Meteor,   // F1: 커서 위치에 큰 범위 피해 + 밀쳐내기
	Rally,    // F2: 아군 전체 공격·이동 속도 증가
	Quake,    // F3: 커서 x 자리 전장을 위아래로 가르는 피해 + 밀쳐내기
	Count
};

/**
 * 주문 Lv 특수 효과 (Spells.csv Fx 열 이름과 같다). 그 Lv에 열리고 위 Lv에도 계속 켜진다. A·B 값의 뜻:
 *   Burn       유성이 떨어진 자리에 불: 초당 유성 피해 x A, B초
 *   Shower     작은 운석 A개가 주변에 더 떨어진다 (피해 x B)
 *   Heal       함성 때 아군 전체 최대 체력의 A만큼 회복
 *   Reinforce  함성 때 지어 둔 병영마다 병사 A명이 바로 나온다
 *   Aftershock 대지 가르기가 A초 뒤 한 번 더 (피해 x B)
 *   Split      양옆으로 A줄 더 갈라진다 (줄 사이 B)
 */
enum class EYJJSpellFx : uint8
{
	None,
	Burn,
	Shower,
	Heal,
	Reinforce,
	Aftershock,
	Split,
	Count
};

/** Spells.csv 한 줄 = 주문 하나의 한 Lv */
struct FYJJSpellLevel
{
	int32 Level = 1;
	float Power = 0.f;      // 유성·대지 가르기: 피해 / 함성: 공격 속도 배율
	float Radius = 0.f;     // 유성: 반경 / 대지 가르기: 갈라진 폭의 절반
	float Cooldown = 0.f;
	float Duration = 0.f;   // 함성: 지속 시간
	EYJJSpellFx Fx = EYJJSpellFx::None;
	float A = 0.f;
	float B = 0.f;
};

/** 어떤 Lv에서 쓰는 값: 그 Lv 줄 수치 + 그 Lv까지 열린 특수 효과 */
struct FYJJSpellStats
{
	int32 Level = 0;
	float Power = 0.f;
	float Radius = 0.f;
	float Cooldown = 0.f;
	float Duration = 0.f;
	bool bFx[static_cast<int32>(EYJJSpellFx::Count)] = {};
	float FxA[static_cast<int32>(EYJJSpellFx::Count)] = {};
	float FxB[static_cast<int32>(EYJJSpellFx::Count)] = {};

	bool Has(EYJJSpellFx Fx) const { return bFx[static_cast<int32>(Fx)]; }
	float A(EYJJSpellFx Fx) const { return FxA[static_cast<int32>(Fx)]; }
	float B(EYJJSpellFx Fx) const { return FxB[static_cast<int32>(Fx)]; }
};

/** Units.csv 한 줄 (병사·몬스터 공통) */
struct FYJJUnitDef
{
	FName Id;
	EYJJTeam Team = EYJJTeam::Player;
	FString NameKey;
	float HP = 0.f;
	float Damage = 0.f;
	float AttackInterval = 0.f;
	float Range = 0.f;          // 몸 가장자리 사이 거리
	float Sight = 0.f;          // 적을 알아채는 거리
	float Speed = 0.f;
	float Radius = 0.f;
	float Mass = 0.f;
	float Splash = 0.f;         // 0이면 단일 대상
	float Knockback = 0.f;      // 범위 공격이 밀쳐내는 힘
	float ProjectileSpeed = 0.f;// 0이면 근접
	int32 ProjectileFrame = 0;  // 투사체 그림 칸 (아틀라스 투사체 줄)
	int32 Bounty = 0;           // 처치 금화 (적만)
	float Souls = 0.f;          // 처치 영혼 (적만)
	int32 AtlasRow = 0;         // Units.png 줄
	float DrawSize = 0.f;       // 그릴 크기 (월드)
	bool bBoss = false;
	bool bHero = false;         // 내 영웅 (중심, 쓰러지면 패배)
	int32 RaiseUnit = INDEX_NONE; // 강령술사: 시체를 이 유닛으로 일으킨다
	int32 BigRow = INDEX_NONE;
	/** 진급: 적을 PromoteKills 마리 잡으면 PromoteTo 가 된다 (졸병 → 베테랑 → 장군 → 영웅) */
	int32 PromoteTo = INDEX_NONE;
	int32 PromoteKills = 0;
	/** 진급 줄의 맨 처음 유닛 (특성·카드 효과는 이 유닛 기준으로 받는다). 진급하지 않는 유닛은 자기 자신 */
	int32 PromoteRoot = INDEX_NONE;
	/** 진급 단계 (0 = 처음) */
	int32 Tier = 0;
	/** 쓰러뜨리면 냄비로 가는 뼈 */
	float Bones = 0.f;
	FName VoiceKey;              // Sounds.csv Voice_<Id>   // 크게 그려지는 유닛: BigUnits.png 줄 (흐릿하지 않게 256px 칸)
};

/** Buildings.csv 한 줄 */
struct FYJJBuildingDef
{
	FName Id;
	FString NameKey;
	FString DescKey;
	int32 Unit = INDEX_NONE;    // FYJJUnitDef 번호
	int32 Cost = 0;             // 첫 건설
	float BuildCostGrowth = 0.f;// 같은 병영을 하나 더 지을 때 비용 배율
	int32 UpgradeCost = 0;      // Lv1 → Lv2
	float UpgradeCostGrowth = 0.f;
	float SpawnInterval = 0.f;
	int32 SpawnCount = 0;
	float CountPerLevel = 0.f;  // 레벨마다 늘어나는 생산 수 (소수는 누적)
	float IntervalScalePerLevel = 0.f;
	float StatPerLevel = 0.f;   // 레벨마다 병사 체력·공격력 +비율
	int32 AtlasCol = 0;         // Buildings.png 칸
	int32 IconCol = 0;          // UI.png 칸
	float BotWeight = 0.f;      // 오토파일럿이 짓는 비중
	int32 BuyCost = 0;          // 병사 즉시 구매: 한 마리 값
};

/** Waves.csv: 물결 레벨별로 나오는 몬스터 */
struct FYJJWaveSpawn
{
	int32 Unit = INDEX_NONE;
	float Rate = 0.f;   // 초당 마리
	int32 Burst = 0;    // 레벨 시작에 한꺼번에 (보스)
};

struct FYJJWaveLevel
{
	float Duration = 0.f;
	TArray<FYJJWaveSpawn> Spawns;
};

/** 컷씬 한 줄: 화자(유닛 번호, INDEX_NONE = 해설), 대사, 시간, 카메라 */
struct FYJJCutLine
{
	int32 Speaker = INDEX_NONE;
	/** 장 제목 카드 (Speaker 열 Title): 화자 없이 가운데 큰 제목 + 아래 한 줄 (글자의 첫 줄 = 제목) */
	bool bTitle = false;
	FName VoiceKey;
	FString TextKey;
	float Duration = 0.f;
	FVector2f Camera = FVector2f::ZeroVector;
	float Zoom = 0.f;   // 0이면 그대로
};

/** Stages.csv 한 줄 + 그 스테이지의 웨이브들 (Waves.csv) */
struct FYJJStageDef
{
	FName Id;
	FString NameKey;
	float HPScale = 1.f;
	TArray<FYJJWaveLevel> Waves;   // 0번 = 1웨이브
	TArray<FYJJCutLine> Cutscene;  // 시작 컷씬 (Cutscenes.csv, Order 순서)
};

/** Perks.csv 한 줄 */
struct FYJJPerkDef
{
	FName Id;
	EYJJPerkKind Kind = EYJJPerkKind::Small;
	FVector2f Pos = FVector2f::ZeroVector;   // 성좌 좌표 (중심 0,0)
	struct FMod
	{
		EYJJStat Stat = EYJJStat::Count;
		int32 Building = INDEX_NONE;   // INDEX_NONE이면 모든 병영
		float Value = 0.f;
	};
	TArray<FMod, TInlineAllocator<2>> Mods;
	/** 중간 별 특수 효과. Target: 병영 번호 / HeroTarget / INDEX_NONE(모든 아군) */
	struct FSpecial
	{
		EYJJSpecial Kind = EYJJSpecial::None;
		int32 Target = INDEX_NONE;
		float A = 0.f;
		float B = 0.f;
	};
	FSpecial Special;
	static constexpr int32 HeroTarget = -2;
	/** 주문 별이 올리는 주문 (EYJJSpell 번호, 아니면 INDEX_NONE) */
	int32 Spell = INDEX_NONE;
	TArray<int32> Links;
	FString NameKey;   // 큰 별 이름 (작은 별은 비움)
	/** 중간 별 효과 그림 (UI.png 칸, 없으면 INDEX_NONE) */
	int32 Icon = INDEX_NONE;
};

/** Broth.csv 한 줄: 왕국의 마지막 냄비 국물 단계 (뼈가 Bones 만큼 모이면 이 단계) */
struct FYJJBrothStage
{
	FName Id;
	FString NameKey;
	float Bones = 0.f;
	/** 뚜껑을 열면 이번 판 내내 아군 체력·공격력 + 비율, 지금 아군 체력 회복 비율 */
	float HP = 0.f;
	float Damage = 0.f;
	float Heal = 0.f;
	FLinearColor Color = FLinearColor::White;
};

/** 말풍선 대화가 시작되는 때 (Dialogues.csv Trigger 열 이름과 같다) */
enum class EYJJTalkTrigger : uint8
{
	StageStart,   // 컷씬이 끝나고 판이 시작될 때
	WaveStart,    // 웨이브 시작 (Wave·MinWave로 고른다)
	BossAppear,   // 보스 등장
	Promote,      // 졸병 진급 (Param = 진급한 유닛)
	PotOpen,      // 냄비 뚜껑을 열었다
	PotOverflow,  // 국이 끓어 넘쳤다
	HeroLow,      // 영웅 체력이 낮다
	Count
};

/** 말풍선 한 줄 */
struct FYJJTalkLine
{
	int32 Speaker = INDEX_NONE;   // 그 유닛 머리 위에 뜬다
	FString TextKey;
	FName VoiceKey;
	float Duration = 0.f;
	float Delay = 0.f;            // 앞 줄이 끝나고 기다리는 시간
};

/** Dialogues.csv 같은 Talk 줄들: 조건이 맞으면 한 판에 한 번 말풍선으로 주고받는다 */
struct FYJJTalk
{
	FName Id;
	int32 Stage = INDEX_NONE;     // INDEX_NONE이면 모든 스테이지
	EYJJTalkTrigger Trigger = EYJJTalkTrigger::StageStart;
	int32 Wave = 0;               // 0이 아니면 그 웨이브에서만
	int32 MinWave = 0;            // 0이 아니면 그 웨이브 이후에만
	int32 Param = INDEX_NONE;     // Promote: 이 유닛으로 진급했을 때만
	float Chance = 1.f;
	TArray<FYJJTalkLine> Lines;
};

/** 판에서 일어난 일 (HUD 말풍선 대화가 듣고 비운다) */
struct FYJJBattleEvent
{
	EYJJTalkTrigger Trigger = EYJJTalkTrigger::StageStart;
	int32 Unit = INDEX_NONE;
	int32 Wave = 0;
};
