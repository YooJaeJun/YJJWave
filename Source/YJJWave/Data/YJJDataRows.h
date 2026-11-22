#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "YJJDataRows.generated.h"

/*
 * CSV 한 줄 = 아래 USTRUCT 하나. 첫 열(Name)이 행 이름, 나머지 열은 변수 이름과 같다.
 * 읽어서 게임용 구조체(Sim/YJJWaveTypes.h)로 푸는 일은 FYJJGameDatabase 가 한다.
 */

/** Tuning.csv, UILayout.csv: 이름 = 값 */
USTRUCT()
struct FYJJNameValueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() float Value = 0.f;
	UPROPERTY() FString Note;
};

/** Units.csv */
USTRUCT()
struct FYJJUnitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Team;
	UPROPERTY() FString NameKey;
	UPROPERTY() float HP = 0.f;
	UPROPERTY() float Damage = 0.f;
	UPROPERTY() float AttackInterval = 0.f;
	UPROPERTY() float Range = 0.f;
	UPROPERTY() float Sight = 0.f;
	UPROPERTY() float Speed = 0.f;
	UPROPERTY() float Radius = 0.f;
	UPROPERTY() float Mass = 0.f;
	UPROPERTY() float Splash = 0.f;
	UPROPERTY() float Knockback = 0.f;
	UPROPERTY() float ProjectileSpeed = 0.f;
	UPROPERTY() int32 ProjectileFrame = 0;
	UPROPERTY() int32 Bounty = 0;
	UPROPERTY() float Souls = 0.f;
	UPROPERTY() int32 AtlasRow = 0;
	UPROPERTY() float DrawSize = 0.f;
	UPROPERTY() int32 Boss = 0;
	UPROPERTY() int32 Hero = 0;
};

/** Buildings.csv */
USTRUCT()
struct FYJJBuildingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString NameKey;
	UPROPERTY() FString DescKey;
	UPROPERTY() FString Unit;
	UPROPERTY() int32 Cost = 0;
	UPROPERTY() float BuildCostGrowth = 0.f;
	UPROPERTY() int32 UpgradeCost = 0;
	UPROPERTY() float UpgradeCostGrowth = 0.f;
	UPROPERTY() float SpawnInterval = 0.f;
	UPROPERTY() int32 SpawnCount = 0;
	UPROPERTY() float CountPerLevel = 0.f;
	UPROPERTY() float IntervalScalePerLevel = 0.f;
	UPROPERTY() float StatPerLevel = 0.f;
	UPROPERTY() int32 AtlasCol = 0;
	UPROPERTY() int32 IconCol = 0;
	UPROPERTY() float BotWeight = 0.f;
};

/** Stages.csv */
USTRUCT()
struct FYJJStageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString NameKey;
	UPROPERTY() float HPScale = 1.f;
	UPROPERTY() FString Note;
};

/** Waves.csv (행 이름은 아무거나 겹치지 않게: M1_Skeleton …) */
USTRUCT()
struct FYJJWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Stage;
	UPROPERTY() int32 Wave = 0;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() FString Unit;
	UPROPERTY() float Rate = 0.f;
	UPROPERTY() int32 Burst = 0;
};

/** Perks.csv (Tools/Data/gen_perks.py 가 만든다) */
USTRUCT()
struct FYJJPerkRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Kind;
	UPROPERTY() float X = 0.f;
	UPROPERTY() float Y = 0.f;
	UPROPERTY() FString Stat;
	UPROPERTY() FString Target;
	UPROPERTY() float Value = 0.f;
	UPROPERTY() FString Stat2;
	UPROPERTY() FString Target2;
	UPROPERTY() float Value2 = 0.f;
	UPROPERTY() FString Special;
	UPROPERTY() FString SpecialTarget;
	UPROPERTY() float SpecialA = 0.f;
	UPROPERTY() float SpecialB = 0.f;
	UPROPERTY() FString Links;
	UPROPERTY() FString NameKey;
};
