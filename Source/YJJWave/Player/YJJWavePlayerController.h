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
	/** 모바일 터치: 한 손가락 = 마우스 왼쪽 (누르기·끌기·떼기), 두 손가락 = 벌리고 오므려 확대, 같이 움직여 이동 */
	virtual bool InputTouch(const FTouchId TouchId, const ETouchType::Type Type, const FVector2D& TouchLocation, const float Force, const uint64 Timestamp) override;

private:
	class AYJJWaveHUD* WaveHUD() const;
	/** 지금 닿아 있는 손가락 자리 */
	TMap<FTouchId, FVector2f> Touches;
	float PinchStartDist = 0.f;
	/** 두 손가락 사이 거리와 가운데 */
	float TouchSpread(FVector2f& OutMid) const;
};
