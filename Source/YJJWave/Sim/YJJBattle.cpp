#include "Sim/YJJBattle.h"
#include "Data/YJJGameData.h"
#include "YJJWave.h"
#include "Algo/Count.h"

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
	Sounds.Reset();
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
	bCardPending = false;
	CardOffer.Reset();
	Sounds.Add(SoundCard);
	CardsTaken.Reset();
	FrontBonus = 1.f;
	Corpses.Reset();
	CorpseCursor = 0;
	NecroTimer = 0.f;
	RallyTime = 0.f;
	RallyAttack = 1.f;
	BonesInPot = 0.f;
	OverflowTime = 0.f;
	LidAge = -1.f;
	BoneFlyBudget = 0.f;
	Served = 0;
	ServedHP = ServedDamage = 0.f;
	bHeroLowSaid = false;
	LastPotStage = INDEX_NONE;
	bEnding = false;
	Stars = 0;
	EndTimer = 0.f;
	SlowMoLeft = 0.f;
	Events.Reset();
	AddEvent(EYJJTalkTrigger::StageStart);
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
		else
		{
			// 주문 별은 찍은 수만큼 같은 이름이 저장돼 있다
			const int32 Points = FMath::Min(static_cast<int32>(Algo::Count(Progress.Perks, Def.Id)), PerkMaxLevel(Perk));
			for (int32 Point = 0; Point < Points; ++Point)
			{
				ApplyPerk(Perk);
			}
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
	Horde.AttackBuff[PlayerTeam] = RallyTime > 0.f ? RallyAttack : 1.f;

	if (Horde.Hero() != INDEX_NONE)
	{
		Horde.Heal(Horde.Hero(), (T.HeroRegen + HeroRegenBonus) * Dt);
	}
	TickPlots(Dt);
	TickEnemies(Dt);
	TickPot(Dt);

	// 영웅이 위태로우면 한 번 말한다
	if (!bHeroLowSaid && Horde.Hero() != INDEX_NONE && Horde.HP[Horde.Hero()] < Horde.MaxHP[Horde.Hero()] * T.HeroLowShare)
	{
		bHeroLowSaid = true;
		AddEvent(EYJJTalkTrigger::HeroLow, YJJData::HeroUnit());
	}
}

void FYJJBattle::AddEvent(EYJJTalkTrigger Trigger, int32 Unit)
{
	Events.Add({ Trigger, Unit, Level });
	// HUD가 오래 안 가져가면 쌓이지 않게
	if (Events.Num() > YJJData::Tune().MaxPendingEvents)
	{
		Events.RemoveAt(0, Events.Num() - YJJData::Tune().MaxPendingEvents, EAllowShrinking::No);
	}
}

FVector2f FYJJBattle::PotPos() const
{
	return FVector2f(YJJData::Tune().PotX, YJJData::Tune().PotY);
}

int32 FYJJBattle::PotStage() const
{
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index < Broth.Num(); ++Index)
	{
		if (BonesInPot >= Broth[Index].Bones)
		{
			Found = Index;
		}
	}
	return Found;
}

float FYJJBattle::PotNextBones() const
{
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	const int32 Next = PotStage() + 1;
	return Broth.IsValidIndex(Next) ? Broth[Next].Bones : 0.f;
}

float FYJJBattle::PotOverflowLeft() const
{
	const bool bLast = YJJData::Broth().Num() > 0 && PotStage() == YJJData::Broth().Num() - 1;
	return bLast ? YJJData::Tune().PotOverflowTime - OverflowTime : -1.f;
}

void FYJJBattle::TickPot(float Dt)
{
	const FYJJTuning& T = YJJData::Tune();
	BoneFlyBudget = FMath::Min(BoneFlyBudget + T.BoneFlyPerSecond * Dt, T.BoneFlyPerSecond);
	if (LidAge >= 0.f)
	{
		LidAge += Dt;
	}
	// 국이 한 단계 진해지면 알린다 (처음 끓을 때 대장이 알려 준다)
	const int32 NowStage = PotStage();
	if (NowStage != INDEX_NONE && NowStage > LastPotStage)
	{
		AddEvent(EYJJTalkTrigger::PotReady, NowStage);
	}
	LastPotStage = NowStage;
	// 마지막 단계(곰탕)에서 오래 두면 끓어 넘쳐 다 잃는다: 언제 열지가 판단
	if (PotOverflowLeft() < 0.f)
	{
		OverflowTime = 0.f;
		return;
	}
	OverflowTime += Dt;
	if (OverflowTime >= T.PotOverflowTime)
	{
		BonesInPot = 0.f;
		OverflowTime = 0.f;
		AddEffect(EYJJEffectKind::PotSteam, PotPos(), T.PotSteamTime, T.PotSteamSize * T.PotOverflowSteamScale);
		Sounds.Add(SoundPotOverflow);
		AddEvent(EYJJTalkTrigger::PotOverflow);
		UE_LOG(LogYJJWave, Display, TEXT("[Battle] 냄비가 끓어 넘쳤다"));
	}
}

bool FYJJBattle::OpenPot()
{
	const int32 BrothStage = PotStage();
	if (bGameOver || BrothStage == INDEX_NONE)
	{
		return false;
	}
	const FYJJTuning& T = YJJData::Tune();
	const FYJJBrothStage& Broth = YJJData::Broth()[BrothStage];
	// 국물은 이번 판 내내: 앞으로 나올 병사 능력치 + 지금 있는 병사 회복
	Mods[ModIndex(EYJJStat::HP, 0)] += Broth.HP;
	Mods[ModIndex(EYJJStat::Damage, 0)] += Broth.Damage;
	Horde.HealTeam(EYJJTeam::Player, Broth.Heal);
	ServedHP += Broth.HP;
	ServedDamage += Broth.Damage;
	++Served;
	BonesInPot = 0.f;
	OverflowTime = 0.f;
	LidAge = 0.f;
	AddEffect(EYJJEffectKind::PotSteam, PotPos(), T.PotSteamTime, T.PotSteamSize);
	AddEffect(EYJJEffectKind::Rally, PotPos(), T.RallyEffectTime, T.PotServeRing);
	Sounds.Add(SoundPotOpen);
	AddEvent(EYJJTalkTrigger::PotOpen, BrothStage);
	UE_LOG(LogYJJWave, Display, TEXT("[Battle] 냄비: %s (체력 +%.0f%%, 공격력 +%.0f%%)"), *Broth.Id.ToString(), Broth.HP * 100.f, Broth.Damage * 100.f);
	return true;
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
	// 전선 보너스: 영웅 자리 ~ 오른쪽 끝 사이 어디까지 밀었나
	const FYJJTuning& T = YJJData::Tune();
	const float PlayerFront = Horde.FrontX(EYJJTeam::Player);
	const float Pushed = (PlayerFront - T.HeroHomeX) / FMath::Max(FieldWidth * T.PlayerAdvanceShare - T.HeroHomeX, 1.f);
	FrontBonus = 1.f + T.FrontBonusMax * FMath::Clamp(Pushed, 0.f, 1.f);

	TickEffects(Dt);
	if (Horde.HitCount > 0)
	{
		Sounds.Add(SoundHit);
	}
	if (Horde.ShotCount > 0)
	{
		Sounds.Add(SoundArrow);
	}
	if (Horde.Deaths.Num() > 0)
	{
		Sounds.Add(SoundDeath);
	}
	Horde.HitCount = Horde.ShotCount = 0;
	CollectRewards();
	TickNecro(Dt);
	if (Horde.Rescued.Num() > 0)
	{
		Sounds.Add(SoundRescue);
	}
	for (const FVector2f& Where : Horde.Rescued)
	{
		AddEffect(EYJJEffectKind::Rescue, Where, YJJData::Tune().RescueEffectTime, YJJData::Tune().RescueEffectSize);
	}
	Horde.Rescued.Reset();
	// 진급: 빛줄기 + 소리, 말풍선 대화가 듣는다
	for (const FYJJPromotion& Promotion : Horde.Promoted)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Promotion.Type);
		AddEffect(EYJJEffectKind::Promote, Promotion.Pos, T.PromoteEffectTime, Def.DrawSize * T.PromoteEffectScale);
		Sounds.Add(SoundPromote);
		AddEvent(EYJJTalkTrigger::Promote, Promotion.Type);
	}
	Horde.Promoted.Reset();
	SlowMoLeft = FMath::Max(SlowMoLeft - Dt, 0.f);
	CheckVictory();
	if (bEnding && !bGameOver)
	{
		EndTimer -= Dt;
		if (EndTimer <= 0.f)
		{
			Finish(true);
		}
	}
}

void FYJJBattle::CheckVictory()
{
	// 마지막 웨이브가 끝났고 남은 적이 없으면 돌파: 별을 매기고 쓰러지는 연출을 조금 보여 준 뒤 결과
	const FYJJTuning& T = YJJData::Tune();
	if (!bGameOver && !bEnding && Level >= NumWaves() && LevelTime >= LevelDuration() && Horde.Num(EYJJTeam::Enemy) == 0)
	{
		bEnding = true;
		EndTimer = T.VictoryDelay;
		const int32 Hero = Horde.Hero();
		const float Health = Hero != INDEX_NONE ? Horde.HP[Hero] / FMath::Max(Horde.MaxHP[Hero], KINDA_SMALL_NUMBER) : 0.f;
		Stars = Health >= T.StarThreeHealth ? 3 : Health >= T.StarTwoHealth ? 2 : 1;
	}
}

float FYJJBattle::TimeScale() const
{
	return SlowMoLeft > 0.f ? YJJData::Tune().BossSlowMo : 1.f;
}

void FYJJBattle::Finish(bool bWon)
{
	bGameOver = true;
	bVictory = bWon;
	Sounds.Add(bWon ? SoundVictory : SoundDefeat);
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
	if (Level > 1)
	{
		OfferCards();
	}
	AddEvent(EYJJTalkTrigger::WaveStart);
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
	// 전장 가운데 띠에서 나온다 (위아래로 다 퍼지면 아군 시야 밖으로 빠져나간다)
	const float Band = FieldHeight * T.EnemySpawnBand * 0.5f;
	const FVector2f Pos(FieldWidth - T.EnemySpawnInset - Random.FRand() * T.SpawnScatter, FieldHeight * 0.5f + Random.FRandRange(-Band, Band));
	SpawnEnemyAt(Unit, Pos);
}

int32 FYJJBattle::SpawnEnemyAt(int32 Unit, const FVector2f& Pos)
{
	const FYJJTuning& T = YJJData::Tune();
	if (Horde.Num(EYJJTeam::Enemy) >= T.MaxUnitsPerTeam)
	{
		return INDEX_NONE;
	}
	FYJJSpawnStats Stats;
	Stats.HP = FMath::Pow(T.LevelHPGrowth, static_cast<float>(Level - 1)) * StageDef().HPScale;
	Stats.Damage = FMath::Pow(T.LevelDamageGrowth, static_cast<float>(Level - 1));
	const int32 Slot = Horde.Spawn(Unit, EYJJTeam::Enemy, Pos, Stats);
	if (Slot == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	++Spawned[static_cast<int32>(EYJJTeam::Enemy)];
	Sounds.Add(YJJData::Unit(Unit).VoiceKey);
	if (YJJData::Unit(Unit).bBoss)
	{
		AddEffect(EYJJEffectKind::Build, Pos, T.BuildEffectTime, YJJData::Unit(Unit).DrawSize * BossEffectRadiusScale);
		Sounds.Add(SoundBossAppear);
		AddEvent(EYJJTalkTrigger::BossAppear, Unit);
		UE_LOG(LogYJJWave, Display, TEXT("[Battle] 보스 등장: %s (웨이브 %d)"), *YJJData::Unit(Unit).Id.ToString(), Level);
	}
	return Slot;
}

void FYJJBattle::TickNecro(float Dt)
{
	const FYJJTuning& T = YJJData::Tune();
	NecroTimer += Dt;
	if (NecroTimer < T.NecroRaiseInterval || Corpses.Num() == 0)
	{
		return;
	}
	NecroTimer = 0.f;
	const float RadiusSq = T.NecroRaiseRadius * T.NecroRaiseRadius;
	for (int32 Slot = 0; Slot < Horde.HighWater(); ++Slot)
	{
		if (!Horde.Alive[Slot] || Horde.Team[Slot] != static_cast<uint8>(EYJJTeam::Enemy))
		{
			continue;
		}
		const int32 Raise = YJJData::Unit(Horde.Type[Slot]).RaiseUnit;
		if (Raise == INDEX_NONE)
		{
			continue;
		}
		// 가까운 시체 몇 구를 일으킨다 (일으킨 시체는 목록에서 뺀다)
		int32 Raised = 0;
		for (int32 Index = Corpses.Num() - 1; Index >= 0 && Raised < T.NecroRaiseCount; --Index)
		{
			if (FVector2f::DistSquared(Corpses[Index], Horde.Pos[Slot]) > RadiusSq)
			{
				continue;
			}
			if (SpawnEnemyAt(Raise, Corpses[Index]) != INDEX_NONE)
			{
				AddEffect(EYJJEffectKind::Raise, Corpses[Index], T.RaiseEffectTime, T.RaiseEffectSize);
				Sounds.Add(SoundRaise);
				++Raised;
			}
			Corpses.RemoveAtSwap(Index, EAllowShrinking::No);
		}
		CorpseCursor = Corpses.Num() > 0 ? CorpseCursor % Corpses.Num() : 0;
	}
}

void FYJJBattle::OfferCards()
{
	const int32 NumCards = YJJData::Cards().Num();
	CardOffer.Reset();
	if (NumCards == 0)
	{
		return;
	}
	const int32 Want = FMath::Min(YJJData::Tune().CardChoices, NumCards);
	while (CardOffer.Num() < Want)
	{
		CardOffer.AddUnique(Random.RandHelper(NumCards));
	}
	bCardPending = true;
}

bool FYJJBattle::PickCard(int32 Choice)
{
	if (!bCardPending || !CardOffer.IsValidIndex(Choice))
	{
		return false;
	}
	const int32 Card = CardOffer[Choice];
	CardsTaken.Add(Card);
	for (const FYJJPerkDef::FMod& Mod : YJJData::Cards()[Card].Mods)
	{
		Mods[ModIndex(Mod.Stat, Mod.Building + 1)] += Mod.Value;
	}
	RebuildSpecials();
	RecomputeHero();
	bCardPending = false;
	CardOffer.Reset();
	UE_LOG(LogYJJWave, Display, TEXT("[Battle] 카드: %s"), *YJJData::Cards()[Card].Id.ToString());
	return true;
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
		Sounds.Add(YJJData::Unit(Def.Unit).VoiceKey);
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
	const float GoldScale = (1.f + Bonus(EYJJStat::Gold, INDEX_NONE)) * FrontBonus;
	const float SoulScale = (1.f + Bonus(EYJJStat::Souls, INDEX_NONE)) * FrontBonus;
	const int32 MaxCorpses = YJJData::Tune().MaxCorpses;
	for (const FYJJDeath& Death : Horde.Deaths)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Death.Type);
		if (Death.Team == EYJJTeam::Enemy)
		{
			// 황금 해골·영혼 폭발: 가끔 몇 배
			const float GoldMult = Random.FRand() < GoldBurstChance ? GoldBurstMult : 1.f;
			const float SoulMult = Random.FRand() < SoulBurstChance ? SoulBurstMult : 1.f;
			Gold += Def.Bounty * GoldScale * GoldMult;
			// 뼈는 냄비로 (마지막 단계에서는 더 들어가지 않는다). 날아가는 뼈 그림은 초당 몇 개만
			if (Def.Bones > 0.f && YJJData::Broth().Num() > 0)
			{
				BonesInPot = FMath::Min(BonesInPot + Def.Bones, YJJData::Broth().Last().Bones);
				if (BoneFlyBudget >= 1.f)
				{
					BoneFlyBudget -= 1.f;
					AddEffect(EYJJEffectKind::BoneFly, Death.Pos, YJJData::Tune().BoneFlyTime, 0.f);
				}
			}
			Souls += Def.Souls * SoulScale * SoulMult;
			SoulsEarned += Def.Souls * SoulScale * SoulMult;
			++Kills;
		}
		else if (Def.bHero && !bGameOver)
		{
			Finish(false);
		}
		// 보스가 쓰러지면: 큰 폭발 + 잠깐 느리게 (말풍선·배너는 사건으로)
		if (Def.bBoss)
		{
			const FYJJTuning& T = YJJData::Tune();
			SlowMoLeft = T.BossSlowMoTime;
			AddEffect(EYJJEffectKind::MeteorBlast, Death.Pos, T.BlastEffectTime * T.BossBlastTimeScale, Def.DrawSize * T.BossBlastScale);
			AddEffect(EYJJEffectKind::Promote, Death.Pos, T.PromoteEffectTime * T.BossBlastTimeScale, Def.DrawSize * T.BossBlastScale);
			Sounds.Add(SoundMeteorBoom);
			AddEvent(EYJJTalkTrigger::BossDown, Death.Type);
		}
		// 강령술사가 쓸 시체 (보스·영웅은 빼고)
		if (!Def.bBoss && !Def.bHero && MaxCorpses > 0)
		{
			if (Corpses.Num() < MaxCorpses)
			{
				Corpses.Add(Death.Pos);
			}
			else
			{
				Corpses[CorpseCursor] = Death.Pos;
				CorpseCursor = (CorpseCursor + 1) % MaxCorpses;
			}
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
		if (Effect.Kind == EYJJEffectKind::Burn)
		{
			Effect.Tick += Dt;
			while (Effect.Tick >= T.BurnTick)
			{
				Effect.Tick -= T.BurnTick;
				Horde.DamageArea(Effect.Pos, Effect.Radius, Effect.Power * T.BurnTick, 0.f, EYJJTeam::Enemy);
			}
		}
		if (Effect.Age < Effect.Life)
		{
			continue;
		}
		if (Effect.Kind == EYJJEffectKind::MeteorFall)
		{
			Horde.DamageArea(Effect.Pos, Effect.Radius, Effect.Power, T.MeteorKnockback, EYJJTeam::Enemy);
			RecentCraters.Add(FVector3f(Effect.Pos.X, Effect.Pos.Y, Effect.Radius));
			Sounds.Add(SoundMeteorBoom);
			const FYJJEffect Landed = Effect;
			Effects.RemoveAtSwap(Index, EAllowShrinking::No);
			AddEffect(EYJJEffectKind::MeteorBlast, Landed.Pos, T.BlastEffectTime, Landed.Radius);
			if (Landed.BurnPower > 0.f)
			{
				AddEffect(EYJJEffectKind::Burn, Landed.Pos, Landed.BurnTime, Landed.Radius, Landed.BurnPower);
			}
			continue;
		}
		if (Effect.Kind == EYJJEffectKind::QuakeHit)
		{
			const FYJJEffect Landed = Effect;
			Effects.RemoveAtSwap(Index, EAllowShrinking::No);
			QuakeStrike(Landed.Pos.X, Landed.Radius, Landed.Power);
			continue;
		}
		Effects.RemoveAtSwap(Index, EAllowShrinking::No);
	}
}

FYJJEffect& FYJJBattle::AddEffect(EYJJEffectKind Kind, const FVector2f& Pos, float Life, float Radius, float Power)
{
	FYJJEffect& Effect = Effects.AddDefaulted_GetRef();
	Effect.Kind = Kind;
	Effect.Pos = Pos;
	Effect.Life = Life;
	Effect.Radius = Radius;
	Effect.Power = Power;
	return Effect;
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

int32 FYJJBattle::BuyCost(int32 Building, int32 Count) const
{
	return YJJData::Buildings().IsValidIndex(Building) ? YJJData::Building(Building).BuyCost * Count : 0;
}

bool FYJJBattle::CanBuy(int32 Building, int32 Count) const
{
	return !bGameOver && Count > 0 && YJJData::Buildings().IsValidIndex(Building) && Gold >= BuyCost(Building, Count)
		&& Horde.Num(EYJJTeam::Player) + Count <= YJJData::Tune().MaxUnitsPerTeam;
}

bool FYJJBattle::Buy(int32 Building, int32 Count)
{
	if (!CanBuy(Building, Count))
	{
		return false;
	}
	Gold -= BuyCost(Building, Count);
	SpawnSquad(Building, Count);
	Sounds.Add(SoundBuy);
	return true;
}

void FYJJBattle::SpawnSquad(int32 Building, int32 Count)
{
	const FYJJTuning& T = YJJData::Tune();
	int32 BestLevel = 1;
	for (const FYJJPlot& Plot : Plots)
	{
		BestLevel = Plot.Building == Building ? FMath::Max(BestLevel, Plot.Level) : BestLevel;
	}
	const FYJJSpawnStats Stats = PlayerStats(Building, BestLevel);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2f Pos(CampEdgeX + Random.FRandRange(0.f, T.SpawnScatter), FieldHeight * Half + Random.FRandRange(-T.BuySpread, T.BuySpread));
		if (Horde.Spawn(YJJData::Building(Building).Unit, EYJJTeam::Player, Pos, Stats) != INDEX_NONE)
		{
			++Spawned[static_cast<int32>(EYJJTeam::Player)];
		}
	}
	AddEffect(EYJJEffectKind::Build, FVector2f(CampEdgeX, FieldHeight * Half), T.BuildEffectTime, T.PlotSize);
	Sounds.Add(YJJData::Unit(YJJData::Building(Building).Unit).VoiceKey);
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
	Sounds.Add(SoundBuild);
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
	Sounds.Add(SoundUpgrade);
	AddEffect(EYJJEffectKind::Build, Plots[PlotIndex].Pos, YJJData::Tune().BuildEffectTime, YJJData::Tune().PlotSize);
	return true;
}

// ---------------------------------------------------------------------------------------------
// 스펠

float FYJJBattle::CooldownMax(EYJJSpell Spell) const
{
	const FYJJTuning& T = YJJData::Tune();
	return SpellNow(Spell).Cooldown * FMath::Max(1.f - Bonus(EYJJStat::SpellCooldown, INDEX_NONE), T.MinCooldownScale);
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
	const float Power = 1.f + Bonus(EYJJStat::SpellPower, INDEX_NONE);
	const FYJJSpellStats Stats = SpellNow(Spell);
	Cooldowns[static_cast<int32>(Spell)] = CooldownMax(Spell);
	const FVector2f Target(FMath::Clamp(Where.X, CampEdgeX, FieldWidth), FMath::Clamp(Where.Y, 0.f, FieldHeight));
	switch (Spell)
	{
	case EYJJSpell::Meteor: CastMeteor(Stats, Target, Power); break;
	case EYJJSpell::Rally: CastRally(Stats, Power); break;
	case EYJJSpell::Quake: CastQuake(Stats, Target.X, Power); break;
	default: break;
	}
	return true;
}

void FYJJBattle::CastMeteor(const FYJJSpellStats& Stats, const FVector2f& Target, float Power)
{
	const FYJJTuning& T = YJJData::Tune();
	const float Damage = Stats.Power * Power;
	// 불바다: 떨어진 자리에 남는 불 (초당 유성 피해 x A, B초)
	const float BurnPower = Stats.Has(EYJJSpellFx::Burn) ? Damage * Stats.A(EYJJSpellFx::Burn) : 0.f;
	const float BurnTime = Stats.B(EYJJSpellFx::Burn);
	DropMeteor(Target, T.MeteorFallTime, Stats.Radius, Damage, BurnPower, BurnTime);
	Sounds.Add(SoundMeteorFall);
	if (MeteorTwiceDelay > 0.f)
	{
		DropMeteor(Target, T.MeteorFallTime + MeteorTwiceDelay, Stats.Radius, Damage, BurnPower, BurnTime);
	}
	// 유성우: 주변에 작은 운석 A개 (피해 x B)
	if (Stats.Has(EYJJSpellFx::Shower))
	{
		const int32 Count = FMath::RoundToInt(Stats.A(EYJJSpellFx::Shower));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2f Offset = FVector2f(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Stats.Radius * T.ShowerSpread;
			const FVector2f Pos(FMath::Clamp(Target.X + Offset.X, CampEdgeX, FieldWidth), FMath::Clamp(Target.Y + Offset.Y, 0.f, FieldHeight));
			DropMeteor(Pos, T.MeteorFallTime + T.ShowerDelay * (Index + 1), Stats.Radius * T.ShowerRadiusScale, Damage * Stats.B(EYJJSpellFx::Shower), 0.f, 0.f);
		}
	}
}

void FYJJBattle::DropMeteor(const FVector2f& Pos, float Delay, float Radius, float Damage, float BurnPower, float BurnTime)
{
	FYJJEffect& Fall = AddEffect(EYJJEffectKind::MeteorFall, Pos, Delay, Radius, Damage);
	Fall.BurnPower = BurnPower;
	Fall.BurnTime = BurnTime;
}

void FYJJBattle::CastRally(const FYJJSpellStats& Stats, float Power)
{
	const FYJJTuning& T = YJJData::Tune();
	RallyTime = Stats.Duration * Power;
	RallyAttack = Stats.Power;
	Sounds.Add(SoundRally);
	Sounds.Add(SoundHeroVoice);
	const float Heal = RallyHealFraction + (Stats.Has(EYJJSpellFx::Heal) ? Stats.A(EYJJSpellFx::Heal) : 0.f);
	if (Heal > 0.f)
	{
		Horde.HealTeam(EYJJTeam::Player, Heal);
	}
	// 지원군: 지어 둔 병영마다 병사가 바로 나온다
	if (Stats.Has(EYJJSpellFx::Reinforce))
	{
		const int32 Count = FMath::RoundToInt(Stats.A(EYJJSpellFx::Reinforce));
		for (int32 Building = 0; Building < YJJData::Buildings().Num(); ++Building)
		{
			if (BuildingCount(Building) > 0)
			{
				SpawnSquad(Building, Count);
			}
		}
	}
	AddEffect(EYJJEffectKind::Rally, FVector2f(CampEdgeX, FieldHeight * Half), T.RallyEffectTime, FieldHeight);
}

void FYJJBattle::CastQuake(const FYJJSpellStats& Stats, float X, float Power)
{
	const FYJJTuning& T = YJJData::Tune();
	const float Damage = Stats.Power * Power;
	AddQuake(X, T.QuakeDelay, Stats.Radius, Damage);
	// 여진: A초 뒤 같은 자리 한 번 더 (피해 x B)
	if (Stats.Has(EYJJSpellFx::Aftershock))
	{
		AddQuake(X, T.QuakeDelay + Stats.A(EYJJSpellFx::Aftershock), Stats.Radius, Damage * Stats.B(EYJJSpellFx::Aftershock));
	}
	// 세 갈래: 양옆으로 번갈아 A줄 더 (줄 사이 B), 바깥 줄일수록 조금 늦게
	if (Stats.Has(EYJJSpellFx::Split))
	{
		const int32 Count = FMath::RoundToInt(Stats.A(EYJJSpellFx::Split));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Ring = Index / 2 + 1;
			const float Side = Index % 2 == 0 ? 1.f : -1.f;
			AddQuake(X + Side * Ring * Stats.B(EYJJSpellFx::Split), T.QuakeDelay + T.QuakeSplitDelay * Ring, Stats.Radius, Damage);
		}
	}
}

void FYJJBattle::AddQuake(float X, float Delay, float HalfWidth, float Damage)
{
	if (X < CampEdgeX || X > FieldWidth)
	{
		return;
	}
	AddEffect(EYJJEffectKind::QuakeHit, FVector2f(X, FieldHeight * Half), Delay, HalfWidth, Damage);
}

void FYJJBattle::QuakeStrike(float X, float HalfWidth, float Damage)
{
	const FYJJTuning& T = YJJData::Tune();
	const float Step = FMath::Max(HalfWidth * T.QuakeHitStep, 1.f);
	for (float Y = Step * Half; Y < FieldHeight + Step * Half; Y += Step)
	{
		Horde.DamageArea(FVector2f(X, Y), HalfWidth, Damage, T.QuakeKnockback, EYJJTeam::Enemy);
		RecentCraters.Add(FVector3f(X, Y, HalfWidth * T.QuakeScarScale));
	}
	Sounds.Add(SoundQuake);
	AddEffect(EYJJEffectKind::Quake, FVector2f(X, FieldHeight * Half), T.QuakeEffectTime, HalfWidth);
}

int32 FYJJBattle::SpellLevel(EYJJSpell Spell) const
{
	int32 SpellLv = 1;
	const TConstArrayView<FYJJPerkDef> Perks = YJJData::Perks();
	for (int32 Perk = 0; Perk < PerkAllocated.Num(); ++Perk)
	{
		if (Perks[Perk].Kind == EYJJPerkKind::Spell && Perks[Perk].Spell == static_cast<int32>(Spell))
		{
			SpellLv += PerkAllocated[Perk];
		}
	}
	return FMath::Clamp(SpellLv, 1, YJJData::SpellMaxLevel(Spell));
}

int32 FYJJBattle::PerkMaxLevel(int32 Perk) const
{
	const FYJJPerkDef& Def = YJJData::Perks()[Perk];
	// 주문은 Lv 1에서 시작하므로 별에는 (최고 Lv - 1)번 찍는다
	return Def.Kind == EYJJPerkKind::Spell ? FMath::Max(YJJData::SpellMaxLevel(static_cast<EYJJSpell>(Def.Spell)) - 1, 0) : 1;
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
	if (!YJJData::Perks().IsValidIndex(Perk) || PerkLevel(Perk) >= PerkMaxLevel(Perk))
	{
		return false;
	}
	// 주문 별: 한 번 찍었으면 이웃과 상관없이 계속 올린다
	if (IsAllocated(Perk))
	{
		return true;
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
	++PerkAllocated[Perk];
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
		ApplySpecialDef(Def, Specials);
	}
	for (const int32 Card : CardsTaken)
	{
		ApplySpecialDef(YJJData::Cards()[Card], Specials);
	}
	Horde.SetSpecials(Specials);
}

void FYJJBattle::WriteProgress(FYJJProgress& Progress) const
{
	Progress.Souls = Souls;
	Progress.Perks.Reset();
	for (int32 Perk = 0; Perk < PerkAllocated.Num(); ++Perk)
	{
		if (YJJData::Perks()[Perk].Kind == EYJJPerkKind::Start)
		{
			continue;
		}
		// 주문 별은 찍은 수만큼 같은 이름을 적는다
		for (int32 Point = 0; Point < PerkAllocated[Perk]; ++Point)
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

void FYJJBattle::ApplySpecialDef(const FYJJPerkDef& Def, TArray<FYJJUnitSpecial>& Specials)
{
	const FYJJPerkDef::FSpecial& Special = Def.Special;
	if (Special.Kind == EYJJSpecial::None)
	{
		return;
	}
	const TConstArrayView<FYJJUnitDef> Units = YJJData::Units();
	// 효과 받는 유닛 종류: 병영의 병사 / 영웅 / 모든 아군
	TArray<int32, TInlineAllocator<16>> Targets;
	for (int32 Unit = 0; Unit < Units.Num(); ++Unit)
	{
		const bool bPlayer = Units[Unit].Team == EYJJTeam::Player;
		const bool bMatch = Special.Target == INDEX_NONE ? bPlayer
			: Special.Target == FYJJPerkDef::HeroTarget ? Units[Unit].bHero
			: YJJData::Building(Special.Target).Unit == Units[Unit].PromoteRoot;
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
