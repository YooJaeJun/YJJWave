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

void FYJJHorde::Reset(const FVector2f& FieldSize, float InWallX, int32 InCapacity)
{
	const FYJJTuning& T = YJJData::Tune();
	Config.CellSize = T.GridCellSize;
	Config.RetargetTicks = FMath::Max(1, T.RetargetTicks);
	Config.MaxNeighbors = T.MaxNeighbors;
	Config.SeparationStiffness = T.SeparationStiffness;
	Config.MaxPushPerTick = T.MaxPushPerTick;
	Config.Acceleration = T.UnitAcceleration;
	Config.ImpulseDamping = T.ImpulseDamping;
	Config.LateralDrift = T.LateralDrift;
	Config.FieldMargin = T.FieldMargin;
	Config.HitFlashTime = T.HitFlashTime;
	Config.AttackAnimTime = T.AttackAnimTime;
	Config.ArrowHitRadius = T.ArrowHitRadius;
	Config.DeathFling = T.DeathFling;
	Config.WallAttackReach = T.WallAttackReach;
	Config.BigUnitRadius = T.BigUnitRadius;
	Config.MultishotSpread = T.MultishotSpread;

	Field = FieldSize;
	WallX = InWallX;
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
	Resize(Flash); Resize(AttackAnim); Resize(WalkPhase); Resize(FaceLeft);
	Resize(Gen); Resize(Target); Resize(TargetGen); Resize(Cooldown); Resize(Damage); Resize(AttackInterval);
	Resize(Speed); Resize(Splash); Resize(Multishot); Resize(Impulse); Resize(PosNext); Resize(LastHitDir); Resize(Pending);

	FreeSlots.Reset();
	BigUnits.Reset();
	Deaths.Reset();
	Projectiles.Reset();
	HighWaterMark = 0;
	TeamCount[0] = TeamCount[1] = 0;
	Front[0] = Front[1] = -1.f;
	WallDamage = 0.f;
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
	for (int32 TeamId = 0; TeamId < static_cast<int32>(EYJJTeam::Count); ++TeamId)
	{
		FYJJGrid& Grid = Grids[TeamId];
		FMemory::Memzero(Grid.Start.GetData(), Grid.Start.Num() * sizeof(int32));
	}

	// 1) 칸별 수
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot])
		{
			FYJJGrid& Grid = Grids[Team[Slot]];
			const FIntPoint C = Grid.CellOf(Pos[Slot]);
			++Grid.Start[Grid.Index(C.X, C.Y) + 1];
		}
	}
	// 2) 누적 → 시작 위치
	for (FYJJGrid& Grid : Grids)
	{
		for (int32 Index = 1; Index < Grid.Start.Num(); ++Index)
		{
			Grid.Start[Index] += Grid.Start[Index - 1];
		}
		Grid.Items.SetNumUninitialized(Grid.Start.Last(), EAllowShrinking::No);
	}
	// 3) 채우기 (Start를 커서로 쓰고 다시 한 칸 민다)
	for (int32 Slot = 0; Slot < HighWaterMark; ++Slot)
	{
		if (Alive[Slot])
		{
			FYJJGrid& Grid = Grids[Team[Slot]];
			const FIntPoint C = Grid.CellOf(Pos[Slot]);
			Grid.Items[Grid.Start[Grid.Index(C.X, C.Y)]++] = Slot;
		}
	}
	for (FYJJGrid& Grid : Grids)
	{
		for (int32 Index = Grid.Start.Num() - 1; Index > 0; --Index)
		{
			Grid.Start[Index] = Grid.Start[Index - 1];
		}
		Grid.Start[0] = 0;
	}
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
		const int32 Found = FindNearestEnemy(Slot, Def.Sight);
		Target[Slot] = Found;
		TargetGen[Slot] = Found != INDEX_NONE ? Gen[Found] : 0;
	}

	Cooldown[Slot] -= Dt * AttackBuff[TeamId];
	AttackAnim[Slot] = FMath::Max(AttackAnim[Slot] - Dt, 0.f);
	Flash[Slot] = FMath::Max(Flash[Slot] - Dt, 0.f);
	Pending[Slot] = INDEX_NONE;

	const float MoveSpeed = Speed[Slot] * MoveBuff[TeamId];
	const float MarchDir = MyTeam == EYJJTeam::Player ? 1.f : -1.f;
	FVector2f Desired = FVector2f::ZeroVector;
	const int32 Victim = Target[Slot];

	if (Victim != INDEX_NONE)
	{
		const FVector2f Delta = Pos[Victim] - P;
		const float Dist = Delta.Size();
		const float Reach = Def.Range + Def.Radius + YJJData::Unit(Type[Victim]).Radius;
		if (Dist > Reach)
		{
			Desired = Delta * (MoveSpeed / FMath::Max(Dist, NearZero));
		}
		else if (Cooldown[Slot] <= 0.f)
		{
			Pending[Slot] = Victim;
			Cooldown[Slot] = AttackInterval[Slot];
			AttackAnim[Slot] = Config.AttackAnimTime;
		}
		FaceLeft[Slot] = Delta.X < 0.f ? 1 : 0;
	}
	else if (MyTeam == EYJJTeam::Enemy && P.X - Def.Radius <= WallX + Config.WallAttackReach)
	{
		// 성벽에 붙은 적은 성벽을 때린다
		if (Cooldown[Slot] <= 0.f)
		{
			Pending[Slot] = WallTarget;
			Cooldown[Slot] = AttackInterval[Slot];
			AttackAnim[Slot] = Config.AttackAnimTime;
		}
		FaceLeft[Slot] = 1;
	}
	else
	{
		// 대상이 없으면 앞으로 행군. 슬롯마다 다른 박자로 위아래로 조금씩 흔들려 줄이 퍼진다
		const float Drift = FMath::Sin(static_cast<float>(Steps) * Dt * DriftPhaseSpeed + Slot * DriftSlotPhase) * Config.LateralDrift;
		Desired = FVector2f(MarchDir * MoveSpeed, Drift * MoveSpeed);
		FaceLeft[Slot] = MarchDir < 0.f ? 1 : 0;
	}

	const float Blend = FMath::Min(Config.Acceleration * Dt, 1.f);
	FVector2f NewVel = Vel[Slot] + (Desired - Vel[Slot]) * Blend;
	Vel[Slot] = NewVel;
	const FVector2f Kick = Impulse[Slot];
	Impulse[Slot] = Kick * FMath::Max(1.f - Config.ImpulseDamping * Dt, 0.f);
	WalkPhase[Slot] += NewVel.Size() * Dt / FMath::Max(Def.DrawSize, NearZero);

	FVector2f Next = P + (NewVel + Kick) * Dt + Separation(Slot);
	const float MinX = MyTeam == EYJJTeam::Enemy ? WallX + Def.Radius : Config.FieldMargin;
	Next.X = FMath::Clamp(Next.X, MinX, Field.X - Config.FieldMargin);
	Next.Y = FMath::Clamp(Next.Y, Config.FieldMargin, Field.Y - Config.FieldMargin);
	PosNext[Slot] = Next;
}

void FYJJHorde::Hit(int32 Victim, float Amount, const FVector2f& From)
{
	HP[Victim] -= Amount;
	Flash[Victim] = Config.HitFlashTime;
	const FVector2f Dir = Pos[Victim] - From;
	LastHitDir[Victim] = Dir.GetSafeNormal();
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

void FYJJHorde::DamageArea(const FVector2f& Center, float Radius, float Amount, float Knockback, EYJJTeam Victims)
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
				Hit(Other, Amount, Center);
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
		FYJJProjectile& Arrow = Projectiles.AddDefaulted_GetRef();
		Arrow.Start = Pos[Slot];
		Arrow.Pos = Pos[Slot];
		Arrow.Aim = Pos[Aimed];
		Arrow.Target = Aimed;
		Arrow.TargetGen = Gen[Aimed];
		Arrow.Damage = Damage[Slot];
		Arrow.Splash = Splash[Slot];
		Arrow.Knockback = Def.Knockback;
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
			WallDamage += Damage[Slot];
			continue;
		}
		const FYJJUnitDef& Def = YJJData::Unit(Type[Slot]);
		if (Def.ProjectileSpeed > 0.f)
		{
			Shoot(Slot, Victim);
		}
		else if (Splash[Slot] > 0.f)
		{
			DamageArea(Pos[Victim], Splash[Slot], Damage[Slot], Def.Knockback, Opponent(static_cast<EYJJTeam>(Team[Slot])));
		}
		else
		{
			Hit(Victim, Damage[Slot], Pos[Slot]);
		}
	}
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
	UpdateProjectiles(Dt);
	EndPhase(EYJJHordePhase::Combat);
	CollectDead();
	EndPhase(EYJJHordePhase::Cleanup);
	++Steps;

	StepMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * MsPerSecond);
}
