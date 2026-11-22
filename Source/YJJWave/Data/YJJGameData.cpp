#include "Data/YJJGameData.h"
#include "Data/YJJDataRows.h"
#include "Data/YJJLoc.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "YJJWave.h"

FAutoConsoleCommand FYJJGameDatabase::ReloadCommand(
	TEXT("yjj.ReloadData"),
	TEXT("Content/YJJWave/Data/*.csv 를 다시 읽는다"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FYJJGameDatabase::Get().Reload();
		YJJLoc::Reload();
	}));

FYJJGameDatabase& FYJJGameDatabase::Get()
{
	static FYJJGameDatabase Instance;
	if (!Instance.bLoaded)
	{
		Instance.Load();
	}
	return Instance;
}

void FYJJGameDatabase::Reload()
{
	Load();
}

FString FYJJGameDatabase::DataPath(const TCHAR* File)
{
	return FPaths::ProjectContentDir() / DataFolder / File;
}

void FYJJGameDatabase::Problem(const FString& Message)
{
	Problems.Add(Message);
	UE_LOG(LogYJJWave, Error, TEXT("[Data] %s"), *Message);
}

UDataTable* FYJJGameDatabase::LoadTable(const TCHAR* File, UScriptStruct* RowStruct)
{
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
	Table->RowStruct = RowStruct;
	Table->AddToRoot();
	Tables.Add(Table);

	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *DataPath(File)))
	{
		Problem(FString::Printf(TEXT("%s 를 읽지 못했다 (%s)"), File, *DataPath(File)));
		return Table;
	}
	for (const FString& Message : Table->CreateTableFromCSVString(Csv))
	{
		Problem(FString::Printf(TEXT("%s: %s"), File, *Message));
	}
	return Table;
}

void FYJJGameDatabase::FillNameValues(const TCHAR* File, const UScriptStruct* Struct, void* Data)
{
	const UDataTable* Table = LoadTable(File, FYJJNameValueRow::StaticStruct());
	TSet<FName> Used;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FNumericProperty* Number = CastField<FNumericProperty>(*It);
		if (!Number)
		{
			continue;
		}
		const FName Name = Number->GetFName();
		const FYJJNameValueRow* Row = Table->FindRow<FYJJNameValueRow>(Name, ContextName, false);
		if (!Row)
		{
			Problem(FString::Printf(TEXT("%s 에 %s 가 없다"), File, *Name.ToString()));
			continue;
		}
		Used.Add(Name);
		void* Value = Number->ContainerPtrToValuePtr<void>(Data);
		if (Number->IsFloatingPoint())
		{
			Number->SetFloatingPointPropertyValue(Value, Row->Value);
		}
		else
		{
			Number->SetIntPropertyValue(Value, static_cast<int64>(FMath::RoundToInt(Row->Value)));
		}
	}
	for (const FName& RowName : Table->GetRowNames())
	{
		if (!Used.Contains(RowName))
		{
			Problem(FString::Printf(TEXT("%s 의 %s 는 코드가 쓰지 않는다"), File, *RowName.ToString()));
		}
	}
}

void FYJJGameDatabase::Load()
{
	bLoaded = true;
	Problems.Reset();
	for (UDataTable* Table : Tables)
	{
		Table->RemoveFromRoot();
	}
	Tables.Reset();

	FillNameValues(TEXT("Tuning.csv"), FYJJTuning::StaticStruct(), &Tuning);
	FillNameValues(TEXT("UILayout.csv"), FYJJUILayout::StaticStruct(), &Layout);
	LoadUnits();
	LoadBuildings();
	LoadStages();
	LoadPerks();
	LoadCards();

	UE_LOG(LogYJJWave, Display, TEXT("[Data] 유닛 %d, 병영 %d, 스테이지 %d, 특성 %d, 문제 %d"), Units.Num(), Buildings.Num(), Stages.Num(), Perks.Num(), Problems.Num());
}

int32 FYJJGameDatabase::FindUnit(const FString& Id, const TCHAR* Where)
{
	const int32 Index = Units.IndexOfByPredicate([&Id](const FYJJUnitDef& Def) { return Def.Id == FName(*Id); });
	if (Index == INDEX_NONE)
	{
		Problem(FString::Printf(TEXT("%s: 모르는 유닛 %s"), Where, *Id));
	}
	return Index;
}

int32 FYJJGameDatabase::FindBuilding(const FString& Id, const TCHAR* Where)
{
	const int32 Index = Buildings.IndexOfByPredicate([&Id](const FYJJBuildingDef& Def) { return Def.Id == FName(*Id); });
	if (Index == INDEX_NONE)
	{
		Problem(FString::Printf(TEXT("%s: 모르는 병영 %s"), Where, *Id));
	}
	return Index;
}

void FYJJGameDatabase::LoadUnits()
{
	Units.Reset();
	TArray<FString> RaiseNames;
	const UDataTable* Table = LoadTable(TEXT("Units.csv"), FYJJUnitRow::StaticStruct());
	Table->ForeachRow<FYJJUnitRow>(ContextName, [this, &RaiseNames](const FName& Name, const FYJJUnitRow& Row)
	{
		FYJJUnitDef& Def = Units.AddDefaulted_GetRef();
		Def.Id = Name;
		Def.Team = Row.Team == TEXT("Enemy") ? EYJJTeam::Enemy : EYJJTeam::Player;
		Def.NameKey = Row.NameKey;
		Def.HP = Row.HP;
		Def.Damage = Row.Damage;
		Def.AttackInterval = Row.AttackInterval;
		Def.Range = Row.Range;
		Def.Sight = Row.Sight;
		Def.Speed = Row.Speed;
		Def.Radius = Row.Radius;
		Def.Mass = Row.Mass;
		Def.Splash = Row.Splash;
		Def.Knockback = Row.Knockback;
		Def.ProjectileSpeed = Row.ProjectileSpeed;
		Def.ProjectileFrame = Row.ProjectileFrame;
		Def.Bounty = Row.Bounty;
		Def.Souls = Row.Souls;
		Def.AtlasRow = Row.AtlasRow;
		Def.DrawSize = Row.DrawSize;
		Def.bBoss = Row.Boss != 0;
		Def.bHero = Row.Hero != 0;
		Def.BigRow = Row.BigRow;
		Def.VoiceKey = FName(*(FString(TEXT("Voice_")) + Name.ToString()));
		RaiseNames.Add(Row.RaiseUnit);
		if (Row.HP <= 0.f || Row.Radius <= 0.f || Row.Mass <= 0.f || Row.AttackInterval <= 0.f)
		{
			Problem(FString::Printf(TEXT("Units.csv %s: 체력·반지름·질량·공격 간격은 0보다 커야 한다"), *Name.ToString()));
		}
	});
	for (int32 Unit = 0; Unit < Units.Num(); ++Unit)
	{
		Units[Unit].RaiseUnit = RaiseNames[Unit].IsEmpty() ? INDEX_NONE : FindUnit(RaiseNames[Unit], TEXT("Units.csv RaiseUnit"));
	}
}

void FYJJGameDatabase::LoadBuildings()
{
	Buildings.Reset();
	const UDataTable* Table = LoadTable(TEXT("Buildings.csv"), FYJJBuildingRow::StaticStruct());
	TArray<TPair<int32, FYJJBuildingDef>> Sorted;
	Table->ForeachRow<FYJJBuildingRow>(ContextName, [this, &Sorted](const FName& Name, const FYJJBuildingRow& Row)
	{
		FYJJBuildingDef Def;
		Def.Id = Name;
		Def.NameKey = Row.NameKey;
		Def.DescKey = Row.DescKey;
		Def.Unit = FindUnit(Row.Unit, TEXT("Buildings.csv"));
		Def.Cost = Row.Cost;
		Def.BuildCostGrowth = Row.BuildCostGrowth;
		Def.UpgradeCost = Row.UpgradeCost;
		Def.UpgradeCostGrowth = Row.UpgradeCostGrowth;
		Def.SpawnInterval = Row.SpawnInterval;
		Def.SpawnCount = Row.SpawnCount;
		Def.CountPerLevel = Row.CountPerLevel;
		Def.IntervalScalePerLevel = Row.IntervalScalePerLevel;
		Def.StatPerLevel = Row.StatPerLevel;
		Def.AtlasCol = Row.AtlasCol;
		Def.IconCol = Row.IconCol;
		Def.BotWeight = Row.BotWeight;
		Def.BuyCost = Row.BuyCost;
		if (Def.Unit == INDEX_NONE || Def.SpawnInterval <= 0.f)
		{
			Problem(FString::Printf(TEXT("Buildings.csv %s: 유닛이 없거나 생산 주기가 0"), *Name.ToString()));
			return;
		}
		Sorted.Emplace(Row.Order, Def);
	});
	Sorted.StableSort([](const TPair<int32, FYJJBuildingDef>& A, const TPair<int32, FYJJBuildingDef>& B) { return A.Key < B.Key; });
	for (const TPair<int32, FYJJBuildingDef>& Entry : Sorted)
	{
		Buildings.Add(Entry.Value);
	}
}

void FYJJGameDatabase::LoadStages()
{
	Stages.Reset();
	const UDataTable* StageTable = LoadTable(TEXT("Stages.csv"), FYJJStageRow::StaticStruct());
	TArray<TPair<int32, FYJJStageDef>> Sorted;
	StageTable->ForeachRow<FYJJStageRow>(ContextName, [&Sorted](const FName& Name, const FYJJStageRow& Row)
	{
		FYJJStageDef Def;
		Def.Id = Name;
		Def.NameKey = Row.NameKey;
		Def.HPScale = Row.HPScale;
		Sorted.Emplace(Row.Order, Def);
	});
	Sorted.StableSort([](const TPair<int32, FYJJStageDef>& A, const TPair<int32, FYJJStageDef>& B) { return A.Key < B.Key; });
	for (const TPair<int32, FYJJStageDef>& Entry : Sorted)
	{
		Stages.Add(Entry.Value);
	}

	const UDataTable* WaveTable = LoadTable(TEXT("Waves.csv"), FYJJWaveRow::StaticStruct());
	WaveTable->ForeachRow<FYJJWaveRow>(ContextName, [this](const FName& Name, const FYJJWaveRow& Row)
	{
		FYJJStageDef* Stage = Stages.FindByPredicate([&Row](const FYJJStageDef& Def) { return Def.Id == FName(*Row.Stage); });
		if (!Stage || Row.Wave < 1)
		{
			Problem(FString::Printf(TEXT("Waves.csv %s: 모르는 스테이지 %s 또는 웨이브 %d"), *Name.ToString(), *Row.Stage, Row.Wave));
			return;
		}
		if (Stage->Waves.Num() < Row.Wave)
		{
			Stage->Waves.SetNum(Row.Wave);
		}
		FYJJWaveLevel& Wave = Stage->Waves[Row.Wave - 1];
		Wave.Duration = FMath::Max(Wave.Duration, Row.Duration);
		FYJJWaveSpawn Spawn;
		Spawn.Unit = FindUnit(Row.Unit, TEXT("Waves.csv"));
		Spawn.Rate = Row.Rate;
		Spawn.Burst = Row.Burst;
		if (Spawn.Unit != INDEX_NONE)
		{
			Wave.Spawns.Add(Spawn);
		}
	});
	for (const FYJJStageDef& Stage : Stages)
	{
		if (Stage.Waves.Num() == 0)
		{
			Problem(FString::Printf(TEXT("Waves.csv: %s 스테이지에 웨이브가 없다"), *Stage.Id.ToString()));
		}
		for (int32 Index = 0; Index < Stage.Waves.Num(); ++Index)
		{
			if (Stage.Waves[Index].Duration <= 0.f)
			{
				Problem(FString::Printf(TEXT("Waves.csv: %s %d웨이브가 비었다"), *Stage.Id.ToString(), Index + 1));
			}
		}
	}
}

bool FYJJGameDatabase::ParseStat(const FString& Name, EYJJStat& Out)
{
	for (int32 Index = 0; Index < static_cast<int32>(EYJJStat::Count); ++Index)
	{
		if (Name == YJJData::StatKey(static_cast<EYJJStat>(Index)))
		{
			Out = static_cast<EYJJStat>(Index);
			return true;
		}
	}
	return false;
}

bool FYJJGameDatabase::ParseSpecial(const FString& Name, EYJJSpecial& Out)
{
	for (int32 Index = 1; Index < static_cast<int32>(EYJJSpecial::Count); ++Index)
	{
		if (Name == YJJData::SpecialKey(static_cast<EYJJSpecial>(Index)))
		{
			Out = static_cast<EYJJSpecial>(Index);
			return true;
		}
	}
	return false;
}

void FYJJGameDatabase::ReadEffects(const TCHAR* File, const FName& Name, const FYJJEffectRow& Row, FYJJPerkDef& Def)
{
	Def.NameKey = Row.NameKey;
	auto AddMod = [this, File, &Def, &Name](const FString& Stat, const FString& Target, float Value)
	{
		if (Stat.IsEmpty())
		{
			return;
		}
		FYJJPerkDef::FMod Mod;
		if (!ParseStat(Stat, Mod.Stat))
		{
			Problem(FString::Printf(TEXT("%s %s: 모르는 능력치 %s"), File, *Name.ToString(), *Stat));
			return;
		}
		Mod.Building = Target.IsEmpty() ? INDEX_NONE : FindBuilding(Target, File);
		Mod.Value = Value;
		Def.Mods.Add(Mod);
	};
	AddMod(Row.Stat, Row.Target, Row.Value);
	AddMod(Row.Stat2, Row.Target2, Row.Value2);
	if (!Row.Special.IsEmpty())
	{
		if (!ParseSpecial(Row.Special, Def.Special.Kind))
		{
			Problem(FString::Printf(TEXT("%s %s: 모르는 특수 효과 %s"), File, *Name.ToString(), *Row.Special));
		}
		Def.Special.Target = Row.SpecialTarget.IsEmpty() ? INDEX_NONE
			: Row.SpecialTarget == TEXT("Hero") ? FYJJPerkDef::HeroTarget
			: FindBuilding(Row.SpecialTarget, File);
		Def.Special.A = Row.SpecialA;
		Def.Special.B = Row.SpecialB;
	}
}

void FYJJGameDatabase::LoadCards()
{
	Cards.Reset();
	const UDataTable* Table = LoadTable(TEXT("Cards.csv"), FYJJCardRow::StaticStruct());
	Table->ForeachRow<FYJJCardRow>(ContextName, [this](const FName& Name, const FYJJCardRow& Row)
	{
		FYJJPerkDef& Def = Cards.AddDefaulted_GetRef();
		Def.Id = Name;
		Def.Kind = EYJJPerkKind::Notable;
		ReadEffects(TEXT("Cards.csv"), Name, Row, Def);
	});
}

void FYJJGameDatabase::LoadPerks()
{
	Perks.Reset();
	const UDataTable* Table = LoadTable(TEXT("Perks.csv"), FYJJPerkRow::StaticStruct());
	TArray<FString> LinkNames;
	Table->ForeachRow<FYJJPerkRow>(ContextName, [this, &LinkNames](const FName& Name, const FYJJPerkRow& Row)
	{
		FYJJPerkDef& Def = Perks.AddDefaulted_GetRef();
		Def.Id = Name;
		Def.Kind = Row.Kind == TEXT("Start") ? EYJJPerkKind::Start
			: Row.Kind == TEXT("Notable") ? EYJJPerkKind::Notable
			: Row.Kind == TEXT("Keystone") ? EYJJPerkKind::Keystone
			: EYJJPerkKind::Small;
		Def.Pos = FVector2f(Row.X, Row.Y);
		ReadEffects(TEXT("Perks.csv"), Name, Row, Def);
		LinkNames.Add(Row.Links);
	});

	// 이름 → 번호 (양쪽 모두에 적힌 선이라도 한 번만)
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		TArray<FString> Names;
		LinkNames[Perk].ParseIntoArrayWS(Names);
		for (const FString& LinkName : Names)
		{
			const int32 Other = Perks.IndexOfByPredicate([&LinkName](const FYJJPerkDef& Def) { return Def.Id == FName(*LinkName); });
			if (Other == INDEX_NONE)
			{
				Problem(FString::Printf(TEXT("Perks.csv %s: 모르는 이웃 %s"), *Perks[Perk].Id.ToString(), *LinkName));
				continue;
			}
			Perks[Perk].Links.AddUnique(Other);
			Perks[Other].Links.AddUnique(Perk);
		}
	}
}

// ---------------------------------------------------------------------------------------------

const FYJJTuning& YJJData::Tune() { return FYJJGameDatabase::Get().Tuning; }
const FYJJUILayout& YJJData::Layout() { return FYJJGameDatabase::Get().Layout; }
TConstArrayView<FYJJUnitDef> YJJData::Units() { return FYJJGameDatabase::Get().Units; }
const FYJJUnitDef& YJJData::Unit(int32 Index) { return FYJJGameDatabase::Get().Units[Index]; }
TConstArrayView<FYJJBuildingDef> YJJData::Buildings() { return FYJJGameDatabase::Get().Buildings; }
const FYJJBuildingDef& YJJData::Building(int32 Index) { return FYJJGameDatabase::Get().Buildings[Index]; }
TConstArrayView<FYJJStageDef> YJJData::Stages() { return FYJJGameDatabase::Get().Stages; }

int32 YJJData::HeroUnit()
{
	return FYJJGameDatabase::Get().Units.IndexOfByPredicate([](const FYJJUnitDef& Def) { return Def.bHero; });
}
TConstArrayView<FYJJPerkDef> YJJData::Perks() { return FYJJGameDatabase::Get().Perks; }
TConstArrayView<FYJJPerkDef> YJJData::Cards() { return FYJJGameDatabase::Get().Cards; }

const TCHAR* YJJData::StatKey(EYJJStat Stat)
{
	switch (Stat)
	{
	case EYJJStat::HP: return TEXT("HP");
	case EYJJStat::Damage: return TEXT("Damage");
	case EYJJStat::AttackSpeed: return TEXT("AttackSpeed");
	case EYJJStat::MoveSpeed: return TEXT("MoveSpeed");
	case EYJJStat::SpawnRate: return TEXT("SpawnRate");
	case EYJJStat::SpawnCount: return TEXT("SpawnCount");
	case EYJJStat::Multishot: return TEXT("Multishot");
	case EYJJStat::Splash: return TEXT("Splash");
	case EYJJStat::Gold: return TEXT("Gold");
	case EYJJStat::Souls: return TEXT("Souls");
	case EYJJStat::HeroHP: return TEXT("HeroHP");
	case EYJJStat::SpellPower: return TEXT("SpellPower");
	case EYJJStat::SpellCooldown: return TEXT("SpellCooldown");
	default: return TEXT("");
	}
}

bool YJJData::IsFlatStat(EYJJStat Stat)
{
	return Stat == EYJJStat::SpawnCount || Stat == EYJJStat::Multishot;
}

const TCHAR* YJJData::SpecialKey(EYJJSpecial Special)
{
	switch (Special)
	{
	case EYJJSpecial::LastStand: return TEXT("LastStand");
	case EYJJSpecial::Revive: return TEXT("Revive");
	case EYJJSpecial::HeroRegen: return TEXT("HeroRegen");
	case EYJJSpecial::Fury: return TEXT("Fury");
	case EYJJSpecial::ArrowBurst: return TEXT("ArrowBurst");
	case EYJJSpecial::ExtraShot: return TEXT("ExtraShot");
	case EYJJSpecial::GoldBurst: return TEXT("GoldBurst");
	case EYJJSpecial::SoulBurst: return TEXT("SoulBurst");
	case EYJJSpecial::Shockwave: return TEXT("Shockwave");
	case EYJJSpecial::MeteorTwice: return TEXT("MeteorTwice");
	case EYJJSpecial::RallyHeal: return TEXT("RallyHeal");
	case EYJJSpecial::Thorns: return TEXT("Thorns");
	case EYJJSpecial::Lifesteal: return TEXT("Lifesteal");
	default: return TEXT("");
	}
}
