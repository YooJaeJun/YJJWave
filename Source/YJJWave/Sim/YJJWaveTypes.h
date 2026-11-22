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
	Start,    // 처음부터 찍혀 있는 중심 별
	Small,    // 잔잔한 숫자
	Notable,  // 성격이 뚜렷한 큰 별
};

/** 스펠 */
enum class EYJJSpell : uint8
{
	Meteor,   // Q: 커서 위치에 큰 범위 피해 + 밀쳐내기
	Rally,    // E: 아군 전체 공격·이동 속도 증가
	Count
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

/** Stages.csv 한 줄 + 그 스테이지의 웨이브들 (Waves.csv) */
struct FYJJStageDef
{
	FName Id;
	FString NameKey;
	float HPScale = 1.f;
	TArray<FYJJWaveLevel> Waves;   // 0번 = 1웨이브
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
	TArray<int32> Links;
	FString NameKey;   // 큰 별 이름 (작은 별은 비움)
};
