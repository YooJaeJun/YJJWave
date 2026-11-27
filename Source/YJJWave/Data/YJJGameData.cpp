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
	LoadSkills();
	LoadBuildings();
	LoadRegions();
	LoadStages();
	LoadSpells();
	LoadPerks();
	LoadCards();
	LoadBroth();
	LoadDialogues();

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
	TArray<FString> PromoteNames;
	const UDataTable* Table = LoadTable(TEXT("Units.csv"), FYJJUnitRow::StaticStruct());
	Table->ForeachRow<FYJJUnitRow>(ContextName, [this, &RaiseNames, &PromoteNames](const FName& Name, const FYJJUnitRow& Row)
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
		Def.bPassTerrain = Row.PassTerrain != 0;
		Def.bCamp = Row.Camp != 0;
		Def.BigRow = Row.BigRow;
		Def.BigCell = Row.BigCell;
		Def.BigSub = FMath::Max(Row.BigSub, 1);
		Def.ShadowW = Row.ShadowW;
		if (Row.BigRow >= 0 && Row.BigCell <= 0.f)
		{
			Problem(FString::Printf(TEXT("Units.csv %s: BigRow 가 있으면 BigCell 도 있어야 한다"), *Name.ToString()));
		}
		Def.VoiceKey = FName(*(FString(TEXT("Voice_")) + Name.ToString()));
		Def.Bones = Row.Bones;
		Def.PromoteKills = Row.PromoteKills;
		RaiseNames.Add(Row.RaiseUnit);
		PromoteNames.Add(Row.PromoteTo);
		if (Row.HP <= 0.f || Row.Radius <= 0.f || Row.Mass <= 0.f || Row.AttackInterval <= 0.f)
		{
			Problem(FString::Printf(TEXT("Units.csv %s: 체력·반지름·질량·공격 간격은 0보다 커야 한다"), *Name.ToString()));
		}
	});
	for (int32 Unit = 0; Unit < Units.Num(); ++Unit)
	{
		Units[Unit].RaiseUnit = RaiseNames[Unit].IsEmpty() ? INDEX_NONE : FindUnit(RaiseNames[Unit], TEXT("Units.csv RaiseUnit"));
		Units[Unit].PromoteTo = PromoteNames[Unit].IsEmpty() ? INDEX_NONE : FindUnit(PromoteNames[Unit], TEXT("Units.csv PromoteTo"));
		Units[Unit].PromoteRoot = Unit;
		if (Units[Unit].PromoteTo != INDEX_NONE && Units[Unit].PromoteKills <= 0)
		{
			Problem(FString::Printf(TEXT("Units.csv %s: 진급하려면 PromoteKills가 0보다 커야 한다"), *Units[Unit].Id.ToString()));
			Units[Unit].PromoteTo = INDEX_NONE;
		}
	}
	// 진급 줄: 아무도 진급해 오지 않는 유닛에서 시작해 끝까지 따라가며 단계와 맨 처음 유닛을 적는다
	TArray<bool> IsTarget;
	IsTarget.Init(false, Units.Num());
	for (const FYJJUnitDef& Def : Units)
	{
		if (Def.PromoteTo != INDEX_NONE)
		{
			IsTarget[Def.PromoteTo] = true;
		}
	}
	for (int32 Unit = 0; Unit < Units.Num(); ++Unit)
	{
		if (IsTarget[Unit] || Units[Unit].PromoteTo == INDEX_NONE)
		{
			continue;
		}
		int32 Next = Units[Unit].PromoteTo;
		for (int32 Tier = 1; Next != INDEX_NONE && Tier < Units.Num(); ++Tier)
		{
			Units[Next].PromoteRoot = Unit;
			Units[Next].Tier = Tier;
			Next = Units[Next].PromoteTo;
		}
	}
}

FName FYJJGameDatabase::VoiceFor(const FString& Voice, const FString& Speaker)
{
	return Voice.IsEmpty() ? NAME_None : FName(*Voice);
}

void FYJJGameDatabase::LoadRegions()
{
	Regions.Reset();
	const UDataTable* Table = LoadTable(TEXT("Regions.csv"), FYJJRegionRow::StaticStruct());
	TArray<TPair<int32, FYJJRegionDef>> Sorted;
	Table->ForeachRow<FYJJRegionRow>(ContextName, [&Sorted](const FName& Name, const FYJJRegionRow& Row)
	{
		FYJJRegionDef Def;
		Def.Id = Name;
		Def.NameKey = Row.NameKey;
		Def.MapPos = FVector2f(Row.MapX, Row.MapY);
		Def.bOpen = Row.Open != 0;
		Sorted.Emplace(Row.Order, Def);
	});
	Sorted.StableSort([](const auto& A, const auto& B) { return A.Key < B.Key; });
	for (const auto& Entry : Sorted)
	{
		Regions.Add(Entry.Value);
	}
}

void FYJJGameDatabase::LoadBroth()
{
	Broth.Reset();
	const UDataTable* Table = LoadTable(TEXT("Broth.csv"), FYJJBrothRow::StaticStruct());
	Table->ForeachRow<FYJJBrothRow>(ContextName, [this](const FName& Name, const FYJJBrothRow& Row)
	{
		FYJJBrothStage& Stage = Broth.AddDefaulted_GetRef();
		Stage.Id = Name;
		Stage.NameKey = Row.NameKey;
		Stage.Bones = Row.Bones;
		Stage.HP = Row.HP;
		Stage.Damage = Row.Damage;
		Stage.Heal = Row.Heal;
		Stage.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Row.Color));
	});
	Broth.StableSort([](const FYJJBrothStage& A, const FYJJBrothStage& B) { return A.Bones < B.Bones; });
	if (Broth.Num() == 0)
	{
		Problem(TEXT("Broth.csv: 국물 단계가 없다"));
	}
}

void FYJJGameDatabase::LoadSkills()
{
	Skills.Reset();
	HeroSkills.Reset();
	for (FYJJUnitDef& Def : Units)
	{
		Def.Skill = INDEX_NONE;
	}
	const UDataTable* Table = LoadTable(TEXT("Skills.csv"), FYJJSkillRow::StaticStruct());
	Table->ForeachRow<FYJJSkillRow>(ContextName, [this](const FName& Name, const FYJJSkillRow& Row)
	{
		FYJJSkillDef Def;
		Def.Id = Name;
		Def.Unit = FindUnit(Row.Unit, TEXT("Skills.csv"));
		Def.NameKey = Row.NameKey;
		Def.DescKey = Row.DescKey;
		Def.Icon = Row.Icon;
		Def.SummonUnit = Row.Summon.IsEmpty() ? INDEX_NONE : FindUnit(Row.Summon, TEXT("Skills.csv Summon"));
		Def.SummonCount = Def.SummonUnit != INDEX_NONE ? FMath::Max(Row.SummonCount, 0) : 0;
		if (Row.Trigger == SkillManualName)
		{
			Def.Trigger = EYJJSkillTrigger::Manual;
		}
		else if (Row.Trigger == SkillAutoName)
		{
			Def.Trigger = EYJJSkillTrigger::Auto;
		}
		else
		{
			Problem(FString::Printf(TEXT("Skills.csv %s: 모르는 Trigger %s"), *Name.ToString(), *Row.Trigger));
		}
		Def.Cooldown = Row.Cooldown;
		Def.FirstDelay = Row.FirstDelay;
		Def.Windup = FMath::Max(Row.Windup, 0.f);
		Def.Hits = FMath::Max(Row.Hits, 1);
		Def.HitInterval = Row.HitInterval;
		Def.Radius = Row.Radius;
		Def.Reach = Row.Reach;
		Def.DamageScale = Row.DamageScale;
		Def.Knockback = Row.Knockback;
		Def.AutoCount = FMath::Max(Row.AutoCount, 1);
		if (Def.Unit == INDEX_NONE)
		{
			return;
		}
		// 내 영웅은 Manual 후보가 여럿일 수 있다 (판 시작에 고른다). 다른 유닛은 하나만
		const bool bHeroChoice = Units[Def.Unit].bHero && Def.Trigger == EYJJSkillTrigger::Manual;
		if (Units[Def.Unit].Skill != INDEX_NONE && !bHeroChoice)
		{
			Problem(FString::Printf(TEXT("Skills.csv %s: %s 는 이미 스킬이 있다"), *Name.ToString(), *Row.Unit));
			return;
		}
		if (bHeroChoice)
		{
			HeroSkills.Add(Skills.Num());
		}
		if (Units[Def.Unit].Skill == INDEX_NONE)
		{
			Units[Def.Unit].Skill = Skills.Num();
		}
		Skills.Add(Def);
	});
}

bool FYJJGameDatabase::ParseTrigger(const FString& Name, EYJJTalkTrigger& Out)
{
	for (int32 Index = 0; Index < static_cast<int32>(EYJJTalkTrigger::Count); ++Index)
	{
		if (Name == YJJData::TriggerKey(static_cast<EYJJTalkTrigger>(Index)))
		{
			Out = static_cast<EYJJTalkTrigger>(Index);
			return true;
		}
	}
	return false;
}

void FYJJGameDatabase::LoadDialogues()
{
	Talks.Reset();
	const UDataTable* Table = LoadTable(TEXT("Dialogues.csv"), FYJJDialogueRow::StaticStruct());
	TArray<TPair<int32, const FYJJDialogueRow*>> Rows;
	Table->ForeachRow<FYJJDialogueRow>(ContextName, [&Rows](const FName& Name, const FYJJDialogueRow& Row)
	{
		Rows.Emplace(Row.Order, &Row);
	});
	Rows.StableSort([](const auto& A, const auto& B) { return A.Key < B.Key; });
	for (const auto& Entry : Rows)
	{
		const FYJJDialogueRow& Row = *Entry.Value;
		const FName TalkId(*Row.Talk);
		FYJJTalk* Talk = Talks.FindByPredicate([&TalkId](const FYJJTalk& Other) { return Other.Id == TalkId; });
		if (!Talk)
		{
			// 첫 줄(Order가 가장 작은 줄)이 조건을 정한다
			Talk = &Talks.AddDefaulted_GetRef();
			Talk->Id = TalkId;
			Talk->Stage = Row.Stage.IsEmpty() ? INDEX_NONE : Stages.IndexOfByPredicate([&Row](const FYJJStageDef& Def) { return Def.Id == FName(*Row.Stage); });
			if (!Row.Stage.IsEmpty() && Talk->Stage == INDEX_NONE)
			{
				Problem(FString::Printf(TEXT("Dialogues.csv %s: 모르는 스테이지 %s"), *Row.Talk, *Row.Stage));
			}
			if (!ParseTrigger(Row.Trigger, Talk->Trigger))
			{
				Problem(FString::Printf(TEXT("Dialogues.csv %s: 모르는 Trigger %s"), *Row.Talk, *Row.Trigger));
			}
			Talk->Wave = Row.Wave;
			Talk->MinWave = Row.MinWave;
			Talk->Param = Row.Param.IsEmpty() ? INDEX_NONE : FindUnit(Row.Param, TEXT("Dialogues.csv Param"));
			Talk->Chance = Row.Chance;
		}
		FYJJTalkLine& Line = Talk->Lines.AddDefaulted_GetRef();
		Line.Speaker = FindUnit(Row.Speaker, TEXT("Dialogues.csv Speaker"));
		Line.TextKey = Row.TextKey;
		Line.VoiceKey = VoiceFor(Row.Voice, Row.Speaker);
		Line.Duration = Row.Duration;
		Line.Delay = Row.Delay;
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
	StageTable->ForeachRow<FYJJStageRow>(ContextName, [this, &Sorted](const FName& Name, const FYJJStageRow& Row)
	{
		FYJJStageDef Def;
		Def.Id = Name;
		Def.NameKey = Row.NameKey;
		Def.HPScale = Row.HPScale;
		Def.MapPos = FVector2f(Row.MapX, Row.MapY);
		Def.Ground = Row.Ground;
		Def.WallHP = Row.WallHP;
		TArray<FString> Builds;
		Row.StartBuild.ParseIntoArrayWS(Builds);
		for (const FString& Build : Builds)
		{
			const int32 Building = FindBuilding(Build, TEXT("Stages.csv StartBuild"));
			if (Building != INDEX_NONE)
			{
				Def.StartBuild.Add(Building);
			}
		}
		TArray<FString> Camps;
		Row.Camps.ParseIntoArrayWS(Camps);
		for (const FString& Token : Camps)
		{
			FString UnitName, Where, X, Y;
			if (!Token.Split(TEXT("@"), &UnitName, &Where) || !Where.Split(TEXT("/"), &X, &Y))
			{
				Problem(FString::Printf(TEXT("Stages.csv Camps: '%s' 는 유닛@x/y 꼴이어야 한다"), *Token));
				continue;
			}
			FYJJCampSpot& Spot = Def.Camps.AddDefaulted_GetRef();
			Spot.Unit = FindUnit(UnitName, TEXT("Stages.csv Camps"));
			Spot.Pos = FVector2f(FCString::Atof(*X), FCString::Atof(*Y));
		}
		TArray<FString> Blockers;
		Row.Blockers.ParseIntoArrayWS(Blockers);
		for (const FString& Token : Blockers)
		{
			TArray<FString> Parts;
			Token.ParseIntoArray(Parts, TEXT("/"));
			if (Parts.Num() != BlockerFields)
			{
				Problem(FString::Printf(TEXT("Stages.csv Blockers: '%s' 는 x/y/반너비/반높이 꼴이어야 한다"), *Token));
				continue;
			}
			Def.Blockers.Emplace(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]));
		}
		Def.Region = Regions.IndexOfByPredicate([&Row](const FYJJRegionDef& Region) { return Region.Id == FName(*Row.Region); });
		if (Def.Region == INDEX_NONE)
		{
			Problem(FString::Printf(TEXT("Stages.csv %s: 모르는 지역 %s"), *Name.ToString(), *Row.Region));
		}
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
	// 시작 컷씬
	const UDataTable* CutTable = LoadTable(TEXT("Cutscenes.csv"), FYJJCutsceneRow::StaticStruct());
	TArray<TPair<int32, TPair<int32, FYJJCutLine>>> Lines;
	CutTable->ForeachRow<FYJJCutsceneRow>(ContextName, [this, &Lines](const FName& Name, const FYJJCutsceneRow& Row)
	{
		const int32 Stage = Stages.IndexOfByPredicate([&Row](const FYJJStageDef& Def) { return Def.Id == FName(*Row.Stage); });
		if (Stage == INDEX_NONE)
		{
			Problem(FString::Printf(TEXT("Cutscenes.csv %s: 모르는 스테이지 %s"), *Name.ToString(), *Row.Stage));
			return;
		}
		FYJJCutLine Line;
		Line.bTitle = Row.Speaker == TitleName;
		Line.Speaker = Line.bTitle ? INDEX_NONE : FindUnit(Row.Speaker, TEXT("Cutscenes.csv"));
		Line.VoiceKey = VoiceFor(Row.Voice, Row.Speaker);
		Line.TextKey = Row.TextKey;
		Line.Duration = Row.Duration;
		Line.Camera = FVector2f(Row.CamX, Row.CamY);
		Line.Zoom = Row.Zoom;
		Lines.Emplace(Stage, TPair<int32, FYJJCutLine>(Row.Order, Line));
	});
	Lines.StableSort([](const auto& A, const auto& B) { return A.Value.Key < B.Value.Key; });
	for (const auto& Entry : Lines)
	{
		Stages[Entry.Key].Cutscene.Add(Entry.Value.Value);
	}

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
		Def.Icon = Row.Icon;
		Def.FlavorKey = Row.FlavorKey;
		ReadEffects(TEXT("Cards.csv"), Name, Row, Def);
	});
}

void FYJJGameDatabase::LoadSpells()
{
	for (TArray<FYJJSpellLevel>& Levels : SpellLevels)
	{
		Levels.Reset();
	}
	const UDataTable* Table = LoadTable(TEXT("Spells.csv"), FYJJSpellRow::StaticStruct());
	Table->ForeachRow<FYJJSpellRow>(ContextName, [this](const FName& Name, const FYJJSpellRow& Row)
	{
		int32 Spell = INDEX_NONE;
		if (!ParseSpell(Row.Spell, Spell))
		{
			Problem(FString::Printf(TEXT("Spells.csv %s: 모르는 주문 %s"), *Name.ToString(), *Row.Spell));
			return;
		}
		FYJJSpellLevel& Level = SpellLevels[Spell].AddDefaulted_GetRef();
		Level.Level = Row.Level;
		Level.Power = Row.Power;
		Level.Radius = Row.Radius;
		Level.Cooldown = Row.Cooldown;
		Level.Duration = Row.Duration;
		Level.A = Row.A;
		Level.B = Row.B;
		if (!Row.Fx.IsEmpty() && !ParseSpellFx(Row.Fx, Level.Fx))
		{
			Problem(FString::Printf(TEXT("Spells.csv %s: 모르는 특수 효과 %s"), *Name.ToString(), *Row.Fx));
		}
	});
	for (int32 Spell = 0; Spell < static_cast<int32>(EYJJSpell::Count); ++Spell)
	{
		SpellLevels[Spell].Sort([](const FYJJSpellLevel& A, const FYJJSpellLevel& B) { return A.Level < B.Level; });
		if (SpellLevels[Spell].Num() == 0)
		{
			Problem(FString::Printf(TEXT("Spells.csv: %s 줄이 없다"), YJJData::SpellKey(static_cast<EYJJSpell>(Spell))));
			SpellLevels[Spell].AddDefaulted();
		}
	}
}

bool FYJJGameDatabase::ParseSpell(const FString& Name, int32& Out)
{
	for (int32 Index = 0; Index < static_cast<int32>(EYJJSpell::Count); ++Index)
	{
		if (Name == YJJData::SpellKey(static_cast<EYJJSpell>(Index)))
		{
			Out = Index;
			return true;
		}
	}
	return false;
}

bool FYJJGameDatabase::ParseSpellFx(const FString& Name, EYJJSpellFx& Out)
{
	for (int32 Index = 1; Index < static_cast<int32>(EYJJSpellFx::Count); ++Index)
	{
		if (Name == YJJData::SpellFxKey(static_cast<EYJJSpellFx>(Index)))
		{
			Out = static_cast<EYJJSpellFx>(Index);
			return true;
		}
	}
	return false;
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
			: Row.Kind == TEXT("Spell") ? EYJJPerkKind::Spell
			: EYJJPerkKind::Small;
		Def.Pos = FVector2f(Row.X, Row.Y);
		if (Def.Kind == EYJJPerkKind::Spell && !ParseSpell(Row.Spell, Def.Spell))
		{
			Problem(FString::Printf(TEXT("Perks.csv %s: 모르는 주문 %s"), *Name.ToString(), *Row.Spell));
			Def.Kind = EYJJPerkKind::Small;
		}
		ReadEffects(TEXT("Perks.csv"), Name, Row, Def);
		Def.Icon = Row.Icon;
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
TConstArrayView<FYJJRegionDef> YJJData::Regions() { return FYJJGameDatabase::Get().Regions; }

int32 YJJData::HeroUnit()
{
	return FYJJGameDatabase::Get().Units.IndexOfByPredicate([](const FYJJUnitDef& Def) { return Def.bHero; });
}
TConstArrayView<FYJJPerkDef> YJJData::Perks() { return FYJJGameDatabase::Get().Perks; }
TConstArrayView<FYJJPerkDef> YJJData::Cards() { return FYJJGameDatabase::Get().Cards; }
TConstArrayView<FYJJBrothStage> YJJData::Broth() { return FYJJGameDatabase::Get().Broth; }
TConstArrayView<FYJJSkillDef> YJJData::Skills() { return FYJJGameDatabase::Get().Skills; }
const FYJJSkillDef& YJJData::Skill(int32 Index) { return FYJJGameDatabase::Get().Skills[Index]; }
TConstArrayView<int32> YJJData::HeroSkills() { return FYJJGameDatabase::Get().HeroSkills; }
TConstArrayView<FYJJTalk> YJJData::Talks() { return FYJJGameDatabase::Get().Talks; }

const TCHAR* YJJData::TriggerKey(EYJJTalkTrigger Trigger)
{
	switch (Trigger)
	{
	case EYJJTalkTrigger::StageStart: return TEXT("StageStart");
	case EYJJTalkTrigger::WaveStart: return TEXT("WaveStart");
	case EYJJTalkTrigger::BossAppear: return TEXT("BossAppear");
	case EYJJTalkTrigger::Promote: return TEXT("Promote");
	case EYJJTalkTrigger::PotOpen: return TEXT("PotOpen");
	case EYJJTalkTrigger::PotOverflow: return TEXT("PotOverflow");
	case EYJJTalkTrigger::HeroLow: return TEXT("HeroLow");
	case EYJJTalkTrigger::PotReady: return TEXT("PotReady");
	case EYJJTalkTrigger::BossDown: return TEXT("BossDown");
	case EYJJTalkTrigger::CampDown: return TEXT("CampDown");
	case EYJJTalkTrigger::ChokePassed: return TEXT("ChokePassed");
	default: return TEXT("");
	}
}

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

const TCHAR* YJJData::SpellKey(EYJJSpell Spell)
{
	switch (Spell)
	{
	case EYJJSpell::Meteor: return TEXT("Meteor");
	case EYJJSpell::Rally: return TEXT("Rally");
	case EYJJSpell::Ladle: return TEXT("Ladle");
	default: return TEXT("");
	}
}

const TCHAR* YJJData::SpellFxKey(EYJJSpellFx Fx)
{
	switch (Fx)
	{
	case EYJJSpellFx::Burn: return TEXT("Burn");
	case EYJJSpellFx::Shower: return TEXT("Shower");
	case EYJJSpellFx::Heal: return TEXT("Heal");
	case EYJJSpellFx::Reinforce: return TEXT("Reinforce");
	default: return TEXT("");
	}
}

TConstArrayView<FYJJSpellLevel> YJJData::SpellLevels(EYJJSpell Spell)
{
	return FYJJGameDatabase::Get().SpellLevels[static_cast<int32>(Spell)];
}

int32 YJJData::SpellMaxLevel(EYJJSpell Spell)
{
	return SpellLevels(Spell).Num();
}

FYJJSpellStats YJJData::SpellStats(EYJJSpell Spell, int32 Level)
{
	const TConstArrayView<FYJJSpellLevel> Levels = SpellLevels(Spell);
	const int32 Last = FMath::Clamp(Level, 1, Levels.Num()) - 1;
	FYJJSpellStats Stats;
	Stats.Level = Last + 1;
	Stats.Power = Levels[Last].Power;
	Stats.Radius = Levels[Last].Radius;
	Stats.Cooldown = Levels[Last].Cooldown;
	Stats.Duration = Levels[Last].Duration;
	for (int32 Index = 0; Index <= Last; ++Index)
	{
		const FYJJSpellLevel& Row = Levels[Index];
		if (Row.Fx != EYJJSpellFx::None)
		{
			const int32 Fx = static_cast<int32>(Row.Fx);
			Stats.bFx[Fx] = true;
			Stats.FxA[Fx] = Row.A;
			Stats.FxB[Fx] = Row.B;
		}
	}
	return Stats;
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
