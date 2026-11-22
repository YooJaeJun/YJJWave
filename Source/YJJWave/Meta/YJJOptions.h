#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "YJJOptions.generated.h"

/** 옵션 저장 (Saved/SaveGames/Options.sav). 해상도는 엔진 GameUserSettings 에도 저장된다 */
UCLASS()
class YJJWAVE_API UYJJOptionsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float Music = 1.f;
	UPROPERTY() float Sfx = 1.f;
	UPROPERTY() float Voice = 1.f;
	UPROPERTY() FString Language;
	UPROPERTY() int32 Resolution = INDEX_NONE;
};

/** 옵션: 사운드(배경음·효과음·보이스), 언어, 해상도 */
class YJJWAVE_API FYJJOptions
{
public:
	static FYJJOptions& Get();

	float Music = 1.f;
	float Sfx = 1.f;
	float Voice = 1.f;
	FString Language;
	int32 Resolution = INDEX_NONE;   // Resolutions() 번호, INDEX_NONE이면 지금 그대로

	void Save() const;
	/** 언어·해상도를 엔진에 적용 */
	void Apply() const;
	/** 고를 수 있는 해상도 (창 모드) */
	static TConstArrayView<FIntPoint> Resolutions();

private:
	FYJJOptions() = default;
	void Load();
	bool bLoaded = false;

	static constexpr const TCHAR* Slot = TEXT("Options");
	static constexpr int32 UserIndex = 0;
	static const FIntPoint ResolutionList[];
	static const int32 NumResolutions;
};
