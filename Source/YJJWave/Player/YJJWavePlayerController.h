#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "YJJWavePlayerController.generated.h"

/** 마우스·키를 HUD로 넘긴다 (화면 전체가 HUD 캔버스라 위젯이 없다) */
UCLASS()
class YJJWAVE_API AYJJWavePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AYJJWavePlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	class AYJJWaveHUD* WaveHUD() const;
};
