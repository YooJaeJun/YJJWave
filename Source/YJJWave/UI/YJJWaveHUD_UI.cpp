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
	const float CornerUV = UiAtlas.CellUV.X * UI().PanelCornerFraction;
	const float Corner = FMath::Min(UI().PanelCorner, FMath::Min(W, H) * Half);
	const float Xs[NineSlice] = { X, X + Corner, X + W - Corner };
	const float Ws[NineSlice] = { Corner, W - Corner * 2.f, Corner };
	const float Ys[NineSlice] = { Y, Y + Corner, Y + H - Corner };
	const float Hs[NineSlice] = { Corner, H - Corner * 2.f, Corner };
	const float Us[NineSlice] = { 0.f, CornerUV, UiAtlas.CellUV.X - CornerUV };
	const float UWs[NineSlice] = { CornerUV, UiAtlas.CellUV.X - CornerUV * 2.f, CornerUV };

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
			AddQuad(Batch, P0, P1 - P0, UV0 + FVector2f(Us[Column], Us[Row]), FVector2f(UWs[Column], UWs[Row]), Color, false);
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
	return ButtonAt(Virtual) != nullptr || Virtual.Y < UI().TopY + UI().TopH || Virtual.Y > UI().CardY - UI().PanelPad;
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

// ---------------------------------------------------------------------------------------------
// 위·아래 막대

void AYJJWaveHUD::DrawTopBar()
{
	const FYJJBattle* Current = Battle();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const float Y = UI().TopY;
	const float H = UI().TopH;
	const float Pad = UI().PanelPad;
	const float Icon = UI().IconSize;

	// 왼쪽: 물결 레벨 + 남은 시간
	{
		const float X = UI().LevelPanelX;
		const float W = UI().LevelPanelW;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanel));
		DrawLabel(YJJLoc::Format(TEXT("UI.Level"), { Current->Level }), X + Pad, Y + Pad, UI().BigText, YJJUI::Paper);
		const float Left = FMath::Max(Current->LevelDuration() - Current->LevelTime, 0.f);
		const int32 Seconds = FMath::CeilToInt(Left);
		DrawLabel(YJJLoc::Format(TEXT("UI.Timer"), { Seconds / 60, FString::Printf(TEXT("%02d"), Seconds % 60) }), X + W - Pad, Y + Pad, UI().BigText, YJJUI::Highlight, 1.f);
		DrawBar(X + Pad, Y + H - Pad - UI().BarH, W - Pad * 2.f, UI().BarH, Current->LevelTime / FMath::Max(Current->LevelDuration(), KINDA_SMALL_NUMBER), YJJUI::LevelBar);
	}

	// 가운데: 아군 VS 적, 성벽 체력
	{
		const float X = UI().VsPanelX;
		const float W = UI().VsPanelW;
		const float Mid = X + W * Half;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanel));
		DrawIcon(FMath::RoundToInt(UI().UiSword), X + Pad, Y + Pad, Icon);
		DrawLabel(FormatCount(View().Num(EYJJTeam::Player)), X + Pad + Icon + UI().IconGap, Y + Pad, UI().BigText, YJJUI::Ally);
		DrawLabel(YJJLoc::Get(TEXT("UI.VS")), Mid, Y + Pad, UI().BigText, YJJUI::Paper, Half);
		DrawIcon(FMath::RoundToInt(UI().UiSkull), X + W - Pad - Icon, Y + Pad, Icon);
		DrawLabel(FormatCount(View().Num(EYJJTeam::Enemy)), X + W - Pad - Icon - UI().IconGap, Y + Pad, UI().BigText, YJJUI::Enemy, 1.f);

		const float BarX = X + Pad + Icon * UI().SmallIconScale + UI().IconGap;
		const float BarY = Y + H - Pad - UI().BarH;
		DrawIcon(FMath::RoundToInt(UI().UiWall), X + Pad, BarY + (UI().BarH - Icon * UI().SmallIconScale) * Half, Icon * UI().SmallIconScale);
		DrawBar(BarX, BarY, X + W - Pad - BarX, UI().BarH, Current->WallHP / FMath::Max(Current->WallMaxHP, KINDA_SMALL_NUMBER), YJJUI::WallBar);
		DrawLabel(YJJLoc::Format(TEXT("UI.WallHP"), { FormatCount(FMath::CeilToFloat(Current->WallHP)), FormatCount(Current->WallMaxHP) }), (BarX + X + W - Pad) * Half, BarY + UI().BarTextLift, UI().SmallText, YJJUI::Paper, Half);
	}

	// 오른쪽: 금화·영혼·배속
	{
		const float X = UI().ResPanelX;
		const float W = UI().ResPanelW;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanel));
		const float RowH = (H - Pad * 2.f) * Half;
		DrawIcon(FMath::RoundToInt(UI().UiCoin), X + Pad, Y + Pad, RowH);
		DrawLabel(FormatCount(Current->Gold), X + Pad + RowH + UI().IconGap, Y + Pad, UI().MidText, YJJUI::Gold);
		DrawIcon(FMath::RoundToInt(UI().UiSoul), X + Pad, Y + Pad + RowH, RowH);
		DrawLabel(FormatCount(Current->Souls), X + Pad + RowH + UI().IconGap, Y + Pad + RowH, UI().MidText, YJJUI::Soul);

		const FString Speed = Mode && Mode->IsPaused() ? YJJLoc::Get(TEXT("UI.Paused")) : YJJLoc::Format(TEXT("UI.Speed"), { Mode ? FMath::RoundToInt(Mode->GetSpeed()) : 1 });
		const float SpeedW = UI().SpeedButtonW;
		const float SpeedX = X + W - Pad - SpeedW;
		DrawPanel(SpeedX, Y + Pad, SpeedW, H - Pad * 2.f, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
		DrawLabel(Speed, SpeedX + SpeedW * Half, Y + H * Half - UI().MidText * Half, UI().MidText, YJJUI::Paper, Half, SpeedW - Pad);
		AddButton(EButton::Speed, 0, SpeedX, Y + Pad, SpeedW, H - Pad * 2.f);
	}
}

void AYJJWaveHUD::DrawBossBar()
{
	const FYJJHordeView& Units = View();
	if (Units.Boss == INDEX_NONE)
	{
		return;
	}
	const FYJJUnitDef& Def = YJJData::Unit(Units.Type[Units.Boss]);
	const float X = UI().VsPanelX;
	const float W = UI().VsPanelW;
	const float Y = UI().TopY + UI().TopH + UI().BossBarGap;
	DrawPanel(X, Y, W, UI().BossBarH, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
	const float Pad = UI().PanelPad;
	DrawLabel(YJJLoc::Get(Def.NameKey), X + Pad, Y + (UI().BossBarH - UI().MidText) * Half, UI().MidText, YJJUI::Bad);
	const float BarX = X + W * UI().BossBarSplit;
	DrawBar(BarX, Y + (UI().BossBarH - UI().BarH) * Half, X + W - Pad - BarX, UI().BarH, Units.BossHP / FMath::Max(Units.BossMaxHP, KINDA_SMALL_NUMBER), YJJUI::BossBar);
}

void AYJJWaveHUD::DrawBottomBar()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJBuildingDef> Defs = YJJData::Buildings();
	const float CardW = UI().CardW;
	const float CardH = UI().CardH;
	const float Gap = UI().CardGap;
	const float Pad = UI().PanelPad;
	const float SpellW = UI().SpellW;
	const int32 NumSpells = static_cast<int32>(EYJJSpell::Count);
	const float Total = Defs.Num() * (CardW + Gap) + UI().CardGroupGap + (NumSpells + 1) * (SpellW + Gap) - Gap;
	float X = (VirtualWidth - Total) * Half;
	const float Y = UI().CardY;

	// 병영 카드
	for (int32 Index = 0; Index < Defs.Num(); ++Index)
	{
		const FYJJBuildingDef& Def = Defs[Index];
		const bool bSelected = Index == SelectedBuilding;
		const int32 Cost = Current->BuildCost(Index);
		const bool bAfford = Current->Gold >= Cost;
		DrawPanel(X, Y, CardW, CardH, bSelected ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(UI().UiCard));
		DrawIcon(Def.IconCol, X + (CardW - UI().CardIcon) * Half, Y + Pad, UI().CardIcon);
		DrawLabel(FString::FromInt(Index + 1), X + Pad, Y + Pad * Half, UI().SmallText, YJJUI::Paper);
		const int32 Owned = Current->BuildingCount(Index);
		if (Owned > 0)
		{
			DrawLabel(YJJLoc::Format(TEXT("UI.Owned"), { Owned }), X + CardW - Pad, Y + Pad * Half, UI().SmallText, YJJUI::Highlight, 1.f);
		}
		DrawLabel(YJJLoc::Get(Def.NameKey), X + CardW * Half, Y + Pad + UI().CardIcon, UI().SmallText, YJJUI::Paper, Half, CardW - Pad);
		const float CostY = Y + CardH - Pad - UI().SmallText;
		DrawIcon(FMath::RoundToInt(UI().UiCoin), X + Pad, CostY, UI().SmallText);
		DrawLabel(FormatCount(Cost), X + CardW - Pad, CostY, UI().SmallText, bAfford ? YJJUI::Gold : YJJUI::Bad, 1.f);
		AddButton(EButton::BuildCard, Index, X, Y, CardW, CardH);
		X += CardW + Gap;
	}
	X += UI().CardGroupGap;

	// 스펠
	for (int32 Index = 0; Index < NumSpells; ++Index)
	{
		const EYJJSpell Spell = static_cast<EYJJSpell>(Index);
		const bool bReady = Current->CanCast(Spell);
		const bool bArmed = Spell == EYJJSpell::Meteor && bAimingMeteor;
		DrawPanel(X, Y, SpellW, CardH, bArmed ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(UI().UiCard));
		const int32 IconCell = FMath::RoundToInt(Spell == EYJJSpell::Meteor ? UI().UiMeteor : UI().UiRally);
		DrawIcon(IconCell, X + (SpellW - UI().CardIcon) * Half, Y + Pad, UI().CardIcon, bReady ? FLinearColor::White : YJJUI::Muted);
		DrawLabel(YJJLoc::Get(Spell == EYJJSpell::Meteor ? TEXT("Key.Meteor") : TEXT("Key.Rally")), X + Pad, Y + Pad * Half, UI().SmallText, YJJUI::Paper);
		DrawLabel(YJJLoc::Get(Spell == EYJJSpell::Meteor ? TEXT("Spell.Meteor") : TEXT("Spell.Rally")), X + SpellW * Half, Y + Pad + UI().CardIcon, UI().SmallText, YJJUI::Paper, Half, SpellW - Pad);
		if (!bReady)
		{
			const float Fraction = Current->CooldownLeft(Spell) / FMath::Max(Current->CooldownMax(Spell), KINDA_SMALL_NUMBER);
			DrawBox(X + Pad, Y + Pad + UI().CardIcon * (1.f - Fraction), SpellW - Pad * 2.f, UI().CardIcon * Fraction, YJJUI::Shade);
			DrawLabel(FString::FromInt(FMath::CeilToInt(Current->CooldownLeft(Spell))), X + SpellW * Half, Y + Pad + UI().CardIcon * Half - UI().MidText * Half, UI().MidText, YJJUI::Paper, Half);
		}
		AddButton(EButton::Spell, Index, X, Y, SpellW, CardH);
		X += SpellW + Gap;
	}

	// 특성 성좌 버튼 (찍을 수 있으면 반짝)
	const bool bCanPerk = Current->Souls >= Current->PerkCost();
	const float Glow = bCanPerk ? Half + Half * FMath::Sin(GetWorld()->GetRealTimeSeconds() * UI().GlowSpeed) : 0.f;
	DrawPanel(X, Y, SpellW, CardH, FMath::Lerp(YJJUI::PanelTint, YJJUI::Soul, Glow), FMath::RoundToInt(UI().UiCard));
	DrawIcon(FMath::RoundToInt(UI().UiStar), X + (SpellW - UI().CardIcon) * Half, Y + Pad, UI().CardIcon);
	DrawLabel(YJJLoc::Get(TEXT("Key.Tree")), X + Pad, Y + Pad * Half, UI().SmallText, YJJUI::Paper);
	DrawLabel(YJJLoc::Get(TEXT("UI.Tree")), X + SpellW * Half, Y + Pad + UI().CardIcon, UI().SmallText, YJJUI::Paper, Half, SpellW - Pad);
	AddButton(EButton::Tree, 0, X, Y, SpellW, CardH);
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
	const float X = FMath::Min(Mouse.X + UI().TipOffset, VirtualWidth - W);
	const float Y = FMath::Clamp(Mouse.Y - H * Half, UI().TopY + UI().TopH, UI().CardY - H);
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
	DrawBox(0.f, 0.f, VirtualWidth, VirtualHeight, YJJUI::Shade);
	const float W = UI().GameOverW;
	const float H = UI().GameOverH;
	const float X = (VirtualWidth - W) * Half;
	const float Y = (VirtualHeight - H) * Half;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanel));
	const float Pad = UI().PanelPad;
	DrawLabel(YJJLoc::Get(TEXT("UI.GameOver")), X + W * Half, Y + Pad * 2.f, UI().TitleText, YJJUI::Bad, Half);
	DrawLabel(YJJLoc::Format(TEXT("UI.GameOverStats"), { Current->Level, FormatCount(Current->Kills), FMath::RoundToInt(Current->Time) }), X + W * Half, Y + H * Half, UI().MidText, YJJUI::Paper, Half);
	DrawLabel(YJJLoc::Get(TEXT("UI.Restart")), X + W * Half, Y + H - Pad * 2.f - UI().MidText, UI().MidText, YJJUI::Highlight, Half);
}

void AYJJWaveHUD::DrawDebugLine()
{
	if (!bShowDebug)
	{
		return;
	}
	const FYJJHordeView& Horde = View();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const int32 Fps = FMath::RoundToInt(1000.f / FMath::Max(FrameMs, KINDA_SMALL_NUMBER));
	const FString Text = YJJLoc::Format(TEXT("UI.Debug"), {
		Horde.Num(EYJJTeam::Player) + Horde.Num(EYJJTeam::Enemy),
		FString::Printf(TEXT("%.1f"), Horde.StepMs),
		Fps,
		Horde.Projectiles.Num(),
		FString::Printf(TEXT("%.1f"), UnitDrawMs),
		FString::Printf(TEXT("%.1f"), Mode ? Mode->LastSimWaitMs() : 0.f) });
	DrawLabel(Text, UI().PanelPad, VirtualHeight - UI().PanelPad - UI().TinyText, UI().TinyText, YJJUI::Muted);
}

// ---------------------------------------------------------------------------------------------
// 특성 성좌

FVector2f AYJJWaveHUD::PerkToVirtual(const FVector2f& PerkPos) const
{
	return FVector2f(UI().TreeCenterX, UI().TreeCenterY) + PerkPos * UI().TreeScale;
}

int32 AYJJWaveHUD::PerkAt(const FVector2f& Virtual) const
{
	const TConstArrayView<FYJJPerkDef> Perks = YJJData::Perks();
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		const float Radius = Perks[Perk].Kind == EYJJPerkKind::Small ? UI().PerkSmall : UI().PerkBig;
		if (FVector2f::DistSquared(PerkToVirtual(Perks[Perk].Pos), Virtual) <= Radius * Radius)
		{
			return Perk;
		}
	}
	return INDEX_NONE;
}

FString AYJJWaveHUD::PerkText(int32 Perk) const
{
	const FYJJPerkDef& Def = YJJData::Perks()[Perk];
	TArray<FString> Lines;
	for (const FYJJPerkDef::FMod& Mod : Def.Mods)
	{
		const FString Target = Mod.Building == INDEX_NONE ? YJJLoc::Get(TEXT("Target.All")) : YJJLoc::Get(YJJData::Building(Mod.Building).NameKey);
		const FString Value = YJJData::IsFlatStat(Mod.Stat)
			? FString::Printf(TEXT("+%d"), FMath::RoundToInt(Mod.Value))
			: FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(Mod.Value * Percent));
		Lines.Add(YJJLoc::Format(*(FString(TEXT("Stat.")) + YJJData::StatKey(Mod.Stat)), { Value, Target }));
	}
	return FString::Join(Lines, TEXT("\n"));
}

void AYJJWaveHUD::DrawTree()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJPerkDef> Perks = YJJData::Perks();
	DrawBox(0.f, 0.f, VirtualWidth, VirtualHeight, YJJUI::TreeBack);

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
	HoverPerk = PerkAt(Mouse);
	const float Pulse = Half + Half * FMath::Sin(GetWorld()->GetRealTimeSeconds() * UI().GlowSpeed);
	for (int32 Perk = 0; Perk < Perks.Num(); ++Perk)
	{
		const FYJJPerkDef& Def = Perks[Perk];
		const bool bSmall = Def.Kind == EYJJPerkKind::Small;
		float Radius = bSmall ? UI().PerkSmall : UI().PerkBig;
		FLinearColor Color = YJJUI::PerkLocked;
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
	DrawLabel(YJJLoc::Get(TEXT("UI.TreeTitle")), VirtualWidth * Half, UI().TopY, UI().TitleText, YJJUI::Highlight, Half);
	DrawIcon(FMath::RoundToInt(UI().UiSoul), Pad, UI().TopY, UI().IconSize);
	DrawLabel(FormatCount(Current->Souls), Pad + UI().IconSize + UI().IconGap, UI().TopY, UI().BigText, YJJUI::Soul);
	DrawLabel(YJJLoc::Format(TEXT("UI.TreeNext"), { FormatCount(Current->PerkCost()), Current->NumAllocated() }), Pad, UI().TopY + UI().IconSize + Pad, UI().SmallText, YJJUI::Paper);
	const float CloseW = UI().SpeedButtonW;
	const float CloseX = VirtualWidth - Pad - CloseW;
	DrawPanel(CloseX, UI().TopY, CloseW, UI().IconSize, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
	DrawLabel(YJJLoc::Get(TEXT("UI.TreeClose")), CloseX + CloseW * Half, UI().TopY + (UI().IconSize - UI().SmallText) * Half, UI().SmallText, YJJUI::Paper, Half, CloseW - Pad);
	AddButton(EButton::TreeClose, 0, CloseX, UI().TopY, CloseW, UI().IconSize);

	// 설명창
	if (HoverPerk != INDEX_NONE)
	{
		const FYJJPerkDef& Def = Perks[HoverPerk];
		const FString Title = !Def.NameKey.IsEmpty() ? YJJLoc::Get(Def.NameKey) : YJJLoc::Get(Def.Kind == EYJJPerkKind::Start ? TEXT("Perk.Start") : TEXT("Perk.Small"));
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
		const float X = FMath::Min(Mouse.X + UI().TipOffset, VirtualWidth - W);
		const float Y = FMath::Clamp(Mouse.Y - H * Half, 0.f, VirtualHeight - H);
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(UI().UiPanelDark));
		DrawLabel(Title, X + Pad, Y + Pad, UI().MidText, Def.Kind == EYJJPerkKind::Notable ? YJJUI::Highlight : YJJUI::Paper, 0.f, W - Pad * 2.f);
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
		}
		return;
	}

	if (bTreeOpen)
	{
		const int32 Perk = PerkAt(ToVirtual(Mouse));
		if (Perk != INDEX_NONE)
		{
			Current->Allocate(Perk);
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
	else if (Key == EKeys::F3)
	{
		bShowDebug = !bShowDebug;
	}
	else if (Key == EKeys::Home)
	{
		// 전선으로 시점 이동
		const float Front = Current->Horde.FrontX(EYJJTeam::Enemy);
		FocusWorld(FVector2f(Front > 0.f ? Front : Current->WallX, Current->FieldHeight * Half), Zoom);
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
