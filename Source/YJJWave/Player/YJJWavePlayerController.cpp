#include "Player/YJJWavePlayerController.h"
#include "InputKeyEventArgs.h"
#include "UI/YJJWaveHUD.h"

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
	}
}

bool AYJJWavePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (AYJJWaveHUD* HUD = WaveHUD())
	{
		const bool bPressed = Params.Event == IE_Pressed || Params.Event == IE_DoubleClick;
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
