#include "Sim/YJJHorde.h"
#include "Async/ParallelFor.h"
#include "Data/YJJGameData.h"
#include "HAL/PlatformTime.h"

void FYJJGrid::Init(const FVector2f& FieldSize, float CellSize)
{
	Cell = CellSize;
	InvCell = 1.f / CellSize;
	W = FMath::Max(1, FMath::CeilToInt(FieldSize.X * InvCell));
	H = FMath::Max(1, FMath::CeilToInt(FieldSize.Y * InvCell));
	Start.Reset();
	Start.AddZeroed(W * H + 1);
	Items.Reset();
}

FIntPoint FYJJGrid::CellOf(const FVector2f& P) const
{
	return FIntPoint(
		FMath::Clamp(FMath::FloorToInt(P.X * InvCell), 0, W - 1),
		FMath::Clamp(FMath::FloorToInt(P.Y * InvCell), 0, H - 1));
}

void FYJJHorde::Reset(const FVector2f& FieldSize, int32 InCapacity)
{
	const FYJJTuning& T = YJJData::Tune();
	Config.CellSize = T.GridCellSize;
	Config.RetargetTicks = FMath::Max(1, T.RetargetTicks);
	Config.MaxNeighbors = T.MaxNeighbors;
	Config.SeparationStiffness = T.SeparationStiffness;
	Config.MaxPushPerTick = T.MaxPushPerTick;
	Config.CrowdSlide = T.CrowdSlide;
	Config.Acceleration = T.UnitAcceleration;
	Config.ImpulseDamping = T.ImpulseDamping;
	Config.LateralDrift = T.LateralDrift;
	Config.FieldMargin = T.FieldMargin;
	Config.HitFlashTime = T.HitFlashTime;
	Config.AttackAnimTime = T.AttackAnimTime;
	Config.ArrowHitRadius = T.ArrowHitRadius;
	Config.DeathFling = T.DeathFling;
	Config.BigUnitRadius = T.BigUnitRadius;
	Config.MultishotSpread = T.MultishotSpread;
	Config.ReorderTicks = T.ReorderTicks;
	Config.AttackAnimShare = T.AttackAnimShare;
	Config.AttackAnimMax = T.AttackAnimMax;
	Config.WalkStrideScale = T.WalkStrideScale;
	Config.WalkStrideMin = T.WalkStrideMin;
	Config.WalkStrideMax = T.WalkStrideMax;
	Config.PlayerAdvanceX = FieldSize.X * T.PlayerAdvanceShare;
	Config.PlayerChaseMargin = T.PlayerChaseMargin;
	Config.WallX = T.CampEdgeX;
	Config.GateY = FieldSize.Y * HalfShare;
	Config.GateHalf = T.GateHalf;
	Config.GateExit = T.GateExit;
	Config.StrikeShare = T.AttackStrikeShare;
	Config.WallReach = T.WallReach;

	Field = FieldSize;
	Random.Initialize(RandomSeed);

	MaxNormalRadius = 0.f;
	for (const FYJJUnitDef& Def : YJJData::Units())
	{
		if (Def.Radius < Config.BigUnitRadius)
		{
			MaxNormalRadius = FMath::Max(MaxNormalRadius, Def.Radius);
		}
	}

	// 다시 시작할 때 이전 판 값이 남지 않게 비우고 0으로 채운다
	auto Resize = [InCapacity](auto& Array) { Array.Reset(); Array.AddZeroed(InCapacity); };
	Resize(Alive); Resize(Team); Resize(Type); Resize(Pos); Resize(Vel); Resize(HP); Resize(MaxHP);
	Resize(Flash); Resize(AttackAnim); Resize(AttackLen); Resize(WalkPhase); Resize(FaceLeft);
	Resize(Gen); Resize(Target); Resize(TargetGen); Resize(Cooldown); Resize(Damage); Resize(AttackInterval);
	Resize(Speed); Resize(Splash); Resize(Multishot); Resize(Impulse); Resize(PosNext); Resize(LastHitDir); Resize(Pending); Resize(Flags); Resize(Seed); Resize(Kills); Resize(Windup); Resize(Strike); Resize(StrikeGen);
	HeroSlot = INDEX_NONE;

	FreeSlots.Reset();
	BigUnits.Reset();
	Deaths.Reset();
	Projectiles.Reset();
	Rescued.Reset();
	Promoted.Reset();
	WallDamage = 0.f;
	Specials.Reset();
	HighWaterMark = 0;
	TeamCount[0] = TeamCount[1] = 0;
	Front[0] = Front[1] = -1.f;
	MoveBuff[0] = MoveBuff[1] = 1.f;
	AttackBuff[0] = AttackBuff[1] = 1.f;
	Steps = 0;
	for (FYJJGrid& Grid : Grids)
	{
		Grid.Init(FieldSize, Config.CellSize);
	}
}

int32 FYJJHorde::Spawn(int32 InType, EYJJTeam InTeam, const FVector2f& InPos, const FYJJSpawnStats& Stats)
{
	int32 Slot = INDEX_NONE;
	if (FreeSlots.Num() > 0)
	{
		Slot = FreeSlots.Pop(EAllowShrinking::No);
	}
	else if (HighWaterMark < Alive.Num())
	{
		Slot = HighWaterMark++;
	}
	if (Slot == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const FYJJUnitDef& Def = YJJData::Unit(InType);
	Alive[Slot] = 1;
	Team[Slot] = static_cast<uint8>(InTeam);
	Type[Slot] = static_cast<uint16>(InType);
	Pos[Slot] = FVector2f(InPos.X, FMath::Clamp(InPos.Y, Config.FieldMargin, Field.Y - Config.FieldMargin));
	PosNext[Slot] = Pos[Slot];
	Vel[Slot] = FVector2f::ZeroVector;
	Impulse[Slot] = FVector2f::ZeroVector;
	LastHitDir[Slot] = FVector2f::ZeroVector;
	MaxHP[Slot] = HP[Slot] = Def.HP * Stats.HP;
	Damage[Slot] = Def.Damage * Stats.Damage;
	AttackInterval[Slot] = Def.AttackInterval / FMath::Max(Stats.AttackSpeed, NearZero);
	Speed[Slot] = Def.Speed * Stats.MoveSpeed;
	Splash[Slot] = Def.Splash * Stats.Splash;
	Multishot[Slot] = static_cast<uint8>(FMath::Clamp(Stats.Multishot, 1, MAX_uint8));
	Cooldown[Slot] = Random.FRand() * Def.AttackInterval;
	Flash[Slot] = 0.f;
	AttackAnim[Slot] = 0.f;
	WalkPhase[Slot] = Random.FRand();
	Seed[Slot] = Random.FRand() * FullTurn;
	Flags[Slot] = 0;
	Kills[Slot] = 0;
	Windup[Slot] = 0.f;
	Strike[Slot] = INDEX_NONE;
	StrikeGen[Slot] = 0;
	FaceLeft[Slot] = InTeam == EYJJTeam::Enemy ? 1 : 0;
	Target[Slot] = INDEX_NONE;
	TargetGen[Slot] = 0;
	Pending[Slot] = INDEX_NONE;
	++TeamCount[TeamIndex(InTeam)];
	if (Def.Radius >= Config.BigUnitRadius)
	{
		BigUnits.Add(Slot);
	}
	return Slot;
}

bool FYJJHorde::IsValidTarget(int32 Slot, uint32 InGen) const
{
	return Slot >= 0 && Alive[Slot] && Gen[Slot] == InGen;
}

void FYJJHorde::BuildGrids()
{
	// 두 편 격자는 서로 독립이라 동시에 만든다
	ParallelFor(static_cast<int32>(EYJJTeam::Count), [this](int32 TeamId) { BuildGrid(TeamId); });
}

void FYJJHorde::BuildGrid(int32 TeamId)
{
	FYJJGrid& Grid = Grids[TeamId];
	FMemory::Memzero(Grid.Start.GetData(), Grid.Start.Num() * sizeof(int32));
	const uint8 MyTeam = static_cast<uint8>(TeamId);

	// 1) 칸별 수
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot] && Team[Slot] == MyTeam)
		{
			const FIntPoint C = Grid.CellOf(Pos[Slot]);
			++Grid.Start[Grid.Index(C.X, C.Y) + 1];
		}
	}
	// 2) 누적 → 시작 위치
	for (int32 Index = 1; Index < Grid.Start.Num(); ++Index)
	{
		Grid.Start[Index] += Grid.Start[Index - 1];
	}
	Grid.Items.SetNumUninitialized(Grid.Start.Last(), EAllowShrinking::No);
	// 3) 채우기 (Start를 커서로 쓰고 다시 한 칸 민다)
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot] && Team[Slot] == MyTeam)
		{
			const FIntPoint C = Grid.CellOf(Pos[Slot]);
			Grid.Items[Grid.Start[Grid.Index(C.X, C.Y)]++] = Slot;
		}
	}
	for (int32 Index = Grid.Start.Num() - 1; Index > 0; --Index)
	{
		Grid.Start[Index] = Grid.Start[Index - 1];
	}
	Grid.Start[0] = 0;
}

void FYJJHorde::Reorder()
{
	// 격자 칸 순서(위→아래, 왼→오른)로 살아 있는 유닛을 모은다. 격자는 이번 틱 시작에 만든 것
	Order.Reset();
	const int32 NumCells = Grids[0].W * Grids[0].H;
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		for (const FYJJGrid& Grid : Grids)
		{
			for (int32 Item = Grid.Start[Cell]; Item < Grid.Start[Cell + 1]; ++Item)
			{
				if (Alive[Grid.Items[Item]])
				{
					Order.Add(Grid.Items[Item]);
				}
			}
		}
	}
	const int32 Count = Order.Num();
	Remap.Reset();
	Remap.Init(INDEX_NONE, HighWaterMark);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Remap[Order[Index]] = Index;
	}

	// 모든 슬롯 배열을 새 순서로 (뒤쪽 빈 칸은 Alive만 0이면 된다)
	auto Permute = [this, Count](auto& Array)
	{
		const auto Old = Array;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Array[Index] = Old[Order[Index]];
		}
	};
	Permute(Alive); Permute(Team); Permute(Type); Permute(Pos); Permute(Vel); Permute(HP); Permute(MaxHP);
	Permute(Flash); Permute(AttackAnim); Permute(AttackLen); Permute(WalkPhase); Permute(FaceLeft); Permute(Gen); Permute(Target);
	Permute(TargetGen); Permute(Cooldown); Permute(Damage); Permute(AttackInterval); Permute(Speed); Permute(Splash);
	Permute(Multishot); Permute(Impulse); Permute(PosNext); Permute(LastHitDir); Permute(Flags); Permute(Seed); Permute(Kills); Permute(Windup); Permute(Strike); Permute(StrikeGen);
	for (int32 Slot = Count; Slot < HighWaterMark; ++Slot)
	{
		Alive[Slot] = 0;
		++Gen[Slot];
	}

	// 슬롯을 가리키던 것들을 새 번호로
	auto Moved = [this](int32 Old) { return Old >= 0 && Remap.IsValidIndex(Old) ? Remap[Old] : INDEX_NONE; };
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		Target[Slot] = Moved(Target[Slot]);
		Strike[Slot] = Strike[Slot] == WallTarget ? WallTarget : Moved(Strike[Slot]);
		Pending[Slot] = INDEX_NONE;
	}
	for (FYJJProjectile& Shot : Projectiles)
	{
		Shot.Target = Moved(Shot.Target);
	}
	for (int32& Big : BigUnits)
	{
		Big = Moved(Big);
	}
	BigUnits.RemoveAll([](int32 Slot) { return Slot == INDEX_NONE; });
	HeroSlot = Moved(HeroSlot);
	FreeSlots.Reset();
	HighWaterMark = Count;
}

void FYJJHorde::SetHero(int32 Slot, const FVector2f& Home, float Leash, float Arrive)
{
	HeroSlot = Slot;
	HeroHome = Home;
	HeroLeash = Leash;
	HeroArrive = Arrive;
}

void FYJJHorde::Heal(int32 Slot, float Amount)
{
	if (Alive.IsValidIndex(Slot) && Alive[Slot])
	{
		HP[Slot] = FMath::Min(HP[Slot] + Amount, MaxHP[Slot]);
	}
}

void FYJJHorde::SetMaxHP(int32 Slot, float NewMax)
{
	if (Alive.IsValidIndex(Slot) && Alive[Slot] && NewMax > 0.f)
	{
		// 늘어난 만큼 지금 체력도 채운다
		HP[Slot] = FMath::Min(HP[Slot] + FMath::Max(NewMax - MaxHP[Slot], 0.f), NewMax);
		MaxHP[Slot] = NewMax;
	}
}

void FYJJHorde::CopyTo(FYJJHordeView& View) const
{
	const int32 Count = HighWaterMark;
	View.HighWater = Count;
	auto Copy = [Count](auto& Dest, const auto& Source)
	{
		Dest.SetNumUninitialized(Count, EAllowShrinking::No);
		if (Count > 0)
		{
			FMemory::Memcpy(Dest.GetData(), Source.GetData(), Count * Source.GetTypeSize());
		}
	};
	Copy(View.Alive, Alive);
	Copy(View.Team, Team);
	Copy(View.FaceLeft, FaceLeft);
	Copy(View.Type, Type);
	Copy(View.Pos, Pos);
	Copy(View.Flash, Flash);
	Copy(View.AttackAnim, AttackAnim);
	Copy(View.WalkPhase, WalkPhase);
	Copy(View.AttackLen, AttackLen);
	View.Knock.SetNumUninitialized(Count, EAllowShrinking::No);
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		View.Knock[Slot] = Impulse[Slot].Size();
	}
	View.MoveSpeed.SetNumUninitialized(Count, EAllowShrinking::No);
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		View.MoveSpeed[Slot] = Vel[Slot].Size();
	}
	View.Projectiles = Projectiles;
	for (int32 TeamId = 0; TeamId < static_cast<int32>(EYJJTeam::Count); ++TeamId)
	{
		View.Count[TeamId] = TeamCount[TeamId];
		View.Front[TeamId] = Front[TeamId];
	}
	View.Bars.Reset();
	for (const int32 Big : BigUnits)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Type[Big]);
		if (Alive[Big] && (Def.bBoss || Def.bHero || Def.bCamp))
		{
			View.Bars.Add({ Pos[Big], Type[Big], HP[Big], MaxHP[Big] });
		}
	}
	View.Boss = INDEX_NONE;
	for (const int32 Big : BigUnits)
	{
		if (Alive[Big] && YJJData::Unit(Type[Big]).bBoss)
		{
			View.Boss = Big;
			View.BossHP = HP[Big];
			View.BossMaxHP = MaxHP[Big];
			break;
		}
	}
	const bool bHero = HeroSlot != INDEX_NONE && Alive[HeroSlot];
	View.HeroHP = bHero ? HP[HeroSlot] : 0.f;
	View.HeroMaxHP = bHero ? MaxHP[HeroSlot] : 0.f;
	View.StepMs = StepMs;
	FMemory::Memcpy(View.PhaseMs, PhaseMs, sizeof(PhaseMs));
	View.ReorderMs = ReorderMs;
}

int32 FYJJHorde::FindNearestEnemy(int32 Slot, float Sight) const
{
	const FYJJGrid& Grid = Grids[TeamIndex(Opponent(static_cast<EYJJTeam>(Team[Slot])))];
	if (Grid.Items.Num() == 0)
	{
		return INDEX_NONE;
	}
	const FVector2f P = Pos[Slot];
	const FIntPoint C = Grid.CellOf(P);
	const int32 MaxRing = FMath::CeilToInt(Sight * Grid.InvCell);
	float BestDistSq = Sight * Sight;
	int32 Best = INDEX_NONE;

	for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
	{
		// 이 고리의 가장 가까운 점도 지금 찾은 것보다 멀면 끝
		const float RingMin = FMath::Max(0, Ring - 1) * Grid.Cell;
		if (Best != INDEX_NONE && RingMin * RingMin > BestDistSq)
		{
			break;
		}
		const int32 Y0 = FMath::Max(C.Y - Ring, 0);
		const int32 Y1 = FMath::Min(C.Y + Ring, Grid.H - 1);
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			const bool bEdgeRow = Y == C.Y - Ring || Y == C.Y + Ring;
			const int32 Step = bEdgeRow ? 1 : FMath::Max(2 * Ring, 1);
			for (int32 X = C.X - Ring; X <= C.X + Ring; X += Step)
			{
				if (X < 0 || X >= Grid.W)
				{
					continue;
				}
				const int32 Cell = Grid.Index(X, Y);
				for (int32 Item = Grid.Start[Cell]; Item < Grid.Start[Cell + 1]; ++Item)
				{
					const int32 Other = Grid.Items[Item];
					const float DistSq = FVector2f::DistSquared(P, Pos[Other]);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						Best = Other;
					}
				}
			}
		}
	}
	return Best;
}

FVector2f FYJJHorde::Separation(int32 Slot) const
{
	const FYJJUnitDef& Def = YJJData::Unit(Type[Slot]);
	const FVector2f P = Pos[Slot];
	const float MyMass = Def.Mass;
	FVector2f Push = FVector2f::ZeroVector;
	int32 Checked = 0;

	auto PushFrom = [&](int32 Other)
	{
		if (Other == Slot)
		{
			return;
		}
		const FYJJUnitDef& OtherDef = YJJData::Unit(Type[Other]);
		const FVector2f Delta = P - Pos[Other];
		const float MinDist = Def.Radius + OtherDef.Radius;
		const float DistSq = Delta.SizeSquared();
		if (DistSq >= MinDist * MinDist)
		{
			return;
		}
		const float Dist = FMath::Sqrt(DistSq);
		// 겹친 만큼 밀어낸다. 무거운 쪽은 덜 밀린다
		const float Share = OtherDef.Mass / (MyMass + OtherDef.Mass);
		const FVector2f Dir = Dist > NearZero ? Delta / Dist : FVector2f(FMath::Cos(static_cast<float>(Slot)), FMath::Sin(static_cast<float>(Slot)));
		Push += Dir * ((MinDist - Dist) * Share);
	};

	const int32 Rings = FMath::CeilToInt((Def.Radius + MaxNormalRadius) / Config.CellSize);
	for (const FYJJGrid& Grid : Grids)
	{
		const FIntPoint C = Grid.CellOf(P);
		for (int32 Y = FMath::Max(C.Y - Rings, 0); Y <= FMath::Min(C.Y + Rings, Grid.H - 1) && Checked < Config.MaxNeighbors; ++Y)
		{
			for (int32 X = FMath::Max(C.X - Rings, 0); X <= FMath::Min(C.X + Rings, Grid.W - 1) && Checked < Config.MaxNeighbors; ++X)
			{
				const int32 Cell = Grid.Index(X, Y);
				for (int32 Item = Grid.Start[Cell]; Item < Grid.Start[Cell + 1] && Checked < Config.MaxNeighbors; ++Item)
				{
					PushFrom(Grid.Items[Item]);
					++Checked;
				}
			}
		}
	}
	// 보스처럼 큰 유닛은 격자 고리 밖에 있어도 따로 본다
	for (const int32 Big : BigUnits)
	{
		PushFrom(Big);
	}

	Push *= Config.SeparationStiffness;
	const float Size = Push.Size();
	return Size > Config.MaxPushPerTick ? Push * (Config.MaxPushPerTick / Size) : Push;
}

void FYJJHorde::ThinkAndMove(int32 Slot, float Dt)
{
	const FYJJUnitDef& Def = YJJData::Unit(Type[Slot]);
	const EYJJTeam MyTeam = static_cast<EYJJTeam>(Team[Slot]);
	const int32 TeamId = TeamIndex(MyTeam);
	const FVector2f P = Pos[Slot];

	// 대상: 방금 죽었거나 몇 틱마다(슬롯별로 엇갈려) 다시 찾는다.
	// 대상이 원래 없던 유닛(행군 중)은 엇갈린 주기에만 찾는다 → 매 틱 넓은 시야를 다 뒤지지 않게
	const bool bTargetDied = Target[Slot] != INDEX_NONE && !IsValidTarget(Target[Slot], TargetGen[Slot]);
	if (bTargetDied)
	{
		Target[Slot] = INDEX_NONE;
	}
	if (bTargetDied || (Steps + Slot) % Config.RetargetTicks == 0)
	{
		const bool bInsideWall = MyTeam == EYJJTeam::Player && Slot != HeroSlot && P.X < Config.WallX;
		int32 Found = bInsideWall ? INDEX_NONE : FindNearestEnemy(Slot, Def.Sight);
		// 영웅은 자리를 지킨다: 자리에서 너무 먼 적은 쫓지 않는다
		if (Slot == HeroSlot && Found != INDEX_NONE && FVector2f::DistSquared(Pos[Found], HeroHome) > HeroLeash * HeroLeash)
		{
			Found = INDEX_NONE;
		}
		Target[Slot] = Found;
		TargetGen[Slot] = Found != INDEX_NONE ? Gen[Found] : 0;
	}

	Cooldown[Slot] -= Dt * AttackBuff[TeamId];
	AttackAnim[Slot] = FMath::Max(AttackAnim[Slot] - Dt, 0.f);
	Flash[Slot] = FMath::Max(Flash[Slot] - Dt, 0.f);
	Pending[Slot] = INDEX_NONE;
	// 휘두르는 중: 동작이 내리치는 순간에 맞는다 (대상이 그새 쓰러졌으면 헛방)
	if (Strike[Slot] != INDEX_NONE)
	{
		Windup[Slot] -= Dt * AttackBuff[TeamId];
		if (Windup[Slot] <= 0.f)
		{
			if (Strike[Slot] == WallTarget || IsValidTarget(Strike[Slot], StrikeGen[Slot]))
			{
				Pending[Slot] = Strike[Slot];
			}
			Strike[Slot] = INDEX_NONE;
		}
	}

	const float MoveSpeed = Speed[Slot] * MoveBuff[TeamId];
	const float MarchDir = MyTeam == EYJJTeam::Player ? 1.f : -1.f;
	FVector2f Desired = FVector2f::ZeroVector;
	const int32 Victim = Target[Slot];
	// 아군 병사는 전선 끝에서 멈춘다: 적 진영 깊이 있는 적은 쫓지 않고 다가오기를 기다린다 (원거리는 닿으면 쏜다)
	const bool bHoldLine = MyTeam == EYJJTeam::Player && Slot != HeroSlot && P.X >= Config.PlayerAdvanceX;

	if (Slot == HeroSlot && ChargeLeft > 0.f)
	{
		// 돌격 중: 앞만 보고 내달린다 (때리기는 TickCharge)
		Desired = ChargeDir * ChargeSpeed;
		Vel[Slot] = Desired;
		FaceLeft[Slot] = ChargeDir.X < 0.f ? 1 : 0;
	}
	else if (Victim != INDEX_NONE)
	{
		const FVector2f Delta = Pos[Victim] - P;
		const float Dist = Delta.Size();
		const float Reach = Def.Range + Def.Radius + YJJData::Unit(Type[Victim]).Radius;
		if (Dist > Reach)
		{
			const bool bTooDeep = bHoldLine && Pos[Victim].X > Config.PlayerAdvanceX + Config.PlayerChaseMargin;
			Desired = bTooDeep ? FVector2f::ZeroVector : Delta * (MoveSpeed / FMath::Max(Dist, NearZero));
		}
		else if (Cooldown[Slot] <= 0.f && Strike[Slot] == INDEX_NONE)
		{
			BeginStrike(Slot, Victim);
		}
		FaceLeft[Slot] = Delta.X < 0.f ? 1 : 0;
	}
	else if (Slot == HeroSlot)
	{
		// 영웅: 싸울 적이 없으면 자리로 돌아간다
		const FVector2f Home = HeroHome - P;
		const float Dist = Home.Size();
		Desired = Dist > HeroArrive ? Home * (MoveSpeed / Dist) : FVector2f::ZeroVector;
	}
	else if (MyTeam == EYJJTeam::Enemy)
	{
		// 적: 싸울 상대가 없으면 성벽으로 간다. 닿으면 성벽을 때린다
		const float Drift = FMath::Sin(static_cast<float>(Steps) * Dt * DriftPhaseSpeed + Seed[Slot]) * Config.LateralDrift;
		const bool bAtWall = P.X <= Config.WallX + Def.Radius + Def.Range + Config.WallReach;
		Desired = FVector2f(bAtWall ? 0.f : -MoveSpeed, Drift * MoveSpeed);
		FaceLeft[Slot] = 1;
		if (bAtWall && Cooldown[Slot] <= 0.f && Strike[Slot] == INDEX_NONE)
		{
			BeginStrike(Slot, WallTarget);
		}
	}
	else if (MyTeam == EYJJTeam::Player && P.X < Config.WallX)
	{
		// 성벽 안 아군: 성문으로 모여 밖으로 나간다
		const FVector2f Gate(Config.WallX + Config.GateExit, FMath::Clamp(P.Y, Config.GateY - Config.GateHalf * GateFunnel, Config.GateY + Config.GateHalf * GateFunnel));
		const FVector2f ToGate = Gate - P;
		Desired = ToGate * (MoveSpeed / FMath::Max(ToGate.Size(), NearZero));
		FaceLeft[Slot] = ToGate.X < 0.f ? 1 : 0;
	}
	else
	{
		// 대상이 없으면 앞으로 행군 (전선 끝에서는 제자리). 슬롯마다 다른 박자로 위아래로 조금씩 흔들려 줄이 퍼진다
		const float Drift = FMath::Sin(static_cast<float>(Steps) * Dt * DriftPhaseSpeed + Seed[Slot]) * Config.LateralDrift;
		Desired = FVector2f(bHoldLine ? 0.f : MarchDir * MoveSpeed, Drift * MoveSpeed);
		FaceLeft[Slot] = MarchDir < 0.f ? 1 : 0;
	}

	// 앞이 막히면(밀려나는 쪽과 가려는 쪽이 맞서면) 파고드는 몫을 빼서 옆으로 비켜 간다 → 포개지지 않고 가로로 줄이 선다
	const FVector2f Sep = Separation(Slot);
	const float SepSize = Sep.Size();
	if (SepSize > NearZero && !(Slot == HeroSlot && ChargeLeft > 0.f))
	{
		const FVector2f Away = Sep / SepSize;
		const float Into = FVector2f::DotProduct(Desired, Away);
		if (Into < 0.f)
		{
			Desired -= Away * (Into * Config.CrowdSlide);
		}
	}
	const float Blend = FMath::Min(Config.Acceleration * Dt, 1.f);
	FVector2f NewVel = Vel[Slot] + (Desired - Vel[Slot]) * Blend;
	Vel[Slot] = NewVel;
	const FVector2f Kick = Impulse[Slot];
	Impulse[Slot] = Kick * FMath::Max(1.f - Config.ImpulseDamping * Dt, 0.f);
	// 걸음 한 바퀴 거리: 그림 크기에 비례하되 너무 길지 않게 (큰 보스가 멈춘 듯 보이지 않게)
	const float Stride = FMath::Clamp(Def.DrawSize * Config.WalkStrideScale, Config.WalkStrideMin, Config.WalkStrideMax);
	WalkPhase[Slot] += NewVel.Size() * Dt / FMath::Max(Stride, NearZero);

	FVector2f Next = P + (NewVel + Kick) * Dt + Sep;
	if (Def.bCamp)
	{
		// 진지는 밀려나지 않는다
		Next = P;
	}
	Next.X = FMath::Clamp(Next.X, Config.FieldMargin, Field.X - Config.FieldMargin);
	Next.Y = FMath::Clamp(Next.Y, Config.FieldMargin, Field.Y - Config.FieldMargin);
	// 길목 지형: 못 지나가는 유닛은 타원 가장자리로 밀려나 옆으로 미끄러진다
	if (!Def.bPassTerrain)
	{
		for (const FVector4f& Block : Blockers)
		{
			const float NX = (Next.X - Block.X) / FMath::Max(Block.Z + Def.Radius, NearZero);
			const float NY = (Next.Y - Block.Y) / FMath::Max(Block.W + Def.Radius, NearZero);
			const float Inside = NX * NX + NY * NY;
			if (Inside < 1.f && Inside > NearZero)
			{
				const float Out = FMath::InvSqrt(Inside);
				Next = FVector2f(Block.X + (Next.X - Block.X) * Out, Block.Y + (Next.Y - Block.Y) * Out);
			}
		}
	}
	// 성벽: 적은 넘지 못하고, 아군은 성문으로만 드나든다
	if (MyTeam == EYJJTeam::Enemy)
	{
		Next.X = FMath::Max(Next.X, Config.WallX + Def.Radius);
	}
	else if (FMath::Abs(Next.Y - Config.GateY) > Config.GateHalf - Def.Radius)
	{
		Next.X = P.X < Config.WallX ? FMath::Min(Next.X, Config.WallX - Def.Radius) : FMath::Max(Next.X, Config.WallX + Def.Radius);
	}
	PosNext[Slot] = Next;
}

void FYJJHorde::Hit(int32 Victim, float Amount, const FVector2f& From, int32 Attacker, bool bMelee)
{
	const bool bWasStanding = HP[Victim] > 0.f;
	HP[Victim] -= Amount;
	Flash[Victim] = Config.HitFlashTime;
	++HitCount;
	const FVector2f Dir = Pos[Victim] - From;
	LastHitDir[Victim] = Dir.GetSafeNormal();

	// 중간 별 특수 효과
	const FYJJUnitSpecial& Mine = SpecialOf(Victim);
	if (Mine.LastStandAt > 0.f && HP[Victim] > 0.f && HP[Victim] < MaxHP[Victim] * Mine.LastStandAt && !(Flags[Victim] & YJJUnitFlag::LastStandUsed))
	{
		Flags[Victim] |= YJJUnitFlag::LastStandUsed;
		HP[Victim] = FMath::Min(HP[Victim] + MaxHP[Victim] * Mine.LastStandHeal, MaxHP[Victim]);
		Rescued.Add(Pos[Victim]);
	}
	if (Attacker == INDEX_NONE || !Alive[Attacker])
	{
		return;
	}
	// 마지막 한 방이면 처치 수 (진급)
	if (bWasStanding && HP[Victim] <= 0.f && Team[Attacker] != Team[Victim])
	{
		Kills[Attacker] = static_cast<uint16>(FMath::Min<int32>(Kills[Attacker] + 1, MAX_uint16));
		const FYJJUnitDef& Def = YJJData::Unit(Type[Attacker]);
		if (Def.PromoteTo != INDEX_NONE && Kills[Attacker] >= Def.PromoteKills && HP[Attacker] > 0.f)
		{
			Promote(Attacker);
		}
	}
	const FYJJUnitSpecial& Theirs = SpecialOf(Attacker);
	if (Theirs.Lifesteal > 0.f)
	{
		HP[Attacker] = FMath::Min(HP[Attacker] + Amount * Theirs.Lifesteal, MaxHP[Attacker]);
	}
	if (bMelee && Mine.Thorns > 0.f)
	{
		HP[Attacker] -= Amount * Mine.Thorns;
		Flash[Attacker] = Config.HitFlashTime;
	}
}

void FYJJHorde::Promote(int32 Slot)
{
	const FYJJUnitDef& Old = YJJData::Unit(Type[Slot]);
	const int32 NewType = Old.PromoteTo;
	const FYJJUnitDef& New = YJJData::Unit(NewType);
	// 특성·병영 레벨로 오른 배율은 그대로 가져간다
	MaxHP[Slot] *= New.HP / FMath::Max(Old.HP, NearZero);
	HP[Slot] = MaxHP[Slot];
	Damage[Slot] *= New.Damage / FMath::Max(Old.Damage, NearZero);
	AttackInterval[Slot] *= New.AttackInterval / FMath::Max(Old.AttackInterval, NearZero);
	Speed[Slot] *= New.Speed / FMath::Max(Old.Speed, NearZero);
	Splash[Slot] = Old.Splash > 0.f ? Splash[Slot] * New.Splash / Old.Splash : New.Splash;
	Type[Slot] = static_cast<uint16>(NewType);
	Kills[Slot] = 0;
	Flags[Slot] = 0;
	if (New.Radius >= Config.BigUnitRadius && Old.Radius < Config.BigUnitRadius)
	{
		BigUnits.Add(Slot);
	}
	Promoted.Add({ Pos[Slot], NewType });
}

const FYJJUnitSpecial& FYJJHorde::SpecialOf(int32 Slot) const
{
	return Specials.IsValidIndex(Type[Slot]) ? Specials[Type[Slot]] : NoSpecial;
}

void FYJJHorde::BeginStrike(int32 Slot, int32 Victim)
{
	Cooldown[Slot] = AttackInterval[Slot];
	StartAttackAnim(Slot);
	Strike[Slot] = Victim;
	StrikeGen[Slot] = Victim >= 0 ? Gen[Victim] : 0;
	Windup[Slot] = AttackLen[Slot] * Config.StrikeShare;
}

void FYJJHorde::StartAttackAnim(int32 Slot)
{
	AttackLen[Slot] = FMath::Clamp(AttackInterval[Slot] * Config.AttackAnimShare, Config.AttackAnimTime, Config.AttackAnimMax);
	AttackAnim[Slot] = AttackLen[Slot];
}

float FYJJHorde::AttackDamage(int32 Slot) const
{
	// 분노: 체력이 낮으면 더 세게
	const FYJJUnitSpecial& Mine = SpecialOf(Slot);
	const bool bFury = Mine.FuryAt > 0.f && HP[Slot] < MaxHP[Slot] * Mine.FuryAt;
	return Damage[Slot] * (bFury ? 1.f + Mine.FuryBonus : 1.f);
}

void FYJJHorde::SetSpecials(const TArray<FYJJUnitSpecial>& InSpecials)
{
	Specials = InSpecials;
}

void FYJJHorde::HealTeam(EYJJTeam InTeam, float Fraction)
{
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot] && Team[Slot] == static_cast<uint8>(InTeam))
		{
			HP[Slot] = FMath::Min(HP[Slot] + MaxHP[Slot] * Fraction, MaxHP[Slot]);
		}
	}
}

int32 FYJJHorde::FindEnemyNear(const FVector2f& Center, float Radius, EYJJTeam Victims, int32 Skip) const
{
	const FYJJGrid& Grid = Grids[TeamIndex(Victims)];
	const FIntPoint Min = Grid.CellOf(Center - FVector2f(Radius));
	const FIntPoint Max = Grid.CellOf(Center + FVector2f(Radius));
	float BestDistSq = Radius * Radius;
	int32 Best = INDEX_NONE;
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			const int32 Cell = Grid.Index(X, Y);
			for (int32 Item = Grid.Start[Cell]; Item < Grid.Start[Cell + 1]; ++Item)
			{
				const int32 Other = Grid.Items[Item];
				const float DistSq = FVector2f::DistSquared(Center, Pos[Other]);
				if (Other != Skip && Alive[Other] && HP[Other] > 0.f && DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = Other;
				}
			}
		}
	}
	return Best;
}

void FYJJHorde::DamageArea(const FVector2f& Center, float Radius, float Amount, float Knockback, EYJJTeam Victims, int32 Attacker)
{
	const FYJJGrid& Grid = Grids[TeamIndex(Victims)];
	const FIntPoint Min = Grid.CellOf(Center - FVector2f(Radius));
	const FIntPoint Max = Grid.CellOf(Center + FVector2f(Radius));
	const float RadiusSq = Radius * Radius;
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			const int32 Cell = Grid.Index(X, Y);
			for (int32 Item = Grid.Start[Cell]; Item < Grid.Start[Cell + 1]; ++Item)
			{
				const int32 Other = Grid.Items[Item];
				if (!Alive[Other] || FVector2f::DistSquared(Center, Pos[Other]) > RadiusSq)
				{
					continue;
				}
				Hit(Other, Amount, Center, Attacker, Attacker != INDEX_NONE);
				if (Knockback > 0.f)
				{
					// 가벼울수록, 가운데일수록 멀리 날아간다
					const FVector2f Delta = Pos[Other] - Center;
					const float Dist = Delta.Size();
					const float Falloff = 1.f - Dist / FMath::Max(Radius, NearZero);
					const FVector2f Dir = Dist > NearZero ? Delta / Dist : FVector2f(1.f, 0.f);
					Impulse[Other] += Dir * (Knockback * Falloff / FMath::Max(YJJData::Unit(Type[Other]).Mass, NearZero));
				}
			}
		}
	}
}

void FYJJHorde::Shoot(int32 Slot, int32 Victim)
{
	const FYJJUnitDef& Def = YJJData::Unit(Type[Slot]);
	const EYJJTeam MyTeam = static_cast<EYJJTeam>(Team[Slot]);
	const int32 Shots = Multishot[Slot];
	int32 LastAimed = Victim;
	for (int32 Shot = 0; Shot < Shots; ++Shot)
	{
		// 첫 발은 대상, 나머지는 대상 근처의 다른 적
		int32 Aimed = Victim;
		if (Shot > 0)
		{
			Aimed = FindEnemyNear(Pos[Victim], Config.MultishotSpread, Opponent(MyTeam), LastAimed);
			Aimed = Aimed != INDEX_NONE ? Aimed : Victim;
			LastAimed = Aimed;
		}
		++ShotCount;
		FYJJProjectile& Arrow = Projectiles.AddDefaulted_GetRef();
		Arrow.Start = Pos[Slot];
		Arrow.Pos = Pos[Slot];
		Arrow.Aim = Pos[Aimed];
		Arrow.Target = Aimed;
		Arrow.TargetGen = Gen[Aimed];
		Arrow.Damage = AttackDamage(Slot);
		Arrow.Splash = FMath::Max(Splash[Slot], SpecialOf(Slot).ArrowSplash);
		Arrow.Knockback = Def.Knockback * SpecialOf(Slot).KnockbackScale;
		Arrow.Speed = Def.ProjectileSpeed;
		Arrow.Team = MyTeam;
		Arrow.Frame = static_cast<uint8>(Def.ProjectileFrame);
	}
}

void FYJJHorde::ApplyAttacks()
{
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		const int32 Victim = Pending[Slot];
		if (!Alive[Slot] || Victim == INDEX_NONE)
		{
			continue;
		}
		if (Victim == WallTarget)
		{
			WallDamage += AttackDamage(Slot);
			++HitCount;
			continue;
		}
		const FYJJUnitDef& Def = YJJData::Unit(Type[Slot]);
		if (Def.ProjectileSpeed > 0.f)
		{
			Shoot(Slot, Victim);
		}
		else if (Splash[Slot] > 0.f)
		{
			DamageArea(Pos[Victim], Splash[Slot], AttackDamage(Slot), Def.Knockback * SpecialOf(Slot).KnockbackScale, Opponent(static_cast<EYJJTeam>(Team[Slot])), Slot);
			if (Def.bBoss || Def.bHero)
			{
				Slashes.Add({ Pos[Victim], Splash[Slot], Pos[Victim].X >= Pos[Slot].X ? 1.f : -1.f });
			}
		}
		else
		{
			Hit(Victim, AttackDamage(Slot), Pos[Slot], Slot, true);
		}
		if (Def.ProjectileSpeed <= 0.f && HitSparks.Num() < MaxSparks)
		{
			// 난전 불꽃: 맞은 쪽으로 조금 치우친 자리
			HitSparks.Add(FMath::Lerp(Pos[Slot], Pos[Victim], SparkLean));
		}
	}
}

void FYJJHorde::StartCharge(const FVector2f& Toward, float Duration, float InSpeed, float DamageScale, float Radius, float Knockback, float HitInterval)
{
	if (HeroSlot == INDEX_NONE || !Alive[HeroSlot])
	{
		return;
	}
	const FVector2f Delta = Toward - Pos[HeroSlot];
	ChargeDir = Delta.SizeSquared() > NearZero ? Delta.GetSafeNormal() : FVector2f(1.f, 0.f);
	ChargeLeft = Duration;
	ChargeTick = 0.f;
	ChargeSpeed = InSpeed;
	ChargeDamageScale = DamageScale;
	ChargeRadius = Radius;
	ChargeKnockback = Knockback;
	ChargeHitInterval = HitInterval;
}

void FYJJHorde::TickCharge(float Dt)
{
	if (ChargeLeft <= 0.f)
	{
		return;
	}
	ChargeLeft -= Dt;
	if (HeroSlot == INDEX_NONE || !Alive[HeroSlot])
	{
		ChargeLeft = 0.f;
		return;
	}
	ChargeTick -= Dt;
	if (ChargeTick > 0.f)
	{
		return;
	}
	ChargeTick = ChargeHitInterval;
	// 영웅 바로 앞을 쓸어 버린다: 맞은 적은 돌격 방향으로 날아간다
	const FVector2f Ahead = Pos[HeroSlot] + ChargeDir * ChargeRadius * HalfShare;
	DamageArea(Ahead, ChargeRadius, AttackDamage(HeroSlot) * ChargeDamageScale, ChargeKnockback, EYJJTeam::Enemy, HeroSlot);
	Slashes.Add({ Ahead, ChargeRadius, ChargeDir.X >= 0.f ? 1.f : -1.f });
}

void FYJJHorde::UpdateProjectiles(float Dt)
{
	for (int32 Index = Projectiles.Num() - 1; Index >= 0; --Index)
	{
		FYJJProjectile& Shot = Projectiles[Index];
		if (IsValidTarget(Shot.Target, Shot.TargetGen))
		{
			Shot.Aim = Pos[Shot.Target];
		}
		const FVector2f Delta = Shot.Aim - Shot.Pos;
		const float Dist = Delta.Size();
		const float Move = Shot.Speed * Dt;
		if (Dist > Move)
		{
			Shot.Pos += Delta * (Move / Dist);
			continue;
		}

		const EYJJTeam Victims = Opponent(Shot.Team);
		if (Shot.Splash > 0.f)
		{
			DamageArea(Shot.Aim, Shot.Splash, Shot.Damage, Shot.Knockback, Victims);
		}
		else
		{
			// 대상이 먼저 죽었으면 떨어진 자리 근처의 다른 적이 맞는다
			const int32 Victim = IsValidTarget(Shot.Target, Shot.TargetGen) ? Shot.Target : FindEnemyNear(Shot.Aim, Config.ArrowHitRadius, Victims, INDEX_NONE);
			if (Victim != INDEX_NONE)
			{
				Hit(Victim, Shot.Damage, Shot.Start);
			}
		}
		Projectiles.RemoveAtSwap(Index, EAllowShrinking::No);
	}
}

void FYJJHorde::CollectDead()
{
	Front[0] = Front[1] = -1.f;
	TroopFront = -1.f;
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (!Alive[Slot])
		{
			continue;
		}
		const EYJJTeam MyTeam = static_cast<EYJJTeam>(Team[Slot]);
		if (HP[Slot] > 0.f)
		{
			float& F = Front[TeamIndex(MyTeam)];
			F = F < 0.f ? Pos[Slot].X : (MyTeam == EYJJTeam::Player ? FMath::Max(F, Pos[Slot].X) : FMath::Min(F, Pos[Slot].X));
			if (MyTeam == EYJJTeam::Player && Slot != HeroSlot && Pos[Slot].X > Config.WallX)
			{
				TroopFront = FMath::Max(TroopFront, Pos[Slot].X);
			}
			continue;
		}
		// 부활: 처음 쓰러질 때 한 번 다시 일어난다
		const FYJJUnitSpecial& Mine = SpecialOf(Slot);
		if (Mine.ReviveHP > 0.f && !(Flags[Slot] & YJJUnitFlag::Revived))
		{
			Flags[Slot] |= YJJUnitFlag::Revived;
			HP[Slot] = MaxHP[Slot] * Mine.ReviveHP;
			Rescued.Add(Pos[Slot]);
			continue;
		}
		FYJJDeath& Death = Deaths.AddDefaulted_GetRef();
		Death.Pos = Pos[Slot];
		Death.Fling = LastHitDir[Slot] * Config.DeathFling + Impulse[Slot];
		Death.Type = Type[Slot];
		Death.Team = MyTeam;
		Death.bFaceLeft = FaceLeft[Slot] != 0;

		Alive[Slot] = 0;
		++Gen[Slot];
		--TeamCount[TeamIndex(MyTeam)];
		FreeSlots.Add(Slot);
		BigUnits.RemoveSwap(Slot, EAllowShrinking::No);
		if (Slot == HeroSlot)
		{
			HeroSlot = INDEX_NONE;
		}
	}
}

void FYJJHorde::Step(float Dt)
{
	const double StartTime = FPlatformTime::Seconds();
	double Lap = StartTime;
	auto EndPhase = [this, &Lap](EYJJHordePhase Phase)
	{
		const double Now = FPlatformTime::Seconds();
		PhaseMs[static_cast<int32>(Phase)] = static_cast<float>((Now - Lap) * MsPerSecond);
		Lap = Now;
	};

	BuildGrids();
	EndPhase(EYJJHordePhase::Grid);

	const int32 NumChunks = FMath::DivideAndRoundUp(HighWaterMark, ParallelChunk);
	ParallelFor(NumChunks, [this, Dt](int32 Chunk)
	{
		const int32 Begin = Chunk * ParallelChunk;
		const int32 End = FMath::Min(Begin + ParallelChunk, HighWaterMark);
		for (int32 Slot = Begin; Slot < End; ++Slot)
		{
			if (Alive[Slot])
			{
				ThinkAndMove(Slot, Dt);
			}
		}
	});
	EndPhase(EYJJHordePhase::Think);

	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot])
		{
			Pos[Slot] = PosNext[Slot];
		}
	}

	ApplyAttacks();
	TickCharge(Dt);
	UpdateProjectiles(Dt);
	EndPhase(EYJJHordePhase::Combat);
	CollectDead();
	EndPhase(EYJJHordePhase::Cleanup);
	if (Config.ReorderTicks > 0 && Steps % Config.ReorderTicks == 0)
	{
		const double ReorderStart = FPlatformTime::Seconds();
		Reorder();
		ReorderMs = static_cast<float>((FPlatformTime::Seconds() - ReorderStart) * MsPerSecond);
	}
	++Steps;

	StepMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * MsPerSecond);
}
