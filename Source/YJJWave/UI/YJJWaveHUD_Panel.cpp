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

	// ---- 아군 VS 적 (영웅·보스 체력은 머리 위) ----
	{
		const float H = L.IconSize + Pad * 2.f;
		const float Icon = L.IconSize;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawIcon(FMath::RoundToInt(L.UiSword), X + Pad, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Units.Num(EYJJTeam::Player)), X + Pad + Icon + L.IconGap, Y + Pad + Icon * Half, L.BigText, YJJUI::Ally, 0.f);
		DrawLabelCentered(YJJLoc::Get(TEXT("UI.VS")), X + W * Half, Y + Pad + Icon * Half, L.MidText, YJJUI::Paper);
		DrawIcon(FMath::RoundToInt(L.UiSkull), X + W - Pad - Icon, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Units.Num(EYJJTeam::Enemy)), X + W - Pad - Icon - L.IconGap, Y + Pad + Icon * Half, L.BigText, YJJUI::Enemy, 1.f);
		Y += H + L.SectionGap;
	}

	// ---- 성벽 체력 (0이 되면 패배) ----
	{
		const float H = L.TextBarH + Pad * 2.f;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		DrawLabelLeftMiddle(YJJLoc::Get(TEXT("UI.Wall")), X + Pad, Y + H * Half, L.SmallText, YJJUI::Paper, 0.f);
		const float BarX = X + Pad + L.WallLabelW;
		const float BarW = X + W - Pad - BarX;
		DrawBar(BarX, Y + Pad, BarW, L.TextBarH, Current->WallHP / FMath::Max(Current->WallMaxHP, KINDA_SMALL_NUMBER), YJJUI::Ally);
		DrawBarLabel(YJJLoc::Format(TEXT("UI.WallHP"), { FormatCount(FMath::CeilToFloat(Current->WallHP)), FormatCount(Current->WallMaxHP) }), BarX, Y + Pad, BarW, L.TextBarH);
		Y += H + L.SectionGap;
	}

	// ---- 금화 · 영혼 (한 줄에 나란히) ----
	{
		const float H = L.ResRowH + Pad * 2.f;
		DrawPanel(X, Y, W, H, YJJUI::PanelTint, DarkCell);
		const float Icon = L.ResRowH;
		const float CY = Y + Pad + Icon * Half;
		DrawIcon(FMath::RoundToInt(L.UiCoin), X + Pad, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Current->Gold), X + Pad + Icon + L.IconGap, CY, L.MidText, YJJUI::Gold, 0.f);
		DrawIcon(FMath::RoundToInt(L.UiSoul), X + W * Half + Pad, Y + Pad, Icon);
		DrawLabelLeftMiddle(FormatCount(Current->Souls), X + W * Half + Pad + Icon + L.IconGap, CY, L.MidText, YJJUI::Soul, 0.f);
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

	// ---- 주문 F1·F2·F3 (3칸) + 특성 T (가로 막대) ----
	{
		const int32 NumSpells = static_cast<int32>(EYJJSpell::Count);
		const float SlotW = (W - L.CardGap * (NumSpells - 1)) / NumSpells;
		const float H = L.SpellH;
		for (int32 Index = 0; Index < NumSpells; ++Index)
		{
			const EYJJSpell Spell = static_cast<EYJJSpell>(Index);
			const float SX = X + Index * (SlotW + L.CardGap);
			const bool bReady = Current->CanCast(Spell);
			const bool bArmed = AimingSpell == Index;
			const FString Key = YJJData::SpellKey(Spell);
			DrawPanel(SX, Y, SlotW, H, bArmed ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
			DrawIcon(SpellIcon(Spell), SX + (SlotW - L.CardIcon) * Half, Y + Pad, L.CardIcon, bReady ? FLinearColor::White : YJJUI::Muted);
			DrawLabel(YJJLoc::Get(TEXT("Key.") + Key), SX + Pad, Y + Pad * Half, L.SmallText, YJJUI::Paper);
			DrawLabel(YJJLoc::Format(TEXT("Spell.Lv"), { Current->SpellLevel(Spell) }), SX + SlotW - Pad, Y + Pad * Half, L.SmallText, YJJUI::Highlight, 1.f);
			DrawLabelCentered(YJJLoc::Get(TEXT("Spell.") + Key), SX + SlotW * Half, Y + Pad + L.CardIcon + L.SmallText * Half, L.SmallText, YJJUI::Paper, SlotW - Pad);
			if (!bReady && !Current->bGameOver)
			{
				const float Fraction = Current->CooldownLeft(Spell) / FMath::Max(Current->CooldownMax(Spell), KINDA_SMALL_NUMBER);
				DrawBox(SX + Pad, Y + Pad + L.CardIcon * (1.f - Fraction), SlotW - Pad * 2.f, L.CardIcon * Fraction, YJJUI::Shade);
				if (Spell != EYJJSpell::Ladle)
				{
					DrawLabelCentered(FString::FromInt(FMath::CeilToInt(Current->CooldownLeft(Spell))), SX + SlotW * Half, Y + Pad + L.CardIcon * Half, L.MidText, YJJUI::Paper);
				}
			}
			if (Spell == EYJJSpell::Ladle)
			{
				// 국자는 횟수: 그림 위에 "남은 수", 다음 하나가 차오르는 막대는 그림 아래
				DrawLabelCentered(YJJLoc::Format(TEXT("Spell.Charges"), { FMath::FloorToInt(Current->SmackCharges()), Current->SmackMax() }), SX + SlotW * Half, Y + Pad + L.CardIcon * Half, L.MidText, YJJUI::Highlight);
				const float Next = FMath::Frac(Current->SmackCharges());
				if (Current->SmackCharges() < Current->SmackMax())
				{
					DrawBar(SX + Pad, Y + Pad + L.CardIcon - L.SkillChargeBarH, SlotW - Pad * 2.f, L.SkillChargeBarH, Next, YJJUI::Highlight);
				}
			}
			AddButton(EButton::Spell, Index, SX, Y, SlotW, H);
		}

		Y += H + L.CardGap;

		// 특성 성좌: 가로 막대. 찍을 수 있으면 반짝, 열려 있으면 닫기
		const float TH = L.TreeBarH;
		const bool bCanPerk = Current->Souls >= Current->PerkCost();
		const float Glow = bCanPerk && !bTreeOpen ? Half + Half * FMath::Sin(GetWorld()->GetRealTimeSeconds() * L.GlowSpeed) : 0.f;
		DrawPanel(X, Y, W, TH, bTreeOpen ? YJJUI::Highlight : FMath::Lerp(YJJUI::PanelTint, YJJUI::Soul, Glow), FMath::RoundToInt(L.UiCard));
		DrawIcon(FMath::RoundToInt(L.UiStar), X + Pad, Y + (TH - L.TreeBarIcon) * Half, L.TreeBarIcon);
		DrawLabelCentered(YJJLoc::Get(bTreeOpen ? TEXT("UI.TreeClose") : TEXT("UI.Tree")), X + W * Half, Y + TH * Half, L.MidText, YJJUI::Paper, W - Pad * 2.f - L.TreeBarIcon * 2.f);
		AddButton(bTreeOpen ? EButton::TreeClose : EButton::Tree, 0, X, Y, W, TH);
		Y += TH + L.SectionGap;
	}

	// ---- 냄비 + 넣은 양념 ----
	Y += DrawPotPanel(X, Y, W) + L.SectionGap;
	Y += DrawSeasonings(X, Y, W) + L.SectionGap;


	DrawBuyPopup(X, W);
	DrawSpeedButton();
}

void AYJJWaveHUD::DrawSpeedButton()
{
	// 화면 오른쪽 위: 배속 (누르면 바뀜) · 일시정지 표시
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const FYJJUILayout& L = UI();
	const float W = L.SpeedButtonW;
	const float H = L.SpeedButtonH;
	const float X = VirtualW - L.PanelPad - W;
	const float Y = L.PanelPad;
	// 옵션 버튼은 배속 왼쪽
	const float OptionsW = L.OptionsButtonTopW;
	const float OptionsX = X - L.CardGap - OptionsW;
	DrawPanel(OptionsX, Y, OptionsW, H, bOptionsOpen ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
	DrawLabelCentered(YJJLoc::Get(TEXT("UI.Options")), OptionsX + OptionsW * Half, Y + H * Half, L.MidText, YJJUI::Paper, OptionsW - L.PanelPad);
	AddButton(EButton::OpenOptions, 0, OptionsX, Y, OptionsW, H);
	const FString Speed = Mode && Mode->IsPaused() ? YJJLoc::Get(TEXT("UI.Paused")) : YJJLoc::Format(TEXT("UI.Speed"), { Mode ? FMath::RoundToInt(Mode->SpeedToggle()) : 1 });
	// 2배속이면 금빛으로 깜빡여 켜져 있음을 알린다 (누르면 1배속 ↔ 2배속)
	const bool bFast = Mode && !Mode->IsPaused() && Mode->SpeedToggle() > 1.f;
	const float Glow = bFast ? Half + Half * FMath::Sin(AnimClock * L.GlowSpeed) : 0.f;
	DrawPanel(X, Y, W, H, FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Glow), FMath::RoundToInt(L.UiCard));
	DrawLabelCentered(Speed, X + W * Half, Y + H * Half, L.MidText, bFast ? YJJUI::Ink : YJJUI::Paper, W - L.PanelPad);
	AddButton(EButton::Speed, 0, X, Y, W, H);
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
