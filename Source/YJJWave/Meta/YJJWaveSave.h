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
	/** 내용 서명 (YJJGuard::Sign). 안 맞으면 고친 파일로 보고 불러오지 않는다 */
	UPROPERTY() FString Signature;
	/** 고른 영웅 (서명 밖: 고르는 건 자유) */
	UPROPERTY() FName Hero;

	static FYJJProgress Load();
	static void Store(const FYJJProgress& Progress);

private:
	static FString SlotName();
	/** 서명할 내용 (순서·형식이 바뀌면 옛 저장이 무효가 된다) */
	FString SignedContent() const;

	static constexpr int32 UserIndex = 0;
	static constexpr const TCHAR* DefaultSlot = TEXT("YJJWave");
	static constexpr const TCHAR* SlotSwitch = TEXT("YJJSave=");
};
