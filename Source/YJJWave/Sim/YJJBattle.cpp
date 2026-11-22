#include "Sim/YJJBattle.h"
#include "Data/YJJGameData.h"
#include "YJJWave.h"

int32 FYJJBattle::ModIndex(EYJJStat Stat, int32 Column) const
{
	return static_cast<int32>(Stat) * (YJJData::Buildings().Num() + 1) + Column;
}

void FYJJBattle::Reset(int32 InStage, const FYJJProgress& Progress)
{
	const FYJJTuning& T = YJJData::Tune();
	Random.Initialize(RandomSeed);
	Stage = FMath::Clamp(InStage, 0, FMath::Max(YJJData::Stages().Num() - 1, 0));
	FieldWidth = T.FieldWidth;
	FieldHeight = T.FieldHeight;
	CampEdgeX = T.CampEdgeX;
	Horde.Reset(FVector2f(FieldWidth, FieldHeight), T.UnitCapacity);

	// 진지 칸: 진지 끝 왼쪽에 Columns x Rows, 세로 가운데 정렬
	Plots.Reset();
	const float GridHeight = (T.PlotRows - 1) * T.PlotSpacingY;
	for (int32 Row = 0; Row < T.PlotRows; ++Row)
	{
		for (int32 Column = 0; Column < T.PlotColumns; ++Column)
		{
			FYJJPlot& Plot = Plots.AddDefaulted_GetRef();
			Plot.Pos = FVector2f(T.PlotStartX + Column * T.PlotSpacingX, FieldHeight * Half - GridHeight * Half + Row * T.PlotSpacingY);
		}
	}

	Effects.Reset();
	RecentDeaths.Reset();
	RecentCraters.Reset();
	Gold = T.StartGold;
	Souls = Progress.Souls;
	SoulsEarned = 0.0;
	Kills = 0;
	Spawned[0] = Spawned[1] = 0;
	Time = 0.f;
	bGameOver = false;
	bVictory = false;
	RallyTime = 0.f;
	for (float& Cooldown : Cooldowns)
	{
		Cooldown = 0.f;
	}

	// 특성: 시작 별 + 저장된 별 (비용 없이 다시 적용)
	Mods.Reset();
	Mods.AddZeroed(static_cast<int32>(EYJJStat::Count) * (YJJData::Buildings().Num() + 1));
	PerkAllocated.Reset();
	PerkAllocated.AddZeroed(YJJData::Perks().Num());
	PerksTaken = 0;
	for (int32 Perk = 0; Perk < YJJData::Perks().Num(); ++Perk)
	{
		const FYJJPerkDef& Def = YJJData::Perks()[Perk];
		if (Def.Kind == EYJJPerkKind::Start)
		{
			PerkAllocated[Perk] = 1;
		}
		else if (Progress.Perks.Contains(Def.Id))
		{
			ApplyPerk(Perk);
		}
	}

	EnemyCarry.Reset();
	EnemyCarry.AddZeroed(YJJData::Units().Num());
	SpawnHero();
	StartLevel(1);
}

// ---------------------------------------------------------------------------------------------
// 틱

void FYJJBattle::PreStep(float Dt)
{
	if (bGameOver)
	{
		return;
	}
	const FYJJTuning& T = YJJData::Tune();
	Time += Dt;
	Gold += T.PassiveGold * Dt * (1.f + Bonus(EYJJStat::Gold, INDEX_NONE));

	LevelTime += Dt;
	if (LevelTime >= LevelDuration() && Level < NumWaves())
	{
		StartLevel(Level + 1);
	}

	for (float& Cooldown : Cooldowns)
	{
		Cooldown = FMath::Max(Cooldown - Dt, 0.f);
	}
	RallyTime = FMath::Max(RallyTime - Dt, 0.f);
	const int32 PlayerTeam = static_cast<int32>(EYJJTeam::Player);
	Horde.MoveBuff[PlayerTeam] = RallyTime > 0.f ? T.RallyMoveSpeed : 1.f;
	Horde.AttackBuff[PlayerTeam] = RallyTime > 0.f ? T.RallyAttackSpeed : 1.f;

	if (Horde.Hero() != INDEX_NONE)
	{
		Horde.Heal(Horde.Hero(), (T.HeroRegen + HeroRegenBonus) * Dt);
	}
	TickPlots(Dt);
	TickEnemies(Dt);
}

void FYJJBattle::Tick(float Dt)
{
	PreStep(Dt);
	Horde.Step(Dt);
	PostStep(Dt);
}

void FYJJBattle::PostStep(float Dt)
{
	if (bGameOver)
	{
		return;
	}
	TickEffects(Dt);
	CollectRewards();
	for (const FVector2f& Where : Horde.Rescued)
	{
		AddEffect(EYJJEffectKind::Rescue, Where, YJJData::Tune().RescueEffectTime, YJJData::Tune().RescueEffectSize);
	}
	Horde.Rescued.Reset();
	CheckVictory();
}

void FYJJBattle::CheckVictory()
{
	// 마지막 웨이브가 끝났고 남은 적이 없으면 돌파
	if (!bGameOver && Level >= NumWaves() && LevelTime >= LevelDuration() && Horde.Num(EYJJTeam::Enemy) == 0)
	{
		Finish(true);
	}
}

void FYJJBattle::Finish(bool bWon)
{
	bGameOver = true;
	bVictory = bWon;
	UE_LOG(LogYJJWave, Display, TEXT("[Battle] 스테이지 %d %s: 웨이브 %d/%d, 처치 %d, 시간 %.0f초, 영혼 +%.0f"),
		Stage + 1, bWon ? TEXT("돌파") : TEXT("패배 (영웅 쓰러짐)"), Level, NumWaves(), Kills, Time, SoulsEarned);
}

void FYJJBattle::SpawnHero()
{
	const FYJJTuning& T = YJJData::Tune();
	const int32 HeroUnit = YJJData::HeroUnit();
	if (HeroUnit == INDEX_NONE)
	{
		return;
	}
	FYJJSpawnStats Stats;
	Stats.HP = 1.f + Bonus(EYJJStat::HeroHP, INDEX_NONE);
	const FVector2f Home(T.HeroHomeX, FieldHeight * Half);
	const int32 Slot = Horde.Spawn(HeroUnit, EYJJTeam::Player, Home, Stats);
	Horde.SetHero(Slot, Home, T.HeroLeash, T.HeroArrive);
}

void FYJJBattle::RecomputeHero()
{
	const int32 HeroUnit = YJJData::HeroUnit();
	if (HeroUnit != INDEX_NONE)
	{
		Horde.SetMaxHP(Horde.Hero(), YJJData::Unit(HeroUnit).HP * (1.f + Bonus(EYJJStat::HeroHP, INDEX_NONE)));
	}
}

void FYJJBattle::StartLevel(int32 NewLevel)
{
	Level = NewLevel;
	LevelTime = 0.f;
	if (NumWaves() == 0)
	{
		return;
	}
	for (const FYJJWaveSpawn& Spawn : StageDef().Waves[Level - 1].Spawns)
	{
		for (int32 Index = 0; Index < Spawn.Burst; ++Index)
		{
			SpawnEnemy(Spawn.Unit);
		}
	}
	UE_LOG(LogYJJWave, Display, TEXT("[Battle] 스테이지 %d 웨이브 %d/%d 시작 (아군 %d, 적 %d, 금화 %.0f)"), Stage + 1, Level, NumWaves(), Horde.Num(EYJJTeam::Player), Horde.Num(EYJJTeam::Enemy), Gold);
}

const FYJJStageDef& FYJJBattle::StageDef() const
{
	return YJJData::Stages()[Stage];
}

int32 FYJJBattle::NumWaves() const
{
	return YJJData::Stages().IsValidIndex(Stage) ? StageDef().Waves.Num() : 0;
}

float FYJJBattle::LevelDuration() const
{
	return NumWaves() > 0 ? StageDef().Waves[FMath::Clamp(Level, 1, NumWaves()) - 1].Duration : 0.f;
}

void FYJJBattle::SpawnEnemy(int32 Unit)
{
	const FYJJTuning& T = YJJData::Tune();
	if (Horde.Num(EYJJTeam::Enemy) >= T.MaxUnitsPerTeam)
	{
		return;
	}
	FYJJSpawnStats Stats;
	Stats.HP = FMath::Pow(T.LevelHPGrowth, static_cast<float>(Level - 1)) * StageDef().HPScale;
	Stats.Damage = FMath::Pow(T.LevelDamageGrowth, static_cast<float>(Level - 1));
	// 전장 가운데 띠에서 나온다 (위아래로 다 퍼지면 아군 시야 밖으로 빠져나간다)
	const float Band = FieldHeight * T.EnemySpawnBand * 0.5f;
	const FVector2f Pos(FieldWidth - T.EnemySpawnInset - Random.FRand() * T.SpawnScatter, FieldHeight * 0.5f + Random.FRandRange(-Band, Band));
	const int32 Slot = Horde.Spawn(Unit, EYJJTeam::Enemy, Pos, Stats);
	if (Slot == INDEX_NONE)
	{
		return;
	}
	++Spawned[static_cast<int32>(EYJJTeam::Enemy)];
	if (YJJData::Unit(Unit).bBoss)
	{
		AddEffect(EYJJEffectKind::Build, Pos, T.BuildEffectTime, YJJData::Unit(Unit).DrawSize * BossEffectRadiusScale);
		UE_LOG(LogYJJWave, Display, TEXT("[Battle] 보스 등장: %s (웨이브 %d)"), *YJJData::Unit(Unit).Id.ToString(), Level);
	}
}

void FYJJBattle::TickEnemies(float Dt)
{
	// 마지막 웨이브는 시간이 다 되면 더 나오지 않는다 (남은 적을 다 잡으면 돌파)
	if (NumWaves() == 0 || LevelTime >= LevelDuration())
	{
		return;
	}
	for (const FYJJWaveSpawn& Spawn : StageDef().Waves[Level - 1].Spawns)
	{
		float& Carry = EnemyCarry[Spawn.Unit];
		Carry += Spawn.Rate * Dt;
		while (Carry >= 1.f)
		{
			Carry -= 1.f;
			SpawnEnemy(Spawn.Unit);
		}
	}
}

FYJJSpawnStats FYJJBattle::PlayerStats(int32 Building, int32 BuildingLevel) const
{
	const FYJJBuildingDef& Def = YJJData::Building(Building);
	const float LevelScale = 1.f + Def.StatPerLevel * (BuildingLevel - 1);
	FYJJSpawnStats Stats;
	Stats.HP = LevelScale * (1.f + Bonus(EYJJStat::HP, Building));
	Stats.Damage = LevelScale * (1.f + Bonus(EYJJStat::Damage, Building));
	Stats.AttackSpeed = 1.f + Bonus(EYJJStat::AttackSpeed, Building);
	Stats.MoveSpeed = 1.f + Bonus(EYJJStat::MoveSpeed, Building);
	Stats.Splash = 1.f + Bonus(EYJJStat::Splash, Building);
	Stats.Multishot = 1 + FMath::RoundToInt(Bonus(EYJJStat::Multishot, Building));
	return Stats;
}

int32 FYJJBattle::SpawnCount(int32 PlotIndex) const
{
	const FYJJPlot& Plot = Plots[PlotIndex];
	if (Plot.Building == INDEX_NONE)
	{
		return 0;
	}
	const FYJJBuildingDef& Def = YJJData::Building(Plot.Building);
	return Def.SpawnCount + FMath::FloorToInt(Def.CountPerLevel * (Plot.Level - 1)) + FMath::RoundToInt(Bonus(EYJJStat::SpawnCount, Plot.Building));
}

float FYJJBattle::SpawnInterval(int32 PlotIndex) const
{
	const FYJJPlot& Plot = Plots[PlotIndex];
	if (Plot.Building == INDEX_NONE)
	{
		return 0.f;
	}
	const FYJJBuildingDef& Def = YJJData::Building(Plot.Building);
	return Def.SpawnInterval * FMath::Pow(Def.IntervalScalePerLevel, static_cast<float>(Plot.Level - 1)) / (1.f + Bonus(EYJJStat::SpawnRate, Plot.Building));
}

void FYJJBattle::TickPlots(float Dt)
{
	const FYJJTuning& T = YJJData::Tune();
	for (int32 PlotIndex = 0; PlotIndex < Plots.Num(); ++PlotIndex)
	{
		FYJJPlot& Plot = Plots[PlotIndex];
		Plot.Pulse = FMath::Max(Plot.Pulse - Dt, 0.f);
		if (Plot.Building == INDEX_NONE)
		{
			continue;
		}
		Plot.SpawnTimer -= Dt;
		if (Plot.SpawnTimer > 0.f)
		{
			continue;
		}
		Plot.SpawnTimer += SpawnInterval(PlotIndex);
		Plot.Pulse = T.PlotPulseTime;

		const FYJJBuildingDef& Def = YJJData::Building(Plot.Building);
		const FYJJSpawnStats Stats = PlayerStats(Plot.Building, Plot.Level);
		const int32 Count = SpawnCount(PlotIndex);
		for (int32 Index = 0; Index < Count && Horde.Num(EYJJTeam::Player) < T.MaxUnitsPerTeam; ++Index)
		{
			const FVector2f Offset(Random.FRandRange(0.f, T.SpawnScatter), Random.FRandRange(-T.SpawnScatter, T.SpawnScatter));
			if (Horde.Spawn(Def.Unit, EYJJTeam::Player, Plot.Pos + Offset, Stats) != INDEX_NONE)
			{
				++Spawned[static_cast<int32>(EYJJTeam::Player)];
			}
		}
	}
}

void FYJJBattle::CollectRewards()
{
	const float GoldScale = 1.f + Bonus(EYJJStat::Gold, INDEX_NONE);
	const float SoulScale = 1.f + Bonus(EYJJStat::Souls, INDEX_NONE);
	for (const FYJJDeath& Death : Horde.Deaths)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Death.Type);
		if (Death.Team == EYJJTeam::Enemy)
		{
			// 황금 해골·영혼 폭발: 가끔 몇 배
			const float GoldMult = Random.FRand() < GoldBurstChance ? GoldBurstMult : 1.f;
			const float SoulMult = Random.FRand() < SoulBurstChance ? SoulBurstMult : 1.f;
			Gold += Def.Bounty * GoldScale * GoldMult;
			Souls += Def.Souls * SoulScale * SoulMult;
			SoulsEarned += Def.Souls * SoulScale * SoulMult;
			++Kills;
		}
		else if (Def.bHero && !bGameOver)
		{
			Finish(false);
		}
	}
	RecentDeaths.Append(Horde.Deaths);
	Horde.Deaths.Reset();

	// HUD가 오래 안 가져가면 (창이 최소화 등) 쌓이지 않게 자른다
	const int32 Keep = YJJData::Tune().MaxPendingDeaths;
	if (RecentDeaths.Num() > Keep)
	{
		RecentDeaths.RemoveAt(0, RecentDeaths.Num() - Keep, EAllowShrinking::No);
	}
}

void FYJJBattle::TickEffects(float Dt)
{
	const FYJJTuning& T = YJJData::Tune();
	for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
	{
		FYJJEffect& Effect = Effects[Index];
		Effect.Age += Dt;
		if (Effect.Age < Effect.Life)
		{
			continue;
		}
		if (Effect.Kind == EYJJEffectKind::MeteorFall)
		{
			Horde.DamageArea(Effect.Pos, Effect.Radius, Effect.Power, T.MeteorKnockback, EYJJTeam::Enemy);
			RecentCraters.Add(Effect.Pos);
			const FVector2f BlastPos = Effect.Pos;
			const float BlastRadius = Effect.Radius;
			Effects.RemoveAtSwap(Index, EAllowShrinking::No);
			AddEffect(EYJJEffectKind::MeteorBlast, BlastPos, T.BlastEffectTime, BlastRadius);
			continue;
		}
		Effects.RemoveAtSwap(Index, EAllowShrinking::No);
	}
}

void FYJJBattle::AddEffect(EYJJEffectKind Kind, const FVector2f& Pos, float Life, float Radius, float Power)
{
	FYJJEffect& Effect = Effects.AddDefaulted_GetRef();
	Effect.Kind = Kind;
	Effect.Pos = Pos;
	Effect.Life = Life;
	Effect.Radius = Radius;
	Effect.Power = Power;
}

// ---------------------------------------------------------------------------------------------
// 병영

int32 FYJJBattle::BuildingCount(int32 Building) const
{
	int32 Count = 0;
	for (const FYJJPlot& Plot : Plots)
	{
		Count += Plot.Building == Building ? 1 : 0;
	}
	return Count;
}

int32 FYJJBattle::BuildCost(int32 Building) const
{
	const FYJJBuildingDef& Def = YJJData::Building(Building);
	return FMath::RoundToInt(Def.Cost * FMath::Pow(Def.BuildCostGrowth, static_cast<float>(BuildingCount(Building))));
}

bool FYJJBattle::CanBuild(int32 Plot, int32 Building) const
{
	return !bGameOver && Plots.IsValidIndex(Plot) && Plots[Plot].Building == INDEX_NONE
		&& YJJData::Buildings().IsValidIndex(Building) && Gold >= BuildCost(Building);
}

bool FYJJBattle::Build(int32 PlotIndex, int32 Building)
{
	if (!CanBuild(PlotIndex, Building))
	{
		return false;
	}
	Gold -= BuildCost(Building);
	FYJJPlot& Plot = Plots[PlotIndex];
	Plot.Building = Building;
	Plot.Level = 1;
	Plot.SpawnTimer = 0.f;
	AddEffect(EYJJEffectKind::Build, Plot.Pos, YJJData::Tune().BuildEffectTime, YJJData::Tune().PlotSize);
	return true;
}

int32 FYJJBattle::UpgradeCost(int32 PlotIndex) const
{
	if (!Plots.IsValidIndex(PlotIndex) || Plots[PlotIndex].Building == INDEX_NONE || Plots[PlotIndex].Level >= YJJData::Tune().MaxBuildingLevel)
	{
		return 0;
	}
	const FYJJBuildingDef& Def = YJJData::Building(Plots[PlotIndex].Building);
	return FMath::RoundToInt(Def.UpgradeCost * FMath::Pow(Def.UpgradeCostGrowth, static_cast<float>(Plots[PlotIndex].Level - 1)));
}

bool FYJJBattle::CanUpgrade(int32 PlotIndex) const
{
	const int32 Cost = UpgradeCost(PlotIndex);
	return !bGameOver && Cost > 0 && Gold >= Cost;
}

bool FYJJBattle::Upgrade(int32 PlotIndex)
{
	if (!CanUpgrade(PlotIndex))
	{
		return false;
	}
	Gold -= UpgradeCost(PlotIndex);
	++Plots[PlotIndex].Level;
	AddEffect(EYJJEffectKind::Build, Plots[PlotIndex].Pos, YJJData::Tune().BuildEffectTime, YJJData::Tune().PlotSize);
	return true;
}

// ---------------------------------------------------------------------------------------------
// 스펠

float FYJJBattle::CooldownMax(EYJJSpell Spell) const
{
	const FYJJTuning& T = YJJData::Tune();
	const float Base = Spell == EYJJSpell::Meteor ? T.MeteorCooldown : T.RallyCooldown;
	return Base * FMath::Max(1.f - Bonus(EYJJStat::SpellCooldown, INDEX_NONE), T.MinCooldownScale);
}

bool FYJJBattle::CanCast(EYJJSpell Spell) const
{
	return !bGameOver && Cooldowns[static_cast<int32>(Spell)] <= 0.f;
}

bool FYJJBattle::Cast(EYJJSpell Spell, const FVector2f& Where)
{
	if (!CanCast(Spell))
	{
		return false;
	}
	const FYJJTuning& T = YJJData::Tune();
	const float Power = 1.f + Bonus(EYJJStat::SpellPower, INDEX_NONE);
	Cooldowns[static_cast<int32>(Spell)] = CooldownMax(Spell);
	if (Spell == EYJJSpell::Meteor)
	{
		const FVector2f Target(FMath::Clamp(Where.X, CampEdgeX, FieldWidth), FMath::Clamp(Where.Y, 0.f, FieldHeight));
		AddEffect(EYJJEffectKind::MeteorFall, Target, T.MeteorFallTime, T.MeteorRadius, T.MeteorDamage * Power);
		if (MeteorTwiceDelay > 0.f)
		{
			AddEffect(EYJJEffectKind::MeteorFall, Target, T.MeteorFallTime + MeteorTwiceDelay, T.MeteorRadius, T.MeteorDamage * Power);
		}
	}
	else
	{
		RallyTime = T.RallyDuration * Power;
		if (RallyHealFraction > 0.f)
		{
			Horde.HealTeam(EYJJTeam::Player, RallyHealFraction);
		}
		AddEffect(EYJJEffectKind::Rally, FVector2f(CampEdgeX, FieldHeight * Half), T.RallyEffectTime, FieldHeight);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// 특성 성좌

int32 FYJJBattle::PerkCost() const
{
	const FYJJTuning& T = YJJData::Tune();
	return FMath::RoundToInt(T.PerkCostBase * FMath::Pow(T.PerkCostGrowth, static_cast<float>(PerksTaken)));
}

bool FYJJBattle::IsReachable(int32 Perk) const
{
	if (!YJJData::Perks().IsValidIndex(Perk) || IsAllocated(Perk))
	{
		return false;
	}
	for (const int32 Link : YJJData::Perks()[Perk].Links)
	{
		if (IsAllocated(Link))
		{
			return true;
		}
	}
	return false;
}

bool FYJJBattle::CanAllocate(int32 Perk) const
{
	// 판이 끝난 뒤(결과 창)에도 찍을 수 있다: 영혼은 판을 하며 모으고 다음 판에도 남는다
	return IsReachable(Perk) && Souls >= PerkCost();
}

bool FYJJBattle::Allocate(int32 Perk)
{
	if (!CanAllocate(Perk))
	{
		return false;
	}
	Souls -= PerkCost();
	ApplyPerk(Perk);
	return true;
}

void FYJJBattle::ApplyPerk(int32 Perk)
{
	PerkAllocated[Perk] = 1;
	++PerksTaken;
	for (const FYJJPerkDef::FMod& Mod : YJJData::Perks()[Perk].Mods)
	{
		Mods[ModIndex(Mod.Stat, Mod.Building + 1)] += Mod.Value;
	}
	RebuildSpecials();
	RecomputeHero();
}

void FYJJBattle::RebuildSpecials()
{
	const TConstArrayView<FYJJUnitDef> Units = YJJData::Units();
	TArray<FYJJUnitSpecial> Specials;
	Specials.SetNum(Units.Num());
	HeroRegenBonus = 0.f;
	GoldBurstChance = SoulBurstChance = MeteorTwiceDelay = RallyHealFraction = 0.f;
	GoldBurstMult = SoulBurstMult = 1.f;
	// 화살 수 특수 효과는 병영 능력치(Multishot)로 더한다: 다시 계산하려고 먼저 뺀다
	for (int32 Column = 0; Column <= YJJData::Buildings().Num(); ++Column)
	{
		Mods[ModIndex(EYJJStat::Multishot, Column)] = 0.f;
	}

	for (int32 Perk = 0; Perk < PerkAllocated.Num(); ++Perk)
	{
		const FYJJPerkDef& Def = YJJData::Perks()[Perk];
		if (!PerkAllocated[Perk])
		{
			continue;
		}
		for (const FYJJPerkDef::FMod& Mod : Def.Mods)
		{
			if (Mod.Stat == EYJJStat::Multishot)
			{
				Mods[ModIndex(Mod.Stat, Mod.Building + 1)] += Mod.Value;
			}
		}
		const FYJJPerkDef::FSpecial& Special = Def.Special;
		if (Special.Kind == EYJJSpecial::None)
		{
			continue;
		}
		// 효과 받는 유닛 종류: 병영의 병사 / 영웅 / 모든 아군
		TArray<int32, TInlineAllocator<16>> Targets;
		for (int32 Unit = 0; Unit < Units.Num(); ++Unit)
		{
			const bool bPlayer = Units[Unit].Team == EYJJTeam::Player;
			const bool bMatch = Special.Target == INDEX_NONE ? bPlayer
				: Special.Target == FYJJPerkDef::HeroTarget ? Units[Unit].bHero
				: YJJData::Building(Special.Target).Unit == Unit;
			if (bMatch)
			{
				Targets.Add(Unit);
			}
		}
		for (const int32 Unit : Targets)
		{
			FYJJUnitSpecial& Out = Specials[Unit];
			switch (Special.Kind)
			{
			case EYJJSpecial::LastStand: Out.LastStandAt = FMath::Max(Out.LastStandAt, Special.A); Out.LastStandHeal += Special.B; break;
			case EYJJSpecial::Revive: Out.ReviveHP = FMath::Max(Out.ReviveHP, Special.A); break;
			case EYJJSpecial::Fury: Out.FuryAt = FMath::Max(Out.FuryAt, Special.A); Out.FuryBonus += Special.B; break;
			case EYJJSpecial::ArrowBurst: Out.ArrowSplash = FMath::Max(Out.ArrowSplash, Special.A); break;
			case EYJJSpecial::Shockwave: Out.KnockbackScale *= Special.A; break;
			case EYJJSpecial::Thorns: Out.Thorns += Special.A; break;
			case EYJJSpecial::Lifesteal: Out.Lifesteal += Special.A; break;
			default: break;
			}
		}
		switch (Special.Kind)
		{
		case EYJJSpecial::ExtraShot: Mods[ModIndex(EYJJStat::Multishot, Special.Target + 1)] += Special.A; break;
		case EYJJSpecial::HeroRegen: HeroRegenBonus += Special.A; break;
		case EYJJSpecial::GoldBurst: GoldBurstChance += Special.A; GoldBurstMult = FMath::Max(GoldBurstMult, Special.B); break;
		case EYJJSpecial::SoulBurst: SoulBurstChance += Special.A; SoulBurstMult = FMath::Max(SoulBurstMult, Special.B); break;
		case EYJJSpecial::MeteorTwice: MeteorTwiceDelay = Special.A; break;
		case EYJJSpecial::RallyHeal: RallyHealFraction += Special.A; break;
		default: break;
		}
	}
	Horde.SetSpecials(Specials);
}

void FYJJBattle::WriteProgress(FYJJProgress& Progress) const
{
	Progress.Souls = Souls;
	Progress.Perks.Reset();
	for (int32 Perk = 0; Perk < PerkAllocated.Num(); ++Perk)
	{
		if (PerkAllocated[Perk] && YJJData::Perks()[Perk].Kind != EYJJPerkKind::Start)
		{
			Progress.Perks.Add(YJJData::Perks()[Perk].Id);
		}
	}
}

float FYJJBattle::Bonus(EYJJStat Stat, int32 Building) const
{
	if (Mods.Num() == 0)
	{
		return 0.f;
	}
	const float Common = Mods[ModIndex(Stat, 0)];
	return Building == INDEX_NONE ? Common : Common + Mods[ModIndex(Stat, Building + 1)];
}
