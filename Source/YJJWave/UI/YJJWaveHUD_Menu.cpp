// 타이틀 ("국거리 디펜스") → 월드맵 (지역 7곳, 지금은 숲지·공동묘지만 열림) → 전투
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"

void AYJJWaveHUD::DrawTexture(UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Color)
{
	if (!Texture || !Texture->GetResource())
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, Texture, 1);
	AddQuad(Batch, ViewOffset + FVector2f(X, Y) * ViewScale, FVector2f(W, H) * ViewScale, FVector2f::ZeroVector, FVector2f::UnitVector, Color, false);
}

void AYJJWaveHUD::DrawUnitSprite(int32 Unit, float CenterX, float FootY, float Size, bool bFaceLeft)
{
	if (!YJJData::Units().IsValidIndex(Unit))
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const FYJJUnitDef& Def = YJJData::Unit(Unit);
	const FYJJAtlas& Atlas = AtlasFor(Def);
	if (!Atlas.Texture || !Atlas.Texture->GetResource())
	{
		return;
	}
	// 숨쉬기 그림 (유닛마다 박자를 조금 다르게)
	const int32 Frames = FMath::Max(FMath::RoundToInt(L.AnimIdleFrames), 1);
	const int32 Frame = FMath::RoundToInt(L.AnimIdleFirst) + FMath::FloorToInt(AnimClock * L.AnimIdleFps + CenterX) % Frames;
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, Atlas.Texture, 1);
	AddQuad(Batch, ViewOffset + FVector2f(CenterX - Size * Half, FootY - Size * L.FootAnchor) * ViewScale, FVector2f(Size * ViewScale), Atlas.UV(Frame, RowFor(Def)), Atlas.CellUV, FLinearColor::White, bFaceLeft);
}

void AYJJWaveHUD::DrawMenuButton(EButton Kind, int32 Index, float X, float Y, float W, float H, const FString& Text, bool bHighlight)
{
	const FYJJButton* Hover = ButtonAt(ToVirtual(MouseScreen()));
	const bool bHover = Hover && Hover->Kind == static_cast<int32>(Kind) && Hover->Index == Index && Hover->Rect.Min == FVector2f(X, Y);
	const FLinearColor Tint = bHighlight ? YJJUI::Highlight : (bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Half) : YJJUI::PanelTint);
	DrawPanel(X, Y, W, H, Tint, FMath::RoundToInt(UI().UiCard));
	DrawLabelCentered(Text, X + W * Half, Y + H * Half, UI().MidText, YJJUI::Paper, W - UI().PanelPad);
	AddButton(Kind, Index, X, Y, W, H);
}

void AYJJWaveHUD::DrawTitle()
{
	const FYJJUILayout& L = UI();
	const float CenterX = VirtualW * Half;

	// 바닥: 첫 스테이지 땅을 화면 가득, 살짝 어둡게
	DrawTexture(GroundFor(0), 0.f, 0.f, VirtualW, VirtualHeight, FLinearColor::White);
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::WithAlpha(YJJUI::Ink, L.TitleShade));

	// 가운데 아래 마지막 냄비 (김이 오른다), 양옆에 졸병 무리와 해골 무리가 노려본다
	const float PotSize = L.TitlePotSize;
	const float PotFoot = VirtualHeight * L.TitlePotY;
	if (BuildingAtlas.Texture && FxAtlas.Texture)
	{
		const FVector2f PotTopLeft(CenterX - PotSize * Half, PotFoot - PotSize * L.PotAnchorY);
		{
			const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, 2);
			AddQuad(Batch, ViewOffset + PotTopLeft * ViewScale, FVector2f(PotSize * ViewScale), BuildingAtlas.UV(FMath::RoundToInt(L.PotCell), 0), BuildingAtlas.CellUV, FLinearColor::White, false);
		}
		const int32 Puffs = FMath::RoundToInt(L.PotSteamPuffs);
		const FYJJQuadBatch Steam = BeginBatch(Canvas->Canvas, FxAtlas.Texture, Puffs);
		const FVector2f Rim = PotTopLeft + FVector2f(PotSize * Half, PotSize * L.PotRimY);
		for (int32 Index = 0; Index < Puffs; ++Index)
		{
			const float Phase = FMath::Frac(AnimClock * Half + static_cast<float>(Index) / FMath::Max(Puffs, 1));
			const float Puff = PotSize * L.TitleSteamScale * FMath::Lerp(Half, 1.f, Phase);
			const FVector2f Spot = Rim + FVector2f(FMath::Sin(Phase * FullTurn + Index) * PotSize * PotSteamSpread, -PotSize * L.TitleSteamRise * Phase);
			AddQuad(Steam, ViewOffset + (Spot - FVector2f(Puff * Half)) * ViewScale, FVector2f(Puff * ViewScale), FxAtlas.UV(FMath::RoundToInt(L.FxSteam)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, L.PotSteamAlpha * FMath::Sin(Phase * UE_PI)), false);
		}
	}
	const int32 Grunt = YJJData::Buildings().Num() > 0 ? YJJData::Building(0).Unit : INDEX_NONE;
	const int32 Skeleton = YJJData::Units().IndexOfByPredicate([](const FYJJUnitDef& Def) { return Def.Team == EYJJTeam::Enemy && !Def.bBoss; });
	const int32 Rows = FMath::RoundToInt(L.TitleArmyRows);
	const int32 Cols = FMath::RoundToInt(L.TitleArmyCols);
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float FootY = VirtualHeight * L.TitleArmyY + Row * L.TitleArmyRowGap;
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const float Offset = PotSize * Half + L.TitleArmyGap * (Col + Half) + Row * L.TitleArmyGap * Half;
			DrawUnitSprite(Grunt, CenterX - Offset, FootY, L.TitleUnitSize, false);
			DrawUnitSprite(Skeleton, CenterX + Offset, FootY, L.TitleUnitSize, true);
		}
	}

	// 제목 + 한 줄
	const float TitleY = VirtualHeight * L.TitleLogoY;
	DrawLabelCentered(YJJLoc::Get(TEXT("Title.Name")), CenterX, TitleY, L.TitleLogoSize, YJJUI::Highlight, VirtualW - L.PanelPad * 4.f);
	DrawLabelCentered(YJJLoc::Get(TEXT("Title.Tagline")), CenterX, TitleY + L.TitleLogoSize * Half + L.TitleTaglineGap, L.BigText, YJJUI::Paper);

	// 버튼: 시작 · 옵션 · 종료
	const float W = L.TitleButtonW;
	const float H = L.TitleButtonH;
	float Y = VirtualHeight * L.TitleButtonsY;
	DrawMenuButton(EButton::TitleStart, 0, CenterX - W * Half, Y, W, H, YJJLoc::Get(TEXT("Title.Start")), true);
	Y += H + L.TitleButtonGap;
	DrawMenuButton(EButton::OpenOptions, 0, CenterX - W * Half, Y, W, H, YJJLoc::Get(TEXT("UI.Options")), false);
	Y += H + L.TitleButtonGap;
	DrawMenuButton(EButton::TitleQuit, 0, CenterX - W * Half, Y, W, H, YJJLoc::Get(TEXT("Title.Quit")), false);
	DrawLabel(YJJLoc::Get(TEXT("Title.Hint")), VirtualW - L.PanelPad, VirtualHeight - L.PanelPad - L.TinyText, L.TinyText, YJJUI::Muted, 1.f);
}

void AYJJWaveHUD::DrawWorldMap()
{
	const FYJJUILayout& L = UI();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (!Mode)
	{
		return;
	}
	DrawTexture(WorldMapTexture, 0.f, 0.f, VirtualW, VirtualHeight, FLinearColor::White);
	const FVector2f MapSize(VirtualW, VirtualHeight);

	// 아직 없는 지역은 구름이 덮는다 (천천히 일렁인다). 열리면 걷힌다
	if (FxAtlas.Texture)
	{
		const int32 Puffs = FMath::RoundToInt(L.RegionCloudPuffs);
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, Puffs * YJJData::Regions().Num());
		for (const FYJJRegionDef& Region : YJJData::Regions())
		{
			if (Region.bOpen)
			{
				continue;
			}
			const FVector2f P = Region.MapPos * MapSize;
			for (int32 Index = 0; Index < Puffs * RegionCloudLayers; ++Index)
			{
				// 가운데 하나 + 안쪽·바깥 고리 (겹쳐 짙게), 번호마다 다른 박자로 부풀었다 줄었다
				const int32 Slot = Index % Puffs;
				const float Angle = Slot * FullTurn / FMath::Max(Puffs - 1, 1) + Index;
				const float Ring = Slot == 0 ? 0.f : L.RegionCloudRadius * (Slot % 2 ? Half : 1.f);
				const float Breath = 1.f + FMath::Sin(AnimClock * L.RegionCloudSpeed + Index) * L.RegionCloudBreath;
				const float Size = L.RegionCloudSize * Breath;
				const FVector2f Spot = P + FVector2f(FMath::Cos(Angle) * Ring * L.RegionCloudWide, FMath::Sin(Angle) * Ring);
				AddQuad(Batch, ViewOffset + (Spot - FVector2f(Size * Half)) * ViewScale, FVector2f(Size * ViewScale), FxAtlas.UV(FMath::RoundToInt(L.FxSteam)), FxAtlas.CellUV, FLinearColor::White, false);
			}
		}
	}
	// 지역 이름 (어두운 띠 위), 잠긴 곳은 "준비 중"
	for (const FYJJRegionDef& Region : YJJData::Regions())
	{
		const FVector2f P = Region.MapPos * MapSize;
		const FString Name = YJJLoc::Get(Region.NameKey);
		float NameW = 0.f;
		MakeFont(Name, L.BigText, 1.f, 0.f, NameW);
		const float BandW = NameW + L.PanelPad * 2.f;
		const float BandY = P.Y - L.RegionLabelLift - L.BigText * Half - L.PanelPad * Half;
		DrawPanel(P.X - BandW * Half, BandY, BandW, L.BigText + L.PanelPad, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
		DrawLabelCentered(Name, P.X, P.Y - L.RegionLabelLift, L.BigText, Region.bOpen ? YJJUI::Highlight : YJJUI::Muted);
		if (!Region.bOpen)
		{
			DrawLabelCentered(YJJLoc::Get(TEXT("Map.Soon")), P.X, P.Y, L.MidText, YJJUI::Ink);
		}
	}

	// 스테이지 길 + 자리: 깬 곳은 초록, 열린 곳은 노랑(반짝), 잠긴 곳은 회색. 아래에 이름과 별
	const TConstArrayView<FYJJStageDef> Stages = YJJData::Stages();
	for (int32 Stage = 1; Stage < Stages.Num(); ++Stage)
	{
		DrawLine(Stages[Stage - 1].MapPos * MapSize, Stages[Stage].MapPos * MapSize, YJJUI::WithAlpha(YJJUI::Ink, L.StagePathAlpha), L.StagePathWidth);
	}
	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float Pulse = Half + Half * FMath::Sin(AnimClock * L.GlowSpeed);
	for (int32 Stage = 0; Stage < Stages.Num(); ++Stage)
	{
		const FVector2f P = Stages[Stage].MapPos * MapSize;
		const bool bOpen = Mode->IsStageOpen(Stage);
		const bool bCleared = Mode->StageStars(Stage) > 0;
		const bool bHover = bOpen && FVector2f::Distance(Mouse, P) <= L.StageNodeSize * Half;
		const float Size = L.StageNodeSize * (bHover ? L.StageNodeHover : 1.f);
		const FLinearColor Color = !bOpen ? YJJUI::PerkLocked : bCleared ? YJJUI::Good : FMath::Lerp(YJJUI::Highlight, FLinearColor::White, Pulse * Half);
		DrawIcon(FMath::RoundToInt(L.UiBadge), P.X - Size * Half, P.Y - Size * Half, Size, Color);
		DrawLabelCentered(FString::FromInt(Stage + 1), P.X, P.Y, L.BigText, bOpen ? YJJUI::Ink : YJJUI::Muted);
		DrawLabelCentered(YJJLoc::Get(Stages[Stage].NameKey), P.X, P.Y + Size * Half + L.StageNameGap, L.MidText, bOpen ? YJJUI::Paper : YJJUI::Muted);
		const int32 MaxStars = FMath::RoundToInt(L.ResultStarCount);
		const float Star = L.StageStarSize;
		const float StarsLeft = P.X - MaxStars * Star * Half;
		for (int32 Index = 0; Index < MaxStars; ++Index)
		{
			const bool bEarned = Index < Mode->StageStars(Stage);
			DrawIcon(FMath::RoundToInt(L.UiStar), StarsLeft + Index * Star, P.Y + Size * Half + L.StageNameGap + L.MidText, Star, bEarned ? YJJUI::PerkTaken : YJJUI::WithAlpha(YJJUI::Ink, L.ResultStarEmptyAlpha));
		}
		if (bOpen && MapPanel == EYJJMapPanel::None)
		{
			AddButton(EButton::WorldStage, Stage, P.X - Size * Half, P.Y - Size * Half, Size, Size);
		}
	}

	// 머리글 + 타이틀로
	const float Pad = L.PanelPad;
	// 머리글은 어두운 판 위에 (지도 가장자리 구름이 희어서)
	DrawPanel(Pad, Pad, L.MapHeaderW, L.TitleText + L.SmallText + L.TextPad * 2.f + Pad, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	DrawLabel(YJJLoc::Get(TEXT("Map.Title")), Pad + L.TextPad, Pad + L.TextPad, L.TitleText, YJJUI::Highlight);
	DrawLabel(YJJLoc::Get(TEXT("Map.Hint")), Pad + L.TextPad, Pad + L.TextPad + L.TitleText + Pad, L.SmallText, YJJUI::Paper);
	DrawMenuButton(EButton::WorldBack, 0, VirtualW - Pad - L.TitleButtonW, Pad, L.TitleButtonW, L.TitleButtonH, YJJLoc::Get(TEXT("Map.Back")), false);
}

void AYJJWaveHUD::DrawMapButtons()
{
	// 월드맵 아래 가운데: 영웅 · 스킬 · 특성 (누르면 창, 다시 누르면 닫힘)
	const FYJJUILayout& L = UI();
	const TCHAR* Keys[MapPanelCount] = { TEXT("Map.Heroes"), TEXT("Map.Skills"), TEXT("Map.Perks") };
	const float Total = MapPanelCount * L.MapButtonW + (MapPanelCount - 1) * L.MapButtonGap;
	const float Y = VirtualHeight - L.MapButtonH - L.MapButtonMargin;
	for (int32 Index = 0; Index < MapPanelCount; ++Index)
	{
		const float X = VirtualW * Half - Total * Half + Index * (L.MapButtonW + L.MapButtonGap);
		DrawMenuButton(EButton::MapPanel, Index, X, Y, L.MapButtonW, L.MapButtonH, YJJLoc::Get(Keys[Index]), MapPanel == static_cast<EYJJMapPanel>(Index));
	}
}

void AYJJWaveHUD::DrawHeroLineup()
{
	// 영웅 편성: 위에 자리 둘 (2번 자리는 잠김), 아래 영웅 카드를 누르면 1번 자리에
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const TConstArrayView<FYJJHeroDef> Heroes = YJJData::Heroes();
	if (!Mode || Heroes.Num() == 0)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const float Pad = L.TextPad;
	const int32 Picked = YJJData::HeroIndex(Mode->GetProgress().Hero);
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::Shade);
	const float CenterX = VirtualW * Half;
	DrawLabel(YJJLoc::Get(TEXT("UI.HeroSelectTitle")), CenterX, L.MapPanelTitleY, L.TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Get(TEXT("UI.HeroSelectSub")), CenterX, L.MapPanelTitleY + L.TitleText + L.CardSubGap, L.SmallText, YJJUI::Paper, Half);
	// 자리 둘
	const float SlotsW = HeroSlotCount * L.HeroSlotW + (HeroSlotCount - 1) * L.HeroSelectGap;
	for (int32 SlotIndex = 0; SlotIndex < HeroSlotCount; ++SlotIndex)
	{
		const float X = CenterX - SlotsW * Half + SlotIndex * (L.HeroSlotW + L.HeroSelectGap);
		const float Y = L.HeroSlotY;
		const bool bOpen = SlotIndex == 0;
		DrawPanel(X, Y, L.HeroSlotW, L.HeroSlotH, bOpen ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Quarter) : YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
		DrawLabel(YJJLoc::Format(TEXT("UI.HeroSlot"), { SlotIndex + 1 }), X + Pad, Y + Pad * Half, L.SmallText, YJJUI::Highlight);
		if (bOpen)
		{
			const FYJJHeroDef& Hero = Heroes[Picked];
			const float Face = L.HeroSlotH - Pad * 2.f - L.SmallText;
			DrawIcon(Hero.Portrait, X + (L.HeroSlotW - Face) * Half, Y + Pad + L.SmallText * Half, Face);
			DrawLabel(YJJLoc::Get(YJJData::Unit(Hero.Unit).NameKey), X + L.HeroSlotW * Half, Y + L.HeroSlotH - Pad - L.SmallText, L.SmallText, YJJUI::Paper, Half);
		}
		else
		{
			DrawLabelCentered(YJJLoc::Format(TEXT("UI.HeroSlotLocked"), { YJJData::Tune().HeroSlot2Stage }), X + L.HeroSlotW * Half, Y + L.HeroSlotH * Half, L.SmallText, YJJUI::Muted, L.HeroSlotW - Pad * 2.f);
		}
	}
	// 영웅 카드
	const float CardW = L.HeroSelectCardW;
	const float CardH = L.HeroLineupCardH;
	const float Total = Heroes.Num() * CardW + (Heroes.Num() - 1) * L.HeroSelectGap;
	const float Y = L.HeroSelectY;
	for (int32 Index = 0; Index < Heroes.Num(); ++Index)
	{
		const FYJJHeroDef& Hero = Heroes[Index];
		const float X = CenterX - Total * Half + Index * (CardW + L.HeroSelectGap);
		const bool bPicked = Index == Picked;
		const bool bHover = HoverButton(EButton::HeroPick) && ButtonAt(ToVirtual(MouseScreen()))->Index == Index;
		DrawPanel(X, Y, CardW, CardH, bPicked ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Quarter) : (bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Quarter * Half) : YJJUI::PanelTint), FMath::RoundToInt(L.UiCard));
		float RowY = Y + Pad;
		DrawIcon(Hero.Portrait, X + (CardW - L.HeroSelectPortrait) * Half, RowY, L.HeroSelectPortrait);
		RowY += L.HeroSelectPortrait + Pad * Half;
		DrawLabel(YJJLoc::Get(YJJData::Unit(Hero.Unit).NameKey), X + CardW * Half, RowY, L.BigText, bPicked ? YJJUI::Highlight : YJJUI::Paper, Half, CardW - Pad * 2.f);
		RowY += L.BigText + Pad * Half;
		for (const FString& Line : WrapText(YJJLoc::Get(Hero.DescKey), L.SmallText, CardW - Pad * 2.f))
		{
			DrawLabel(Line, X + CardW * Half, RowY, L.SmallText, YJJUI::Paper, Half);
			RowY += L.TipLineH;
		}
		AddButton(EButton::HeroPick, Index, X, Y, CardW, CardH);
	}
}

void AYJJWaveHUD::DrawHeroSkills()
{
	// 고른 영웅의 스킬 셋: 영웅 AI 가 상황(적 수·강함)을 보고 알아서 쓴다
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (!Mode || YJJData::Heroes().Num() == 0)
	{
		return;
	}
	const FYJJHeroDef& Hero = YJJData::Heroes()[YJJData::HeroIndex(Mode->GetProgress().Hero)];
	const FYJJUILayout& L = UI();
	const float Pad = L.TextPad;
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::Shade);
	const float CenterX = VirtualW * Half;
	DrawLabel(YJJLoc::Format(TEXT("UI.SkillsTitle"), { YJJLoc::Get(YJJData::Unit(Hero.Unit).NameKey) }), CenterX, L.MapPanelTitleY, L.TitleText, YJJUI::Highlight, Half);
	DrawLabel(YJJLoc::Get(TEXT("UI.SkillsSub")), CenterX, L.MapPanelTitleY + L.TitleText + L.CardSubGap, L.SmallText, YJJUI::Paper, Half);
	const int32 Count = Hero.Skills.Num();
	const float CardW = L.HeroSelectCardW;
	const float CardH = L.HeroSelectCardH;
	const float Total = Count * CardW + (Count - 1) * L.HeroSelectGap;
	const float Y = L.HeroSlotY;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FYJJSkillDef& Skill = YJJData::Skill(Hero.Skills[Index]);
		const float X = CenterX - Total * Half + Index * (CardW + L.HeroSelectGap);
		DrawPanel(X, Y, CardW, CardH, YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		float RowY = Y + Pad;
		DrawIcon(Skill.Icon, X + (CardW - L.HeroSelectPortrait) * Half, RowY, L.HeroSelectPortrait);
		RowY += L.HeroSelectPortrait + Pad * Half;
		DrawLabel(YJJLoc::Get(Skill.NameKey), X + CardW * Half, RowY, L.BigText, YJJUI::Highlight, Half, CardW - Pad * 2.f);
		RowY += L.BigText + Pad * Half;
		const FString Kind = YJJData::SkillKindKey(Skill.Kind);
		DrawLabel(YJJLoc::Get(*(TEXT("SkillKind.") + Kind)) + TEXT(" · ") + YJJLoc::Format(TEXT("UI.SkillCooldownShort"), { FMath::RoundToInt(Skill.Cooldown) }), X + CardW * Half, RowY, L.SmallText, YJJUI::Gold, Half);
		RowY += L.TipLineH + Pad * Half;
		for (const FString& Line : WrapText(YJJLoc::Format(*Skill.DescKey, { Skill.SummonCount }), L.SmallText, CardW - Pad * 2.f))
		{
			DrawLabel(Line, X + CardW * Half, RowY, L.SmallText, YJJUI::Paper, Half);
			RowY += L.TipLineH;
		}
		RowY += Pad * Half;
		for (const FString& Line : WrapText(YJJLoc::Format(*(TEXT("SkillWhen.") + Kind), { Skill.AutoCount }), L.SmallText, CardW - Pad * 2.f))
		{
			DrawLabel(Line, X + CardW * Half, RowY, L.SmallText, YJJUI::Muted, Half);
			RowY += L.TipLineH;
		}
	}
}
