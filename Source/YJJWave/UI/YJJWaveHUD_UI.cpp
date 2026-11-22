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
	const float Size = H * UI().BarTextScale;
	DrawLabel(Text, X + W * Half, Y + (H - Size * UI().TextLineRatio) * Half + UI().BarTextLift, Size, YJJUI::Paper, Half, W - UI().PanelPad);
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

void AYJJWaveHUD::DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float AlignX, float MaxWidth)
{
	// FCanvasTextItem은 UFont가 있어야 그려진다. 실제 글꼴은 주아체 컴포지트
	if (!BaseFont || Text.IsEmpty() || Color.A <= 0.f)
	{
		return;
	}
	const int32 FontSize = FMath::Max(FMath::RoundToInt(Size * ViewScale * UI().FontPointScale), FMath::RoundToInt(UI().MinFontSize));
	const TSharedPtr<const FCompositeFont> Composite = GetGameFont();
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(BaseFont, FontSize);
	Font.OutlineSettings.OutlineSize = FMath::Max(FMath::RoundToInt(Size * UI().TextOutline * ViewScale), 1);
	Font.OutlineSettings.OutlineColor = YJJUI::WithAlpha(YJJUI::Ink, Color.A);

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	float Width = Measure->Measure(Text, Font).X / ViewScale;
	if (MaxWidth > 0.f && Width > MaxWidth)
	{
		Font.Size = FMath::Max(FMath::FloorToFloat(Font.Size * MaxWidth / Width), UI().MinFontSize);
		Width = Measure->Measure(Text, Font).X / ViewScale;
	}
	FCanvasTextItem Item(FVector2D(ViewOffset.X + (X - Width * AlignX) * ViewScale, ViewOffset.Y + Y * ViewScale), FText::FromString(Text), Font, Color);
	Item.Font = BaseFont;
	Item.SlateFontInfo = Font;
	Canvas->DrawItem(Item);
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
	for (const FYJJButton& Button : Buttons)
	{
		if (Button.Rect.IsInside(Virtual))
		{
			return &Button;
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
	if (HoverPlot == INDEX_NONE || Current->bGameOver)
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
	const float Pad = UI().PanelPad;
	const float LineH = UI().TipLineH;
	const float W = UI().TipW;
	const float H = Pad * 2.f + LineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + UI().TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H * Half, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawLabel(Lines[Line].Key, X + Pad, Y + Pad + LineH * Line, UI().SmallText, Lines[Line].Value, 0.f, W - Pad * 2.f);
	}
}

void AYJJWaveHUD::DrawGameOver()
{
	const FYJJBattle* Current = Battle();
	if (!Current->bGameOver)
	{
		return;
	}
	DrawBox(UI().SidePanelW, 0.f, VirtualW - UI().SidePanelW, VirtualHeight, YJJUI::Shade);
	const float W = UI().GameOverW;
	const float H = UI().GameOverH;
	const float X = WorldCenterVirtualX() - W * Half;
	const float Y = (VirtualHeight - H) * Half;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanel));
	const float Pad = UI().PanelPad;
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	DrawLabel(YJJLoc::Get(Current->bVictory ? TEXT("UI.Victory") : TEXT("UI.GameOver")), X + W * Half, Y + Pad * 2.f, UI().TitleText, Current->bVictory ? YJJUI::Highlight : YJJUI::Bad, Half);
	DrawLabel(YJJLoc::Format(TEXT("UI.GameOverStats"), { YJJLoc::Get(Current->StageDef().NameKey), FormatCount(Current->Kills), FMath::RoundToInt(Current->Time), FormatCount(Current->SoulsEarned) }),
		X + W * Half, Y + H * Half - UI().MidText * Half, UI().MidText, YJJUI::Paper, Half, W - Pad * 2.f);

	// 버튼: 다시(R) / 다음 스테이지(N)
	const bool bNext = Mode && Mode->HasNextStage();
	const int32 NumButtons = bNext ? 2 : 1;
	const float ButtonW = (W - Pad * 2.f - UI().CardGap * (NumButtons - 1)) / NumButtons;
	const float ButtonY = Y + H - Pad - UI().ResultButtonH;
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		const bool bIsNext = bNext && Index == 1;
		const float BX = X + Pad + Index * (ButtonW + UI().CardGap);
		DrawPanel(BX, ButtonY, ButtonW, UI().ResultButtonH, bIsNext ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(UI().UiCard));
		DrawLabel(YJJLoc::Get(bIsNext ? TEXT("UI.NextStage") : TEXT("UI.Restart")), BX + ButtonW * Half, ButtonY + (UI().ResultButtonH - UI().MidText) * Half, UI().MidText, YJJUI::Paper, Half, ButtonW - Pad);
		AddButton(bIsNext ? EButton::NextStage : EButton::Restart, 0, BX, ButtonY, ButtonW, UI().ResultButtonH);
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
		else if (Current->IsReachable(Perk))
		{
			Color = Current->CanAllocate(Perk) ? FMath::Lerp(YJJUI::PerkReach, FLinearColor::White, Pulse) : YJJUI::PerkReach;
		}
		if (Perk == HoverPerk)
		{
			Radius *= UI().PerkHoverScale;
		}
		const FVector2f Center = PerkToVirtual(Def.Pos);
		DrawIcon(FMath::RoundToInt(bSmall ? UI().UiStar : UI().UiBigStar), Center.X - Radius, Center.Y - Radius, Radius * 2.f, Color);
	}

	// 머리글
	const float Pad = UI().PanelPad;
	// 머리글은 성좌 위 가운데 (영혼·닫기 버튼은 좌측 패널)
	DrawLabel(YJJLoc::Get(TEXT("UI.TreeTitle")), WorldCenterVirtualX(), UI().TreeTitleY, UI().TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Format(TEXT("UI.TreeNext"), { FormatCount(Current->PerkCost()), Current->NumAllocated() }), WorldCenterVirtualX(), UI().TreeTitleY + UI().TitleText + Pad, UI().SmallText, YJJUI::Paper, Half);

	// 설명창
	if (HoverPerk != INDEX_NONE)
	{
		const FYJJPerkDef& Def = Perks[HoverPerk];
		const FString Title = !Def.NameKey.IsEmpty() ? YJJLoc::Get(Def.NameKey) : YJJLoc::Get(Def.Kind == EYJJPerkKind::Start ? TEXT("Perk.Start") : TEXT("Perk.Small"));
		const FString KindLine = YJJLoc::Get(Def.Kind == EYJJPerkKind::Notable ? TEXT("Perk.KindNotable") : Def.Kind == EYJJPerkKind::Keystone ? TEXT("Perk.KindKeystone") : TEXT("Perk.KindSmall"));
		const FString Body = PerkText(HoverPerk);
		FString Status;
		FLinearColor StatusColor = YJJUI::Muted;
		if (Current->IsAllocated(HoverPerk))
		{
			Status = YJJLoc::Get(TEXT("Perk.Taken"));
			StatusColor = YJJUI::PerkTaken;
		}
		else if (!Current->IsReachable(HoverPerk))
		{
			Status = YJJLoc::Get(TEXT("Perk.Locked"));
		}
		else
		{
			const bool bAfford = Current->Souls >= Current->PerkCost();
			Status = YJJLoc::Format(bAfford ? TEXT("Perk.Cost") : TEXT("Perk.NeedSouls"), { FormatCount(Current->PerkCost()) });
			StatusColor = bAfford ? YJJUI::Soul : YJJUI::Bad;
		}
		TArray<FString> BodyLines;
		Body.ParseIntoArrayLines(BodyLines);
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
			DrawLabel(BodyLines[Line], X + Pad, Y + Pad + LineH * (Line + 1), UI().SmallText, YJJUI::Paper, 0.f, W - Pad * 2.f);
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
		if (bAimingMeteor)
		{
			bAimingMeteor = false;
			return;
		}
		bDragging = true;
		DragLast = DragStart = Mouse;
		return;
	}

	if (const FYJJButton* Button = ButtonAt(ToVirtual(Mouse)))
	{
		switch (static_cast<EButton>(Button->Kind))
		{
		case EButton::BuildCard:
			SelectedBuilding = Button->Index;
			break;
		case EButton::Spell:
			if (static_cast<EYJJSpell>(Button->Index) == EYJJSpell::Meteor)
			{
				bAimingMeteor = !bAimingMeteor && Current->CanCast(EYJJSpell::Meteor);
			}
			else
			{
				Current->Cast(EYJJSpell::Rally, FVector2f::ZeroVector);
			}
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
			Current->PickCard(Button->Index);
			break;
		}
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
	ClickWorld();
}

void AYJJWaveHUD::HandleMouseUp(bool bRight)
{
	if (bRight)
	{
		bDragging = false;
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
	if (bAimingMeteor)
	{
		Current->Cast(EYJJSpell::Meteor, World);
		bAimingMeteor = false;
		return;
	}
	const int32 Plot = PlotAt(World);
	if (Plot == INDEX_NONE)
	{
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
	const int32 BuildingIndex = BuildingIndexForKey(Key);
	// 카드를 고르는 중이면 1·2·3 은 카드
	if (Current->bCardPending && Current->PickCard(BuildingIndex))
	{
		return;
	}
	if (YJJData::Buildings().IsValidIndex(BuildingIndex))
	{
		SelectedBuilding = BuildingIndex;
		return;
	}

	if (Key == EKeys::Q)
	{
		// 커서가 전장 위면 바로 그 자리에, 아니면 조준 상태로
		if (!IsMouseOverUI())
		{
			Current->Cast(EYJJSpell::Meteor, ScreenToWorld(MouseScreen()));
			bAimingMeteor = false;
		}
		else
		{
			bAimingMeteor = Current->CanCast(EYJJSpell::Meteor);
		}
	}
	else if (Key == EKeys::E)
	{
		Current->Cast(EYJJSpell::Rally, FVector2f::ZeroVector);
	}
	else if (Key == EKeys::T || Key == EKeys::Tab)
	{
		bTreeOpen = !bTreeOpen;
	}
	else if (Key == EKeys::Escape)
	{
		bTreeOpen = false;
		bAimingMeteor = false;
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
	else if (Key == EKeys::F3)
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
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	const float Pad = L.PanelPad;
	DrawBox(L.SidePanelW, 0.f, VirtualW - L.SidePanelW, VirtualHeight, YJJUI::Shade);
	DrawLabel(YJJLoc::Get(TEXT("UI.CardTitle")), WorldCenterVirtualX(), L.CardTitleY, L.TitleText, YJJUI::Highlight, Half);

	const int32 Count = Current->CardOffer.Num();
	const float Total = Count * L.CardW + (Count - 1) * L.CardPickGap;
	const float Y = L.CardTitleY + L.TitleText + Pad * 2.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FYJJPerkDef& Def = YJJData::Cards()[Current->CardOffer[Index]];
		const float X = WorldCenterVirtualX() - Total * Half + Index * (L.CardW + L.CardPickGap);
		DrawPanel(X, Y, L.CardW, L.CardPickH, YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabel(FString::FromInt(Index + 1), X + Pad, Y + Pad, L.MidText, YJJUI::Highlight);
		DrawLabel(YJJLoc::Get(Def.NameKey), X + L.CardW * Half, Y + Pad, L.MidText, YJJUI::Paper, Half, L.CardW - Pad * 4.f);
		TArray<FString> Lines;
		EffectText(Def).ParseIntoArrayLines(Lines);
		for (int32 Line = 0; Line < Lines.Num(); ++Line)
		{
			DrawLabel(Lines[Line], X + Pad, Y + Pad * 2.f + L.MidText + Line * L.TipLineH, L.SmallText, YJJUI::Paper, 0.f, L.CardW - Pad * 2.f);
		}
		AddButton(EButton::Card, Index, X, Y, L.CardW, L.CardPickH);
	}
}

void AYJJWaveHUD::AddBanner(const FString& Text, const FLinearColor& Color)
{
	FYJJBanner& Banner = Banners.AddDefaulted_GetRef();
	Banner.Text = Text;
	Banner.Color = Color;
	Shake();
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
		DrawLabel(Banner.Text, WorldCenterVirtualX(), L.BannerY + Index * L.BannerText, L.BannerText * Pop, YJJUI::WithAlpha(Banner.Color, Alpha), Half);
	}
}
