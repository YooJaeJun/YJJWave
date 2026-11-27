#pragma once

#include "CoreMinimal.h"
#include "Sim/YJJWaveTypes.h"

/*
 * 초대용량 유닛 시뮬레이션.
 *
 * 유닛은 액터가 아니라 아래 배열(SoA)의 한 칸(슬롯)이다. 죽은 슬롯은 빈 목록에 들어가 다시 쓰인다.
 * 슬롯을 가리킬 때는 (슬롯, 세대) 쌍을 쓴다 → 죽고 다른 유닛이 들어온 슬롯을 잘못 때리지 않는다.
 *
 * 한 틱(Step):
 *   1) 편별 균일 격자를 계수 정렬로 다시 만든다 (O(N))
 *   2) ParallelFor: 대상 찾기(몇 틱에 한 번, 엇갈려서) → 이동 의도 → 이웃 밀어내기 → 다음 위치 (자기 칸에만 쓴다)
 *   3) 순차: 공격 적용 (근접 피해·범위·밀쳐내기, 원거리는 투사체 생성, 성벽 공격)
 *   4) 순차: 투사체 이동·명중
 *   5) 순차: 죽은 유닛 정리 → Deaths (게임 규칙이 보상, HUD가 시체 연출에 쓴다)
 */

/** 편 하나의 균일 격자. Items에 그 칸 유닛 슬롯이 Start[칸]~Start[칸+1] 범위로 들어 있다 */
struct FYJJGrid
{
	int32 W = 0;
	int32 H = 0;
	float Cell = 1.f;
	float InvCell = 1.f;
	TArray<int32> Start;
	TArray<int32> Items;

	void Init(const FVector2f& FieldSize, float CellSize);
	FIntPoint CellOf(const FVector2f& Pos) const;
	int32 Index(int32 X, int32 Y) const { return Y * W + X; }
};

/** 유닛이 생길 때 정해지는 능력치 배율 (특성·병영 레벨·물결 레벨이 만든다) */
struct FYJJSpawnStats
{
	float HP = 1.f;
	float Damage = 1.f;
	float AttackSpeed = 1.f;
	float MoveSpeed = 1.f;
	float Splash = 1.f;
	int32 Multishot = 1;
};

/** 날아가는 화살·뼈 화살 */
struct FYJJProjectile
{
	FVector2f Start = FVector2f::ZeroVector;
	FVector2f Pos = FVector2f::ZeroVector;
	FVector2f Aim = FVector2f::ZeroVector;
	int32 Target = INDEX_NONE;
	uint32 TargetGen = 0;
	float Damage = 0.f;
	float Splash = 0.f;
	float Knockback = 0.f;
	float Speed = 0.f;
	EYJJTeam Team = EYJJTeam::Player;
	uint8 Frame = 0;
};

/** 이번 틱에 죽은 유닛 (소비하는 쪽이 비운다) */
struct FYJJDeath
{
	FVector2f Pos = FVector2f::ZeroVector;
	FVector2f Fling = FVector2f::ZeroVector;
	int32 Type = 0;
	EYJJTeam Team = EYJJTeam::Player;
	bool bFaceLeft = false;
};

/** 이번 틱에 진급한 유닛 (소비하는 쪽이 비운다) */
struct FYJJPromotion
{
	FVector2f Pos = FVector2f::ZeroVector;
	int32 Type = 0;   // 진급한 뒤 유닛
};

/** 머리 위 체력 게이지 (영웅·보스) */
struct FYJJUnitBar
{
	FVector2f Pos = FVector2f::ZeroVector;
	int32 Type = 0;
	float HP = 0.f;
	float MaxHP = 0.f;
};

/** Step 구간 (성능 측정용) */
enum class EYJJHordePhase : uint8
{
	Grid,      // 격자 다시 만들기 (순차)
	Think,     // 대상 찾기·이동·밀어내기 (ParallelFor)
	Combat,    // 위치 반영 + 공격 적용 + 투사체 (순차)
	Cleanup,   // 죽은 유닛 정리 (순차)
	Count
};

/**
 * 그리기용 사본. 시뮬은 작업 스레드에서 돌고, HUD는 프레임 시작에 복사해 둔 이 사본만 읽는다
 * (시뮬과 그리기가 동시에 돌 수 있게).
 */
/** 영웅·보스의 큰 휘두르기 (전투가 가져가 붓 초승달 연출을 띄운다) */
struct FYJJSlash
{
	FVector2f Pos = FVector2f::ZeroVector;
	float Radius = 0.f;
	/** 1 = 오른쪽으로 휘두름, -1 = 왼쪽 */
	float Dir = 1.f;
};

struct FYJJHordeView
{
	int32 HighWater = 0;
	TArray<uint8> Alive;
	TArray<uint8> Team;
	TArray<uint8> FaceLeft;
	TArray<uint16> Type;
	TArray<FVector2f> Pos;
	TArray<float> MoveSpeed;   // 지금 속력 (멈춰 있으면 숨쉬기 그림)
	TArray<float> Flash;
	TArray<float> AttackAnim;
	TArray<float> WalkPhase;
	TArray<float> AttackLen;   // 이번 공격 동작 길이 (AttackAnim 진행도 = 1 - AttackAnim/AttackLen)
	TArray<float> Knock;       // 밀쳐진 속력 (크면 휘청 그림)
	TArray<FYJJProjectile> Projectiles;
	int32 Count[static_cast<int32>(EYJJTeam::Count)] = { 0, 0 };
	float Front[static_cast<int32>(EYJJTeam::Count)] = { -1.f, -1.f };
	/** 영웅·보스 머리 위 게이지 */
	TArray<FYJJUnitBar> Bars;
	/** 살아 있는 첫 보스 (없으면 INDEX_NONE) */
	int32 Boss = INDEX_NONE;
	float BossHP = 0.f;
	float BossMaxHP = 0.f;
	float HeroHP = 0.f;
	float HeroMaxHP = 0.f;
	float StepMs = 0.f;
	float PhaseMs[static_cast<int32>(EYJJHordePhase::Count)] = {};
	float ReorderMs = 0.f;

	int32 Num(EYJJTeam InTeam) const { return Count[static_cast<int32>(InTeam)]; }
};

/** 시뮬이 Tuning.csv 에서 읽어 두는 값 */
struct FYJJHordeConfig
{
	float CellSize = 0.f;
	int32 RetargetTicks = 1;
	int32 MaxNeighbors = 0;
	float SeparationStiffness = 0.f;
	float MaxPushPerTick = 0.f;
	float Acceleration = 0.f;
	float ImpulseDamping = 0.f;
	float LateralDrift = 0.f;
	float FieldMargin = 0.f;
	float HitFlashTime = 0.f;
	float AttackAnimTime = 0.f;
	float ArrowHitRadius = 0.f;
	float DeathFling = 0.f;
	float BigUnitRadius = 0.f;
	float MultishotSpread = 0.f;
	int32 ReorderTicks = 0;
	float AttackAnimShare = 0.f;
	float AttackAnimMax = 0.f;
	float WalkStrideScale = 0.f;
	float WalkStrideMin = 0.f;
	float WalkStrideMax = 0.f;
	/** 아군이 나아가는 끝 x (적 진영을 남겨 두고 그 앞에서 밀고 당긴다) */
	float PlayerAdvanceX = 0.f;
	/** 성벽 x, 성문 가운데 y와 반 높이, 성문을 나와 모이는 거리 */
	float WallX = 0.f;
	float GateY = 0.f;
	float GateHalf = 0.f;
	float GateExit = 0.f;
	/** 공격 동작 중 이 비율에서 맞는다 (휘두르기 전에 맞지 않게) */
	float StrikeShare = 0.f;
	/** 적이 성벽을 때리는 거리 여유 */
	float WallReach = 0.f;
	/** 그 끝보다 이만큼 더 안쪽 적은 쫓지 않는다 */
	float PlayerChaseMargin = 0.f;
};


class YJJWAVE_API FYJJHorde
{
public:
	/** 넓이 FieldSize 의 전장, 유닛 최대 Capacity 마리 */
	void Reset(const FVector2f& FieldSize, int32 Capacity);

	/** 영웅(중심): 자리 Home 에서 Leash 안의 적만 쫓고, 적은 상대가 없으면 영웅을 향해 온다 */
	void SetHero(int32 Slot, const FVector2f& Home, float Leash, float Arrive);
	int32 Hero() const { return HeroSlot; }
	/** 영웅 자리를 옮긴다 (졸병 앞줄을 따라 라인을 이끈다) */
	void SetHeroHome(const FVector2f& Home) { HeroHome = Home; }
	void Heal(int32 Slot, float Amount);
	void SetMaxHP(int32 Slot, float NewMax);

	/** 빈 슬롯이 없으면 INDEX_NONE */
	int32 Spawn(int32 Type, EYJJTeam Team, const FVector2f& Pos, const FYJJSpawnStats& Stats);
	/** 길목 지형 (가운데 x, y, 반너비, 반높이: 월드). PassTerrain 유닛 말고는 타원 안으로 못 들어간다 */
	void SetBlockers(const TArray<FVector4f>& InBlockers) { Blockers = InBlockers; }
	/** 아군 병사가 멈추는 선 (적 진지가 있으면 끝까지 밀고 간다) */
	void SetPlayerAdvanceX(float X) { Config.PlayerAdvanceX = X; }
	/** 영웅 돌격: Toward 쪽으로 Duration 동안 내달리며 HitInterval마다 앞의 적을 때리고 날린다 (게임 스레드, 걸음 사이에) */
	void StartCharge(const FVector2f& Toward, float Duration, float InSpeed, float DamageScale, float Radius, float Knockback, float HitInterval);
	bool IsCharging() const { return ChargeLeft > 0.f; }

	void Step(float Dt);

	/** 그리기용 사본을 만든다 (시뮬이 돌지 않을 때 게임 스레드에서) */
	void CopyTo(FYJJHordeView& View) const;

	/** 원 안의 Victims 편에게 피해 + 바깥으로 밀쳐내기 (스펠·범위 공격) */
	void DamageArea(const FVector2f& Center, float Radius, float Damage, float Knockback, EYJJTeam Victims, int32 Attacker = INDEX_NONE);

	/** 유닛 종류별 특수 효과 (특성이 바뀔 때 FYJJBattle이 넣는다) */
	void SetSpecials(const TArray<FYJJUnitSpecial>& InSpecials);
	/** 한 편 전체를 최대 체력의 Fraction만큼 회복 */
	void HealTeam(EYJJTeam InTeam, float Fraction);

	// ---- 읽기 (HUD·규칙) ----
	int32 Capacity() const { return Alive.Num(); }
	int32 HighWater() const { return HighWaterMark; }
	int32 Num(EYJJTeam InTeam) const { return TeamCount[static_cast<int32>(InTeam)]; }
	FVector2f FieldSize() const { return Field; }
	/** 아군 가장 앞(오른쪽) x, 적 가장 앞(왼쪽) x. 그 편이 없으면 -1 */
	float FrontX(EYJJTeam InTeam) const { return Front[static_cast<int32>(InTeam)]; }
	/** 영웅을 뺀 아군 가장 앞 x. 없으면 -1 */
	float TroopFrontX() const { return TroopFront; }
	float LastStepMs() const { return StepMs; }
	float LastReorderMs() const { return ReorderMs; }
	float LastPhaseMs(EYJJHordePhase Phase) const { return PhaseMs[static_cast<int32>(Phase)]; }
	int64 StepCount() const { return Steps; }

	// 유닛 배열 (슬롯 0 ~ HighWater-1, Alive가 0이면 빈 칸)
	TArray<uint8> Alive;
	TArray<uint8> Team;
	TArray<uint16> Type;
	TArray<FVector2f> Pos;
	TArray<FVector2f> Vel;
	TArray<float> HP;
	TArray<float> MaxHP;
	TArray<float> Flash;       // 맞은 뒤 남은 깜빡임 시간
	TArray<float> AttackAnim;  // 휘두르는 그림을 보여 줄 남은 시간
	TArray<float> WalkPhase;   // 걷기 그림 넘김
	TArray<float> AttackLen;   // 이번 공격 동작 길이 (공격 간격에 비례, 큰 유닛은 느릿하게)
	TArray<uint8> FaceLeft;

	/** 이번 틱까지 죽은 유닛. 쓰는 쪽(FYJJBattle)이 비운다 */
	TArray<FYJJDeath> Deaths;
	TArray<FYJJProjectile> Projectiles;
	/** 적이 성벽에 준 피해 (FYJJBattle이 가져가 0으로) */
	float WallDamage = 0.f;
	/** 이번 걸음의 근접 타격 자리 (난전 불꽃, MaxSparks까지. 전투가 가져가고 비운다) */
	TArray<FVector2f> HitSparks;
	int32 MaxSparks = 0;
	/** 이번 걸음의 영웅·보스 휘두르기 (전투가 가져가고 비운다) */
	TArray<FYJJSlash> Slashes;
	/** 이번 틱 최후의 저항·부활이 일어난 자리 (FYJJBattle이 연출로 바꾸고 비운다) */
	TArray<FVector2f> Rescued;
	/** 적을 잡아 진급한 유닛 (FYJJBattle이 연출로 바꾸고 비운다) */
	TArray<FYJJPromotion> Promoted;
	/** 소리용: 이번 틱들 동안 타격·화살 수 (FYJJBattle이 가져가 0으로) */
	int32 HitCount = 0;
	int32 ShotCount = 0;
	/** 편별 이동·공격 속도 버프 배율 (스펠) */
	float MoveBuff[static_cast<int32>(EYJJTeam::Count)] = { 1.f, 1.f };
	float AttackBuff[static_cast<int32>(EYJJTeam::Count)] = { 1.f, 1.f };

private:
	void BuildGrids();
	void BuildGrid(int32 TeamId);
	/** 살아 있는 유닛을 격자 칸 순서로 앞에서부터 다시 채운다 (이웃끼리 메모리도 가깝게 → 캐시 적중) */
	void Reorder();
	void ThinkAndMove(int32 Slot, float Dt);
	int32 FindNearestEnemy(int32 Slot, float Sight) const;
	FVector2f Separation(int32 Slot) const;
	void ApplyAttacks();
	void Shoot(int32 Slot, int32 Target);
	/** 피해. Attacker(때린 유닛)가 있으면 흡혈, 근접이면 가시 반사도 */
	void Hit(int32 Victim, float Damage, const FVector2f& From, int32 Attacker = INDEX_NONE, bool bMelee = false);
	const FYJJUnitSpecial& SpecialOf(int32 Slot) const;
	/** 공격 시작: 동작을 틀고, StrikeShare 뒤에 맞는다 */
	void BeginStrike(int32 Slot, int32 Victim);
	/** 진급: 다음 유닛 종류로 바뀌고 능력치는 같은 비율만큼 오른다, 체력은 가득 */
	void Promote(int32 Slot);
	/** 분노까지 더한 이번 공격 피해 */
	float AttackDamage(int32 Slot) const;
	/** 공격 동작을 시작한다 (길이는 공격 간격에 비례) */
	void StartAttackAnim(int32 Slot);
	void UpdateProjectiles(float Dt);
	void CollectDead();
	bool IsValidTarget(int32 Slot, uint32 Gen) const;
	int32 FindEnemyNear(const FVector2f& Center, float Radius, EYJJTeam Victims, int32 Skip) const;

	static int32 TeamIndex(EYJJTeam InTeam) { return static_cast<int32>(InTeam); }
	static EYJJTeam Opponent(EYJJTeam InTeam) { return InTeam == EYJJTeam::Player ? EYJJTeam::Enemy : EYJJTeam::Player; }

	// 슬롯별 내부 상태
	TArray<uint32> Gen;
	TArray<int32> Target;
	TArray<uint32> TargetGen;
	TArray<float> Cooldown;
	TArray<float> Damage;
	TArray<float> AttackInterval;
	TArray<float> Speed;
	TArray<float> Splash;
	TArray<uint8> Multishot;
	TArray<FVector2f> Impulse;
	TArray<FVector2f> PosNext;
	TArray<FVector2f> LastHitDir;
	/** 이번 틱 공격 대상 (INDEX_NONE 없음) */
	TArray<int32> Pending;
	/** 유닛별 한 번 쓰는 효과 표시 (EYJJUnitFlag 비트) */
	TArray<uint8> Flags;
	/** 유닛마다 고정된 난수 (행군 흔들림 박자). 슬롯 번호를 쓰면 재배치 뒤 이웃끼리 박자가 같아져 줄무늬가 생긴다 */
	TArray<float> Seed;
	/** 이 유닛이 쓰러뜨린 적 수 (진급) */
	TArray<uint16> Kills;
	/** 휘두르는 중: 맞기까지 남은 시간과 맞을 대상 (WallTarget = 성벽) */
	TArray<float> Windup;
	TArray<int32> Strike;
	TArray<uint32> StrikeGen;

	TArray<int32> FreeSlots;
	TArray<int32> BigUnits;   // 반지름이 큰 유닛(보스): 모두가 따로 밀어내기 검사
	int32 HighWaterMark = 0;
	int32 HeroSlot = INDEX_NONE;
	int32 TeamCount[static_cast<int32>(EYJJTeam::Count)] = { 0, 0 };
	float Front[static_cast<int32>(EYJJTeam::Count)] = { -1.f, -1.f };
	float TroopFront = -1.f;
	FVector2f Field = FVector2f::ZeroVector;
	FVector2f HeroHome = FVector2f::ZeroVector;
	float HeroLeash = 0.f;
	float HeroArrive = 0.f;
	float MaxNormalRadius = 0.f;
	float StepMs = 0.f;
	float PhaseMs[static_cast<int32>(EYJJHordePhase::Count)] = {};
	float ReorderMs = 0.f;
	int64 Steps = 0;
	/** Reorder용 (옛 슬롯 → 새 슬롯) */
	TArray<int32> Remap;
	TArray<int32> Order;
	FRandomStream Random;
	FYJJHordeConfig Config;
	/** 난전 불꽃: 때린 쪽에서 맞은 쪽으로 이 비율 자리 */
	static constexpr float SparkLean = 0.65f;
	TArray<FVector4f> Blockers;
	// 영웅 돌격
	void TickCharge(float Dt);
	FVector2f ChargeDir = FVector2f::ZeroVector;
	float ChargeLeft = 0.f;
	float ChargeTick = 0.f;
	float ChargeSpeed = 0.f;
	float ChargeDamageScale = 0.f;
	float ChargeRadius = 0.f;
	float ChargeKnockback = 0.f;
	float ChargeHitInterval = 0.f;
	FYJJGrid Grids[static_cast<int32>(EYJJTeam::Count)];
	TArray<FYJJUnitSpecial> Specials;
	FYJJUnitSpecial NoSpecial;

	static constexpr int32 ParallelChunk = 256;
	/** Pending·Strike 대상이 성벽 */
	static constexpr int32 WallTarget = -2;
	/** 성문 안쪽 줄을 이 비율 안으로 모은다 (가장자리에 걸리지 않게) */
	static constexpr float GateFunnel = 0.7f;
	static constexpr float NearZero = 1.e-4f;
	static constexpr double MsPerSecond = 1000.0;
	static constexpr float HalfShare = 0.5f;
	static constexpr uint32 RandomSeed = 20261122u;
	static constexpr float DriftPhaseSpeed = 0.37f;
	static constexpr float FullTurn = 2.f * UE_PI;
};
