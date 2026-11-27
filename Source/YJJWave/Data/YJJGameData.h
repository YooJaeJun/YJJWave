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
 *   Broth.csv      냄비 국물 단계                 Dialogues.csv  게임 중 말풍선 대화
 *   Regions.csv    월드맵 지역                    Skills.csv     영웅·보스 큰 범위 스킬
 *   Heroes.csv     고를 수 있는 영웅과 스킬 3개
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
	TArray<FYJJRegionDef> Regions;       // Order 순서 (월드맵)
	TArray<FYJJPerkDef> Perks;
	/** Cards.csv: 이번 판 한정 카드 (효과 구조는 특성과 같다) */
	TArray<FYJJPerkDef> Cards;
	/** 주문별 Lv 줄 (Lv 순서, 0번 = Lv 1) */
	TArray<FYJJSpellLevel> SpellLevels[static_cast<int32>(EYJJSpell::Count)];
	/** Broth.csv: 냄비 국물 단계 (Bones 오름차순) */
	TArray<FYJJBrothStage> Broth;
	/** Dialogues.csv: 말풍선 대화 */
	TArray<FYJJTalk> Talks;
	/** Skills.csv: 영웅·보스 큰 범위 스킬 (유닛 Def.Skill 이 번호) */
	TArray<FYJJSkillDef> Skills;
	/** Heroes.csv: 고를 수 있는 영웅 (Order 순서) */
	TArray<FYJJHeroDef> Heroes;

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
	void LoadRegions();
	void LoadStages();
	void LoadPerks();
	void LoadCards();
	void LoadSpells();
	void LoadBroth();
	void LoadDialogues();
	void LoadSkills();
	void LoadHeroes();
	bool ParseTrigger(const FString& Name, EYJJTalkTrigger& Out);
	/** Voice 열: 비우면 소리 없음 (대사는 글로만) */
	static FName VoiceFor(const FString& Voice, const FString& Speaker);
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
	/** Stages.csv Blockers 한 칸: x/y/반너비/반높이 */
	static constexpr int32 BlockerFields = 4;
	static constexpr const TCHAR* ContextName = TEXT("YJJData");
	static constexpr const TCHAR* TitleName = TEXT("Title");
	/** Skills.csv Trigger 열 이름 */
	static constexpr const TCHAR* SkillManualName = TEXT("Manual");
	static constexpr const TCHAR* SkillAutoName = TEXT("Auto");
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
	YJJWAVE_API TConstArrayView<FYJJRegionDef> Regions();
	/** 영웅 유닛 번호 (Units.csv Hero=1, 없으면 INDEX_NONE) */
	YJJWAVE_API int32 HeroUnit();
	YJJWAVE_API TConstArrayView<FYJJPerkDef> Perks();
	YJJWAVE_API TConstArrayView<FYJJPerkDef> Cards();
	YJJWAVE_API TConstArrayView<FYJJBrothStage> Broth();
	YJJWAVE_API TConstArrayView<FYJJTalk> Talks();
	YJJWAVE_API TConstArrayView<FYJJSkillDef> Skills();
	YJJWAVE_API const FYJJSkillDef& Skill(int32 Index);
	YJJWAVE_API TConstArrayView<FYJJHeroDef> Heroes();
	/** Heroes.csv 에서 Id 인 영웅 번호 (없으면 0) */
	YJJWAVE_API int32 HeroIndex(const FName& Id);
	/** Dialogues.csv Trigger 열 이름 */
	YJJWAVE_API const TCHAR* TriggerKey(EYJJTalkTrigger Trigger);

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
