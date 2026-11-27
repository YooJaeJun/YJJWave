#pragma once

#include "CoreMinimal.h"
#include "Sim/YJJHorde.h"
#include "Sim/YJJWaveTypes.h"
#include "Data/YJJGameData.h"
#include "Meta/YJJGuard.h"

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
	Rescue,       // 최후의 저항·부활 (초록 고리)
	Raise,        // 강령술사가 시체를 일으킴 (보라 연기)
	Burn,         // 불바다 (살아 있는 동안 BurnTick마다 피해)
	Promote,      // 진급 빛줄기
	SmackFall,    // 국자가 내려오는 중 (끝나면 쾅)
	Slash,        // 영웅·보스의 큰 휘두르기 (Power = 방향 1/-1)
	Spark,        // 난전 불꽃 (근접 타격 자리의 작은 먹 튀김)
	SkillWarn,    // 큰 스킬 예고: 쓸어 버릴 원 (Life = 기 모으는 시간)
	SkillBlast,   // 큰 스킬 쾅 고리
	SkillText,    // 싹쓸이 글자 (Power = 쓰러뜨린 수)
	DamageNumber, // 영웅·보스 타격 피해 숫자 (Power = 피해, Side 1 아군이 때림 -1 적이 때림)
};

/** 화면 연출 + 늦게 터지는 스킬 (MeteorFall·SmackFall 끝, Burn 동안의 피해) */
struct FYJJEffect
{
	EYJJEffectKind Kind = EYJJEffectKind::Build;
	FVector2f Pos = FVector2f::ZeroVector;
	float Age = 0.f;
	float Life = 0.f;
	float Radius = 0.f;
	float Power = 0.f;
	/** Burn: 다음 피해까지 쌓인 시간 */
	float Tick = 0.f;
	/** MeteorFall: 떨어진 자리에 남길 불 (초당 피해, 시간) */
	float BurnPower = 0.f;
	float BurnTime = 0.f;
	/** 스킬 연출: 1 = 아군, -1 = 적 (색) */
	float Side = 0.f;
};

/** 판이 끝나도 남는 것: 영혼과 찍은 특성, 깬 스테이지, 스테이지별 최고 별 (저장 파일과 주고받는다) */
struct FYJJProgress
{
	double Souls = 0.0;
	TArray<FName> Perks;
	int32 Cleared = 0;   // 깬 스테이지 수 (다음에 열리는 스테이지 = Cleared)
	TArray<int32> Stars; // 스테이지 번호 → 최고 별 (0 = 아직)
	/** 출전할 영웅 (Heroes.csv 이름, 비면 첫 영웅) */
	FName Hero;
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
	/** 병사 즉시 구매: 이 병영 병사 Count 마리를 진지 끝에서 바로 내보낸다 (능력치는 그 병영의 가장 높은 레벨) */
	int32 BuyCost(int32 Building, int32 Count) const;
	bool CanBuy(int32 Building, int32 Count) const;
	bool Buy(int32 Building, int32 Count);
	/** 지금 레벨에서 한 번에 내보내는 수와 주기 */
	int32 SpawnCount(int32 Plot) const;
	float SpawnInterval(int32 Plot) const;

	// ---- 주문 (F1·F2·F3, Lv은 특성 성좌의 주문 별로 올린다) ----
	bool CanCast(EYJJSpell Spell) const;
	bool Cast(EYJJSpell Spell, const FVector2f& Where);
	/** 영웅 돌격 (E): 쿨다운이 차 있으면 다음 걸음 사이에 Toward 쪽으로 내달린다 */
	bool HeroCharge(const FVector2f& Toward);
	/** 돌격 준비 (0 ~ 1, 1이면 쓸 수 있다) */
	float ChargeReady() const;
	bool HasHero() const { return Horde.Hero() != INDEX_NONE; }
	/** 이번 판 영웅 (Heroes.csv) */
	const FYJJHeroDef& HeroDef() const { return YJJData::Heroes()[HeroIndex]; }
	/** 영웅 액티브 스킬 Index 번 (Z·X·C = 0·1·2): 준비되면 다음 걸음 사이에 쓴다 */
	bool HeroSkill(int32 Index);
	/** 그 스킬 준비 (0 ~ 1, 없거나 영웅이 없으면 음수). 걸음이 끝날 때 적어 둔 값 */
	float HeroSkillReady(int32 Index) const { return HeroSkillReadyNow.IsValidIndex(Index) ? HeroSkillReadyNow[Index] : -1.f; }
	/** 그 스킬 정의 (없으면 nullptr) */
	const FYJJSkillDef* HeroSkillDef(int32 Index) const;
	int32 NumHeroSkills() const { return HeroDef().Skills.Num(); }
	/** 카드를 고르는 중 (전장 조작을 막는다) */
	bool IsChoosing() const { return bCardPending; }
	/** 봇: 지금 쓰면 둘레 적이 AutoCount 이상 걸린다 */
	bool HeroSkillWorthIt(int32 Index) const;
	float HeroHP() const;
	float HeroMaxHP() const;
	FVector2f HeroPos() const { return Horde.Hero() != INDEX_NONE ? Horde.Pos[Horde.Hero()] : FVector2f::ZeroVector; }
	/** 국자는 다음 한 번이 차오르기까지 */
	float CooldownLeft(EYJJSpell Spell) const;
	float CooldownMax(EYJJSpell Spell) const;
	float RallyLeft() const { return RallyTime; }
	/** 1 + 그 주문 별에 찍은 수 */
	int32 SpellLevel(EYJJSpell Spell) const;
	FYJJSpellStats SpellNow(EYJJSpell Spell) const { return YJJData::SpellStats(Spell, SpellLevel(Spell)); }

	// ---- 특성 성좌 (판이 끝나도 남는다) ----
	int32 PerkCost() const;
	bool IsAllocated(int32 Perk) const { return PerkAllocated.IsValidIndex(Perk) && PerkAllocated[Perk] != 0; }
	/** 찍은 수 (주문 별은 여러 번, 나머지는 0 또는 1) */
	int32 PerkLevel(int32 Perk) const { return PerkAllocated.IsValidIndex(Perk) ? PerkAllocated[Perk] : 0; }
	int32 PerkMaxLevel(int32 Perk) const;
	bool IsReachable(int32 Perk) const;
	bool CanAllocate(int32 Perk) const;
	bool Allocate(int32 Perk);
	int32 NumAllocated() const { return PerksTaken; }
	/** 특성이 올린 비율 합 (Building = INDEX_NONE 이면 모든 병영 공통분만) */
	float Bonus(EYJJStat Stat, int32 Building) const;

	// ---- 국자 내려치기 (스킬 F3): 전장을 클릭하면 그 자리를 큰 국자로 쾅, 횟수가 차오른다 ----
	bool CanSmack() const { return !bGameOver && SmackCharge >= 1.f; }
	bool Smack(const FVector2f& Where);
	/** 지금 쓸 수 있는 횟수 (소수 = 다음 한 번이 차오르는 중) */
	float SmackCharges() const { return SmackCharge; }
	int32 SmackMax() const;
	/** HUD가 화면을 흔들고 0으로 (보스 등장·쓰러짐에만) */
	int32 ShakeRequests = 0;

	// ---- 왕국의 마지막 냄비 (적의 뼈를 고아 군대를 먹인다, 이번 판 한정) ----
	/** 지금 국물 단계 (Broth.csv 번호, 뼈가 모자라면 INDEX_NONE) */
	int32 PotStage() const;
	float PotBones() const { return BonesInPot; }
	/** 다음 단계까지 필요한 뼈 (마지막 단계면 0) */
	float PotNextBones() const;
	/** 마지막 단계에서 끓어 넘치기까지 남은 시간 (마지막 단계가 아니면 음수) */
	float PotOverflowLeft() const;
	bool CanOpenPot() const { return !bGameOver && PotStage() != INDEX_NONE; }
	/** 뚜껑을 연다: 지금 단계 국물로 이번 판 내내 아군 체력·공격력 +, 지금 아군 회복. 냄비는 비운다 */
	bool OpenPot();
	/** 뚜껑을 연 지 (HUD 뚜껑 들썩 연출, 음수면 연 적 없음) */
	float PotLidAge() const { return LidAge; }
	/** 끓어 넘친 지 (HUD 김 연출, 음수면 넘친 적 없음) */
	float PotOverflowAge() const { return OverflowAge; }
	int32 BrothServed() const { return Served; }
	float BrothHP() const { return ServedHP; }
	float BrothDamage() const { return ServedDamage; }

	/** 말풍선 대화가 듣는 사건 (HUD가 가져가고 비운다) */
	TArray<FYJJBattleEvent> Events;

	// ---- 카드 (웨이브가 바뀔 때 3장 중 1장, 이번 판 한정) ----
	bool bCardPending = false;
	TArray<int32> CardOffer;
	TArray<int32> CardsTaken;
	bool PickCard(int32 Choice);

	// ---- 상태 ----
	FYJJHorde Horde;
	TArray<FYJJPlot> Plots;
	TArray<FYJJEffect> Effects;
	/** HUD가 시체·피 연출에 쓰고 비운다 */
	TArray<FYJJDeath> RecentDeaths;
	/** HUD가 바닥에 그을음으로 굽고 비운다 (x, y, 반경) */
	TArray<FVector3f> RecentCraters;
	/** 소리 사건 (Sounds.csv 키). 게임 모드가 틀고 비운다 */
	TArray<FName> Sounds;

	int32 Stage = 0;
	/** 금화·영혼은 메모리에 숨겨 둔다 (Meta/YJJGuard.h) */
	FYJJGuardedNumber Gold = 0.0;
	FYJJGuardedNumber Souls = 0.0;
	/** 이번 판에 얻은 영혼 (결과 창) */
	double SoulsEarned = 0.0;
	int32 Level = 1;   // 지금 웨이브 (1부터)
	float LevelTime = 0.f;
	float Time = 0.f;
	int32 Kills = 0;
	/** 아군 앞줄이 밀고 나간 만큼 금화·영혼 배율 (1 ~ 1+FrontBonusMax) */
	float FrontBonus = 1.f;
	int32 Spawned[static_cast<int32>(EYJJTeam::Count)] = { 0, 0 };
	bool bGameOver = false;
	bool bVictory = false;
	/** 이겼지만 마무리 연출 중 (적이 다 쓰러졌고 VictoryDelay 뒤에 결과) */
	bool bEnding = false;
	/** 이번 판 별 (이겼을 때 성벽 체력 비율로 1~3, 지면 0) */
	int32 Stars = 0;
	/** 성벽: 적이 때려 0이 되면 패배 */
	float WallHP = 0.f;
	float WallMaxHP = 0.f;
	/** 성벽을 막 맞았다 (HUD 깜빡임) */
	float WallFlash = 0.f;
	/** 영웅이 쓰러지면 이만큼 뒤 성문 앞에서 다시 */
	float HeroRespawnLeft = 0.f;
	/** 보스가 쓰러질 때 잠깐 느리게 (게임 모드가 배속에 곱한다) */
	float TimeScale() const;
	/** 실제 시간으로 흐르는 연출 (멈칫). 게임 모드가 프레임마다 */
	void TickRealTime(float RealDeltaSeconds) { HitStopLeft = FMath::Max(HitStopLeft - RealDeltaSeconds, 0.f); }
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
	int32 SpawnEnemyAt(int32 Unit, const FVector2f& Pos);
	void OfferCards();
	void TickNecro(float Dt);
	void TickPot(float Dt);
	void AddEvent(EYJJTalkTrigger Trigger, int32 Unit = INDEX_NONE);
	void ApplySpecialDef(const FYJJPerkDef& Def, TArray<FYJJUnitSpecial>& Specials);
	void SpawnHero();
	FYJJSpawnStats PlayerStats(int32 Building, int32 BuildingLevel) const;
	FYJJEffect& AddEffect(EYJJEffectKind Kind, const FVector2f& Pos, float Life, float Radius, float Power = 0.f);
	void CastMeteor(const FYJJSpellStats& Stats, const FVector2f& Target, float Power);
	void DropMeteor(const FVector2f& Pos, float Delay, float Radius, float Damage, float BurnPower, float BurnTime);
	void CastRally(const FYJJSpellStats& Stats, float Power);
	/** 병영 병사 Count 명을 진지 끝에서 바로 내보낸다 (값 없이) */
	void SpawnSquad(int32 Building, int32 Count);
	void ApplyPerk(int32 Perk);
	void RecomputeHero();
	/** 찍힌 중간 별로 유닛 종류별 특수 효과와 판 단위 효과를 다시 계산 */
	void RebuildSpecials();
	void Finish(bool bWon);

	TArray<uint8> PerkAllocated;
	int32 PerksTaken = 0;
	/** [능력치][0 = 공통, 1+ = 병영 번호+1] */
	TArray<float> Mods;
	TArray<float> EnemyCarry;   // 몬스터별 소수 생산 누적
	float Cooldowns[static_cast<int32>(EYJJSpell::Count)] = {};
	float RallyTime = 0.f;
	/** 지금 함성의 공격 속도 배율 (함성 Lv) */
	float RallyAttack = 1.f;
	FRandomStream Random;

	// 판 단위 특수 효과 (중간 별)
	float HeroRegenBonus = 0.f;
	float GoldBurstChance = 0.f;
	float GoldBurstMult = 1.f;
	float SoulBurstChance = 0.f;
	float SoulBurstMult = 1.f;
	float MeteorTwiceDelay = 0.f;
	float RallyHealFraction = 0.f;

	float SmackCharge = 0.f;
	inline static const FName SoundSmack = TEXT("Smack");
	inline static const FName SoundWallBreak = TEXT("WallBreak");
	inline static const FName SoundSwing = TEXT("Swing");

	// 냄비
	float BonesInPot = 0.f;
	float OverflowTime = 0.f;
	float LidAge = -1.f;
	float OverflowAge = -1.f;
	// 적 진지: 살아 있는 진지 자리 (적은 여기서 나온다, 비면 승리)
	TArray<FVector2f> CampSpots;
	// 영웅 돌격
	float ChargeCooldown = 0.f;
	bool bChargeQueued = false;
	FVector2f ChargeTarget = FVector2f::ZeroVector;
	// 영웅 큰 스킬
	/** 다음 걸음 사이에 쓸 영웅 스킬 (INDEX_NONE 없음) */
	int32 QueuedSkill = INDEX_NONE;
	int32 HeroIndex = 0;
	void SummonAround(const FYJJSkillDef& Skill, const FVector2f& Center);
	TArray<float> HeroSkillReadyNow;
	void HandleSkillEvents();
	// 맞붙은 전선 먼지
	float ClashTimer = 0.f;
	// 길목 줄 (바위 오른쪽 끝 x, 가까운 것끼리 묶음)과 넘었는지
	TArray<float> ChokeXs;
	TArray<uint8> ChokePassed;
	int32 Served = 0;
	float ServedHP = 0.f;
	float ServedDamage = 0.f;
	bool bHeroLowSaid = false;
	int32 LastPotStage = INDEX_NONE;
	float EndTimer = 0.f;
	float SlowMoLeft = 0.f;
	/** 큰 스킬이 맞은 순간 멈칫 (실제 시간으로 줄어든다) */
	float HitStopLeft = 0.f;

	// 강령술: 쓰러진 자리 (오래된 것부터 덮어쓴다)
	TArray<FVector2f> Corpses;
	int32 CorpseCursor = 0;
	float NecroTimer = 0.f;

	int32 ModIndex(EYJJStat Stat, int32 Column) const;

	static constexpr uint32 RandomSeed = 7742u;
	static constexpr float BossEffectRadiusScale = 2.f;
	static constexpr float Half = 0.5f;

	// 소리 사건 키 (Sounds.csv Name)
	inline static const FName SoundHit = TEXT("Hit");
	inline static const FName SoundArrow = TEXT("Arrow");
	inline static const FName SoundDeath = TEXT("Death");
	inline static const FName SoundRescue = TEXT("Rescue");
	inline static const FName SoundRaise = TEXT("Raise");
	inline static const FName SoundVictory = TEXT("Victory");
	inline static const FName SoundDefeat = TEXT("Defeat");
	inline static const FName SoundMeteorFall = TEXT("MeteorFall");
	inline static const FName SoundMeteorBoom = TEXT("MeteorBoom");
	inline static const FName SoundRally = TEXT("Rally");
	inline static const FName SoundHeroVoice = TEXT("Voice_Hero");
	inline static const FName SoundBossAppear = TEXT("BossAppear");
	inline static const FName SoundCard = TEXT("Card");
	inline static const FName SoundBuild = TEXT("Build");
	inline static const FName SoundUpgrade = TEXT("Upgrade");
	inline static const FName SoundBuy = TEXT("Buy");
	inline static const FName SoundPromote = TEXT("Promote");
	inline static const FName SoundPotOpen = TEXT("PotOpen");
	inline static const FName SoundPotOverflow = TEXT("PotOverflow");
};
