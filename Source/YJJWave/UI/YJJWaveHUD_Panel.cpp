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
		// 스테이지 이름 / 웨이브 a/b, 남은 시간
		DrawLabel(YJJLoc::Get(Current->StageDef().NameKey), X + Pad, Y + Pad, L.SmallText, YJJUI::Muted);
		const float WaveY = Y + Pad + L.SmallText;
		DrawLabel(YJJLoc::Format(TEXT("UI.Level"), { Current->Level, Current->NumWaves() }), X + Pad, WaveY, L.BigText, YJJUI::Paper);
		const int32 Seconds = FMath::CeilToInt(FMath::Max(Current->LevelDuration() - Current->LevelTime, 0.f));
		DrawLabel(YJJLoc::Format(TEXT("UI.Timer"), { Seconds / 60, FString::Printf(TEXT("%02d"), Seconds % 60) }), X + W - Pad, WaveY, L.BigText, YJJUI::Highlight, 1.f);
		DrawBar(X + Pad, Y + H - Pad - L.BarH, W - Pad * 2.f, L.BarH, Current->LevelTime / FMath::Max(Current->LevelDuration(), KINDA_SMALL_NUMBER), YJJUI::LevelBar);
		Y += H + L.SectionGap;
	}

	// ---- 아군 VS 적, 영웅 체력 ----
	{
		const float H = L.VsH;
		const float Icon = L.IconSize;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawIcon(FMath::RoundToInt(L.UiSword), X + Pad, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Units.Num(EYJJTeam::Player)), X + Pad + Icon + L.IconGap, Y + Pad + Icon * Half, L.BigText, YJJUI::Ally, 0.f);
		DrawLabelCentered(YJJLoc::Get(TEXT("UI.VS")), X + W * Half, Y + Pad + Icon * Half, L.MidText, YJJUI::Paper);
		DrawIcon(FMath::RoundToInt(L.UiSkull), X + W - Pad - Icon, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Units.Num(EYJJTeam::Enemy)), X + W - Pad - Icon - L.IconGap, Y + Pad + Icon * Half, L.BigText, YJJUI::Enemy, 1.f);

		// 영웅 체력: 숫자가 들어가는 게이지는 숫자 크기에 맞춰 두껍게
		const float BarH = L.TextBarH;
		const float BarY = Y + H - Pad - BarH;
		const float BarX = X + Pad + BarH + L.IconGap;
		DrawIcon(FMath::RoundToInt(L.UiHero), X + Pad, BarY, BarH);
		DrawBar(BarX, BarY, X + W - Pad - BarX, BarH, Units.HeroHP / FMath::Max(Units.HeroMaxHP, KINDA_SMALL_NUMBER), YJJUI::HeroBar);
		DrawBarLabel(YJJLoc::Format(TEXT("UI.HeroHP"), { FormatCount(FMath::CeilToFloat(Units.HeroHP)), FormatCount(Units.HeroMaxHP) }), BarX, BarY, X + W - Pad - BarX, BarH);
		Y += H + L.SectionGap;
	}

	// ---- 보스 (있을 때만) ----
	if (Units.Boss != INDEX_NONE)
	{
		const float H = L.BossH;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawLabelCentered(YJJLoc::Get(YJJData::Unit(Units.Type[Units.Boss]).NameKey), X + Pad + (W * L.BossBarSplit) * Half, Y + H * Half, L.MidText, YJJUI::BossText, W * L.BossBarSplit);
		const float BarX = X + W * L.BossBarSplit + Pad;
		const float BarY = Y + (H - L.TextBarH) * Half;
		DrawBar(BarX, BarY, X + W - Pad - BarX, L.TextBarH, Units.BossHP / FMath::Max(Units.BossMaxHP, KINDA_SMALL_NUMBER), YJJUI::BossBar);
		DrawBarLabel(FormatCount(FMath::CeilToFloat(Units.BossHP)), BarX, BarY, X + W - Pad - BarX, L.TextBarH);
		Y += H + L.SectionGap;
	}

	// ---- 금화·영혼·배속 ----
	{
		const float H = L.ResH;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		const float RowH = (H - Pad * 2.f) * Half;
		DrawIcon(FMath::RoundToInt(L.UiCoin), X + Pad, Y + Pad, RowH);
		DrawLabelLeftMiddle(FormatCount(Current->Gold), X + Pad + RowH + L.IconGap, Y + Pad + RowH * Half, L.SmallText, YJJUI::Gold, 0.f);
		DrawIcon(FMath::RoundToInt(L.UiSoul), X + Pad, Y + Pad + RowH, RowH);
		DrawLabelLeftMiddle(FormatCount(Current->Souls), X + Pad + RowH + L.IconGap, Y + Pad + RowH * 1.5f, L.SmallText, YJJUI::Soul, 0.f);

		const FString Speed = Mode && Mode->IsPaused() ? YJJLoc::Get(TEXT("UI.Paused")) : YJJLoc::Format(TEXT("UI.Speed"), { Mode ? FMath::RoundToInt(Mode->GetSpeed()) : 1 });
		const float SpeedW = L.SpeedButtonW;
		const float SpeedX = X + W - Pad - SpeedW;
		DrawPanel(SpeedX, Y + Pad, SpeedW, H - Pad * 2.f, YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabelCentered(Speed, SpeedX + SpeedW * Half, Y + H * Half, L.MidText, YJJUI::Paper, SpeedW - Pad);
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
			DrawLabelCentered(YJJLoc::Get(Def.NameKey), CX + CardW * Half, CY + Pad + L.CardIcon + L.SmallText * Half, L.SmallText, YJJUI::Paper, CardW - Pad);
			// 금화 아이콘과 비용 숫자를 같은 높이 가운데로
			const float CostCY = CY + L.CardH - Pad - L.SmallText * Half;
			DrawIcon(FMath::RoundToInt(L.UiCoin), CX + Pad, CostCY - L.SmallText * Half, L.SmallText);
			DrawLabelLeftMiddle(FormatCount(Cost), CX + CardW - Pad, CostCY, L.SmallText, Current->Gold >= Cost ? YJJUI::Gold : YJJUI::Bad, 1.f);
			AddButton(EButton::BuildCard, Index, CX, CY, CardW, L.CardH);
			if (Index == BuyPopupFor)
			{
				BuyPopupY = CY + L.CardH;
			}
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
			DrawLabelCentered(YJJLoc::Get(Spell == EYJJSpell::Meteor ? TEXT("Spell.Meteor") : TEXT("Spell.Rally")), SX + SlotW * Half, Y + Pad + L.CardIcon + L.SmallText * Half, L.SmallText, YJJUI::Paper, SlotW - Pad);
			if (!bReady && !Current->bGameOver)
			{
				const float Fraction = Current->CooldownLeft(Spell) / FMath::Max(Current->CooldownMax(Spell), KINDA_SMALL_NUMBER);
				DrawBox(SX + Pad, Y + Pad + L.CardIcon * (1.f - Fraction), SlotW - Pad * 2.f, L.CardIcon * Fraction, YJJUI::Shade);
				DrawLabelCentered(FString::FromInt(FMath::CeilToInt(Current->CooldownLeft(Spell))), SX + SlotW * Half, Y + Pad + L.CardIcon * Half, L.MidText, YJJUI::Paper);
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
		DrawLabelCentered(YJJLoc::Get(bTreeOpen ? TEXT("UI.TreeClose") : TEXT("UI.Tree")), TX + SlotW * Half, Y + Pad + L.CardIcon + L.SmallText * Half, L.SmallText, YJJUI::Paper, SlotW - Pad);
		AddButton(bTreeOpen ? EButton::TreeClose : EButton::Tree, 0, TX, Y, SlotW, H);
		Y += H + L.SectionGap;
	}

	// ---- 옵션 (맨 아래, 성능 줄 위) ----
	{
		const float H = L.OptionsOpenH;
		const float OY = VirtualHeight - Pad - L.TinyText * 2.f - L.DebugLineGap - Pad - H;
		DrawPanel(X, OY, W, H, bOptionsOpen ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawLabelCentered(YJJLoc::Get(TEXT("UI.Options")), X + W * Half, OY + H * Half, L.MidText, YJJUI::Paper);
		AddButton(EButton::OpenOptions, 0, X, OY, W, H);
	}

	DrawBuyPopup(X, W);
}

void AYJJWaveHUD::DrawBuyPopup(float X, float W)
{
	// 병영 카드를 누르면 그 카드 바로 아래에 x3 · x5 · x10 + 금화 (다른 카드를 누르면 사라진다)
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	if (!YJJData::Buildings().IsValidIndex(BuyPopupFor))
	{
		return;
	}
	const float Pad = L.PanelPad;
	const int32 NumButtons = UE_ARRAY_COUNT(BuyCountKeys);
	const float H = L.BuyH;
	const float Y = BuyPopupY + L.BuyPopupGap;
	DrawPanel(X - Pad * Half, Y - Pad * Half, W + Pad, H + Pad, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	const float ButtonW = (W - L.CardGap * (NumButtons - 1)) / NumButtons;
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		const int32 Count = FMath::RoundToInt(BuyCountOf(Index));
		const int32 Cost = Current->BuyCost(BuyPopupFor, Count);
		const bool bCan = Current->CanBuy(BuyPopupFor, Count);
		const float BX = X + Index * (ButtonW + L.CardGap);
		DrawPanel(BX, Y, ButtonW, H, bCan ? YJJUI::PanelTint : YJJUI::Muted, FMath::RoundToInt(L.UiCard));
		const float RowH = H * Half;
		DrawLabelCentered(YJJLoc::Format(TEXT("UI.BuyCount"), { Count }), BX + ButtonW * Half, Y + RowH * L.BuyTopRow, L.MidText, YJJUI::Paper, ButtonW - Pad);
		const float CostCY = Y + H - RowH * L.BuyBottomRow;
		DrawIcon(FMath::RoundToInt(L.UiCoin), BX + Pad, CostCY - L.SmallText * Half, L.SmallText);
		DrawLabelLeftMiddle(FormatCount(Cost), BX + ButtonW - Pad, CostCY, L.SmallText, bCan ? YJJUI::Gold : YJJUI::Bad, 1.f);
		AddButton(EButton::Buy, Count, BX, Y, ButtonW, H);
	}
}

float AYJJWaveHUD::BuyCountOf(int32 Index) const
{
	const FYJJUILayout& L = UI();
	return Index == 0 ? L.BuyCountA : Index == 1 ? L.BuyCountB : L.BuyCountC;
}
