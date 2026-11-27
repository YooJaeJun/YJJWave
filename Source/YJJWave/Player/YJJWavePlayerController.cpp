#include "Player/YJJWavePlayerController.h"
#include "InputKeyEventArgs.h"
#include "UI/YJJWaveHUD.h"
#include "Data/YJJGameData.h"

AYJJWavePlayerController::AYJJWavePlayerController()
{
	bShowMouseCursor = true;
}

void AYJJWavePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

AYJJWaveHUD* AYJJWavePlayerController::WaveHUD() const
{
	return Cast<AYJJWaveHUD>(GetHUD());
}

void AYJJWavePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (AYJJWaveHUD* HUD = WaveHUD())
	{
		// WASD·화살표로 시점 이동 (누르고 있는 동안)
		FVector2f Pan = FVector2f::ZeroVector;
		Pan.X += (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) ? 1.f : 0.f;
		Pan.X -= (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) ? 1.f : 0.f;
		Pan.Y += (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) ? 1.f : 0.f;
		Pan.Y -= (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) ? 1.f : 0.f;
		HUD->SetPanInput(Pan);
		// 터치를 한 번 쓴 뒤라도 마우스가 움직이면 다시 마우스 자리로
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (Touches.Num() == 0 && GetMousePosition(MouseX, MouseY))
		{
			const FVector2f Mouse(MouseX, MouseY);
			if (HUD->UsingTouchPointer() && !Mouse.Equals(LastMouse))
			{
				HUD->ClearTouchPointer();
			}
			LastMouse = Mouse;
		}
	}
}

float AYJJWavePlayerController::TouchSpread(FVector2f& OutMid) const
{
	TArray<FVector2f> Points;
	Touches.GenerateValueArray(Points);
	if (Points.Num() < 2)
	{
		OutMid = Points.Num() > 0 ? Points[0] : FVector2f::ZeroVector;
		return 0.f;
	}
	OutMid = (Points[0] + Points[1]) * 0.5f;
	return FVector2f::Distance(Points[0], Points[1]);
}

bool AYJJWavePlayerController::InputTouch(const FTouchId TouchId, const ETouchType::Type Type, const FVector2D& TouchLocation, const float Force, const uint64 Timestamp)
{
	AYJJWaveHUD* HUD = WaveHUD();
	if (!HUD)
	{
		return Super::InputTouch(TouchId, Type, TouchLocation, Force, Timestamp);
	}
	const FVector2f Where(TouchLocation.X, TouchLocation.Y);
	FVector2f Mid;
	if (Type == ETouchType::Began)
	{
		Touches.Add(TouchId, Where);
		if (Touches.Num() == 1)
		{
			HUD->SetTouchPointer(Where);
			HUD->HandleMouseDown(false);
		}
		else if (Touches.Num() == 2)
		{
			// 두 번째 손가락: 클릭은 취소하고 확대 시작
			HUD->CancelPointer();
			PinchStartDist = FMath::Max(TouchSpread(Mid), YJJData::Layout().PinchMinPixels);
			HUD->PinchBegin(Mid);
		}
	}
	else if (Type == ETouchType::Moved)
	{
		if (FVector2f* Found = Touches.Find(TouchId))
		{
			*Found = Where;
		}
		if (Touches.Num() == 1)
		{
			HUD->SetTouchPointer(Where);
		}
		else if (Touches.Num() >= 2)
		{
			const float Spread = TouchSpread(Mid);
			HUD->PinchUpdate(Spread / FMath::Max(PinchStartDist, KINDA_SMALL_NUMBER), Mid);
		}
	}
	else if (Type == ETouchType::Ended)
	{
		const bool bSingle = Touches.Num() == 1;
		Touches.Remove(TouchId);
		if (bSingle)
		{
			HUD->SetTouchPointer(Where);
			HUD->HandleMouseUp(false);
		}
		else if (Touches.Num() >= 2)
		{
			// 셋 이상에서 하나를 뗐다: 남은 둘로 다시 시작
			PinchStartDist = FMath::Max(TouchSpread(Mid), YJJData::Layout().PinchMinPixels);
			HUD->PinchBegin(Mid);
		}
	}
	return true;
}

bool AYJJWavePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (AYJJWaveHUD* HUD = WaveHUD())
	{
		const bool bPressed = Params.Event == IE_Pressed || Params.Event == IE_DoubleClick;
		if (Params.Key.IsMouseButton() && Touches.Num() == 0)
		{
			HUD->ClearTouchPointer();
		}
		if (Params.Key == EKeys::LeftMouseButton || Params.Key == EKeys::RightMouseButton)
		{
			const bool bRight = Params.Key == EKeys::RightMouseButton;
			if (bPressed)
			{
				HUD->HandleMouseDown(bRight);
			}
			else if (Params.Event == IE_Released)
			{
				HUD->HandleMouseUp(bRight);
			}
		}
		else if (Params.Key == EKeys::MouseScrollUp && bPressed)
		{
			HUD->HandleWheel(1.f);
		}
		else if (Params.Key == EKeys::MouseScrollDown && bPressed)
		{
			HUD->HandleWheel(-1.f);
		}
		else if (Params.Event == IE_Pressed)
		{
			HUD->HandleKey(Params.Key);
		}
	}
	return Super::InputKey(Params);
}
