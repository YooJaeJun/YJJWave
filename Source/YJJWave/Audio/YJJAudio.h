#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UAudioComponent;
class USoundBase;

/** 소리 종류 (옵션 볼륨이 따로) */
enum class EYJJSoundCategory : uint8
{
	Music,
	Sfx,
	Voice,
	Count
};

/** Sounds.csv 한 줄 */
struct FYJJSoundDef
{
	TArray<FString> Sounds;   // 여러 개면 무작위
	float Volume = 1.f;
	EYJJSoundCategory Category = EYJJSoundCategory::Sfx;
	float Cooldown = 0.f;     // 같은 키를 이 시간 안에 다시 틀지 않는다 (수천 마리 타격음이 겹치지 않게)
	float PitchMin = 1.f;
	float PitchMax = 1.f;
};

/**
 * 소리 재생. 키(Hit, Voice_Grunt, Music_Battle …)는 Content/YJJWave/Data/Sounds.csv,
 * 소리 파일은 Tools/Audio/generate_audio.py 가 합성해 /Game/YJJWave/Audio 로 임포트한 것.
 * 볼륨 = 표의 Volume x 옵션(배경음·효과음·보이스).
 */
class YJJWAVE_API FYJJAudio
{
public:
	static void Play(const UObject* WorldContext, FName Key);
	/** 배경음을 바꾼다 (같으면 그대로). NAME_None이면 끈다 */
	static void PlayMusic(const UObject* WorldContext, FName Key);
	/** 옵션 볼륨이 바뀌었을 때 지금 배경음에 반영 */
	static void RefreshMusicVolume();
	static void Reload();

private:
	static const FYJJSoundDef* Find(FName Key);
	static USoundBase* LoadSound(const FString& Name);
	static float CategoryVolume(EYJJSoundCategory Category);

	static TMap<FName, FYJJSoundDef> Defs;
	static TMap<FName, double> LastPlayed;
	static TArray<double> RecentVoices;
	static TWeakObjectPtr<UAudioComponent> Music;
	static FName MusicKey;
	static bool bLoaded;
	static TSet<FString> Missing;

	static constexpr const TCHAR* AudioFolder = TEXT("/Game/YJJWave/Audio");
	static constexpr const TCHAR* TableFile = TEXT("YJJWave/Data/Sounds.csv");
	static constexpr int32 MinColumns = 7;
	static constexpr double VoiceWindow = 1.0;
};
