#pragma once

#include "CoreMinimal.h"
#include "Data/YJJTuning.h"
#include "HAL/IConsoleManager.h"
#include "Sim/YJJWaveTypes.h"

class UDataTable;
class UScriptStruct;

/**
 * 게임 데이터 전부. 처음 쓸 때 Content/YJJWave/Data/*.csv 를 엔진 DataTable로 읽어 아래 표로 푼다.
 * 게임 중에는 콘솔 yjj.ReloadData 로 다시 읽는다 (판을 다시 시작해야 전부 반영).
 *
 *   Tuning.csv     규칙 수치 (FYJJTuning)        UILayout.csv   화면 배치·연출·아틀라스 (FYJJUILayout)
 *   Units.csv      병사·몬스터                    Buildings.csv  병영
 *   Waves.csv      물결 레벨별 몬스터             Perks.csv      특성 성좌 (Tools/Data/gen_perks.py)
 * 글자는 Localization.csv (YJJLoc).
 */
class YJJWAVE_API FYJJGameDatabase
{
public:
	static FYJJGameDatabase& Get();

	void Reload();

	FYJJTuning Tuning;
	FYJJUILayout Layout;
	TArray<FYJJUnitDef> Units;
	TArray<FYJJBuildingDef> Buildings;   // Order 순서
	TArray<FYJJWaveLevel> Waves;         // 0번 = 1레벨
	TArray<FYJJPerkDef> Perks;

	/** 읽으면서 생긴 문제 (빠진 이름, 모르는 유닛…) */
	TArray<FString> Problems;

private:
	FYJJGameDatabase() = default;

	void Load();
	void Problem(const FString& Message);
	UDataTable* LoadTable(const TCHAR* File, UScriptStruct* RowStruct);
	/** Name,Value 표를 USTRUCT의 같은 이름 숫자 변수에 넣는다 */
	void FillNameValues(const TCHAR* File, const UScriptStruct* Struct, void* Data);

	void LoadUnits();
	void LoadBuildings();
	void LoadWaves();
	void LoadPerks();
	int32 FindUnit(const FString& Id, const TCHAR* Where);
	int32 FindBuilding(const FString& Id, const TCHAR* Where);
	bool ParseStat(const FString& Name, EYJJStat& Out);

	static FString DataPath(const TCHAR* File);

	TArray<UDataTable*> Tables;   // AddToRoot로 붙잡아 둔다
	bool bLoaded = false;

	static FAutoConsoleCommand ReloadCommand;
	static constexpr const TCHAR* DataFolder = TEXT("YJJWave/Data");
	static constexpr const TCHAR* ContextName = TEXT("YJJData");
};

// 자주 쓰는 표를 짧게 부르는 길 (모두 FYJJGameDatabase::Get()을 거친다)
namespace YJJData
{
	YJJWAVE_API const FYJJTuning& Tune();
	YJJWAVE_API const FYJJUILayout& Layout();
	YJJWAVE_API TConstArrayView<FYJJUnitDef> Units();
	YJJWAVE_API const FYJJUnitDef& Unit(int32 Index);
	YJJWAVE_API TConstArrayView<FYJJBuildingDef> Buildings();
	YJJWAVE_API const FYJJBuildingDef& Building(int32 Index);
	YJJWAVE_API TConstArrayView<FYJJWaveLevel> Waves();
	YJJWAVE_API TConstArrayView<FYJJPerkDef> Perks();

	/** Perks.csv Stat 열·Localization Stat.<이름> 에 쓰는 이름 */
	YJJWAVE_API const TCHAR* StatKey(EYJJStat Stat);
	/** 비율이 아니라 +정수인 능력치 (한 번에 나오는 수, 화살 수) */
	YJJWAVE_API bool IsFlatStat(EYJJStat Stat);
}
