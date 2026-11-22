#pragma once

#include "CoreMinimal.h"
#include "Sim/YJJHorde.h"
#include "Sim/YJJWaveTypes.h"

/** 진지의 건설 칸 하나 */
struct FYJJPlot
{
	FVector2f Pos = FVector2f::ZeroVector;
	int32 Building = INDEX_NONE;
	int32 Level = 0;
	float SpawnTimer = 0.f;
	/** 방금 병사를 내보냈다 (HUD가 들썩이는 연출에 쓴다) */
	float Pulse = 0.f;
};

enum class EYJJEffectKind : uint8
{
	MeteorFall,   // 떨어지는 중 (끝나면 터진다)
	MeteorBlast,  // 터진 불꽃
	Rally,        // 함성 고리
	Build,        // 건설·레벨업 먼지
};

/** 화면 연출 (규칙에 쓰이는 것은 MeteorFall 이 끝날 때의 피해뿐) */
struct FYJJEffect
{
	EYJJEffectKind Kind = EYJJEffectKind::Build;
	FVector2f Pos = FVector2f::ZeroVector;
	float Age = 0.f;
	float Life = 0.f;
	float Radius = 0.f;
	float Power = 0.f;
};

/** 판이 끝나도 남는 것: 영혼과 찍은 특성, 깬 스테이지 (저장 파일과 주고받는다) */
struct FYJJProgress
{
	double Souls = 0.0;
	TArray<FName> Perks;
	int32 Cleared = 0;   // 깬 스테이지 수 (다음에 열리는 스테이지 = Cleared)
};

/**
 * 한 판(스테이지 하나)의 규칙: 진지·병영·웨이브·보스·영웅·금화·영혼·특성 성좌·스펠.
 * 유닛 자체는 Horde가 움직인다. 고정 틱(Tuning SimStep)으로 PreStep → Horde.Step → PostStep.
 *
 * 이기는 법: 마지막 웨이브가 끝나고(보스 포함) 적이 하나도 없으면 스테이지 돌파.
 * 지는 법: 내 영웅(중심)이 쓰러지면.
 */
class YJJWAVE_API FYJJBattle
{
public:
	void Reset(int32 InStage, const FYJJProgress& Progress);
	/** 한 틱 = PreStep(규칙·생산, 게임 스레드) → Horde.Step(작업 스레드 가능) → PostStep(연출·보상·승패, 게임 스레드) */
	void Tick(float Dt);
	void PreStep(float Dt);
	void PostStep(float Dt);

	/** 지금 영혼·특성을 Progress에 적는다 (저장 전에) */
	void WriteProgress(FYJJProgress& Progress) const;

	// ---- 병영 ----
	int32 BuildCost(int32 Building) const;
	bool CanBuild(int32 Plot, int32 Building) const;
	bool Build(int32 Plot, int32 Building);
	/** 최대 레벨이면 0 */
	int32 UpgradeCost(int32 Plot) const;
	bool CanUpgrade(int32 Plot) const;
	bool Upgrade(int32 Plot);
	int32 BuildingCount(int32 Building) const;
	/** 지금 레벨에서 한 번에 내보내는 수와 주기 */
	int32 SpawnCount(int32 Plot) const;
	float SpawnInterval(int32 Plot) const;

	// ---- 스펠 ----
	bool CanCast(EYJJSpell Spell) const;
	bool Cast(EYJJSpell Spell, const FVector2f& Where);
	float CooldownLeft(EYJJSpell Spell) const { return Cooldowns[static_cast<int32>(Spell)]; }
	float CooldownMax(EYJJSpell Spell) const;
	float RallyLeft() const { return RallyTime; }

	// ---- 특성 성좌 (판이 끝나도 남는다) ----
	int32 PerkCost() const;
	bool IsAllocated(int32 Perk) const { return PerkAllocated.IsValidIndex(Perk) && PerkAllocated[Perk] != 0; }
	bool IsReachable(int32 Perk) const;
	bool CanAllocate(int32 Perk) const;
	bool Allocate(int32 Perk);
	int32 NumAllocated() const { return PerksTaken; }
	/** 특성이 올린 비율 합 (Building = INDEX_NONE 이면 모든 병영 공통분만) */
	float Bonus(EYJJStat Stat, int32 Building) const;

	// ---- 상태 ----
	FYJJHorde Horde;
	TArray<FYJJPlot> Plots;
	TArray<FYJJEffect> Effects;
	/** HUD가 시체·피 연출에 쓰고 비운다 */
	TArray<FYJJDeath> RecentDeaths;
	/** HUD가 바닥에 그을음으로 굽고 비운다 */
	TArray<FVector2f> RecentCraters;

	int32 Stage = 0;
	double Gold = 0.0;
	double Souls = 0.0;
	/** 이번 판에 얻은 영혼 (결과 창) */
	double SoulsEarned = 0.0;
	int32 Level = 1;   // 지금 웨이브 (1부터)
	float LevelTime = 0.f;
	float Time = 0.f;
	int32 Kills = 0;
	int32 Spawned[static_cast<int32>(EYJJTeam::Count)] = { 0, 0 };
	bool bGameOver = false;
	bool bVictory = false;
	float FieldWidth = 0.f;
	float FieldHeight = 0.f;
	float CampEdgeX = 0.f;

	int32 NumWaves() const;
	float LevelDuration() const;
	const FYJJStageDef& StageDef() const;

private:
	void TickPlots(float Dt);
	void TickEnemies(float Dt);
	void TickEffects(float Dt);
	void CollectRewards();
	void CheckVictory();
	void StartLevel(int32 NewLevel);
	void SpawnEnemy(int32 Unit);
	void SpawnHero();
	FYJJSpawnStats PlayerStats(int32 Building, int32 BuildingLevel) const;
	void AddEffect(EYJJEffectKind Kind, const FVector2f& Pos, float Life, float Radius, float Power = 0.f);
	void ApplyPerk(int32 Perk);
	void RecomputeHero();
	void Finish(bool bWon);

	TArray<uint8> PerkAllocated;
	int32 PerksTaken = 0;
	/** [능력치][0 = 공통, 1+ = 병영 번호+1] */
	TArray<float> Mods;
	TArray<float> EnemyCarry;   // 몬스터별 소수 생산 누적
	float Cooldowns[static_cast<int32>(EYJJSpell::Count)] = { 0.f, 0.f };
	float RallyTime = 0.f;
	FRandomStream Random;

	int32 ModIndex(EYJJStat Stat, int32 Column) const;

	static constexpr uint32 RandomSeed = 7742u;
	static constexpr float BossEffectRadiusScale = 2.f;
	static constexpr float Half = 0.5f;
};
