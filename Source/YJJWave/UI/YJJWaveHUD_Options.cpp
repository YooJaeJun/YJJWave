// 옵션 화면: 사운드(배경음·효과음·보이스), 언어, 해상도. 바꾸는 즉시 적용·저장
#include "Audio/YJJAudio.h"
#include "Data/YJJLoc.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Meta/YJJOptions.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"

float* AYJJWaveHUD::OptionVolume(int32 Row)
{
	FYJJOptions& Options = FYJJOptions::Get();
	return Row == 0 ? &Options.Music : Row == 1 ? &Options.Sfx : &Options.Voice;
}

void AYJJWaveHUD::DrawOptions()
{
	const FYJJUILayout& L = UI();
	const FYJJOptions& Options = FYJJOptions::Get();
	const float Pad = L.PanelPad;
	const float RowH = L.OptionsRowH;
	const int32 NumRows = NumVolumeRows + OptionsExtraRows;
	const float W = L.OptionsW;
	const float H = Pad * 3.f + L.TitleText + RowH * NumRows + RowH;
	const float X = WorldCenterVirtualX() - W * Half;
	const float Y = (VirtualHeight - H) * Half;
	DrawBox(L.SidePanelW, 0.f, VirtualW - L.SidePanelW, VirtualHeight, YJJUI::Shade);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	DrawLabelCentered(YJJLoc::Get(TEXT("UI.OptionsTitle")), X + W * Half, Y + Pad + L.TitleText * Half, L.TitleText, YJJUI::Highlight);

	const float LabelX = X + Pad * 2.f;
	const float ControlX = X + Pad * 2.f + L.OptionsLabelW;
	const float ControlW = X + W - Pad * 2.f - ControlX;
	const float Button = L.OptionsButtonW;
	float RowY = Y + Pad * 2.f + L.TitleText;
	auto RowCenter = [&]() { return RowY + RowH * Half; };
	auto SmallButton = [&](float BX, const FString& Text, int32 Index, bool bSelected)
	{
		const float BH = RowH - Pad;
		DrawPanel(BX, RowY + Pad * Half, Button, BH, bSelected ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabelCentered(Text, BX + Button * Half, RowCenter(), L.MidText, YJJUI::Paper, Button - Pad * Half);
		AddButton(EButton::Option, Index, BX, RowY + Pad * Half, Button, BH);
	};

	// 볼륨 3줄: [-] 막대 [+]
	for (int32 Row = 0; Row < NumVolumeRows; ++Row)
	{
		const float Value = *OptionVolume(Row);
		DrawLabelLeftMiddle(YJJLoc::Get(VolumeKeys[Row]), LabelX, RowCenter(), L.MidText, YJJUI::Paper, 0.f);
		SmallButton(ControlX, YJJLoc::Get(TEXT("Option.Minus")), OptionVolumeDown + Row, false);
		const float BarX = ControlX + Button + Pad;
		const float BarW = ControlW - (Button + Pad) * 2.f;
		const float BarY = RowCenter() - L.TextBarH * Half;
		DrawBar(BarX, BarY, BarW, L.TextBarH, Value, YJJUI::HeroBar);
		DrawBarLabel(YJJLoc::Format(TEXT("UI.Percent"), { FMath::RoundToInt(Value * Percent) }), BarX, BarY, BarW, L.TextBarH);
		SmallButton(ControlX + ControlW - Button, YJJLoc::Get(TEXT("Option.Plus")), OptionVolumeUp + Row, false);
		RowY += RowH;
	}

	// 언어: 버튼 하나씩
	DrawLabelLeftMiddle(YJJLoc::Get(TEXT("Option.Language")), LabelX, RowCenter(), L.MidText, YJJUI::Paper, 0.f);
	{
		const float LangW = (ControlW - Pad) * Half;
		for (int32 Index = 0; Index < NumLanguages; ++Index)
		{
			const FString Code = LanguageCodes[Index];
			const bool bSelected = YJJLoc::Language() == Code;
			const float BX = ControlX + Index * (LangW + Pad);
			const float BH = RowH - Pad;
			DrawPanel(BX, RowY + Pad * Half, LangW, BH, bSelected ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
			DrawLabelCentered(YJJLoc::Get(*(FString(TEXT("Lang.")) + Code)), BX + LangW * Half, RowCenter(), L.MidText, YJJUI::Paper, LangW - Pad);
			AddButton(EButton::Option, OptionLanguage + Index, BX, RowY + Pad * Half, LangW, BH);
		}
		RowY += RowH;
	}

	// 해상도: ◀ 1920 x 1080 ▶
	DrawLabelLeftMiddle(YJJLoc::Get(TEXT("Option.Resolution")), LabelX, RowCenter(), L.MidText, YJJUI::Paper, 0.f);
	{
		SmallButton(ControlX, YJJLoc::Get(TEXT("Option.Prev")), OptionResolutionPrev, false);
		const FIntPoint Size = FYJJOptions::Resolutions().IsValidIndex(Options.Resolution)
			? FYJJOptions::Resolutions()[Options.Resolution]
			: FIntPoint(FMath::RoundToInt(ScreenSize.X), FMath::RoundToInt(ScreenSize.Y));
		DrawLabelCentered(YJJLoc::Format(TEXT("UI.Resolution"), { Size.X, Size.Y }), ControlX + ControlW * Half, RowCenter(), L.MidText, YJJUI::Paper);
		SmallButton(ControlX + ControlW - Button, YJJLoc::Get(TEXT("Option.Next")), OptionResolutionNext, false);
		RowY += RowH;
	}

	// 닫기
	const float CloseW = L.OptionsCloseW;
	const float CloseX = X + (W - CloseW) * Half;
	DrawPanel(CloseX, RowY + Pad * Half, CloseW, RowH - Pad, YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
	DrawLabelCentered(YJJLoc::Get(TEXT("Option.Close")), CloseX + CloseW * Half, RowCenter(), L.MidText, YJJUI::Paper, CloseW - Pad);
	AddButton(EButton::Option, OptionClose, CloseX, RowY + Pad * Half, CloseW, RowH - Pad);
}

void AYJJWaveHUD::HandleOption(int32 Index)
{
	FYJJOptions& Options = FYJJOptions::Get();
	const float Step = UI().OptionsVolumeStep;
	if (Index >= OptionVolumeDown && Index < OptionVolumeDown + NumVolumeRows)
	{
		float* Value = OptionVolume(Index - OptionVolumeDown);
		*Value = FMath::Clamp(FMath::RoundToFloat((*Value - Step) / Step) * Step, 0.f, 1.f);
	}
	else if (Index >= OptionVolumeUp && Index < OptionVolumeUp + NumVolumeRows)
	{
		float* Value = OptionVolume(Index - OptionVolumeUp);
		*Value = FMath::Clamp(FMath::RoundToFloat((*Value + Step) / Step) * Step, 0.f, 1.f);
	}
	else if (Index >= OptionLanguage && Index < OptionLanguage + NumLanguages)
	{
		Options.Language = LanguageCodes[Index - OptionLanguage];
	}
	else if (Index == OptionResolutionPrev || Index == OptionResolutionNext)
	{
		const int32 Count = FYJJOptions::Resolutions().Num();
		int32 Current = Options.Resolution;
		if (!FYJJOptions::Resolutions().IsValidIndex(Current))
		{
			// 지금 창 크기에 가장 가까운 것부터
			Current = 0;
			for (int32 Candidate = 0; Candidate < Count; ++Candidate)
			{
				Current = FYJJOptions::Resolutions()[Candidate].Y <= ScreenSize.Y ? Candidate : Current;
			}
		}
		Options.Resolution = (Current + (Index == OptionResolutionNext ? 1 : Count - 1)) % Count;
	}
	else if (Index == OptionClose)
	{
		bOptionsOpen = false;
		return;
	}
	Options.Save();
	Options.Apply();
}
