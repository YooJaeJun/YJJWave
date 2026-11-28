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

	// 스테이지 깃발 (킹덤러쉬식): 새 곳은 빨간 깃발(통통 뛴다), 깬 곳은 파란 깃발, 잠긴 곳은 회색. 발밑에 이름과 별
	const TConstArrayView<FYJJStageDef> Stages = YJJData::Stages();
	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float Pulse = Half + Half * FMath::Sin(AnimClock * L.GlowSpeed);
	for (int32 Stage = 0; Stage < Stages.Num(); ++Stage)
	{
		const FVector2f P = Stages[Stage].MapPos * MapSize;
		const bool bOpen = Mode->IsStageOpen(Stage);
		const bool bCleared = Mode->StageStars(Stage) > 0;
		// P = 깃대가 꽂힌 흙더미 자리. 깃발 그림은 그 위로 선다
		const FVector2f FlagCenter = P - FVector2f(0.f, L.StageFlagSize * L.StageFlagLift);
		const bool bHover = bOpen && FVector2f::Distance(Mouse, FlagCenter) <= L.StageFlagSize * Half;
		const float Size = L.StageFlagSize * (bHover ? L.StageNodeHover : 1.f);
		const bool bNew = bOpen && !bCleared;
		const float Bob = bNew ? FMath::Abs(FMath::Sin(AnimClock * L.StageFlagBobSpeed)) * L.StageFlagBob : 0.f;
		const int32 Flag = FMath::RoundToInt(!bOpen ? L.UiFlagLocked : bCleared ? L.UiFlagCleared : L.UiFlagNew);
		DrawIcon(Flag, P.X - Size * Half, P.Y - Size * (Half + L.StageFlagLift) - Bob, Size, bNew ? FMath::Lerp(FLinearColor::White, YJJUI::Highlight, Pulse * L.StageFlagGlow) : FLinearColor::White);
		// 이름 + 별 (어두운 띠 위)
		const FString Name = YJJLoc::Get(Stages[Stage].NameKey);
		float NameW = 0.f;
		MakeFont(Name, L.SmallText, 1.f, 0.f, NameW);
		const int32 MaxStars = FMath::RoundToInt(L.ResultStarCount);
		const float Star = L.StageStarSize;
		const float BandW = FMath::Max(NameW, MaxStars * Star) + L.PanelPad * 2.f;
		const float BandY = P.Y + L.StageNameGap;
		DrawPanel(P.X - BandW * Half, BandY, BandW, L.SmallText + Star + L.PanelPad, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
		DrawLabelCentered(Name, P.X, BandY + L.PanelPad * Half + L.SmallText * Half, L.SmallText, bOpen ? YJJUI::Paper : YJJUI::Muted);
		const float StarsLeft = P.X - MaxStars * Star * Half;
		for (int32 Index = 0; Index < MaxStars; ++Index)
		{
			const bool bEarned = Index < Mode->StageStars(Stage);
			DrawIcon(FMath::RoundToInt(L.UiStar), StarsLeft + Index * Star, BandY + L.PanelPad * Half + L.SmallText, Star, bEarned ? YJJUI::PerkTaken : YJJUI::WithAlpha(YJJUI::Ink, L.ResultStarEmptyAlpha));
		}
		if (bOpen && MapPanel == EYJJMapPanel::None)
		{
			AddButton(EButton::WorldStage, Stage, P.X - Size * Half, P.Y - Size, Size, Size + L.StageNameGap + L.SmallText + Star + L.PanelPad);
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
	// 월드맵 우측 하단: 그림 + 아래 글자 버튼 셋 (영웅 · 주문 · 특성). 창이 열려 있으면 왼쪽 위 닫기만
	const FYJJUILayout& L = UI();
	if (MapPanel != EYJJMapPanel::None)
	{
		if (MapPanel == EYJJMapPanel::Perks)
		{
			DrawMenuButton(EButton::MapClose, 0, L.BookPad, L.BookPad, L.BookCloseSize, L.BookCloseSize, TEXT("<"), false);
		}
		return;
	}
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const int32 HeroFace = Mode && YJJData::Heroes().Num() > 0 ? YJJData::Heroes()[YJJData::HeroIndex(Mode->GetProgress().Hero)].Portrait : FMath::RoundToInt(L.UiHero);
	const int32 Icons[MapPanelCount] = { HeroFace, FMath::RoundToInt(L.UiMeteor), FMath::RoundToInt(L.UiBigStar) };
	const TCHAR* Keys[MapPanelCount] = { TEXT("Map.Heroes"), TEXT("Map.Spells"), TEXT("Map.Perks") };
	const float Size = L.MapButtonSize;
	const float LabelH = L.MidText + L.PanelPad;
	const float Y = VirtualHeight - L.MapButtonMargin - Size - LabelH;
	for (int32 Index = 0; Index < MapPanelCount; ++Index)
	{
		const float X = VirtualW - L.MapButtonMargin - (MapPanelCount - Index) * Size - (MapPanelCount - 1 - Index) * L.MapButtonGap;
		const bool bHover = HoverButton(EButton::MapPanel) && ButtonAt(ToVirtual(MouseScreen()))->Index == Index;
		DrawPanel(X, Y, Size, Size, bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Half) : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		const float Icon = Size * L.MapButtonIcon;
		DrawIcon(Icons[Index], X + (Size - Icon) * Half, Y + (Size - Icon) * Half, Icon);
		// 글자는 버튼 아래 작은 판에
		DrawPanel(X - L.PanelPad * Half, Y + Size - L.PanelPad, Size + L.PanelPad, LabelH, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
		DrawLabelCentered(YJJLoc::Get(Keys[Index]), X + Size * Half, Y + Size - L.PanelPad + LabelH * Half, L.MidText, YJJUI::Highlight);
		AddButton(EButton::MapPanel, Index, X, Y, Size, Size + LabelH);
	}
}

void AYJJWaveHUD::DrawBookFrame()
{
	const FYJJUILayout& L = UI();
	DrawBox(0.f, 0.f, VirtualW, VirtualHeight, YJJUI::TreeBack);
	DrawMenuButton(EButton::MapClose, 0, L.BookPad, L.BookPad, L.BookCloseSize, L.BookCloseSize, TEXT("<"), false);
}

void AYJJWaveHUD::DrawBookTile(int32 Index, float X, float Y, int32 Icon, bool bPicked, bool bEquipped)
{
	const FYJJUILayout& L = UI();
	const float Size = L.BookTileSize;
	const bool bHover = HoverButton(EButton::BookPick) && ButtonAt(ToVirtual(MouseScreen()))->Index == Index;
	DrawPanel(X, Y, Size, Size, bPicked ? YJJUI::Highlight : (bHover ? FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Half) : YJJUI::PanelTint), FMath::RoundToInt(L.UiCard));
	const float Inner = Size - L.PanelPad * 2.f;
	DrawIcon(Icon, X + L.PanelPad, Y + L.PanelPad, Inner);
	if (bEquipped)
	{
		DrawLabel(TEXT("✔"), X + Size - L.PanelPad, Y + Size - L.PanelPad - L.MidText, L.MidText, YJJUI::Good, 1.f);
	}
	AddButton(EButton::BookPick, Index, X, Y, Size, Size);
}

void AYJJWaveHUD::DrawHeroLineup()
{
	// 킹덤러쉬식 영웅 창: 왼쪽 영웅 목록 · 가운데 큰 영웅 + 출전 자리 둘 · 오른쪽 이름·설명·능력치·스킬·출전 버튼
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	const TConstArrayView<FYJJHeroDef> Heroes = YJJData::Heroes();
	if (!Mode || Heroes.Num() == 0)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	DrawBookFrame();
	const int32 Equipped = YJJData::HeroIndex(Mode->GetProgress().Hero);
	BookView = FMath::Clamp(BookView, 0, Heroes.Num() - 1);
	const FYJJHeroDef& Hero = Heroes[BookView];
	const FYJJUnitDef& Unit = YJJData::Unit(Hero.Unit);
	const float Top = L.BookPad * 2.f + L.BookCloseSize;
	const float Bottom = VirtualHeight - L.BookPad;
	// 왼쪽 목록
	const float ListX = L.BookPad;
	DrawPanel(ListX, Top, L.BookListW, Bottom - Top, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	DrawLabelCentered(YJJLoc::Get(TEXT("Book.Heroes")), ListX + L.BookListW * Half, Top + L.BookPad, L.BigText, YJJUI::Highlight);
	const int32 Cols = FMath::Max(1, FMath::FloorToInt((L.BookListW - L.BookPad) / (L.BookTileSize + L.BookTileGap)));
	const float GridW = Cols * L.BookTileSize + (Cols - 1) * L.BookTileGap;
	for (int32 Index = 0; Index < Heroes.Num(); ++Index)
	{
		const float X = ListX + (L.BookListW - GridW) * Half + (Index % Cols) * (L.BookTileSize + L.BookTileGap);
		const float Y = Top + L.BookPad * 2.f + L.BigText + (Index / Cols) * (L.BookTileSize + L.BookTileGap);
		DrawBookTile(Index, X, Y, Heroes[Index].Portrait, Index == BookView, Index == Equipped);
	}
	// 가운데 큰 영웅 + 출전 자리
	const float InfoX = VirtualW - L.BookPad - L.BookInfoW;
	const float CenterX = (ListX + L.BookListW + InfoX) * Half;
	const float FootY = Top + L.BookHeroSize * L.FootAnchor;
	DrawUnitSprite(Hero.Unit, CenterX, FootY, L.BookHeroSize, false);
	const float SlotY = Bottom - L.BookSlotSize - L.BookPad;
	DrawLabelCentered(YJJLoc::Get(TEXT("Book.Equipped")), CenterX, SlotY - L.MidText, L.MidText, YJJUI::Highlight);
	for (int32 Slot = 0; Slot < HeroSlotCount; ++Slot)
	{
		const float X = CenterX - (HeroSlotCount * L.BookSlotSize + (HeroSlotCount - 1) * L.BookTileGap) * Half + Slot * (L.BookSlotSize + L.BookTileGap);
		DrawPanel(X, SlotY, L.BookSlotSize, L.BookSlotSize, Slot == 0 ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		if (Slot == 0)
		{
			DrawIcon(Heroes[Equipped].Portrait, X + L.PanelPad, SlotY + L.PanelPad, L.BookSlotSize - L.PanelPad * 2.f);
		}
		else
		{
			DrawLabelCentered(YJJLoc::Get(TEXT("Book.Locked")), X + L.BookSlotSize * Half, SlotY + L.BookSlotSize * Half - L.SmallText * Half, L.SmallText, YJJUI::Muted);
			DrawLabelCentered(YJJLoc::Format(TEXT("UI.HeroSlotLocked"), { YJJData::Tune().HeroSlot2Stage }), X + L.BookSlotSize * Half, SlotY + L.BookSlotSize * Half + L.SmallText, L.TinyText, YJJUI::Muted, L.BookSlotSize - L.PanelPad);
		}
	}
	// 오른쪽 설명
	DrawPanel(InfoX, Top, L.BookInfoW, Bottom - Top, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	const float Pad = L.BookPad;
	float RowY = Top + Pad;
	DrawLabel(YJJLoc::Get(Unit.NameKey), InfoX + Pad, RowY, L.TitleText, YJJUI::Highlight);
	RowY += L.TitleText + Pad * Half;
	for (const FString& Line : WrapText(YJJLoc::Get(Hero.DescKey), L.SmallText, L.BookInfoW - Pad * 2.f))
	{
		DrawLabel(Line, InfoX + Pad, RowY, L.SmallText, YJJUI::Paper);
		RowY += L.TipLineH;
	}
	RowY += Pad * Half;
	// 능력치 막대 (영웅들 중 가장 높은 값 기준)
	float MaxHP = 1.f, MaxAtk = 1.f, MaxSpeed = 1.f;
	for (const FYJJHeroDef& Other : Heroes)
	{
		const FYJJUnitDef& O = YJJData::Unit(Other.Unit);
		MaxHP = FMath::Max(MaxHP, O.HP);
		MaxAtk = FMath::Max(MaxAtk, O.Damage / FMath::Max(O.AttackInterval, KINDA_SMALL_NUMBER));
		MaxSpeed = FMath::Max(MaxSpeed, O.Speed);
	}
	const TPair<const TCHAR*, float> Stats[] = {
		{ TEXT("Book.HP"), Unit.HP / MaxHP },
		{ TEXT("Book.Atk"), Unit.Damage / FMath::Max(Unit.AttackInterval, KINDA_SMALL_NUMBER) / MaxAtk },
		{ TEXT("Book.Speed"), Unit.Speed / MaxSpeed } };
	for (const TPair<const TCHAR*, float>& Stat : Stats)
	{
		DrawLabel(YJJLoc::Get(Stat.Key), InfoX + Pad, RowY, L.SmallText, YJJUI::Paper);
		DrawBar(InfoX + L.BookInfoW - Pad - L.BookStatBarW, RowY + L.SmallText * Quarter, L.BookStatBarW, L.BarH, Stat.Value, YJJUI::Gold);
		RowY += L.TipLineH;
	}
	RowY += Pad * Half;
	DrawLabel(YJJLoc::Get(TEXT("Book.Skills")), InfoX + Pad, RowY, L.MidText, YJJUI::Highlight);
	RowY += L.MidText + Pad * Half;
	for (const int32 SkillIndex : Hero.Skills)
	{
		const FYJJSkillDef& Skill = YJJData::Skill(SkillIndex);
		const float Icon = L.TipLineH + Pad * Half;
		DrawIcon(Skill.Icon, InfoX + Pad, RowY, Icon);
		DrawLabel(YJJLoc::Get(Skill.NameKey) + TEXT(" · ") + YJJLoc::Get(*(FString(TEXT("SkillKind.")) + YJJData::SkillKindKey(Skill.Kind))), InfoX + Pad * 1.5f + Icon, RowY + (Icon - L.SmallText) * Half, L.SmallText, YJJUI::Paper, 0.f, L.BookInfoW - Pad * 3.f - Icon - L.BookCooldownW);
		// 쿨타임 (오른쪽 끝)
		DrawLabel(YJJLoc::Format(TEXT("Book.Cooldown"), { FMath::RoundToInt(Skill.Cooldown) }), InfoX + L.BookInfoW - Pad, RowY + (Icon - L.SmallText) * Half, L.SmallText, YJJUI::Highlight, 1.f);
		RowY += Icon + Pad * Quarter;
	}
	// 출전 버튼
	const bool bOn = BookView == Equipped;
	DrawMenuButton(EButton::BookEquip, 0, InfoX + Pad, Bottom - Pad - L.ResultButtonH, L.BookInfoW - Pad * 2.f, L.ResultButtonH, YJJLoc::Get(bOn ? TEXT("Book.EquippedNow") : TEXT("Book.Equip")), !bOn);
}

void AYJJWaveHUD::DrawHeroSkills()
{
	// 킹덤러쉬식 주문 창: 왼쪽 주문 목록 · 가운데 큰 그림 + 장착 주문 셋 (F1·F2·F3 고정) · 오른쪽 이름·설명·Lv 수치
	const FYJJUILayout& L = UI();
	DrawBookFrame();
	const int32 Count = static_cast<int32>(EYJJSpell::Count);
	BookView = FMath::Clamp(BookView, 0, Count - 1);
	const EYJJSpell Spell = static_cast<EYJJSpell>(BookView);
	const FString Key = YJJData::SpellKey(Spell);
	const float Top = L.BookPad * 2.f + L.BookCloseSize;
	const float Bottom = VirtualHeight - L.BookPad;
	const float ListX = L.BookPad;
	DrawPanel(ListX, Top, L.BookListW, Bottom - Top, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	DrawLabelCentered(YJJLoc::Get(TEXT("Book.Spells")), ListX + L.BookListW * Half, Top + L.BookPad, L.BigText, YJJUI::Highlight);
	const int32 Cols = FMath::Max(1, FMath::FloorToInt((L.BookListW - L.BookPad) / (L.BookTileSize + L.BookTileGap)));
	const float GridW = Cols * L.BookTileSize + (Cols - 1) * L.BookTileGap;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float X = ListX + (L.BookListW - GridW) * Half + (Index % Cols) * (L.BookTileSize + L.BookTileGap);
		const float Y = Top + L.BookPad * 2.f + L.BigText + (Index / Cols) * (L.BookTileSize + L.BookTileGap);
		DrawBookTile(Index, X, Y, SpellIcon(static_cast<EYJJSpell>(Index)), Index == BookView, true);
	}
	const float InfoX = VirtualW - L.BookPad - L.BookInfoW;
	const float CenterX = (ListX + L.BookListW + InfoX) * Half;
	const float Big = L.BookHeroSize * Half;
	DrawIcon(SpellIcon(Spell), CenterX - Big * Half, Top + L.BookPad, Big);
	const float SlotY = Bottom - L.BookSlotSize - L.BookPad;
	DrawLabelCentered(YJJLoc::Get(TEXT("Book.EquippedSpells")), CenterX, SlotY - L.MidText, L.MidText, YJJUI::Highlight);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float X = CenterX - (Count * L.BookSlotSize + (Count - 1) * L.BookTileGap) * Half + Index * (L.BookSlotSize + L.BookTileGap);
		DrawPanel(X, SlotY, L.BookSlotSize, L.BookSlotSize, Index == BookView ? YJJUI::Highlight : YJJUI::PanelTint, FMath::RoundToInt(L.UiCard));
		DrawIcon(SpellIcon(static_cast<EYJJSpell>(Index)), X + L.PanelPad, SlotY + L.PanelPad, L.BookSlotSize - L.PanelPad * 2.f);
	}
	DrawPanel(InfoX, Top, L.BookInfoW, Bottom - Top, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanel));
	const float Pad = L.BookPad;
	float RowY = Top + Pad;
	DrawLabel(YJJLoc::Get(TEXT("Spell.") + Key), InfoX + Pad, RowY, L.TitleText, YJJUI::Highlight);
	RowY += L.TitleText + Pad * Half;
	for (const FString& Line : WrapText(YJJLoc::Get(TEXT("SpellDesc.") + Key), L.SmallText, L.BookInfoW - Pad * 2.f))
	{
		DrawLabel(Line, InfoX + Pad, RowY, L.SmallText, YJJUI::Paper);
		RowY += L.TipLineH;
	}
	RowY += Pad * Half;
	TArray<FString> Stats;
	SpellText(Spell).ParseIntoArray(Stats, TEXT("\n"));
	for (const FString& Stat : Stats)
	{
		for (const FString& Line : WrapText(Stat, L.SmallText, L.BookInfoW - Pad * 2.f))
		{
			DrawValueLabel(Line, InfoX + Pad, RowY, L.SmallText, YJJUI::Muted, L.BookInfoW - Pad * 2.f);
			RowY += L.TipLineH;
		}
	}
}
