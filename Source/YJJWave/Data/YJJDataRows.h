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
	UPROPERTY() FString RaiseUnit;
	UPROPERTY() int32 BigRow = -1;
	/** 큰 유닛 그림(Big_이름.png) 칸 크기: 최대 확대에서 화면 크기에 맞춘다 */
	UPROPERTY() float BigCell = 0.f;
	/** 큰 유닛 논리 칸 하나의 중간 프레임 수 */
	UPROPERTY() int32 BigSub = 1;
	UPROPERTY() FString PromoteTo;
	UPROPERTY() int32 PromoteKills = 0;
	UPROPERTY() float Bones = 0.f;
	/** 1이면 길목 지형(Stages.csv Blockers)을 지나간다 (영웅·보스급) */
	UPROPERTY() int32 PassTerrain = 0;
	/** 1이면 적 진지: 움직이지 않고, 다 부수면 이긴다 (적은 살아 있는 진지에서 나온다) */
	UPROPERTY() int32 Camp = 0;
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
	UPROPERTY() int32 BuyCost = 0;
};

/** Stages.csv */
USTRUCT()
struct FYJJStageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString NameKey;
	UPROPERTY() float HPScale = 1.f;
	UPROPERTY() FString Region;
	UPROPERTY() float MapX = 0.f;
	UPROPERTY() float MapY = 0.f;
	UPROPERTY() FString Ground;
	UPROPERTY() float WallHP = 0.f;
	/** 처음부터 지어져 있는 병영 (빈칸으로 여럿) */
	UPROPERTY() FString StartBuild;
	/** 적 진지: "유닛@x/y" 빈칸으로 여럿 (전장 비율). 있으면 진지를 다 부숴야 이긴다 */
	UPROPERTY() FString Camps;
	/** 길목 지형: "x/y/반너비/반높이" 빈칸으로 여럿 (전장 비율 타원). PassTerrain 유닛만 지나간다 */
	UPROPERTY() FString Blockers;
	UPROPERTY() FString Note;
};

/** Regions.csv: 월드맵 지역 */
USTRUCT()
struct FYJJRegionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString NameKey;
	UPROPERTY() float MapX = 0.f;
	UPROPERTY() float MapY = 0.f;
	UPROPERTY() int32 Open = 0;
	UPROPERTY() FString Color;
	UPROPERTY() FString Note;
};

/** Cutscenes.csv: 스테이지 시작 컷씬 한 줄 */
USTRUCT()
struct FYJJCutsceneRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Stage;
	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString Speaker;
	UPROPERTY() FString TextKey;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() float CamX = 0.f;
	UPROPERTY() float CamY = 0.f;
	UPROPERTY() float Zoom = 0.f;
	/** Sounds.csv 이름 (비우면 Voice_<Speaker> 웅얼웅얼) */
	UPROPERTY() FString Voice;
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

/** 효과 열 (Perks.csv·Cards.csv 공통): 능력치 2개 + 특수 효과 1개 */
USTRUCT()
struct FYJJEffectRow : public FTableRowBase
{
	GENERATED_BODY()

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
	UPROPERTY() FString NameKey;
};

/** Perks.csv (Tools/Data/gen_perks.py 가 만든다) */
USTRUCT()
struct FYJJPerkRow : public FYJJEffectRow
{
	GENERATED_BODY()

	UPROPERTY() FString Kind;
	UPROPERTY() float X = 0.f;
	UPROPERTY() float Y = 0.f;
	UPROPERTY() FString Links;
	/** 주문 별(Kind=Spell)이 올리는 주문 이름 */
	UPROPERTY() FString Spell;
	/** 중간 별 효과 그림 (UI.png 칸, -1 없음) */
	UPROPERTY() int32 Icon = -1;
};

/** Broth.csv: 냄비 국물 단계 */
USTRUCT()
struct FYJJBrothRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString NameKey;
	UPROPERTY() float Bones = 0.f;
	UPROPERTY() float HP = 0.f;
	UPROPERTY() float Damage = 0.f;
	UPROPERTY() float Heal = 0.f;
	UPROPERTY() FString Color;
	UPROPERTY() FString Note;
};

/** Dialogues.csv: 말풍선 한 줄. 같은 Talk 끼리 Order 순서로 주고받고, 조건은 Order가 가장 작은 줄의 것 */
USTRUCT()
struct FYJJDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Talk;
	UPROPERTY() int32 Order = 0;
	UPROPERTY() FString Stage;
	UPROPERTY() FString Trigger;
	UPROPERTY() int32 Wave = 0;
	UPROPERTY() int32 MinWave = 0;
	UPROPERTY() FString Param;
	UPROPERTY() float Chance = 1.f;
	UPROPERTY() FString Speaker;
	UPROPERTY() FString TextKey;
	UPROPERTY() FString Voice;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() float Delay = 0.f;
};

/** Spells.csv: 주문 하나의 한 Lv */
USTRUCT()
struct FYJJSpellRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY() FString Spell;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() float Power = 0.f;
	UPROPERTY() float Radius = 0.f;
	UPROPERTY() float Cooldown = 0.f;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() FString Fx;
	UPROPERTY() float A = 0.f;
	UPROPERTY() float B = 0.f;
};

/** Cards.csv: 웨이브가 바뀔 때 고르는 이번 판 한정 카드 */
USTRUCT()
struct FYJJCardRow : public FYJJEffectRow
{
	GENERATED_BODY()

	/** 양념 그림 (UI.png 칸) */
	UPROPERTY() int32 Icon = -1;
	/** 한 줄 맛 설명 (Localization) */
	UPROPERTY() FString FlavorKey;
};
