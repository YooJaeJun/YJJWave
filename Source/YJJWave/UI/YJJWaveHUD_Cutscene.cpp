// 스테이지 시작 컷씬: 위아래 검은 띠, 초상화, 화자 이름, 타자 치듯 나오는 대사, 카메라 이동
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Data/YJJLoc.h"
#include "Render/YJJSpriteRenderItem.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"

void AYJJWaveHUD::UpdateCutsceneCamera(float Dt)
{
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const FYJJCutLine* Line = Mode ? Mode->CutsceneLine() : nullptr;
	if (!Line)
	{
		return;
	}
	const float Blend = FMath::Min(UI().CutCameraFollow * Dt, 1.f);
	const float TargetZoom = Line->Zoom > 0.f ? Line->Zoom : Zoom;
	FocusWorld(CamCenter + (Line->Camera - CamCenter) * Blend, Zoom + (TargetZoom - Zoom) * Blend);
}

void AYJJWaveHUD::DrawCutscene()
{
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const FYJJCutLine* Line = Mode ? Mode->CutsceneLine() : nullptr;
	if (!Line)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const float Pad = L.PanelPad;
	const float Left = L.SidePanelW;
	const float Width = VirtualW - Left;

	// 영화 같은 위아래 띠
	DrawBox(Left, 0.f, Width, L.CutBarH, YJJUI::Ink);
	DrawBox(Left, VirtualHeight - L.CutBarH, Width, L.CutBarH, YJJUI::Ink);
	if (Line->bTitle)
	{
		DrawChapterCard(*Line, Mode->CutsceneTime());
		DrawLabel(YJJLoc::Get(TEXT("Cut.Hint")), VirtualW - Pad, VirtualHeight - L.CutBarH * Half - L.TinyText * Half, L.TinyText, YJJUI::Muted, 1.f);
		return;
	}

	// 대사 상자 (아래 띠 위)
	const float BoxW = FMath::Min(L.CutBoxW, Width - Pad * 4.f);
	const float BoxH = L.CutBoxH;
	const float BoxX = Left + (Width - BoxW) * Half;
	const float BoxY = VirtualHeight - L.CutBarH - BoxH - Pad;
	DrawPanel(BoxX, BoxY, BoxW, BoxH, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));

	// 초상화: 화자 유닛의 숨쉬기 그림
	const float Portrait = L.CutPortrait;
	const float PX = BoxX + Pad;
	const float PY = BoxY + BoxH - Portrait - Pad;
	if (Line->Speaker != INDEX_NONE)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Line->Speaker);
		const FYJJAtlas& Atlas = AtlasFor(Def);
		if (Atlas.Texture && Atlas.Texture->GetResource())
		{
			const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, Atlas.Texture, 1);
			const int32 Frame = FMath::RoundToInt(L.AnimIdleFirst) + FMath::FloorToInt(Mode->CutsceneTime() * L.AnimIdleFps) % FMath::Max(FMath::RoundToInt(L.AnimIdleFrames), 1);
			AddQuad(Batch, ViewOffset + FVector2f(PX, PY) * ViewScale, FVector2f(Portrait * ViewScale), Atlas.UV(Frame, RowFor(Def)), Atlas.CellUV, FLinearColor::White, Def.Team == EYJJTeam::Enemy);
		}
	}

	// 이름 + 대사 (글자가 하나씩 나온다)
	const float TextX = PX + Portrait + Pad;
	const float TextW = BoxX + BoxW - Pad - TextX;
	const FString Name = Line->Speaker != INDEX_NONE ? YJJLoc::Get(YJJData::Unit(Line->Speaker).NameKey) : FString();
	DrawLabel(Name, TextX, BoxY + Pad, L.MidText, Line->Speaker != INDEX_NONE && YJJData::Unit(Line->Speaker).Team == EYJJTeam::Enemy ? YJJUI::BossText : YJJUI::Highlight, 0.f, TextW);
	const FString& Full = YJJLoc::Get(Line->TextKey);
	const int32 Shown = FMath::Clamp(FMath::FloorToInt(Mode->CutsceneTime() * L.CutTypeSpeed), 0, Full.Len());
	DrawLabel(Full.Left(Shown), TextX, BoxY + Pad * 2.f + L.MidText, L.BigText, YJJUI::Paper, 0.f, TextW);
	DrawLabel(YJJLoc::Get(TEXT("Cut.Hint")), BoxX + BoxW - Pad, BoxY + BoxH - Pad - L.TinyText, L.TinyText, YJJUI::Muted, 1.f);
}

void AYJJWaveHUD::DrawChapterCard(const FYJJCutLine& Line, float Time)
{
	const FYJJUILayout& L = UI();
	const float Left = L.SidePanelW;
	const float Width = VirtualW - Left;
	const float CenterX = Left + Width * Half;
	// 제목이 먼저, 아래 줄은 조금 늦게 서서히
	const float TitleAlpha = FMath::Clamp(Time / FMath::Max(L.CutTitleFade, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float SubAlpha = FMath::Clamp((Time - L.CutSubDelay) / FMath::Max(L.CutTitleFade, KINDA_SMALL_NUMBER), 0.f, 1.f);
	DrawBox(Left, L.CutBarH, Width, VirtualHeight - L.CutBarH * 2.f, YJJUI::WithAlpha(YJJUI::Ink, L.CutTitleShade));

	TArray<FString> Lines;
	YJJLoc::Get(Line.TextKey).ParseIntoArrayLines(Lines);
	if (Lines.Num() == 0)
	{
		return;
	}
	const float TitleY = VirtualHeight * L.CutTitleY;
	DrawLabelCentered(Lines[0], CenterX, TitleY, L.TitleText * L.CutTitleScale, YJJUI::WithAlpha(YJJUI::Highlight, TitleAlpha), Width - L.TextPad * 2.f);
	// 제목 아래 가는 줄
	const float RuleW = Width * L.CutRuleW;
	const float RuleY = TitleY + L.TitleText * L.CutTitleScale * Half + L.CutSubGap;
	DrawBox(CenterX - RuleW * Half, RuleY, RuleW, L.CutRuleH, YJJUI::WithAlpha(YJJUI::Highlight, TitleAlpha * L.CutRuleAlpha));

	float Y = RuleY + L.CutSubGap + L.MidText * Half;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		for (const FString& Wrapped : WrapText(Lines[Index], L.MidText, Width * L.CutSubW))
		{
			DrawLabelCentered(Wrapped, CenterX, Y, L.MidText, YJJUI::WithAlpha(YJJUI::Paper, SubAlpha), Width - L.TextPad * 2.f);
			Y += L.TipLineH;
		}
	}
}
