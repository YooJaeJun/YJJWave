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
 *   Stages.csv     스테이지                       Waves.csv      스테이지·웨이브별 몬스터
 *   Perks.csv      특성 성좌 (Tools/Data/gen_perks.py)  Spells.csv     주문 Lv별 수치·특수 효과
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
	TArray<FYJJStageDef> Stages;         // Order 순서, 웨이브 포함
	TArray<FYJJPerkDef> Perks;
	/** Cards.csv: 이번 판 한정 카드 (효과 구조는 특성과 같다) */
	TArray<FYJJPerkDef> Cards;
	/** 주문별 Lv 줄 (Lv 순서, 0번 = Lv 1) */
	TArray<FYJJSpellLevel> SpellLevels[static_cast<int32>(EYJJSpell::Count)];

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
	void LoadStages();
	void LoadPerks();
	void LoadCards();
	void LoadSpells();
	bool ParseSpell(const FString& Name, int32& Out);
	bool ParseSpellFx(const FString& Name, EYJJSpellFx& Out);
	/** 능력치 2개 + 특수 효과를 Def에 읽는다 (Perks·Cards 공통) */
	void ReadEffects(const TCHAR* File, const FName& Name, const struct FYJJEffectRow& Row, FYJJPerkDef& Def);
	int32 FindUnit(const FString& Id, const TCHAR* Where);
	int32 FindBuilding(const FString& Id, const TCHAR* Where);
	bool ParseStat(const FString& Name, EYJJStat& Out);
	bool ParseSpecial(const FString& Name, EYJJSpecial& Out);

	static FString DataPath(const TCHAR* File);

	TArray<UDataTable*> Tables;   // AddToRoot로 붙잡아 둔다
	bool bLoaded = false;

	static FAutoConsoleCommand ReloadCommand;
	static constexpr const TCHAR* DataFolder = TEXT("YJJWave/Data");
	static constexpr const TCHAR* ContextName = TEXT("YJJData");
	static constexpr const TCHAR* TitleName = TEXT("Title");
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
	YJJWAVE_API TConstArrayView<FYJJStageDef> Stages();
	/** 영웅 유닛 번호 (Units.csv Hero=1, 없으면 INDEX_NONE) */
	YJJWAVE_API int32 HeroUnit();
	YJJWAVE_API TConstArrayView<FYJJPerkDef> Perks();
	YJJWAVE_API TConstArrayView<FYJJPerkDef> Cards();

	/** Perks.csv Stat 열·Localization Stat.<이름> 에 쓰는 이름 */
	YJJWAVE_API const TCHAR* StatKey(EYJJStat Stat);
	/** 비율이 아니라 +정수인 능력치 (한 번에 나오는 수, 화살 수) */
	YJJWAVE_API bool IsFlatStat(EYJJStat Stat);
	/** Perks.csv Special 열·Localization Special.<이름> 에 쓰는 이름 */
	YJJWAVE_API const TCHAR* SpecialKey(EYJJSpecial Special);

	/** Spells.csv Spell 열·Localization Spell.<이름> Key.<이름> SpellStat.<이름> 에 쓰는 이름 */
	YJJWAVE_API const TCHAR* SpellKey(EYJJSpell Spell);
	/** Spells.csv Fx 열·Localization SpellFx.<이름> 에 쓰는 이름 */
	YJJWAVE_API const TCHAR* SpellFxKey(EYJJSpellFx Fx);
	YJJWAVE_API TConstArrayView<FYJJSpellLevel> SpellLevels(EYJJSpell Spell);
	/** 그 주문의 최고 Lv (Spells.csv 줄 수) */
	YJJWAVE_API int32 SpellMaxLevel(EYJJSpell Spell);
	/** Lv 수치 + 그 Lv까지 열린 특수 효과 */
	YJJWAVE_API FYJJSpellStats SpellStats(EYJJSpell Spell, int32 Level);
}
