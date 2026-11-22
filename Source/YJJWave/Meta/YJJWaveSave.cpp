#include "Meta/YJJWaveSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "YJJWave.h"

FString UYJJWaveSave::SlotName()
{
	FString Slot;
	return FParse::Value(FCommandLine::Get(), SlotSwitch, Slot) && !Slot.IsEmpty() ? Slot : FString(DefaultSlot);
}

FYJJProgress UYJJWaveSave::Load()
{
	FYJJProgress Progress;
	if (const UYJJWaveSave* Save = Cast<UYJJWaveSave>(UGameplayStatics::LoadGameFromSlot(SlotName(), UserIndex)))
	{
		Progress.Souls = Save->Souls;
		Progress.Perks = Save->Perks;
		Progress.Cleared = Save->Cleared;
	}
	UE_LOG(LogYJJWave, Display, TEXT("[Save] 불러옴 %s: 영혼 %.0f, 특성 %d개, 깬 스테이지 %d"), *SlotName(), Progress.Souls, Progress.Perks.Num(), Progress.Cleared);
	return Progress;
}

void UYJJWaveSave::Store(const FYJJProgress& Progress)
{
	UYJJWaveSave* Save = Cast<UYJJWaveSave>(UGameplayStatics::CreateSaveGameObject(UYJJWaveSave::StaticClass()));
	Save->Souls = Progress.Souls;
	Save->Perks = Progress.Perks;
	Save->Cleared = Progress.Cleared;
	UGameplayStatics::SaveGameToSlot(Save, SlotName(), UserIndex);
}
