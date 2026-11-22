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
		if (bOpen)
		{
			AddButton(EButton::WorldStage, Stage, P.X - Size * Half, P.Y - Size * Half, Size, Size);
		}
	}

	// 머리글 + 타이틀로
	const float Pad = L.PanelPad;
	DrawLabel(YJJLoc::Get(TEXT("Map.Title")), Pad * 2.f, Pad, L.TitleText, YJJUI::Highlight);
	DrawLabel(YJJLoc::Get(TEXT("Map.Hint")), Pad * 2.f, Pad * 2.f + L.TitleText, L.SmallText, YJJUI::Paper);
	DrawMenuButton(EButton::WorldBack, 0, VirtualW - Pad - L.TitleButtonW, Pad, L.TitleButtonW, L.TitleButtonH, YJJLoc::Get(TEXT("Map.Back")), false);
}
