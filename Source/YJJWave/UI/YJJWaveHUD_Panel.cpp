// 좌측 패널: 모든 UI가 여기 있다. 전장은 패널 오른쪽 전부를 쓴다
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"

void AYJJWaveHUD::DrawSidePanel()
{
	const FYJJBattle* Current = Battle();
	const FYJJHordeView& Units = View();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const FYJJUILayout& L = UI();
	const float Pad = L.PanelPad;
	const float X = Pad;
	const float W = L.SidePanelW - Pad * 2.f;
	const int32 DarkCell = FMath::RoundToInt(L.UiPanelDark);
	float Y = Pad;

	DrawPanel(0.f, 0.f, L.SidePanelW, VirtualHeight, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));

	// ---- 물결 레벨 + 남은 시간 ----
	{
		const float H = L.HeaderH;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawLabel(YJJLoc::Format(TEXT("UI.Level"), { Current->Level }), X + Pad, Y + Pad, L.BigText, YJJUI::Paper);
		const int32 Seconds = FMath::CeilToInt(FMath::Max(Current->LevelDuration() - Current->LevelTime, 0.f));
		DrawLabel(YJJLoc::Format(TEXT("UI.Timer"), { Seconds / 60, FString::Printf(TEXT("%02d"), Seconds % 60) }), X + W - Pad, Y + Pad, L.BigText, YJJUI::Highlight, 1.f);
		DrawBar(X + Pad, Y + H - Pad - L.BarH, W - Pad * 2.f, L.BarH, Current->LevelTime / FMath::Max(Current->LevelDuration(), KINDA_SMALL_NUMBER), YJJUI::LevelBar);
		Y += H + L.SectionGap;
	}

	// ---- 아군 VS 적, 성벽 ----
	{
		const float H = L.VsH;
		const float Icon = L.IconSize;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawIcon(FMath::RoundToInt(L.UiSword), X + Pad, Y + Pad, Icon);
		DrawLabel(FormatCount(Units.Num(EYJJTeam::Player)), X + Pad + Icon + L.IconGap, Y + Pad, L.BigText, YJJUI::Ally);
		DrawLabel(YJJLoc::Get(TEXT("UI.VS")), X + W * Half, Y + Pad + (L.BigText - L.MidText) * Half, L.MidText, YJJUI::Paper, Half);
		DrawIcon(FMath::RoundToInt(L.UiSkull), X + W - Pad - Icon, Y + Pad, Icon);
		DrawLabel(FormatCount(Units.Num(EYJJTeam::Enemy)), X + W - Pad - Icon - L.IconGap, Y + Pad, L.BigText, YJJUI::Enemy, 1.f);

		const float SmallIcon = Icon * L.SmallIconScale;
		const float BarY = Y + H - Pad - L.BarH;
		const float BarX = X + Pad + SmallIcon + L.IconGap;
		DrawIcon(FMath::RoundToInt(L.UiWall), X + Pad, BarY + (L.BarH - SmallIcon) * Half, SmallIcon);
		DrawBar(BarX, BarY, X + W - Pad - BarX, L.BarH, Current->WallHP / FMath::Max(Current->WallMaxHP, KINDA_SMALL_NUMBER), YJJUI::WallBar);
		DrawLabel(YJJLoc::Format(TEXT("UI.WallHP"), { FormatCount(FMath::CeilToFloat(Current->WallHP)), FormatCount(Current->WallMaxHP) }),
			(BarX + X + W - Pad) * Half, BarY + L.BarTextLift, L.SmallText, YJJUI::Paper, Half);
		Y += H + L.SectionGap;
	}

	// ---- 보스 (있을 때만) ----
	if (Units.Boss != INDEX_NONE)
	{
		const float H = L.BossH;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawLabel(YJJLoc::Get(YJJData::Unit(Units.Type[Units.Boss]).NameKey), X + Pad, Y + (H - L.MidText) * Half, L.MidText, YJJUI::Bad, 0.f, W * L.BossBarSplit);
		const float BarX = X + W * L.BossBarSplit + Pad;
		DrawBar(BarX, Y + (H - L.BarH) * Half, X + W - Pad - BarX, L.BarH, Units.BossHP / FMath::Max(Units.BossMaxHP, KINDA_SMALL_NUMBER), YJJUI::BossBar);
		Y += H + L.SectionGap;
	}

	// ---- 금화·영혼·배속 ----
	{
		const float H = L.ResH;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		const float RowH = (H - Pad * 2.f) * Half;
		DrawIcon(FMath::RoundToInt(L.UiCoin), X + Pad, Y + Pad, RowH);
		DrawLabel(FormatCount(Current->Gold), X + Pad + RowH + L.IconGap, Y + Pad + (RowH - L.SmallText) * Half, L.SmallText, YJJUI::Gold);
		DrawIcon(FMath::RoundToInt(L.UiSoul), X + Pad, Y + Pad + RowH, RowH);
		DrawLabel(FormatCount(Current->Souls), X + Pad + RowH + L.IconGap, Y + Pad + RowH + (RowH - L.SmallText) * Half, L.SmallText, YJJUI::Soul);

		const FString Speed = Mode && Mode->IsPaused() ? YJJLoc::Get(TEXT("UI.Paused")) : YJJLoc::Format(TEXT("UI.Speed"), { Mode ? FMath::RoundToInt(Mode->GetSpeed()) : 1 });
		const float SpeedW = L.SpeedButtonW;
		const float SpeedX = X + W - Pad - SpeedW;
		DrawPanel(SpeedX, Y + Pad, SpeedW, H - Pad * 2.f, YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabel(Speed, SpeedX + SpeedW * Half, Y + (H - L.MidText) * Half, L.MidText, YJJUI::Paper, Half, SpeedW - Pad);
		AddButton(EButton::Speed, 0, SpeedX, Y + Pad, SpeedW, H - Pad * 2.f);
		Y += H + L.SectionGap;
	}

	// ---- 병영 카드 (2열) ----
	{
		const TConstArrayView<FYJJBuildingDef> Defs = YJJData::Buildings();
		const float CardW = (W - L.CardGap) * Half;
		for (int32 Index = 0; Index < Defs.Num(); ++Index)
		{
			const FYJJBuildingDef& Def = Defs[Index];
			const float CX = X + (Index % 2) * (CardW + L.CardGap);
			const float CY = Y + (Index / 2) * (L.CardH + L.CardGap);
			const int32 Cost = Current->BuildCost(Index);
			DrawPanel(CX, CY, CardW, L.CardH, Index == SelectedBuilding ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
			DrawIcon(Def.IconCol, CX + (CardW - L.CardIcon) * Half, CY + Pad, L.CardIcon);
			DrawLabel(FString::FromInt(Index + 1), CX + Pad, CY + Pad * Half, L.SmallText, YJJUI::Paper);
			const int32 Owned = Current->BuildingCount(Index);
			if (Owned > 0)
			{
				DrawLabel(YJJLoc::Format(TEXT("UI.Owned"), { Owned }), CX + CardW - Pad, CY + Pad * Half, L.SmallText, YJJUI::Highlight, 1.f);
			}
			DrawLabel(YJJLoc::Get(Def.NameKey), CX + CardW * Half, CY + Pad + L.CardIcon, L.SmallText, YJJUI::Paper, Half, CardW - Pad);
			const float CostY = CY + L.CardH - Pad - L.SmallText;
			DrawIcon(FMath::RoundToInt(L.UiCoin), CX + Pad, CostY, L.SmallText);
			DrawLabel(FormatCount(Cost), CX + CardW - Pad, CostY, L.SmallText, Current->Gold >= Cost ? YJJUI::Gold : YJJUI::Bad, 1.f);
			AddButton(EButton::BuildCard, Index, CX, CY, CardW, L.CardH);
		}
		const int32 Rows = FMath::DivideAndRoundUp(Defs.Num(), 2);
		Y += Rows * (L.CardH + L.CardGap) - L.CardGap + L.SectionGap;
	}

	// ---- 스펠 Q·E + 특성 T (3칸) ----
	{
		const int32 NumSpells = static_cast<int32>(EYJJSpell::Count);
		const int32 Slots = NumSpells + 1;
		const float SlotW = (W - L.CardGap * (Slots - 1)) / Slots;
		const float H = L.SpellH;
		for (int32 Index = 0; Index < NumSpells; ++Index)
		{
			const EYJJSpell Spell = static_cast<EYJJSpell>(Index);
			const float SX = X + Index * (SlotW + L.CardGap);
			const bool bReady = Current->CanCast(Spell);
			const bool bArmed = Spell == EYJJSpell::Meteor && bAimingMeteor;
			DrawPanel(SX, Y, SlotW, H, bArmed ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
			const int32 IconCell = FMath::RoundToInt(Spell == EYJJSpell::Meteor ? L.UiMeteor : L.UiRally);
			DrawIcon(IconCell, SX + (SlotW - L.CardIcon) * Half, Y + Pad, L.CardIcon, bReady ? FLinearColor::White : YJJUI::Muted);
			DrawLabel(YJJLoc::Get(Spell == EYJJSpell::Meteor ? TEXT("Key.Meteor") : TEXT("Key.Rally")), SX + Pad, Y + Pad * Half, L.SmallText, YJJUI::Paper);
			DrawLabel(YJJLoc::Get(Spell == EYJJSpell::Meteor ? TEXT("Spell.Meteor") : TEXT("Spell.Rally")), SX + SlotW * Half, Y + Pad + L.CardIcon, L.SmallText, YJJUI::Paper, Half, SlotW - Pad);
			if (!bReady && !Current->bGameOver)
			{
				const float Fraction = Current->CooldownLeft(Spell) / FMath::Max(Current->CooldownMax(Spell), KINDA_SMALL_NUMBER);
				DrawBox(SX + Pad, Y + Pad + L.CardIcon * (1.f - Fraction), SlotW - Pad * 2.f, L.CardIcon * Fraction, YJJUI::Shade);
				DrawLabel(FString::FromInt(FMath::CeilToInt(Current->CooldownLeft(Spell))), SX + SlotW * Half, Y + Pad + (L.CardIcon - L.MidText) * Half, L.MidText, YJJUI::Paper, Half);
			}
			AddButton(EButton::Spell, Index, SX, Y, SlotW, H);
		}

		// 특성 성좌: 찍을 수 있으면 반짝, 열려 있으면 닫기
		const float TX = X + NumSpells * (SlotW + L.CardGap);
		const bool bCanPerk = Current->Souls >= Current->PerkCost();
		const float Glow = bCanPerk && !bTreeOpen ? Half + Half * FMath::Sin(GetWorld()->GetRealTimeSeconds() * L.GlowSpeed) : 0.f;
		DrawPanel(TX, Y, SlotW, H, bTreeOpen ? YJJUI::Highlight : FMath::Lerp(YJJUI::PanelTint, YJJUI::Soul, Glow), FMath::RoundToInt(L.UiCard));
		DrawIcon(FMath::RoundToInt(L.UiStar), TX + (SlotW - L.CardIcon) * Half, Y + Pad, L.CardIcon);
		DrawLabel(YJJLoc::Get(TEXT("Key.Tree")), TX + Pad, Y + Pad * Half, L.SmallText, YJJUI::Paper);
		DrawLabel(YJJLoc::Get(bTreeOpen ? TEXT("UI.TreeClose") : TEXT("UI.Tree")), TX + SlotW * Half, Y + Pad + L.CardIcon, L.SmallText, YJJUI::Paper, Half, SlotW - Pad);
		AddButton(bTreeOpen ? EButton::TreeClose : EButton::Tree, 0, TX, Y, SlotW, H);
		Y += H + L.SectionGap;
	}
}
