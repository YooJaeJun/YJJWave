#include "Meta/YJJWaveSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "YJJWave.h"
#include "Meta/YJJGuard.h"

FString UYJJWaveSave::SlotName()
{
	FString Slot;
	return FParse::Value(FCommandLine::Get(), SlotSwitch, Slot) && !Slot.IsEmpty() ? Slot : FString(DefaultSlot);
}

FString UYJJWaveSave::SignedContent() const
{
	FString Content = FString::Printf(TEXT("%.6f|%d|"), Souls, Cleared);
	for (const FName& Perk : Perks)
	{
		Content += Perk.ToString() + TEXT(",");
	}
	Content += TEXT("|");
	for (const int32 Star : Stars)
	{
		Content += FString::FromInt(Star) + TEXT(",");
	}
	return Content;
}

FYJJProgress UYJJWaveSave::Load()
{
	FYJJProgress Progress;
	const UYJJWaveSave* Save = Cast<UYJJWaveSave>(UGameplayStatics::LoadGameFromSlot(SlotName(), UserIndex));
	// 서명이 없는 옛 저장(서명 도입 전)은 한 번 받아 주고 다음 저장 때 서명한다
	if (Save && !Save->Signature.IsEmpty() && Save->Signature != YJJGuard::Sign(Save->SignedContent()))
	{
		UE_LOG(LogYJJWave, Warning, TEXT("[Save] %s: 서명이 안 맞는다 (파일을 고쳤다). 처음부터 시작"), *SlotName());
		Save = nullptr;
	}
	if (Save)
	{
		Progress.Souls = Save->Souls;
		Progress.Perks = Save->Perks;
		Progress.Cleared = Save->Cleared;
		Progress.Stars = Save->Stars;
	}
	UE_LOG(LogYJJWave, Display, TEXT("[Save] 불러옴 %s: 영혼 %.0f, 특성 %d개, 깬 스테이지 %d"), *SlotName(), Progress.Souls, Progress.Perks.Num(), Progress.Cleared);
	return Progress;
}

void UYJJWaveSave::Store(const FYJJProgress& Progress)
{
	// 메모리 변조·스피드핵이 보인 판은 남기지 않는다
	if (YJJGuard::Tampered())
	{
		UE_LOG(LogYJJWave, Warning, TEXT("[Save] 변조가 감지돼 저장하지 않는다"));
		return;
	}
	UYJJWaveSave* Save = Cast<UYJJWaveSave>(UGameplayStatics::CreateSaveGameObject(UYJJWaveSave::StaticClass()));
	Save->Souls = Progress.Souls;
	Save->Perks = Progress.Perks;
	Save->Cleared = Progress.Cleared;
	Save->Stars = Progress.Stars;
	Save->Signature = YJJGuard::Sign(Save->SignedContent());
	UGameplayStatics::SaveGameToSlot(Save, SlotName(), UserIndex);
}
