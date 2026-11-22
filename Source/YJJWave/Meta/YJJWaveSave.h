#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Sim/YJJBattle.h"
#include "YJJWaveSave.generated.h"

/**
 * 판이 끝나도 남는 것 (Saved/SaveGames/<슬롯>.sav): 영혼, 찍은 특성, 깬 스테이지 수.
 * 슬롯 이름은 명령줄 -YJJSave=이름 (오토파일럿·테스트가 실제 진행을 건드리지 않게), 없으면 YJJWave.
 */
UCLASS()
class YJJWAVE_API UYJJWaveSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() double Souls = 0.0;
	UPROPERTY() TArray<FName> Perks;
	UPROPERTY() int32 Cleared = 0;
	UPROPERTY() TArray<int32> Stars;

	static FYJJProgress Load();
	static void Store(const FYJJProgress& Progress);

private:
	static FString SlotName();

	static constexpr int32 UserIndex = 0;
	static constexpr const TCHAR* DefaultSlot = TEXT("YJJWave");
	static constexpr const TCHAR* SlotSwitch = TEXT("YJJSave=");
};
