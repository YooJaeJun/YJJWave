// 화면 UI (1920x1080 가상 좌표), 특성 성좌 화면, 마우스·키 처리
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"
#include "Audio/YJJAudio.h"

// ---------------------------------------------------------------------------------------------
// 그리기 도구

void AYJJWaveHUD::DrawBox(float X, float Y, float W, float H, const FLinearColor& Color)
{
	FCanvasTileItem Item(FVector2D(ViewOffset.X + X * ViewScale, ViewOffset.Y + Y * ViewScale), FVector2D(W * ViewScale, H * ViewScale), Color);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJWaveHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color, int32 Cell)
{
	if (!UiAtlas.Texture)
	{
		DrawBox(X, Y, W, H, Color);
		return;
	}
	// 9칸으로 나눠 모서리는 그대로, 가운데만 늘린다
	const FVector2f UV0 = UiAtlas.UV(Cell);
	// 아틀라스가 정사각이 아니므로(1024x256) 가로·세로 UV를 따로 계산한다
	const float CornerU = UiAtlas.CellUV.X * UI().PanelCornerFraction;
	const float CornerV = UiAtlas.CellUV.Y * UI().PanelCornerFraction;
	const float Corner = FMath::Min(UI().PanelCorner, FMath::Min(W, H) * Half);
	const float Xs[NineSlice] = { X, X + Corner, X + W - Corner };
	const float Ws[NineSlice] = { Corner, W - Corner * 2.f, Corner };
	const float Ys[NineSlice] = { Y, Y + Corner, Y + H - Corner };
	const float Hs[NineSlice] = { Corner, H - Corner * 2.f, Corner };
	const float Us[NineSlice] = { 0.f, CornerU, UiAtlas.CellUV.X - CornerU };
	const float UWs[NineSlice] = { CornerU, UiAtlas.CellUV.X - CornerU * 2.f, CornerU };
	const float Vs[NineSlice] = { 0.f, CornerV, UiAtlas.CellUV.Y - CornerV };
	const float VHs[NineSlice] = { CornerV, UiAtlas.CellUV.Y - CornerV * 2.f, CornerV };

	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, UiAtlas.Texture, NineSlice * NineSlice);
	for (int32 Row = 0; Row < NineSlice; ++Row)
	{
		for (int32 Column = 0; Column < NineSlice; ++Column)
		{
			if (Ws[Column] <= 0.f || Hs[Row] <= 0.f)
			{
				continue;
			}
			// 조각 사이에 틈이 보이지 않게 화면 픽셀에 맞춘다
			const FVector2f P0(FMath::RoundToFloat(ViewOffset.X + Xs[Column] * ViewScale), FMath::RoundToFloat(ViewOffset.Y + Ys[Row] * ViewScale));
			const FVector2f P1(FMath::RoundToFloat(ViewOffset.X + (Xs[Column] + Ws[Column]) * ViewScale), FMath::RoundToFloat(ViewOffset.Y + (Ys[Row] + Hs[Row]) * ViewScale));
			AddQuad(Batch, P0, P1 - P0, UV0 + FVector2f(Us[Column], Vs[Row]), FVector2f(UWs[Column], VHs[Row]), Color, false);
		}
	}
}

void AYJJWaveHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	const float Border = UI().BarBorder;
	DrawBox(X - Border, Y - Border, W + Border * 2.f, H + Border * 2.f, YJJUI::Ink);
	DrawBox(X, Y, W, H, YJJUI::BarBack);
	DrawBox(X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H, Color);
	// 윗부분 반짝 띠
	DrawBox(X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H * UI().BarShineHeight, YJJUI::WithAlpha(FLinearColor::White, UI().BarShineAlpha));
}

void AYJJWaveHUD::DrawBarLabel(const FString& Text, float X, float Y, float W, float H)
{
	// 게이지 높이에 맞춘 글자 크기, 세로 가운데. 흰 글자 + 진한 외곽선 (게이지 색과 겹치지 않게)
	DrawLabelCentered(Text, X + W * Half, Y + H * Half, H * UI().BarTextScale, YJJUI::Paper, W - UI().PanelPad);
}

void AYJJWaveHUD::DrawIcon(int32 Cell, float X, float Y, float Size, const FLinearColor& Color)
{
	if (!UiAtlas.Texture)
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, UiAtlas.Texture, 1);
	AddQuad(Batch, ViewOffset + FVector2f(X, Y) * ViewScale, FVector2f(Size * ViewScale), UiAtlas.UV(Cell), UiAtlas.CellUV, Color, false);
}

FSlateFontInfo AYJJWaveHUD::MakeFont(const FString& Text, float Size, float Alpha, float MaxWidth, float& OutWidth)
{
	const int32 FontSize = FMath::Max(FMath::RoundToInt(Size * ViewScale * UI().FontPointScale), FMath::RoundToInt(UI().MinFontSize));
	const TSharedPtr<const FCompositeFont> Composite = GetGameFont();
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(BaseFont, FontSize);
	Font.OutlineSettings.OutlineSize = FMath::Max(FMath::RoundToInt(Size * UI().TextOutline * ViewScale), 1);
	Font.OutlineSettings.OutlineColor = YJJUI::WithAlpha(YJJUI::Ink, Alpha);

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	OutWidth = Measure->Measure(Text, Font).X / ViewScale;
	if (MaxWidth > 0.f && OutWidth > MaxWidth)
	{
		Font.Size = FMath::Max(FMath::FloorToFloat(Font.Size * MaxWidth / OutWidth), UI().MinFontSize);
		OutWidth = Measure->Measure(Text, Font).X / ViewScale;
	}
	return Font;
}

void AYJJWaveHUD::DrawText(const FString& Text, const FSlateFontInfo& Font, float Left, float Top, const FLinearColor& Color)
{
	// FCanvasTextItem은 UFont가 있어야 그려진다. 실제 글꼴은 게임 글꼴 컴포지트
	FCanvasTextItem Item(FVector2D(ViewOffset.X + Left * ViewScale, ViewOffset.Y + Top * ViewScale), FText::FromString(Text), Font, Color);
	Item.Font = BaseFont;
	Item.SlateFontInfo = Font;
	Canvas->DrawItem(Item);
}

TArray<FString> AYJJWaveHUD::WrapText(const FString& Text, float Size, float MaxWidth)
{
	TArray<FString> Out;
	TArray<FString> Paragraphs;
	Text.ParseIntoArrayLines(Paragraphs);
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			float Width = 0.f;
			MakeFont(Candidate, Size, 1.f, 0.f, Width);
			if (Width > MaxWidth && !Line.IsEmpty())
			{
				Out.Add(Line);
				Line = Word;
			}
			else
			{
				Line = Candidate;
			}
		}
		if (!Line.IsEmpty())
		{
			Out.Add(Line);
		}
	}
	return Out;
}

void AYJJWaveHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float AlignX, float MaxWidth)
{
	if (!BaseFont || Text.IsEmpty() || Color.A <= 0.f)
	{
		return;
	}
	float Width = 0.f;
	const FSlateFontInfo Font = MakeFont(Text, Size, Color.A, MaxWidth, Width);
	DrawText(Text, Font, X - Width * AlignX, Y, Color);
}

bool AYJJWaveHUD::HoverButton(EButton Kind) const
{
	const FYJJButton* Button = ButtonAt(ToVirtual(MouseScreen()));
	return Button && Button->Kind == static_cast<int32>(Kind);
}

bool AYJJWaveHUD::StartsNumber(const FString& Text, int32 Index)
{
	if (FChar::IsDigit(Text[Index]))
	{
		return true;
	}
	const bool bSign = Text[Index] == TCHAR('+') || Text[Index] == TCHAR('-') || Text[Index] == TCHAR('x');
	return bSign && Text.IsValidIndex(Index + 1) && FChar::IsDigit(Text[Index + 1]) && (Index == 0 || !FChar::IsAlpha(Text[Index - 1]));
}

float AYJJWaveHUD::MeasureWith(const FString& Text, const FSlateFontInfo& Font) const
{
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font).X / ViewScale;
}

void AYJJWaveHUD::DrawValueLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth)
{
	if (!BaseFont || Text.IsEmpty() || Color.A <= 0.f)
	{
		return;
	}
	// 글 전체로 크기를 정한 뒤 (칸을 넘으면 줄인다), 숫자 조각과 나머지를 번갈아 이어 그린다
	float Width = 0.f;
	const FSlateFontInfo Font = MakeFont(Text, Size, Color.A, MaxWidth, Width);
	const FLinearColor ValueColor = YJJUI::WithAlpha(YJJUI::Value, Color.A);
	float Left = X;
	int32 Start = 0;
	while (Start < Text.Len())
	{
		const bool bNumber = StartsNumber(Text, Start);
		int32 End = Start + 1;
		if (bNumber)
		{
			// 숫자, 소수점·쉼표, 끝의 % 까지
			while (End < Text.Len() && (FChar::IsDigit(Text[End]) || ((Text[End] == TCHAR('.') || Text[End] == TCHAR(',')) && Text.IsValidIndex(End + 1) && FChar::IsDigit(Text[End + 1]))))
			{
				++End;
			}
			if (End < Text.Len() && Text[End] == TCHAR('%'))
			{
				++End;
			}
		}
		else
		{
			while (End < Text.Len() && !StartsNumber(Text, End))
			{
				++End;
			}
		}
		const FString Piece = Text.Mid(Start, End - Start);
		DrawText(Piece, Font, Left, Y, bNumber ? ValueColor : Color);
		Left += MeasureWith(Piece, Font);
		Start = End;
	}
}

void AYJJWaveHUD::DrawLabelLeftMiddle(const FString& Text, float X, float CenterY, float Size, const FLinearColor& Color, float AlignX)
{
	// 가로는 AlignX(0 왼쪽, 1 오른쪽), 세로는 CenterY에 글자 몸통 가운데
	float Width = 0.f;
	if (!BaseFont || Text.IsEmpty())
	{
		return;
	}
	MakeFont(Text, Size, Color.A, 0.f, Width);
	DrawLabelCentered(Text, X - Width * AlignX + Width * Half, CenterY, Size, Color, 0.f);
}

void AYJJWaveHUD::DrawLabelCentered(const FString& Text, float CenterX, float CenterY, float Size, const FLinearColor& Color, float MaxWidth)
{
	if (!BaseFont || Text.IsEmpty() || Color.A <= 0.f)
	{
		return;
	}
	float Width = 0.f;
	const FSlateFontInfo Font = MakeFont(Text, Size, Color.A, MaxWidth, Width);
	// 줄 상자 위 → 기준선 = 최대 글자 높이 + 기준선(아래로 음수). 글자 몸통 가운데 = 기준선 - 글자 높이 / 2
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float BaselinePx = static_cast<float>(Measure->GetMaxCharacterHeight(Font) + Measure->GetBaseline(Font));
	const float GlyphPx = Font.Size * UI().TextCapPerPoint;
	const float Top = CenterY - (BaselinePx - GlyphPx * Half) / ViewScale;
	DrawText(Text, Font, CenterX - Width * Half, Top, Color);
}

void AYJJWaveHUD::DrawLine(const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness)
{
	FCanvasLineItem Item(FVector2D(ViewOffset.X + A.X * ViewScale, ViewOffset.Y + A.Y * ViewScale), FVector2D(ViewOffset.X + B.X * ViewScale, ViewOffset.Y + B.Y * ViewScale));
	Item.SetColor(Color);
	Item.LineThickness = FMath::Max(Thickness * ViewScale, 1.f);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJWaveHUD::AddButton(EButton Kind, int32 Index, float X, float Y, float W, float H)
{
	FYJJButton& Button = Buttons.AddDefaulted_GetRef();
	Button.Rect = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H));
	Button.Kind = static_cast<int32>(Kind);
	Button.Index = Index;
}

const FYJJButton* AYJJWaveHUD::ButtonAt(const FVector2f& Virtual) const
{
	// 나중에 그린 것(팝업)이 위에 있으므로 뒤에서부터 찾는다
	for (int32 Index = Buttons.Num() - 1; Index >= 0; --Index)
	{
		if (Buttons[Index].Rect.IsInside(Virtual))
		{
			return &Buttons[Index];
		}
	}
	return nullptr;
}

bool AYJJWaveHUD::IsMouseOverUI() const
{
	const FVector2f Virtual = ToVirtual(MouseScreen());
	return ButtonAt(Virtual) != nullptr || Virtual.X < UI().SidePanelW;
}

FString AYJJWaveHUD::FormatCount(double Value)
{
	const FYJJUILayout& L = YJJData::Layout();
	if (Value >= L.NumberEok)
	{
		return YJJLoc::Format(TEXT("Num.Eok"), { FString::Printf(TEXT("%.2f"), Value / L.NumberEok) });
	}
	if (Value >= L.NumberMan)
	{
		return YJJLoc::Format(TEXT("Num.Man"), { FString::Printf(TEXT("%.1f"), Value / L.NumberMan) });
	}
	return FText::AsNumber(FMath::FloorToInt64(Value)).ToString();
}

void AYJJWaveHUD::DrawPlotTooltip()
{
	const FYJJBattle* Current = Battle();
	if (HoverPlot == INDEX_NONE || Current->bGameOver || Current->IsChoosing())
	{
		return;
	}
	const FYJJPlot& Plot = Current->Plots[HoverPlot];
	TArray<TPair<FString, FLinearColor>> Lines;
	if (Plot.Building == INDEX_NONE)
	{
		const FYJJBuildingDef& Def = YJJData::Building(SelectedBuilding);
		Lines.Emplace(YJJLoc::Format(TEXT("UI.PlotBuild"), { YJJLoc::Get(Def.NameKey) }), YJJUI::Paper);
		Lines.Emplace(YJJLoc::Get(Def.DescKey), YJJUI::Muted);
		const int32 Cost = Current->BuildCost(SelectedBuilding);
		Lines.Emplace(YJJLoc::Format(TEXT("UI.Cost"), { FormatCount(Cost) }), Current->Gold >= Cost ? YJJUI::Gold : YJJUI::Bad);
	}
	else
	{
		const FYJJBuildingDef& Def = YJJData::Building(Plot.Building);
		Lines.Emplace(YJJLoc::Format(TEXT("UI.PlotTitle"), { YJJLoc::Get(Def.NameKey), Plot.Level }), YJJUI::Paper);
		Lines.Emplace(YJJLoc::Format(TEXT("UI.PlotSpawn"), { Current->SpawnCount(HoverPlot), FString::Printf(TEXT("%.1f"), Current->SpawnInterval(HoverPlot)) }), YJJUI::Muted);
		const int32 Cost = Current->UpgradeCost(HoverPlot);
		if (Cost > 0)
		{
			Lines.Emplace(YJJLoc::Format(TEXT("UI.PlotUpgrade"), { FormatCount(Cost) }), Current->Gold >= Cost ? YJJUI::Gold : YJJUI::Bad);
		}
		else
		{
			Lines.Emplace(YJJLoc::Get(TEXT("UI.PlotMax")), YJJUI::Highlight);
		}
	}

	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float Pad = UI().TextPad;
	const float LineH = UI().TipLineH;
	const float W = UI().TipW;
	const float H = Pad * 2.f + LineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + UI().TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H * Half, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawValueLabel(Lines[Line].Key, X + Pad, Y + Pad + LineH * Line, UI().SmallText, Lines[Line].Value, W - Pad * 2.f);
	}
}

float AYJJWaveHUD::ResultStep(float Start) const
{
	return FMath::Clamp((ResultAge - Start) / FMath::Max(UI().ResultStepTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
}

void AYJJWaveHUD::DrawGameOver()
{
	const FYJJBattle* Current = Battle();
	if (!Current->bGameOver)
	{
		return;
	}
	// 결과 창: 창이 커지며 나타난 뒤 위에서부터 차례로 — 제목 → 빈 별 → 별이 하나씩 채워짐 → 기록 → 버튼
	const FYJJUILayout& L = UI();
	const float Pop = FMath::Clamp(ResultAge / FMath::Max(L.ResultPopTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Scale = FMath::Lerp(L.ResultPopStart, 1.f, 1.f - FMath::Square(1.f - Pop)) + FMath::Sin(Pop * UE_PI) * L.ResultPopOvershoot;
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::WithAlpha(YJJUI::Shade, Pop));
	const bool bWon = Current->bVictory;
	const float W = L.GameOverW * Scale;
	const float H = L.ResultWinH * Scale;
	const float X = WorldCenterVirtualX() - W * Half;
	const float Y = (VirtualHeight - H) * Half;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	if (Pop < 1.f)
	{
		return;
	}
	const float Pad = L.PanelPad;
	const float Slide = L.ResultSlide;
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const float Step = L.ResultStepTime;
	float Clock = L.ResultPopTime;
	float RowY = Y + Pad * 2.f;

	// 제목: 위에서 툭 떨어진다
	const float TitleT = ResultStep(Clock);
	DrawLabel(YJJLoc::Get(bWon ? TEXT("UI.Victory") : TEXT("UI.GameOver")), X + W * Half, RowY - (1.f - TitleT) * Slide, L.TitleText,
		YJJUI::WithAlpha(bWon ? YJJUI::Highlight : YJJUI::Bad, TitleT), Half);
	RowY += L.TitleText + Pad;
	Clock += Step;

	// 별: 빈 별이 먼저 놓이고, 얻은 만큼 하나씩 채워진다 (지면 빈 별 셋)
	DrawResultStars(X + W * Half, RowY, Clock, bWon ? Current->Stars : 0);
	RowY += L.ResultStarSize + Pad;
	Clock += Step + (bWon ? Current->Stars : 0) * L.ResultStarDelay + L.ResultStarPopTime;

	// 기록: 아래에서 올라온다
	const float StatsT = ResultStep(Clock);
	DrawValueLabel(YJJLoc::Format(TEXT("UI.GameOverStats"), { YJJLoc::Get(Current->StageDef().NameKey), FormatCount(Current->Kills), FMath::RoundToInt(Current->Time), FormatCount(Current->SoulsEarned) }),
		X + Pad * 2.f, RowY + (1.f - StatsT) * Slide, L.MidText, YJJUI::WithAlpha(YJJUI::Paper, StatsT), W - Pad * 4.f);
	Clock += Step;

	// 버튼: 다 나온 뒤에야 누를 수 있다
	const float ButtonsT = ResultStep(Clock);
	if (ButtonsT <= 0.f)
	{
		return;
	}
	const bool bNext = Mode && Mode->HasNextStage();
	const int32 NumButtons = bNext ? 3 : 2;
	const float ButtonW = (W - Pad * 2.f - L.CardGap * (NumButtons - 1)) / NumButtons;
	const float ButtonY = Y + H - Pad - L.ResultButtonH + (1.f - ButtonsT) * Slide;
	const EButton Kinds[] = { EButton::ResultMap, EButton::Restart, EButton::NextStage };
	const TCHAR* Keys[] = { TEXT("UI.WorldMap"), TEXT("UI.Restart"), TEXT("UI.NextStage") };
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		const float BX = X + Pad + Index * (ButtonW + L.CardGap);
		DrawMenuButton(Kinds[Index], 0, BX, ButtonY, ButtonW, L.ResultButtonH, YJJLoc::Get(Keys[Index]), Index == NumButtons - 1 && bNext);
	}
}

void AYJJWaveHUD::DrawResultStars(float CenterX, float Y, float Start, int32 Stars)
{
	const FYJJUILayout& L = UI();
	const int32 MaxStars = FMath::RoundToInt(L.ResultStarCount);
	const float Size = L.ResultStarSize;
	const float Gap = L.ResultStarGap;
	const float Left = CenterX - (MaxStars * Size + (MaxStars - 1) * Gap) * Half;
	const float EmptyT = ResultStep(Start);
	for (int32 Index = 0; Index < MaxStars; ++Index)
	{
		const float X = Left + Index * (Size + Gap);
		// 빈 별 자리 (먼저 나타난다)
		DrawIcon(FMath::RoundToInt(L.UiStar), X, Y, Size, YJJUI::WithAlpha(YJJUI::Ink, L.ResultStarEmptyAlpha * EmptyT));
		const float Local = ResultAge - (Start + L.ResultStepTime + Index * L.ResultStarDelay);
		if (Index >= Stars || Local <= 0.f)
		{
			continue;
		}
		if (StarsSounded <= Index)
		{
			StarsSounded = Index + 1;
			FYJJAudio::Play(this, SoundStar);
		}
		// 빈 별 안에서 금별이 커지며 차오른다 (살짝 넘쳤다 제자리)
		const float T = FMath::Clamp(Local / FMath::Max(L.ResultStarPopTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float Grow = T + FMath::Sin(T * UE_PI) * (L.ResultStarPopScale - 1.f);
		const float Drawn = Size * Grow;
		DrawIcon(FMath::RoundToInt(L.UiStar), X + (Size - Drawn) * Half, Y + (Size - Drawn) * Half, Drawn, YJJUI::PerkTaken);
	}
}

void AYJJWaveHUD::DrawDebugLine()
{
	if (!bShowDebug)
	{
		return;
	}
	// 좌측 패널 맨 아래 두 줄
	const FYJJHordeView& Horde = View();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const int32 Fps = FMath::RoundToInt(1000.f / FMath::Max(FrameMs, KINDA_SMALL_NUMBER));
	const float X = UI().PanelPad;
	const float W = UI().SidePanelW - UI().PanelPad * 2.f;
	const float Bottom = VirtualHeight - UI().PanelPad;
	DrawLabel(YJJLoc::Format(TEXT("UI.Debug1"), { Horde.Num(EYJJTeam::Player) + Horde.Num(EYJJTeam::Enemy), Fps, Horde.Projectiles.Num() }),
		X, Bottom - UI().TinyText * 2.f - UI().DebugLineGap, UI().TinyText, YJJUI::Muted, 0.f, W);
	DrawLabel(YJJLoc::Format(TEXT("UI.Debug2"), {
			FString::Printf(TEXT("%.1f"), Horde.StepMs),
			FString::Printf(TEXT("%.1f"), UnitDrawMs),
			FString::Printf(TEXT("%.1f"), Mode ? Mode->LastSimWaitMs() : 0.f) }),
		X, Bottom - UI().TinyText, UI().TinyText, YJJUI::Muted, 0.f, W);
}

// ---------------------------------------------------------------------------------------------
// 특성 성좌

FVector2f AYJJWaveHUD::PerkToVirtual(const FVector2f& PerkPos) const
{
	return FVector2f(WorldCenterVirtualX(), UI().TreeCenterY) + PerkPos * UI().TreeScale;
}

int32 AYJJWaveHUD::PerkAt(const FVector2f& Virtual) const
{
	const TConstArrayView<FYJJPerkDef> Perks = YJJData::Perks();
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		const float Radius = PerkRadius(Perks[Perk].Kind);
		if (FVector2f::DistSquared(PerkToVirtual(Perks[Perk].Pos), Virtual) <= Radius * Radius)
		{
			return Perk;
		}
	}
	return INDEX_NONE;
}

FString AYJJWaveHUD::PerkText(int32 Perk) const
{
	return EffectText(YJJData::Perks()[Perk]);
}

FString AYJJWaveHUD::EffectText(const FYJJPerkDef& Def) const
{
	TArray<FString> Lines;
	for (const FYJJPerkDef::FMod& Mod : Def.Mods)
	{
		const FString Target = Mod.Building == INDEX_NONE ? YJJLoc::Get(TEXT("Target.All")) : YJJLoc::Get(YJJData::Building(Mod.Building).NameKey);
		// 쐐기는 깎이는 값도 있어 부호를 그대로 붙인다
		const FString Value = YJJData::IsFlatStat(Mod.Stat)
			? FString::Printf(TEXT("%+d"), FMath::RoundToInt(Mod.Value))
			: FString::Printf(TEXT("%+d%%"), FMath::RoundToInt(Mod.Value * Percent));
		Lines.Add(YJJLoc::Format(*(FString(TEXT("Stat.")) + YJJData::StatKey(Mod.Stat)), { Value, Target }));
	}
	const FYJJPerkDef::FSpecial& Special = Def.Special;
	if (Special.Kind != EYJJSpecial::None)
	{
		const FString Target = Special.Target == INDEX_NONE ? YJJLoc::Get(TEXT("Target.All"))
			: Special.Target == FYJJPerkDef::HeroTarget ? YJJLoc::Get(YJJData::Unit(YJJData::HeroUnit()).NameKey)
			: YJJLoc::Get(YJJData::Unit(YJJData::Building(Special.Target).Unit).NameKey);
		const FString A = FString::Printf(TEXT("%d"), FMath::RoundToInt(Special.A * Percent));
		const FString B = FString::Printf(TEXT("%d"), FMath::RoundToInt(Special.B * Percent));
		const FString RawA = FString::Printf(TEXT("%g"), Special.A);
		const FString RawB = FString::Printf(TEXT("%g"), Special.B);
		// 서식 자리: {0} 대상, {1} A%, {2} B%, {3} A 그대로, {4} B 그대로
		Lines.Add(YJJLoc::Format(*(FString(TEXT("Special.")) + YJJData::SpecialKey(Special.Kind)), { Target, A, B, RawA, RawB }));
	}
	return FString::Join(Lines, TEXT("\n"));
}

float AYJJWaveHUD::PerkRadius(EYJJPerkKind Kind) const
{
	switch (Kind)
	{
	case EYJJPerkKind::Small: return UI().PerkSmall;
	case EYJJPerkKind::Keystone: return UI().PerkKeystone;
	default: return UI().PerkBig;
	}
}

void AYJJWaveHUD::DrawTree()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJPerkDef> Perks = YJJData::Perks();
	DrawBox(UI().SidePanelW, 0.f, VirtualW - UI().SidePanelW, VirtualHeight, YJJUI::TreeBack);

	// 선
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		for (const int32 Link : Perks[Perk].Links)
		{
			if (Link < Perk)
			{
				continue;
			}
			const bool bBoth = Current->IsAllocated(Perk) && Current->IsAllocated(Link);
			const bool bOne = Current->IsAllocated(Perk) || Current->IsAllocated(Link);
			const FLinearColor Color = bBoth ? YJJUI::LinkTaken : (bOne ? YJJUI::PerkReach : YJJUI::LinkIdle);
			DrawLine(PerkToVirtual(Perks[Perk].Pos), PerkToVirtual(Perks[Link].Pos), Color, bBoth ? UI().LinkTakenWidth : UI().LinkWidth);
		}
	}

	// 별
	const FVector2f Mouse = ToVirtual(MouseScreen());
	HoverPerk = ForcedHoverPerk != INDEX_NONE ? ForcedHoverPerk : PerkAt(Mouse);
	const float Pulse = Half + Half * FMath::Sin(GetWorld()->GetRealTimeSeconds() * UI().GlowSpeed);
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		const FYJJPerkDef& Def = Perks[Perk];
		const bool bSmall = Def.Kind == EYJJPerkKind::Small;
		float Radius = PerkRadius(Def.Kind);
		FLinearColor Color = Def.Kind == EYJJPerkKind::Keystone ? YJJUI::PerkKeystone : YJJUI::PerkLocked;
		if (Current->IsAllocated(Perk))
		{
			Color = YJJUI::PerkTaken;
		}
		if (Current->IsReachable(Perk))
		{
			const FLinearColor Base = Current->IsAllocated(Perk) ? YJJUI::PerkTaken : YJJUI::PerkReach;
			Color = Current->CanAllocate(Perk) ? FMath::Lerp(Base, FLinearColor::White, Pulse) : Base;
		}
		if (Perk == HoverPerk)
		{
			Radius *= UI().PerkHoverScale;
		}
		const FVector2f Center = PerkToVirtual(Def.Pos);
		const bool bSpellNode = Def.Kind == EYJJPerkKind::Spell;
		if (Def.Kind == EYJJPerkKind::Notable || bSpellNode)
		{
			// 중간·주문 별: 상태 색 바탕 원 + 효과 그림 (못 찍는 별은 어둡게)
			DrawIcon(FMath::RoundToInt(UI().UiBadge), Center.X - Radius, Center.Y - Radius, Radius * 2.f, Color);
			const int32 Icon = bSpellNode ? SpellIcon(static_cast<EYJJSpell>(Def.Spell)) : Def.Icon;
			if (Icon != INDEX_NONE)
			{
				const bool bLit = Current->IsAllocated(Perk) || Current->IsReachable(Perk);
				const float Dim = bLit ? 1.f : UI().PerkIconDim;
				const float IconSize = Radius * 2.f * UI().PerkIconScale;
				DrawIcon(Icon, Center.X - IconSize * Half, Center.Y - IconSize * Half, IconSize, FLinearColor(Dim, Dim, Dim, 1.f));
			}
		}
		else
		{
			DrawIcon(FMath::RoundToInt(bSmall ? UI().UiStar : UI().UiBigStar), Center.X - Radius, Center.Y - Radius, Radius * 2.f, Color);
		}
		if (bSpellNode)
		{
			// 주문 별: 아래에 Lv
			const EYJJSpell Spell = static_cast<EYJJSpell>(Def.Spell);
			DrawLabelCentered(YJJLoc::Format(TEXT("Spell.LvOf"), { Current->SpellLevel(Spell), YJJData::SpellMaxLevel(Spell) }),
				Center.X, Center.Y + Radius + UI().SpellNodeLvGap + UI().TinyText * Half, UI().TinyText, YJJUI::Highlight);
		}
	}

	// 머리글
	const float Pad = UI().TextPad;
	// 머리글은 성좌 위 가운데 (영혼·닫기 버튼은 좌측 패널)
	DrawLabel(YJJLoc::Get(TEXT("UI.TreeTitle")), WorldCenterVirtualX(), UI().TreeTitleY, UI().TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Format(TEXT("UI.TreeNext"), { FormatCount(Current->PerkCost()), Current->NumAllocated() }), WorldCenterVirtualX(), UI().TreeTitleY + UI().TitleText + Pad, UI().SmallText, YJJUI::Paper, Half);

	// 설명창
	if (HoverPerk != INDEX_NONE)
	{
		const FYJJPerkDef& Def = Perks[HoverPerk];
		const bool bSpell = Def.Kind == EYJJPerkKind::Spell;
		const FString Title = !Def.NameKey.IsEmpty() ? YJJLoc::Get(Def.NameKey) : YJJLoc::Get(Def.Kind == EYJJPerkKind::Start ? TEXT("Perk.Start") : TEXT("Perk.Small"));
		const FString KindLine = YJJLoc::Get(Def.Kind == EYJJPerkKind::Notable ? TEXT("Perk.KindNotable")
			: Def.Kind == EYJJPerkKind::Keystone ? TEXT("Perk.KindKeystone")
			: bSpell ? TEXT("Perk.KindSpell") : TEXT("Perk.KindSmall"));
		const FString Body = bSpell ? SpellText(static_cast<EYJJSpell>(Def.Spell)) : PerkText(HoverPerk);
		FString Status;
		FLinearColor StatusColor = YJJUI::Muted;
		if (Current->IsReachable(HoverPerk))
		{
			const bool bAfford = Current->Souls >= Current->PerkCost();
			Status = YJJLoc::Format(bAfford ? (bSpell ? TEXT("Spell.LevelUp") : TEXT("Perk.Cost")) : TEXT("Perk.NeedSouls"), { FormatCount(Current->PerkCost()) });
			StatusColor = bAfford ? YJJUI::Soul : YJJUI::Bad;
		}
		else if (Current->IsAllocated(HoverPerk))
		{
			Status = YJJLoc::Get(bSpell ? TEXT("Spell.Max") : TEXT("Perk.Taken"));
			StatusColor = YJJUI::PerkTaken;
		}
		else
		{
			Status = YJJLoc::Get(TEXT("Perk.Locked"));
		}
		const TArray<FString> BodyLines = WrapText(Body, UI().SmallText, UI().TipW - Pad * 2.f);
		const float LineH = UI().TipLineH;
		const float W = UI().TipW;
		const float H = Pad * 2.f + LineH * (BodyLines.Num() + 2);
		const float X = FMath::Min(Mouse.X + UI().TipOffset, VirtualW - W);
		const float Y = FMath::Clamp(Mouse.Y - H * Half, 0.f, VirtualHeight - H);
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
		const FLinearColor TitleColor = Def.Kind == EYJJPerkKind::Notable ? YJJUI::Highlight : Def.Kind == EYJJPerkKind::Keystone ? YJJUI::PerkKeystone : YJJUI::Paper;
		DrawLabel(Title, X + Pad, Y + Pad, UI().MidText, TitleColor, 0.f, W - Pad * 2.f - UI().TipKindW);
		DrawLabel(KindLine, X + W - Pad, Y + Pad + (UI().MidText - UI().SmallText), UI().SmallText, YJJUI::Muted, 1.f, UI().TipKindW);
		for (int32 Line = 0; Line < BodyLines.Num(); ++Line)
		{
			DrawValueLabel(BodyLines[Line], X + Pad, Y + Pad + LineH * (Line + 1), UI().SmallText, YJJUI::Paper, W - Pad * 2.f);
		}
		DrawLabel(Status, X + Pad, Y + Pad + LineH * (BodyLines.Num() + 1), UI().SmallText, StatusColor, 0.f, W - Pad * 2.f);
	}
}

// ---------------------------------------------------------------------------------------------
// 입력

void AYJJWaveHUD::HandleMouseDown(bool bRight)
{
	FYJJBattle* Current = EditBattle();
	AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (!Current || !Mode)
	{
		return;
	}
	const FVector2f Mouse = MouseScreen();
	if (bRight)
	{
		if (AimingSpell != INDEX_NONE)
		{
			AimingSpell = INDEX_NONE;
			return;
		}
		bDragging = true;
		DragLast = DragStart = Mouse;
		return;
	}

	if (const FYJJButton* Button = ButtonAt(ToVirtual(Mouse)))
	{
		FYJJAudio::Play(this, SoundClick);
		switch (static_cast<EButton>(Button->Kind))
		{
		case EButton::BuildCard:
			// 누르면 그 카드 아래 구매 팝업, 같은 카드나 다른 카드를 누르면 닫힌다
			SelectedBuilding = Button->Index;
			BuyPopupFor = BuyPopupFor == INDEX_NONE ? Button->Index : INDEX_NONE;
			break;
		case EButton::Spell:
			PressSpell(static_cast<EYJJSpell>(Button->Index));
			break;
		case EButton::Tree:
			bTreeOpen = true;
			break;
		case EButton::TreeClose:
			bTreeOpen = false;
			break;
		case EButton::Speed:
			Mode->CycleSpeed();
			break;
		case EButton::Restart:
			Mode->Restart();
			break;
		case EButton::NextStage:
			Mode->NextStage();
			break;
		case EButton::Card:
			PickSeasoning(Button->Index);
			break;
		case EButton::SkillPick:
			PickHeroSkill(Button->Index);
			break;
		case EButton::Buy:
			Current->Buy(BuyPopupFor, Button->Index);
			break;
		case EButton::Option:
			HandleOption(Button->Index);
			break;
		case EButton::OpenOptions:
			bOptionsOpen = !bOptionsOpen;
			break;
		case EButton::Pot:
			OpenPot();
			break;
		case EButton::Charge:
			TryHeroCharge();
			break;
		case EButton::HeroSkill:
			TryHeroSkill();
			break;
		case EButton::TitleStart:
			Mode->GoToWorldMap();
			break;
		case EButton::TitleQuit:
			if (APlayerController* Player = GetOwningPlayerController())
			{
				Player->ConsoleCommand(TEXT("quit"));
			}
			break;
		case EButton::WorldStage:
			if (Mode->IsStageOpen(Button->Index))
			{
				Mode->StartStage(Button->Index);
			}
			break;
		case EButton::WorldBack:
			Mode->GoToTitle();
			break;
		case EButton::ResultMap:
			Mode->GoToWorldMap();
			break;
		}
		return;
	}
	if (Mode->GetScreen() != EYJJScreen::Battle)
	{
		return;
	}

	if (bOptionsOpen)
	{
		return;
	}
	if (Mode->CutsceneLine())
	{
		Mode->NextCutsceneLine();
		return;
	}
	if (bTreeOpen)
	{
		const int32 Perk = PerkAt(ToVirtual(Mouse));
		if (Perk != INDEX_NONE && Current->Allocate(Perk))
		{
			Mode->SaveProgress();
		}
		return;
	}
	bLeftDown = true;
	bLeftDragged = false;
	DragLast = DragStart = Mouse;
}

void AYJJWaveHUD::HandleMouseUp(bool bRight)
{
	if (bRight)
	{
		bDragging = false;
		return;
	}
	// 끌지 않고 뗐으면 전장 클릭 (짓기·강화·냄비·주문 조준)
	const bool bClick = bLeftDown && !bLeftDragged;
	bLeftDown = bLeftDragged = false;
	if (bClick)
	{
		ClickWorld();
	}
}

void AYJJWaveHUD::HandleWheel(float Delta)
{
	if (bTreeOpen || FMath::IsNearlyZero(Delta))
	{
		return;
	}
	// 커서 아래 지점을 붙잡고 확대·축소
	const FVector2f Mouse = MouseScreen();
	const FVector2f Before = ScreenToWorld(Mouse);
	Zoom = FMath::Clamp(Zoom * FMath::Pow(UI().ZoomStep, Delta), UI().ZoomMin, UI().ZoomMax);
	CamCenter += Before - ScreenToWorld(Mouse);
	ClampCamera();
}

void AYJJWaveHUD::ClickWorld()
{
	FYJJBattle* Current = EditBattle();
	if (Current->bGameOver || IsMouseOverUI())
	{
		return;
	}
	const FVector2f World = ScreenToWorld(MouseScreen());
	if (AimingSpell != INDEX_NONE)
	{
		Current->Cast(static_cast<EYJJSpell>(AimingSpell), World);
		AimingSpell = INDEX_NONE;
		return;
	}
	const int32 Plot = PlotAt(World);
	BuyPopupFor = INDEX_NONE;
	if (Plot == INDEX_NONE)
	{
		if (World.X > Current->CampEdgeX)
		{
			// 진지 밖 전장: 국자로 내려친다 (스킬 F3과 같은 횟수)
			Current->Cast(EYJJSpell::Ladle, World);
		}
		return;
	}
	if (Current->Plots[Plot].Building == INDEX_NONE)
	{
		Current->Build(Plot, SelectedBuilding);
	}
	else
	{
		Current->Upgrade(Plot);
	}
}

void AYJJWaveHUD::HandleKey(const FKey& Key)
{
	FYJJBattle* Current = EditBattle();
	AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (!Current || !Mode)
	{
		return;
	}
	// 타이틀: 엔터·스페이스 시작 / 월드맵: ESC 타이틀
	if (Mode->GetScreen() == EYJJScreen::Title)
	{
		if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
		{
			Mode->GoToWorldMap();
		}
		else if (Key == EKeys::Escape)
		{
			bOptionsOpen = !bOptionsOpen;
		}
		return;
	}
	if (Mode->GetScreen() == EYJJScreen::WorldMap)
	{
		if (Key == EKeys::Escape)
		{
			if (bOptionsOpen)
			{
				bOptionsOpen = false;
			}
			else
			{
				Mode->GoToTitle();
			}
		}
		return;
	}
	// 컷씬: 스페이스·엔터 다음 줄, ESC 건너뛰기
	if (Mode->CutsceneLine())
	{
		if (Key == EKeys::Escape)
		{
			Mode->SkipCutscene();
		}
		else if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
		{
			Mode->NextCutsceneLine();
		}
		return;
	}
	const int32 BuildingIndex = BuildingIndexForKey(Key);
	// 영웅 스킬·카드를 고르는 중이면 1·2·3 은 그 카드
	if (Current->bSkillPickPending)
	{
		PickHeroSkill(BuildingIndex);
		return;
	}
	if (Current->bCardPending)
	{
		PickSeasoning(BuildingIndex);
		return;
	}
	if (YJJData::Buildings().IsValidIndex(BuildingIndex))
	{
		SelectedBuilding = BuildingIndex;
		return;
	}

	const int32 SpellIndex = SpellIndexForKey(Key);
	if (SpellIndex != INDEX_NONE)
	{
		PressSpell(static_cast<EYJJSpell>(SpellIndex));
	}
	else if (Key == EKeys::T || Key == EKeys::Tab)
	{
		bTreeOpen = !bTreeOpen;
	}
	else if (Key == EKeys::E)
	{
		TryHeroCharge();
	}
	else if (Key == EKeys::R)
	{
		TryHeroSkill();
	}
	else if (Key == EKeys::Q)
	{
		OpenPot();
	}
	else if (Key == EKeys::Escape)
	{
		// ESC: 열린 것을 닫고, 아무것도 없으면 옵션을 연다
		if (bOptionsOpen || bTreeOpen || AimingSpell != INDEX_NONE)
		{
			bOptionsOpen = bTreeOpen = false;
			AimingSpell = INDEX_NONE;
		}
		else
		{
			bOptionsOpen = true;
		}
	}
	else if (Key == EKeys::F)
	{
		Mode->CycleSpeed();
	}
	else if (Key == EKeys::P || Key == EKeys::SpaceBar)
	{
		Mode->TogglePause();
	}
	else if (Key == EKeys::R && Current->bGameOver)
	{
		Mode->Restart();
	}
	else if (Key == EKeys::N && Current->bGameOver)
	{
		Mode->NextStage();
	}
	else if (Key == EKeys::F4)
	{
		bShowDebug = !bShowDebug;
	}
	else if (Key == EKeys::Home)
	{
		// 전선으로 시점 이동
		const float Front = Current->Horde.FrontX(EYJJTeam::Enemy);
		FocusWorld(FVector2f(Front > 0.f ? Front : Current->CampEdgeX, Current->FieldHeight * Half), Zoom);
	}
}

int32 AYJJWaveHUD::SpellIndexForKey(const FKey& Key)
{
	if (Key == EKeys::F1) { return static_cast<int32>(EYJJSpell::Meteor); }
	if (Key == EKeys::F2) { return static_cast<int32>(EYJJSpell::Rally); }
	if (Key == EKeys::F3) { return static_cast<int32>(EYJJSpell::Ladle); }
	return INDEX_NONE;
}

void AYJJWaveHUD::PressSpell(EYJJSpell Spell)
{
	FYJJBattle* Current = EditBattle();
	const int32 Index = static_cast<int32>(Spell);
	if (!NeedsAim(Spell))
	{
		const bool bCast = Current->Cast(Spell, FVector2f::ZeroVector);
		AimingSpell = INDEX_NONE;
		if (bCast && Spell == EYJJSpell::Rally)
		{
			// 무엇이 좋아졌는지 바로 보이게
			const FYJJSpellStats Stats = Current->SpellNow(Spell);
			AddBanner(YJJLoc::Format(TEXT("Rally.Banner"), { FString::Printf(TEXT("%g"), Stats.Power), FMath::RoundToInt(Current->RallyLeft()) }), YJJUI::Highlight);
		}
	}
	else
	{
		// 조준 상태로: 전장을 클릭한 자리에 떨어진다 (같은 주문을 다시 누르면 취소, 오른쪽 클릭·ESC도 취소)
		AimingSpell = AimingSpell == Index || !Current->CanCast(Spell) ? INDEX_NONE : Index;
	}
}

int32 AYJJWaveHUD::SpellIcon(EYJJSpell Spell) const
{
	switch (Spell)
	{
	case EYJJSpell::Rally: return FMath::RoundToInt(UI().UiRally);
	case EYJJSpell::Ladle: return FMath::RoundToInt(UI().UiLadle);
	default: return FMath::RoundToInt(UI().UiMeteor);
	}
}

FString AYJJWaveHUD::SpellStatText(EYJJSpell Spell, const FYJJSpellStats& Stats) const
{
	// 서식 자리: {0} 위력, {1} 반경, {2} 재사용 대기, {3} 지속, {4} 폭 (반경 x 2)
	const FString Key = FString(TEXT("SpellStat.")) + YJJData::SpellKey(Spell);
	return YJJLoc::Format(*Key, {
		FString::Printf(TEXT("%g"), Stats.Power),
		FString::Printf(TEXT("%g"), Stats.Radius),
		FString::Printf(TEXT("%g"), Stats.Cooldown),
		FString::Printf(TEXT("%g"), Stats.Duration),
		FString::Printf(TEXT("%g"), Stats.Radius * 2.f) });
}

FString AYJJWaveHUD::SpellText(EYJJSpell Spell) const
{
	const FYJJBattle* Current = Battle();
	const int32 Level = Current->SpellLevel(Spell);
	const int32 MaxLevel = YJJData::SpellMaxLevel(Spell);
	TArray<FString> Lines;
	Lines.Add(YJJLoc::Format(TEXT("Spell.LvOf"), { Level, MaxLevel }));
	Lines.Add(SpellStatText(Spell, YJJData::SpellStats(Spell, Level)));
	if (Level < MaxLevel)
	{
		Lines.Add(YJJLoc::Format(TEXT("Spell.Next"), { Level + 1, SpellStatText(Spell, YJJData::SpellStats(Spell, Level + 1)) }));
	}
	// Lv별 특수 효과: 서식 자리 {0} A, {1} B, {2} A%, {3} B%
	for (const FYJJSpellLevel& Row : YJJData::SpellLevels(Spell))
	{
		if (Row.Fx == EYJJSpellFx::None)
		{
			continue;
		}
		const FString FxKey = FString(TEXT("SpellFx.")) + YJJData::SpellFxKey(Row.Fx);
		const FString Fx = YJJLoc::Format(*FxKey, {
			FString::Printf(TEXT("%g"), Row.A),
			FString::Printf(TEXT("%g"), Row.B),
			FString::Printf(TEXT("%d"), FMath::RoundToInt(Row.A * Percent)),
			FString::Printf(TEXT("%d"), FMath::RoundToInt(Row.B * Percent)) });
		Lines.Add(YJJLoc::Format(Row.Level <= Level ? TEXT("Spell.FxOn") : TEXT("Spell.FxLocked"), { Row.Level, Fx }));
	}
	return FString::Join(Lines, TEXT("\n"));
}

int32 AYJJWaveHUD::BuildingIndexForKey(const FKey& Key)
{
	if (Key == EKeys::One) { return 0; }
	if (Key == EKeys::Two) { return 1; }
	if (Key == EKeys::Three) { return 2; }
	if (Key == EKeys::Four) { return 3; }
	if (Key == EKeys::Five) { return 4; }
	if (Key == EKeys::Six) { return 5; }
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------
// 카드·배너

void AYJJWaveHUD::DrawCardPick()
{
	// 양념 고르기: 웨이브를 버티면 보급 수레가 양념 셋을 가져온다. 하나를 냄비에 넣으면 이번 판 군대가 그 맛으로 강해진다
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	const float Pad = L.TextPad;
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::Shade);
	const float CenterX = WorldCenterVirtualX();
	DrawLabel(YJJLoc::Get(TEXT("UI.CardTitle")), CenterX, L.CardTitleY, L.TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Get(TEXT("UI.CardSub")), CenterX, L.CardTitleY + L.TitleText + L.CardSubGap, L.SmallText, YJJUI::Paper, Half);

	const int32 Count = Current->CardOffer.Num();
	const float Total = Count * L.CardW + (Count - 1) * L.CardPickGap;
	const float Y = L.CardTitleY + L.TitleText + L.CardSubGap + L.SmallText + Pad * 2.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FYJJPerkDef& Def = YJJData::Cards()[Current->CardOffer[Index]];
		const float X = CenterX - Total * Half + Index * (L.CardW + L.CardPickGap);
		const bool bHover = HoverButton(EButton::Card) && ButtonAt(ToVirtual(MouseScreen()))->Index == Index;
		DrawPanel(X, Y, L.CardW, L.CardPickH, bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Half) : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabel(FString::FromInt(Index + 1), X + Pad, Y + Pad, L.MidText, YJJUI::Highlight);
		// 양념 그림 → 이름 → 맛 한 줄 → 효과
		float RowY = Y + Pad;
		if (Def.Icon != INDEX_NONE)
		{
			DrawIcon(Def.Icon, X + (L.CardW - L.CardIconSize) * Half, RowY, L.CardIconSize);
		}
		RowY += L.CardIconSize + Pad * Half;
		DrawLabel(YJJLoc::Get(Def.NameKey), X + L.CardW * Half, RowY, L.MidText, YJJUI::Paper, Half, L.CardW - Pad * 2.f);
		RowY += L.MidText + Pad * Half;
		if (!Def.FlavorKey.IsEmpty())
		{
			for (const FString& Line : WrapText(YJJLoc::Get(Def.FlavorKey), L.TinyText, L.CardW - Pad * 2.f))
			{
				DrawLabel(Line, X + L.CardW * Half, RowY, L.TinyText, YJJUI::Muted, Half);
				RowY += L.TinyText + L.CardLineGap;
			}
			RowY += Pad * Half;
		}
		for (const FString& Line : WrapText(EffectText(Def), L.SmallText, L.CardW - Pad * 2.f))
		{
			DrawValueLabel(Line, X + Pad, RowY, L.SmallText, YJJUI::Paper, L.CardW - Pad * 2.f);
			RowY += L.TipLineH;
		}
		AddButton(EButton::Card, Index, X, Y, L.CardW, L.CardPickH);
	}
}

void AYJJWaveHUD::DrawSkillPick()
{
	// 판 시작: 이번 판에 R로 쓸 대장 기술 하나 (양념 고르기와 같은 카드 모양)
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	const float Pad = L.TextPad;
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::Shade);
	const float CenterX = WorldCenterVirtualX();
	DrawLabel(YJJLoc::Get(TEXT("UI.SkillPickTitle")), CenterX, L.CardTitleY, L.TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Get(TEXT("UI.SkillPickSub")), CenterX, L.CardTitleY + L.TitleText + L.CardSubGap, L.SmallText, YJJUI::Paper, Half);

	const int32 Count = Current->SkillOffer.Num();
	const float Total = Count * L.CardW + (Count - 1) * L.CardPickGap;
	const float Y = L.CardTitleY + L.TitleText + L.CardSubGap + L.SmallText + Pad * 2.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FYJJSkillDef& Def = YJJData::Skill(Current->SkillOffer[Index]);
		const float X = CenterX - Total * Half + Index * (L.CardW + L.CardPickGap);
		const bool bHover = HoverButton(EButton::SkillPick) && ButtonAt(ToVirtual(MouseScreen()))->Index == Index;
		DrawPanel(X, Y, L.CardW, L.CardPickH, bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Half) : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabel(FString::FromInt(Index + 1), X + Pad, Y + Pad, L.MidText, YJJUI::Highlight);
		// 그림 → 이름 → 설명 → 대기 시간
		float RowY = Y + Pad;
		if (Def.Icon != INDEX_NONE)
		{
			DrawIcon(Def.Icon, X + (L.CardW - L.CardIconSize) * Half, RowY, L.CardIconSize);
		}
		RowY += L.CardIconSize + Pad * Half;
		DrawLabel(YJJLoc::Get(Def.NameKey), X + L.CardW * Half, RowY, L.MidText, YJJUI::Paper, Half, L.CardW - Pad * 2.f);
		RowY += L.MidText + Pad;
		if (!Def.DescKey.IsEmpty())
		{
			for (const FString& Line : WrapText(YJJLoc::Format(*Def.DescKey, { Def.SummonCount }), L.SmallText, L.CardW - Pad * 2.f))
			{
				DrawLabel(Line, X + L.CardW * Half, RowY, L.SmallText, YJJUI::Paper, Half);
				RowY += L.TipLineH;
			}
			RowY += Pad * Half;
		}
		DrawLabel(YJJLoc::Format(TEXT("UI.SkillCooldown"), { FMath::RoundToInt(Def.Cooldown) }), X + L.CardW * Half, RowY, L.SmallText, YJJUI::Muted, Half);
		AddButton(EButton::SkillPick, Index, X, Y, L.CardW, L.CardPickH);
	}
}

void AYJJWaveHUD::PickHeroSkill(int32 Choice)
{
	FYJJBattle* Current = EditBattle();
	if (Current && Current->PickHeroSkill(Choice))
	{
		AddBanner(YJJLoc::Format(TEXT("Skill.Picked"), { YJJLoc::Get(Current->HeroSkillDef()->NameKey) }), YJJUI::Highlight);
	}
}

void AYJJWaveHUD::PickSeasoning(int32 Choice)
{
	FYJJBattle* Current = EditBattle();
	if (!Current->CardOffer.IsValidIndex(Choice))
	{
		return;
	}
	const FYJJPerkDef& Def = YJJData::Cards()[Current->CardOffer[Choice]];
	if (Current->PickCard(Choice))
	{
		AddBanner(YJJLoc::Format(TEXT("Card.Added"), { YJJLoc::Get(Def.NameKey) }), YJJUI::Good);
	}
}

float AYJJWaveHUD::DrawSeasonings(float X, float Y, float W)
{
	// 좌측 패널: 이번 판 냄비에 넣은 양념 (그림을 가리키면 효과)
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	const float Pad = L.PanelPad;
	const float Icon = L.SeasoningIcon;
	const int32 PerRow = FMath::Max(FMath::FloorToInt((W - Pad * 2.f + L.SeasoningGap) / (Icon + L.SeasoningGap)), 1);
	const int32 Rows = FMath::Max(FMath::DivideAndRoundUp(Current->CardsTaken.Num(), PerRow), 1);
	const float H = Pad * 2.f + L.SmallText + Pad * Half + Rows * Icon + (Rows - 1) * L.SeasoningGap;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	DrawLabel(YJJLoc::Get(TEXT("UI.Seasonings")), X + Pad, Y + Pad, L.SmallText, YJJUI::Muted);
	const float IconsY = Y + Pad + L.SmallText + Pad * Half;
	if (Current->CardsTaken.Num() == 0)
	{
		DrawLabel(YJJLoc::Get(TEXT("UI.SeasoningsNone")), X + Pad, IconsY + (Icon - L.TinyText) * Half, L.TinyText, YJJUI::Muted, 0.f, W - Pad * 2.f);
		return H;
	}
	for (int32 Index = 0; Index < Current->CardsTaken.Num(); ++Index)
	{
		const FYJJPerkDef& Def = YJJData::Cards()[Current->CardsTaken[Index]];
		const float IX = X + Pad + (Index % PerRow) * (Icon + L.SeasoningGap);
		const float IY = IconsY + (Index / PerRow) * (Icon + L.SeasoningGap);
		if (Def.Icon != INDEX_NONE)
		{
			DrawIcon(Def.Icon, IX, IY, Icon);
		}
		AddButton(EButton::Seasoning, Index, IX, IY, Icon, Icon);
	}
	return H;
}

void AYJJWaveHUD::DrawSeasoningTooltip()
{
	const FYJJButton* Hover = ButtonAt(ToVirtual(MouseScreen()));
	const FYJJBattle* Current = Battle();
	if (!Hover || Hover->Kind != static_cast<int32>(EButton::Seasoning) || !Current->CardsTaken.IsValidIndex(Hover->Index))
	{
		return;
	}
	const FYJJPerkDef& Def = YJJData::Cards()[Current->CardsTaken[Hover->Index]];
	const FYJJUILayout& L = UI();
	const float Pad = L.TextPad;
	const float W = L.TipW;
	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Emplace(YJJLoc::Get(Def.NameKey), YJJUI::Highlight);
	if (!Def.FlavorKey.IsEmpty())
	{
		for (const FString& Line : WrapText(YJJLoc::Get(Def.FlavorKey), L.SmallText, W - Pad * 2.f))
		{
			Lines.Emplace(Line, YJJUI::Muted);
		}
	}
	for (const FString& Line : WrapText(EffectText(Def), L.SmallText, W - Pad * 2.f))
	{
		Lines.Emplace(Line, YJJUI::Paper);
	}
	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float H = Pad * 2.f + L.TipLineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + L.TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawValueLabel(Lines[Line].Key, X + Pad, Y + Pad + L.TipLineH * Line, L.SmallText, Lines[Line].Value, W - Pad * 2.f);
	}
}

void AYJJWaveHUD::AddBanner(const FString& Text, const FLinearColor& Color)
{
	FYJJAudio::Play(this, SoundBanner);
	FYJJBanner& Banner = Banners.AddDefaulted_GetRef();
	Banner.Text = Text;
	Banner.Color = Color;
}

void AYJJWaveHUD::Shake()
{
	ShakeLeft = UI().ShakeTime;
}

void AYJJWaveHUD::UpdateBanners(float Dt)
{
	const FYJJUILayout& L = UI();
	ShakeLeft = FMath::Max(ShakeLeft - Dt, 0.f);
	for (int32 Index = Banners.Num() - 1; Index >= 0; --Index)
	{
		Banners[Index].Age += Dt;
		if (Banners[Index].Age >= L.BannerTime)
		{
			Banners.RemoveAt(Index);
		}
	}

	const FYJJBattle* Current = Battle();
	const FYJJHordeView& Units = View();
	// 아군 수가 처음으로 500·1000·2000… 을 넘을 때
	const int32 Allies = Units.Num(EYJJTeam::Player);
	if (NextMilestone > 0.f && Allies >= NextMilestone)
	{
		AddBanner(YJJLoc::Format(TEXT("UI.Milestone"), { FormatCount(NextMilestone) }), YJJUI::Ally);
		NextMilestone *= FMath::Max(L.MilestoneFactor, 1.f + KINDA_SMALL_NUMBER);
	}
	// 짧은 시간에 많이 잡으면 학살
	KillWindow += Dt;
	if (KillWindow >= L.MassacreWindow)
	{
		const int32 Delta = Current->Kills - KillsAtWindow;
		if (Delta >= L.MassacreKills)
		{
			AddBanner(YJJLoc::Format(TEXT("UI.Massacre"), { FormatCount(Delta) }), YJJUI::Bad);
		}
		KillWindow = 0.f;
		KillsAtWindow = Current->Kills;
	}
	// 보스가 사라지면 (쓰러뜨림)
	if (Units.Boss != INDEX_NONE)
	{
		bBossSeen = true;
	}
	else if (bBossSeen)
	{
		bBossSeen = false;
		if (!Current->bGameOver || Current->bVictory)
		{
			AddBanner(YJJLoc::Get(TEXT("UI.BossDown")), YJJUI::Highlight);
		}
	}
}

void AYJJWaveHUD::DrawBanners()
{
	const FYJJUILayout& L = UI();
	for (int32 Index = 0; Index < Banners.Num(); ++Index)
	{
		const FYJJBanner& Banner = Banners[Index];
		const float T = Banner.Age / FMath::Max(L.BannerTime, KINDA_SMALL_NUMBER);
		// 처음엔 크게 튀어나왔다가 끝에 사라진다
		const float Pop = 1.f + FMath::Max(0.f, 1.f - T * L.BannerPopSpeed) * L.BannerPop;
		const float Alpha = FMath::Clamp((1.f - T) * L.BannerFadeSharpness, 0.f, 1.f);
		// 줄 간격은 튀어나온 최대 크기 기준 (겹치지 않게)
		DrawLabelCentered(Banner.Text, WorldCenterVirtualX(), L.BannerY + Index * L.BannerText * (1.f + L.BannerPop), L.BannerText * Pop, YJJUI::WithAlpha(Banner.Color, Alpha));
	}
}
