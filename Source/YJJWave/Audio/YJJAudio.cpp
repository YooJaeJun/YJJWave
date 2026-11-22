#include "Audio/YJJAudio.h"
#include "Components/AudioComponent.h"
#include "Data/YJJGameData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Meta/YJJOptions.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Sound/SoundBase.h"
#include "YJJWave.h"

TMap<FName, FYJJSoundDef> FYJJAudio::Defs;
TMap<FName, double> FYJJAudio::LastPlayed;
TArray<double> FYJJAudio::RecentVoices;
TWeakObjectPtr<UAudioComponent> FYJJAudio::Music;
FName FYJJAudio::MusicKey;
bool FYJJAudio::bLoaded = false;
TSet<FString> FYJJAudio::Missing;

void FYJJAudio::Reload()
{
	bLoaded = true;
	Defs.Reset();
	FString Csv;
	const FString Path = FPaths::ProjectContentDir() / TableFile;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogYJJWave, Error, TEXT("[Audio] %s 를 읽지 못했다"), *Path);
		return;
	}
	// 열: Name, Sounds, Volume, Category, Cooldown, PitchMin, PitchMax, Note
	const FCsvParser Parser(MoveTemp(Csv));
	const FCsvParser::FRows& Rows = Parser.GetRows();
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		if (Row.Num() < MinColumns || !*Row[0])
		{
			continue;
		}
		FYJJSoundDef& Def = Defs.Add(FName(Row[0]));
		FString(Row[1]).ParseIntoArrayWS(Def.Sounds);
		Def.Volume = FCString::Atof(Row[2]);
		const FString Category(Row[3]);
		Def.Category = Category == TEXT("Music") ? EYJJSoundCategory::Music : Category == TEXT("Voice") ? EYJJSoundCategory::Voice : EYJJSoundCategory::Sfx;
		Def.Cooldown = FCString::Atof(Row[4]);
		Def.PitchMin = FCString::Atof(Row[5]);
		Def.PitchMax = FCString::Atof(Row[6]);
	}
}

const FYJJSoundDef* FYJJAudio::Find(FName Key)
{
	if (!bLoaded)
	{
		Reload();
	}
	return Defs.Find(Key);
}

USoundBase* FYJJAudio::LoadSound(const FString& Name)
{
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), AudioFolder, *Name, *Name);
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sound && !Missing.Contains(Name))
	{
		Missing.Add(Name);
		UE_LOG(LogYJJWave, Warning, TEXT("[Audio] 소리 에셋이 없다: %s (Tools/Audio/BuildAudio.ps1)"), *Path);
	}
	return Sound;
}

float FYJJAudio::CategoryVolume(EYJJSoundCategory Category)
{
	const FYJJOptions& Options = FYJJOptions::Get();
	switch (Category)
	{
	case EYJJSoundCategory::Music: return Options.Music;
	case EYJJSoundCategory::Voice: return Options.Voice;
	default: return Options.Sfx;
	}
}

void FYJJAudio::Play(const UObject* WorldContext, FName Key)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const FYJJSoundDef* Def = Find(Key);
	if (!World || !Def || Def->Sounds.Num() == 0)
	{
		return;
	}
	const float Volume = Def->Volume * CategoryVolume(Def->Category);
	if (Volume <= 0.f)
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	if (const double* Last = LastPlayed.Find(Key); Last && Now >= *Last && Now - *Last < Def->Cooldown)
	{
		return;
	}
	// 보이스는 전체로도 초당 몇 번까지만 (수천 마리가 한꺼번에 떠들지 않게)
	if (Def->Category == EYJJSoundCategory::Voice)
	{
		RecentVoices.RemoveAll([Now](double Time) { return Now - Time > VoiceWindow || Now < Time; });
		if (RecentVoices.Num() >= YJJData::Tune().VoicesPerSecond)
		{
			return;
		}
		RecentVoices.Add(Now);
	}
	LastPlayed.Add(Key, Now);
	if (USoundBase* Sound = LoadSound(Def->Sounds[FMath::RandHelper(Def->Sounds.Num())]))
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, FMath::FRandRange(Def->PitchMin, Def->PitchMax));
	}
}

void FYJJAudio::PlayMusic(const UObject* WorldContext, FName Key)
{
	if (Key == MusicKey && Music.IsValid())
	{
		return;
	}
	const float Fade = YJJData::Tune().MusicFade;
	if (UAudioComponent* Old = Music.Get())
	{
		Old->FadeOut(Fade, 0.f);
	}
	Music.Reset();
	MusicKey = Key;
	const FYJJSoundDef* Def = Key.IsNone() ? nullptr : Find(Key);
	USoundBase* Sound = Def && Def->Sounds.Num() > 0 ? LoadSound(Def->Sounds[0]) : nullptr;
	if (!Sound)
	{
		return;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSound2D(WorldContext, Sound, Def->Volume * CategoryVolume(EYJJSoundCategory::Music));
	if (Component)
	{
		Component->FadeIn(Fade, Def->Volume * CategoryVolume(EYJJSoundCategory::Music));
	}
	Music = Component;
}

void FYJJAudio::RefreshMusicVolume()
{
	const FYJJSoundDef* Def = MusicKey.IsNone() ? nullptr : Find(MusicKey);
	if (UAudioComponent* Component = Music.Get(); Component && Def)
	{
		Component->SetVolumeMultiplier(Def->Volume * CategoryVolume(EYJJSoundCategory::Music));
	}
}
