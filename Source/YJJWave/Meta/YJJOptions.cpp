#include "Meta/YJJOptions.h"
#include "Audio/YJJAudio.h"
#include "Data/YJJLoc.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

const FIntPoint FYJJOptions::ResolutionList[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) };
const int32 FYJJOptions::NumResolutions = UE_ARRAY_COUNT(FYJJOptions::ResolutionList);

FYJJOptions& FYJJOptions::Get()
{
	static FYJJOptions Instance;
	if (!Instance.bLoaded)
	{
		Instance.Load();
	}
	return Instance;
}

TConstArrayView<FIntPoint> FYJJOptions::Resolutions()
{
	return MakeArrayView(ResolutionList, NumResolutions);
}

void FYJJOptions::Load()
{
	bLoaded = true;
	if (const UYJJOptionsSave* Saved = Cast<UYJJOptionsSave>(UGameplayStatics::LoadGameFromSlot(Slot, UserIndex)))
	{
		Music = Saved->Music;
		Sfx = Saved->Sfx;
		Voice = Saved->Voice;
		Shake = Saved->Shake;
		Language = Saved->Language;
		Resolution = Saved->Resolution;
	}
}

void FYJJOptions::Save() const
{
	UYJJOptionsSave* Saved = Cast<UYJJOptionsSave>(UGameplayStatics::CreateSaveGameObject(UYJJOptionsSave::StaticClass()));
	Saved->Music = Music;
	Saved->Sfx = Sfx;
	Saved->Voice = Voice;
	Saved->Shake = Shake;
	Saved->Language = Language;
	Saved->Resolution = Resolution;
	UGameplayStatics::SaveGameToSlot(Saved, Slot, UserIndex);
}

void FYJJOptions::Apply() const
{
	if (!Language.IsEmpty())
	{
		YJJLoc::SetLanguage(Language);
	}
	if (Resolutions().IsValidIndex(Resolution) && GEngine)
	{
		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			Settings->SetScreenResolution(Resolutions()[Resolution]);
			Settings->SetFullscreenMode(EWindowMode::Windowed);
			Settings->ApplyResolutionSettings(false);
			Settings->SaveSettings();
		}
	}
	FYJJAudio::RefreshMusicVolume();
}
