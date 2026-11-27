// 전장 그리기: 바닥 → 진지·성벽 → 그림자 → 유닛 → 날아가는 시체 → 투사체 → 연출
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"
#include "Async/ParallelFor.h"
#include "Render/YJJSpriteRenderItem.h"

void AYJJWaveHUD::DrawWorld()
{
	FCanvasTileItem Back(FVector2D::ZeroVector, FVector2D(ScreenSize.X, ScreenSize.Y), YJJUI::Field);
	Back.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Back);

	HoverPlot = (!bTreeOpen && !IsMouseOverUI()) ? PlotAt(ScreenToWorld(MouseScreen())) : INDEX_NONE;

	DrawGround();
	DrawCamp();
	DrawWall();
	DrawBlockers();
	DrawFrontLines();
	DrawUnits();
	DrawDying();
	DrawProjectiles();
	DrawEffects();
	DrawSpellAim();
	DrawUnitBars();
}

void AYJJWaveHUD::DrawFrontLines()
{
	const FYJJBattle* Current = Battle();
	if (Current->bGameOver)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const FYJJHordeView& Horde = View();
	const EYJJTeam Teams[] = { EYJJTeam::Player, EYJJTeam::Enemy };
	for (const EYJJTeam Team : Teams)
	{
		const float FrontX = Horde.Front[static_cast<int32>(Team)];
		if (FrontX < 0.f || Horde.Num(Team) == 0)
		{
			continue;
		}
		const bool bMine = Team == EYJJTeam::Player;
		const FVector2f Top = WorldToVirtual(FVector2f(FrontX, 0.f));
		const FVector2f Bottom = WorldToVirtual(FVector2f(FrontX, Current->FieldHeight));
		const float Pulse = L.FrontStripAlpha * (1.f + L.FrontStripPulse * FMath::Sin(AnimClock * L.GlowSpeed));
		DrawBox(Top.X - L.FrontStripW * Half, Top.Y, L.FrontStripW, Bottom.Y - Top.Y, YJJUI::WithAlpha(bMine ? YJJUI::HeroBar : YJJUI::BossBar, Pulse));
		if (bMine && Current->FrontBonus > 1.f)
		{
			DrawLabel(YJJLoc::Format(TEXT("UI.FrontBonus"), { FMath::RoundToInt((Current->FrontBonus - 1.f) * Percent) }), Top.X, FMath::Max(Top.Y, 0.f) + L.FrontLabelY, L.SmallText, YJJUI::Good, Half);
		}
	}
}

void AYJJWaveHUD::DrawBlockers()
{
	// 길목 지형: 타원을 위에서 아래로 바위 무더기 줄로 채운다 (아래 줄이 위 줄을 덮는다)
	const FYJJBattle* Current = Battle();
	if (!PropTexture || Current->StageDef().Blockers.Num() == 0)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, PropTexture, Current->StageDef().Blockers.Num() * BlockerRowsGuess);
	for (const FVector4f& Share : Current->StageDef().Blockers)
	{
		const FVector2f Center(Share.X * Current->FieldWidth, Share.Y * Current->FieldHeight);
		const FVector2f Radius(Share.Z * Current->FieldWidth, Share.W * Current->FieldHeight);
		const float RowH = Radius.X * 2.f * L.RockWidthPad / FMath::Max(L.RockAspect, 1.f);
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(Radius.Y * 2.f / FMath::Max(RowH * L.RockRowOverlap, 1.f)));
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float Y = Center.Y - Radius.Y + (Row + Half) * Radius.Y * 2.f / Rows;
			const float Across = FMath::Sqrt(FMath::Max(1.f - FMath::Square((Y - Center.Y) / FMath::Max(Radius.Y, 1.f)), 0.f));
			const float W = FMath::Max(Radius.X * 2.f * L.RockWidthPad * Across, RowH);
			const float Shift = (Row % 2 == 0 ? 1.f : -1.f) * Radius.X * L.RockJitter;
			const FVector2f Feet = WorldToScreen(FVector2f(Center.X + Shift, Y + RowH * Half));
			const FVector2f Size = FVector2f(W, W / FMath::Max(L.RockAspect, 1.f)) * Zoom;
			AddQuad(Batch, Feet - FVector2f(Size.X * Half, Size.Y), Size, FVector2f::ZeroVector, FVector2f::UnitVector, FLinearColor::White, Row % 2 == 1);
		}
	}
}

void AYJJWaveHUD::DrawWall()
{
	// 진지 오른쪽 끝을 따라 돌 성벽을 세로로 잇고, 가운데는 열린 성문 (아군이 지나 나간다)
	const FYJJBattle* Current = Battle();
	const FYJJUILayout& L = UI();
	if (!BuildingAtlas.Texture)
	{
		return;
	}
	const float Gate = YJJData::Tune().GateHalf;
	const float GateY = Current->FieldHeight * Half;
	const float Seg = L.WallSegment;
	const int32 Count = FMath::CeilToInt(Current->FieldHeight / Seg) + 1;
	const FLinearColor Tint = Current->WallFlash > 0.f ? FMath::Lerp(FLinearColor::White, YJJUI::HitTint, Current->WallFlash / FMath::Max(YJJData::Tune().WallFlashTime, KINDA_SMALL_NUMBER)) : FLinearColor::White;
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, Count + 2);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Y = Index * Seg;
		if (FMath::Abs(Y - GateY) < Gate)
		{
			continue;
		}
		const FVector2f Feet = WorldToScreen(FVector2f(Current->CampEdgeX, Y));
		const float Size = L.WallDrawSize * Zoom;
		AddQuad(Batch, FVector2f(Feet.X - Size * Half, Feet.Y - Size * L.WallAnchor), FVector2f(Size), BuildingAtlas.UV(FMath::RoundToInt(L.WallCol), FMath::RoundToInt(L.WallRow)), BuildingAtlas.CellUV, Tint, false);
	}
	// 성문 양쪽 탑
	for (const float Side : { -1.f, 1.f })
	{
		const FVector2f Feet = WorldToScreen(FVector2f(Current->CampEdgeX, GateY + Side * Gate));
		const float Size = L.GateTowerSize * Zoom;
		AddQuad(Batch, FVector2f(Feet.X - Size * Half, Feet.Y - Size * L.WallAnchor), FVector2f(Size), BuildingAtlas.UV(FMath::RoundToInt(L.GateCol), FMath::RoundToInt(L.WallRow)), BuildingAtlas.CellUV, Tint, Side > 0.f);
	}
	// 성문 위 체력
	const FVector2f Top = WorldToVirtual(FVector2f(Current->CampEdgeX, GateY - Gate)) - FVector2f(0.f, L.GateTowerSize * Zoom / ViewScale * L.WallAnchor + L.WallBarGap);
	const float W = L.WallBarW;
	DrawBar(Top.X - W * Half, Top.Y - L.TextBarH, W, L.TextBarH, Current->WallHP / FMath::Max(Current->WallMaxHP, KINDA_SMALL_NUMBER), YJJUI::Ally);
	DrawBarLabel(YJJLoc::Format(TEXT("UI.WallHP"), { FormatCount(FMath::CeilToFloat(Current->WallHP)), FormatCount(Current->WallMaxHP) }), Top.X - W * Half, Top.Y - L.TextBarH, W, L.TextBarH);
}

void AYJJWaveHUD::UpdateCoinPops(float Dt)
{
	CoinBudget = FMath::Min(CoinBudget + UI().CoinPopsPerSecond * Dt, UI().CoinPopsPerSecond);
	for (int32 Index = CoinPops.Num() - 1; Index >= 0; --Index)
	{
		CoinPops[Index].Age += Dt;
		if (CoinPops[Index].Age >= UI().CoinPopTime)
		{
			CoinPops.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}
}

void AYJJWaveHUD::DrawCoinPops()
{
	// 위로 튀었다가 떨어지며 사라진다, 옆에 +금액
	const FYJJUILayout& L = UI();
	for (const FYJJCoinPop& Pop : CoinPops)
	{
		const float T = Pop.Age / FMath::Max(L.CoinPopTime, KINDA_SMALL_NUMBER);
		const float Rise = L.CoinPopJump * (4.f * T * (1.f - T)) + L.CoinPopFloat * T;
		const FVector2f At = WorldToVirtual(Pop.Pos + FVector2f(Pop.Side * L.CoinPopDrift * T, -Rise));
		const float Alpha = FMath::Clamp((1.f - T) / FMath::Max(L.CoinPopFade, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float Size = L.CoinPopSize;
		DrawIcon(FMath::RoundToInt(L.UiCoin), At.X - Size, At.Y - Size * Half, Size, YJJUI::WithAlpha(FLinearColor::White, Alpha));
		DrawLabelLeftMiddle(YJJLoc::Format(TEXT("UI.CoinPop"), { Pop.Value }), At.X + L.CoinPopGap, At.Y, L.SmallText, YJJUI::WithAlpha(YJJUI::Gold, Alpha), 0.f);
	}
}

void AYJJWaveHUD::DrawWavePreview()
{
	// 화면 위 가운데 판: "다음 웨이브 2/3 · 12초" + 올 적 그림과 마리 수 (보스는 붉게)
	const FYJJBattle* Current = Battle();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (Current->bGameOver || bTreeOpen || (Mode && Mode->CutsceneLine()))
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const int32 Next = Current->Level;   // 0부터: 다음 웨이브 번호
	const bool bLast = !Current->StageDef().Waves.IsValidIndex(Next);
	const float Pad = L.TextPad;
	const int32 Seconds = FMath::CeilToInt(FMath::Max(Current->LevelDuration() - Current->LevelTime, 0.f));
	const FString Title = bLast ? YJJLoc::Get(TEXT("UI.LastWave")) : YJJLoc::Format(TEXT("UI.NextWave"), { Next + 1, Current->NumWaves(), Seconds });
	TArray<TPair<int32, int32>> Comers;
	if (!bLast)
	{
		for (const FYJJWaveSpawn& Spawn : Current->StageDef().Waves[Next].Spawns)
		{
			const int32 Count = FMath::RoundToInt(Spawn.Rate * Current->StageDef().Waves[Next].Duration) + Spawn.Burst;
			if (Count > 0)
			{
				Comers.Emplace(Spawn.Unit, Count);
			}
		}
	}
	float TitleW = 0.f;
	MakeFont(Title, L.SmallText, 1.f, 0.f, TitleW);
	const float Item = L.PreviewIcon + L.PreviewCountW;
	const float W = FMath::Max(TitleW, Comers.Num() * Item) + Pad * 2.f;
	const float H = Pad * 2.f + L.SmallText + (Comers.Num() > 0 ? L.PreviewIcon + L.PreviewGap : 0.f);
	const float X = WorldCenterVirtualX() - W * Half;
	const float Y = L.PreviewY;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	DrawLabel(Title, X + W * Half, Y + Pad, L.SmallText, bLast ? YJJUI::Bad : YJJUI::Paper, Half);
	const float RowY = Y + Pad + L.SmallText + L.PreviewGap;
	const float Left = X + (W - Comers.Num() * Item) * Half;
	for (int32 Index = 0; Index < Comers.Num(); ++Index)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Comers[Index].Key);
		const float IX = Left + Index * Item;
		DrawUnitSprite(Comers[Index].Key, IX + L.PreviewIcon * Half, RowY + L.PreviewIcon * L.FootAnchor, L.PreviewIcon, true);
		DrawLabelLeftMiddle(YJJLoc::Format(TEXT("UI.PreviewCount"), { Comers[Index].Value }), IX + L.PreviewIcon, RowY + L.PreviewIcon * Half, L.SmallText, Def.bBoss ? YJJUI::BossText : YJJUI::Paper, 0.f);
	}
}

void AYJJWaveHUD::DrawCombo()
{
	// 몇 초 안에 이어서 쓰러뜨린 적 수. 클수록 크고 붉게, 늘 때마다 툭 커진다
	const FYJJUILayout& L = UI();
	if (ComboCount < FMath::RoundToInt(L.ComboShowAt) || Battle()->bGameOver)
	{
		return;
	}
	const float Heat = FMath::Clamp(static_cast<float>(ComboCount) / FMath::Max(L.ComboHot, 1.f), 0.f, 1.f);
	const float Size = L.ComboText * (1.f + Heat * L.ComboGrow) * (1.f + ComboPop * L.ComboPopScale);
	const FLinearColor Color = FMath::Lerp(YJJUI::Highlight, YJJUI::Bad, Heat);
	const float Fade = FMath::Clamp(ComboTimer / FMath::Max(L.ComboFade, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float X = VirtualW - L.ComboRight;
	DrawLabelCentered(FString::FromInt(ComboCount), X, L.ComboY, Size, YJJUI::WithAlpha(Color, Fade));
	DrawLabelCentered(YJJLoc::Get(TEXT("UI.Combo")), X, L.ComboY + Size * Half + L.ComboLabelGap, L.MidText, YJJUI::WithAlpha(YJJUI::Paper, Fade));
}

void AYJJWaveHUD::DrawSkillTexts()
{
	// 스킬 한 번에 많이 쓰러뜨리면 그 자리에서 글자가 툭 커졌다가 떠오르며 사라진다
	const FYJJUILayout& L = UI();
	for (const FYJJEffect& Effect : Battle()->Effects)
	{
		if (Effect.Kind != EYJJEffectKind::SkillText)
		{
			continue;
		}
		const float T = FMath::Clamp(Effect.Age / FMath::Max(Effect.Life, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const FVector2f At = WorldToVirtual(Effect.Pos - FVector2f(0.f, Effect.Radius * L.SkillTextLift)) - FVector2f(0.f, L.SkillTextRise * T);
		const float Pop = FMath::Square(FMath::Square(1.f - T));
		const float Size = L.SkillTextSize * (1.f + Pop * L.SkillTextPop);
		const FLinearColor Color = Effect.Side < 0.f ? YJJUI::Bad : YJJUI::Highlight;
		DrawLabel(YJJLoc::Format(TEXT("UI.SkillKills"), { FMath::RoundToInt(Effect.Power) }), At.X, At.Y, Size, YJJUI::WithAlpha(Color, 1.f - T * T * T), Half);
	}
}

void AYJJWaveHUD::DrawUnitBars()
{
	// 영웅은 초록, 보스는 빨강. 그림 위쪽에 작게, 보스는 이름도
	const FYJJUILayout& L = UI();
	for (const FYJJUnitBar& Bar : View().Bars)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Bar.Type);
		const FVector2f Top = WorldToVirtual(Bar.Pos - FVector2f(0.f, Def.DrawSize * L.UnitBarLift));
		const float W = FMath::Max(Def.DrawSize * Zoom / ViewScale * L.UnitBarWidth, L.UnitBarMinW);
		const float H = L.UnitBarH;
		const float X = Top.X - W * Half;
		const float Y = Top.Y - H;
		DrawBar(X, Y, W, H, Bar.HP / FMath::Max(Bar.MaxHP, KINDA_SMALL_NUMBER), Def.Team == EYJJTeam::Enemy ? YJJUI::BossBar : YJJUI::HeroBar);
		if (Def.bBoss || Def.bCamp)
		{
			DrawLabel(YJJLoc::Get(Def.NameKey), Top.X, Y - L.UnitBarNameGap - L.SmallText, L.SmallText, YJJUI::BossText, Half);
		}
	}
}

void AYJJWaveHUD::DrawGround()
{
	const FYJJBattle* Current = Battle();
	if (!GroundTarget || !bGroundReady)
	{
		return;
	}
	const FVector2f TopLeft = WorldToScreen(FVector2f::ZeroVector);
	const FVector2f Size = FVector2f(Current->FieldWidth, Current->FieldHeight) * Zoom;
	FCanvasTileItem Item(FVector2D(TopLeft.X, TopLeft.Y), GroundTarget->GetResource(), FVector2D(Size.X, Size.Y), FLinearColor::White);
	Item.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Item);
}

void AYJJWaveHUD::DrawCamp()
{
	const FYJJBattle* Current = Battle();
	if (!BuildingAtlas.Texture)
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, Current->Plots.Num());

	// 진지 칸과 병영. 병사를 내보낼 때 한 번 들썩인다
	for (int32 PlotIndex = 0; PlotIndex < Current->Plots.Num(); ++PlotIndex)
	{
		const FYJJPlot& Plot = Current->Plots[PlotIndex];
		const bool bHover = PlotIndex == HoverPlot;
		const float PulseT = Plot.Pulse / FMath::Max(YJJData::Tune().PlotPulseTime, KINDA_SMALL_NUMBER);
		const float Bounce = 1.f + FMath::Sin(PulseT * UE_PI) * UI().PlotBounce;
		const float Size = UI().PlotDrawSize * Zoom * (bHover ? UI().PlotHoverScale : 1.f);
		const FVector2f Feet = WorldToScreen(Plot.Pos);
		const int32 Column = Plot.Building == INDEX_NONE ? FMath::RoundToInt(UI().EmptyPlotCol) : YJJData::Building(Plot.Building).AtlasCol;
		// 강화 단계마다 다른 그림 (줄 = Lv-1): 돌 기단 → 망루·문장 → 왕관
		const int32 Row = Plot.Building == INDEX_NONE ? 0 : FMath::Clamp(Plot.Level - 1, 0, FMath::RoundToInt(UI().BuildingTiers) - 1);
		const FVector2f QuadSize(Size, Size * Bounce);
		const FVector2f TopLeft(Feet.X - QuadSize.X * Half, Feet.Y - QuadSize.Y * UI().PlotAnchor);
		AddQuad(Batch, TopLeft, QuadSize, BuildingAtlas.UV(Column, Row), BuildingAtlas.CellUV, bHover ? YJJUI::Highlight : FLinearColor::White, false);
	}

	// 병영 레벨 글자
	const float LabelSize = UI().PlotLabelSize * FMath::Clamp(Zoom, UI().PlotLabelZoomMin, 1.f);
	for (const FYJJPlot& Plot : Current->Plots)
	{
		if (Plot.Building != INDEX_NONE)
		{
			const FVector2f Label = WorldToVirtual(Plot.Pos + FVector2f(0.f, UI().PlotLabelOffset));
			DrawLabel(YJJLoc::Format(TEXT("UI.LevelShort"), { Plot.Level }), Label.X, Label.Y, LabelSize, YJJUI::Paper, Half);
		}
	}
}

void AYJJWaveHUD::DrawUnits()
{
	const FYJJBattle* Current = Battle();
	const FYJJHordeView& Horde = View();
	if (!UnitAtlas.Texture || !FxAtlas.Texture || !UnitAtlas.Texture->GetResource() || !FxAtlas.Texture->GetResource())
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();

	// 화면 안의 유닛만, 위에서 아래 순서로 (Y 버킷 계수 정렬)
	const FVector2f Margin(UI().CullMargin);
	const FVector2f WorldMin = ScreenToWorld(FVector2f::ZeroVector) - Margin;
	const FVector2f WorldMax = ScreenToWorld(ScreenSize) + Margin;
	Visible.Reset();
	for (int32 Slot = 0; Slot < Horde.HighWater; ++Slot)
	{
		const FVector2f& P = Horde.Pos[Slot];
		if (Horde.Alive[Slot] && P.X >= WorldMin.X && P.X <= WorldMax.X && P.Y >= WorldMin.Y && P.Y <= WorldMax.Y)
		{
			Visible.Add(Slot);
		}
	}
	const int32 NumBuckets = FMath::Max(FMath::RoundToInt(UI().SortBuckets), 1);
	const float BucketScale = NumBuckets / FMath::Max(WorldMax.Y - WorldMin.Y, KINDA_SMALL_NUMBER);
	// SetNumZeroed는 이미 같은 크기면 지우지 않으므로 비우고 다시 채운다
	BucketStart.Reset();
	BucketStart.AddZeroed(NumBuckets + 1);
	for (const int32 Slot : Visible)
	{
		++BucketStart[FMath::Clamp(FMath::FloorToInt((Horde.Pos[Slot].Y - WorldMin.Y) * BucketScale), 0, NumBuckets - 1) + 1];
	}
	for (int32 Bucket = 1; Bucket <= NumBuckets; ++Bucket)
	{
		BucketStart[Bucket] += BucketStart[Bucket - 1];
	}
	Sorted.SetNumUninitialized(Visible.Num());
	for (const int32 Slot : Visible)
	{
		Sorted[BucketStart[FMath::Clamp(FMath::FloorToInt((Horde.Pos[Slot].Y - WorldMin.Y) * BucketScale), 0, NumBuckets - 1)]++] = Slot;
	}
	const int32 Count = Sorted.Num();
	if (Count == 0)
	{
		UnitDrawMs = 0.f;
		return;
	}
	const TConstArrayView<FYJJUnitDef> Defs = YJJData::Units();
	const FYJJUILayout& L = UI();
	const int32 NumChunks = FMath::DivideAndRoundUp(Count, SpriteChunk);

	// 그림자 (가까이 볼 때만). 인스턴스는 ParallelFor로 채우고 GPU 인스턴싱 한 번으로 그린다
	if (Zoom >= L.ShadowMinZoom)
	{
		TArray<FYJJSpriteInstance>& Shadows = FYJJSpriteRenderItem::Add(Canvas->Canvas, FxAtlas.Texture->GetResource(), Count);
		const FVector2f ShadowUV = FxAtlas.UV(FMath::RoundToInt(L.FxShadow));
		const FLinearColor ShadowColor(1.f, 1.f, 1.f, L.ShadowAlpha);
		ParallelFor(NumChunks, [&](int32 Chunk)
		{
			const int32 End = FMath::Min((Chunk + 1) * SpriteChunk, Count);
			for (int32 Index = Chunk * SpriteChunk; Index < End; ++Index)
			{
				const int32 Slot = Sorted[Index];
				const FYJJUnitDef& Def = Defs[Horde.Type[Slot]];
				const float Size = Def.DrawSize * Zoom;
				const FVector2f Feet = WorldToScreen(Horde.Pos[Slot]);
				// 큰 유닛(AI 원화)은 발 붙은 자리를 진하게: 안 그러면 땅에서 떠 보인다
				const bool bBig = Def.BigRow != INDEX_NONE;
				const float Width = bBig ? L.BigShadowWidth : L.ShadowWidth;
				FYJJSpriteInstance& Sprite = Shadows[Index];
				Sprite.Rect = FVector4f(Feet.X, Feet.Y, Size * Width * Half, Size * L.ShadowHeight * Width / L.ShadowWidth * Half);
				Sprite.UV = FVector4f(ShadowUV.X, ShadowUV.Y, FxAtlas.CellUV.X, FxAtlas.CellUV.Y);
				Sprite.Color = bBig ? FLinearColor(1.f, 1.f, 1.f, L.BigShadowAlpha) : ShadowColor;
			}
		});
	}

	// 큰 유닛(보스·영웅)은 고해상도 아틀라스라 따로 그리되, 앞뒤(y) 순서를 지키도록 작은 유닛을 큰 유닛 사이사이로 끊어 그린다.
	// (작은 유닛을 다 그린 뒤 몰아 그리면 앞에 선 졸병까지 영웅 뒤로 깔려 영웅이 땅에서 떠 보인다)
	BigSorted.Reset();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (BigAtlasFor(Defs[Horde.Type[Sorted[Index]]]))
		{
			BigSorted.Add(Index);
		}
	}
	int32 Cursor = 0;
	for (const int32 BigIndex : BigSorted)
	{
		DrawSmallUnits(Cursor, BigIndex);
		DrawBigUnit(Sorted[BigIndex]);
		Cursor = BigIndex + 1;
	}
	DrawSmallUnits(Cursor, Count);
	UnitDrawMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * MsPerSecond);
}

void AYJJWaveHUD::DrawSmallUnits(int32 Begin, int32 End)
{
	if (End <= Begin)
	{
		return;
	}
	const FYJJHordeView& Horde = View();
	const TConstArrayView<FYJJUnitDef> Defs = YJJData::Units();
	const FYJJUILayout& L = UI();
	const bool bRally = Battle()->RallyLeft() > 0.f;
	TArray<FYJJSpriteInstance>& Sprites = FYJJSpriteRenderItem::Add(Canvas->Canvas, UnitAtlas.Texture->GetResource(), End - Begin);
	ParallelFor(FMath::DivideAndRoundUp(End - Begin, SpriteChunk), [&](int32 Chunk)
	{
		const int32 ChunkEnd = FMath::Min(Begin + (Chunk + 1) * SpriteChunk, End);
		for (int32 Index = Begin + Chunk * SpriteChunk; Index < ChunkEnd; ++Index)
		{
			const int32 Slot = Sorted[Index];
			const FYJJUnitDef& Def = Defs[Horde.Type[Slot]];
			const float Size = Def.DrawSize * Zoom;
			const int32 Frame = PickFrame(Horde, Slot);
			const FVector2f Feet = WorldToScreen(Horde.Pos[Slot]);
			FLinearColor Color = FLinearColor::White;
			if (Horde.Flash[Slot] > 0.f)
			{
				Color = YJJUI::HitTint;
			}
			else if (bRally && Horde.Team[Slot] == static_cast<uint8>(EYJJTeam::Player))
			{
				Color = YJJUI::Highlight;
			}
			const FVector2f UV0 = UnitAtlas.UV(Frame, Def.AtlasRow);
			const bool bFlip = Horde.FaceLeft[Slot] != 0;
			FYJJSpriteInstance& Sprite = Sprites[Index - Begin];
			Sprite.Rect = FVector4f(Feet.X, Feet.Y - Size * L.FootAnchor + Size * Half, Size * Half, Size * Half);
			Sprite.UV = bFlip
				? FVector4f(UV0.X + UnitAtlas.CellUV.X, UV0.Y, -UnitAtlas.CellUV.X, UnitAtlas.CellUV.Y)
				: FVector4f(UV0.X, UV0.Y, UnitAtlas.CellUV.X, UnitAtlas.CellUV.Y);
			Sprite.Color = Color;
		}
	});
}

void AYJJWaveHUD::DrawBigUnit(int32 Slot)
{
	const FYJJHordeView& Horde = View();
	const FYJJUnitDef& Def = YJJData::Unit(Horde.Type[Slot]);
	const FYJJUILayout& L = UI();
	const float Size = Def.DrawSize * Zoom;
	const FVector2f Feet = WorldToScreen(Horde.Pos[Slot]);
	float Sub = 0.f;
	const int32 Frame = PickFrame(Horde, Slot, &Sub);
	const FYJJAtlas& BigAtlas = *BigAtlasFor(Def);
	const FVector2f UV0 = BigAtlas.UV(Frame, 0, FMath::Min(FMath::FloorToInt(Sub * BigAtlas.FrameMul), BigAtlas.FrameMul - 1));
	const bool bFlip = Horde.FaceLeft[Slot] != 0;
	FYJJSpriteInstance& Sprite = FYJJSpriteRenderItem::Add(Canvas->Canvas, BigAtlas.Texture->GetResource(), 1)[0];
	Sprite.Rect = FVector4f(Feet.X, Feet.Y - Size * L.FootAnchor + Size * Half, Size * Half, Size * Half);
	Sprite.UV = bFlip
		? FVector4f(UV0.X + BigAtlas.CellUV.X, UV0.Y, -BigAtlas.CellUV.X, BigAtlas.CellUV.Y)
		: FVector4f(UV0.X, UV0.Y, BigAtlas.CellUV.X, BigAtlas.CellUV.Y);
	// AI 원화는 쨍해서 땅 위에 붙여 놓은 듯 뜬다 → 땅 빛(조금 어둡고 따뜻하게)을 곱한다
	Sprite.Color = Horde.Flash[Slot] > 0.f ? YJJUI::HitTint : FLinearColor(L.ArtTintR, L.ArtTintG, L.ArtTintB, 1.f);
}

int32 AYJJWaveHUD::PickFrame(const FYJJHordeView& Horde, int32 Slot, float* OutSub) const
{
	const FYJJUILayout& L = UI();
	const FYJJTuning& T = YJJData::Tune();
	// 동작 하나 = 첫 칸 + 칸 수 + 진행(0~1). 칸 안에서 남은 진행은 OutSub (큰 유닛 중간 프레임)
	float First = L.AnimIdleFirst;
	float Frames = L.AnimIdleFrames;
	float Fraction = 0.f;
	if (Horde.Knock[Slot] >= L.AnimDazedImpulse)
	{
		// 세게 밀려 휘청 (헤벌레)
		First = L.AnimDazedFirst;
		Frames = L.AnimDazedFrames;
		Fraction = FMath::Frac(AnimClock * L.AnimDazedFps / FMath::Max(L.AnimDazedFrames, 1.f) + Horde.WalkPhase[Slot]);
	}
	else if (Horde.AttackAnim[Slot] > 0.f)
	{
		First = L.AnimAttackFirst;
		Frames = L.AnimAttackFrames;
		Fraction = 1.f - Horde.AttackAnim[Slot] / FMath::Max(Horde.AttackLen[Slot], KINDA_SMALL_NUMBER);
	}
	else if (Horde.Flash[Slot] > 0.f)
	{
		First = L.AnimHitFirst;
		Frames = L.AnimHitFrames;
		Fraction = 1.f - Horde.Flash[Slot] / FMath::Max(T.HitFlashTime, KINDA_SMALL_NUMBER);
	}
	else if (Horde.MoveSpeed[Slot] >= L.AnimIdleSpeed)
	{
		First = L.AnimWalkFirst;
		Frames = L.AnimWalkFrames;
		Fraction = FMath::Frac(Horde.WalkPhase[Slot]);
	}
	else
	{
		// 숨쉬기: 유닛마다 박자를 조금씩 어긋나게 (걷기 위상을 씨앗으로)
		Fraction = FMath::Frac(AnimClock * L.AnimIdleFps / FMath::Max(L.AnimIdleFrames, 1.f) + Horde.WalkPhase[Slot]);
	}
	const float Scaled = FMath::Clamp(Fraction, 0.f, 1.f) * Frames;
	const int32 Step = FMath::Clamp(FMath::FloorToInt(Scaled), 0, FMath::Max(FMath::RoundToInt(Frames) - 1, 0));
	if (OutSub)
	{
		*OutSub = FMath::Clamp(Scaled - Step, 0.f, 1.f);
	}
	return FMath::RoundToInt(First) + Step;
}

void AYJJWaveHUD::DrawDying()
{
	if (!UnitAtlas.Texture || Dying.Num() == 0)
	{
		return;
	}
	// 작은 몸 → 큰 몸 순서로 아틀라스별 배치
	for (int32 Pass = 0; Pass <= BigAtlases.Num(); ++Pass)
	{
		const FYJJAtlas& Atlas = Pass == 0 ? UnitAtlas : BigAtlases[Pass - 1];
		if (!Atlas.Texture)
		{
			continue;
		}
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, Atlas.Texture, Dying.Num());
		for (const FYJJDying& Body : Dying)
		{
			const FYJJUnitDef& Def = YJJData::Unit(Body.Type);
			if (&AtlasFor(Def) != &Atlas)
			{
				continue;
			}
			const float Size = Def.DrawSize * Zoom;
			const FVector2f Center = WorldToScreen(Body.Pos - FVector2f(0.f, Body.Height + Def.DrawSize * (UI().FootAnchor - Half)));
			AddQuadRotated(Batch, Center, FVector2f(Size), Body.Angle, Atlas.UV(FMath::RoundToInt(UI().AnimHitFirst), RowFor(Def)), Atlas.CellUV, FLinearColor::White, Body.bFaceLeft);
		}
	}
}

void AYJJWaveHUD::DrawProjectiles()
{
	const FYJJHordeView& Horde = View();
	if (!UnitAtlas.Texture || Horde.Projectiles.Num() == 0)
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, UnitAtlas.Texture, Horde.Projectiles.Num());
	const int32 Row = FMath::RoundToInt(UI().ProjectileRow);
	const FVector2f Size(UI().ProjectileSize * Zoom);
	for (const FYJJProjectile& Shot : Horde.Projectiles)
	{
		// 포물선: 가는 동안 위로 솟았다가 내려온다 (높이는 거리에 비례)
		const float Total = FVector2f::Distance(Shot.Start, Shot.Aim);
		const float Done = FVector2f::Distance(Shot.Start, Shot.Pos);
		const float T = Total > KINDA_SMALL_NUMBER ? FMath::Clamp(Done / Total, 0.f, 1.f) : 1.f;
		const float Arc = FMath::Min(Total * UI().ArrowArcPerDistance, UI().ArrowArcMax);
		const float Height = FMath::Sin(T * UE_PI) * Arc;
		const float Slope = Total > KINDA_SMALL_NUMBER ? FMath::Cos(T * UE_PI) * UE_PI * Arc / Total : 0.f;
		const FVector2f Dir = (Shot.Aim - Shot.Start).GetSafeNormal();
		const float Angle = FMath::Atan2(Dir.Y - Slope, Dir.X);
		const FVector2f Center = WorldToScreen(Shot.Pos - FVector2f(0.f, Height));
		AddQuadRotated(Batch, Center, Size, Angle, UnitAtlas.UV(Shot.Frame, Row), UnitAtlas.CellUV, FLinearColor::White, false);
	}
}

void AYJJWaveHUD::DrawEffects()
{
	const FYJJBattle* Current = Battle();
	if (!FxAtlas.Texture || Current->Effects.Num() == 0)
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, Current->Effects.Num() * EffectQuadsGuess);
	for (const FYJJEffect& Effect : Current->Effects)
	{
		const float T = FMath::Clamp(Effect.Age / FMath::Max(Effect.Life, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const FVector2f Ground = WorldToScreen(Effect.Pos);
		switch (Effect.Kind)
		{
		case EYJJEffectKind::MeteorFall:
		{
			// 떨어질 자리 표시 + 하늘에서 비스듬히 내려오는 운석
			const float RingSize = Effect.Radius * 2.f * Zoom * FMath::Lerp(UI().AimRingStart, 1.f, T);
			AddQuad(Batch, Ground - FVector2f(RingSize * Half), FVector2f(RingSize), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::Bad, T), false);
			const FVector2f Sky = Effect.Pos + FVector2f(UI().MeteorOffsetX, -UI().MeteorFallHeight) * (1.f - T);
			const float Size = UI().MeteorDrawSize * Zoom;
			const float Angle = FMath::Atan2(UI().MeteorFallHeight, -UI().MeteorOffsetX);
			AddQuadRotated(Batch, WorldToScreen(Sky), FVector2f(Size), Angle, FxAtlas.UV(FMath::RoundToInt(UI().FxMeteor)), FxAtlas.CellUV, FLinearColor::White, false);
			break;
		}
		case EYJJEffectKind::MeteorBlast:
		{
			const float Size = Effect.Radius * 2.f * Zoom * FMath::Lerp(UI().BlastStartScale, UI().BlastEndScale, T);
			AddQuad(Batch, Ground - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxBlast)), FxAtlas.CellUV, FLinearColor(1.f, 1.f, 1.f, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::Rally:
		{
			const float Size = Effect.Radius * Zoom * T * 2.f;
			AddQuad(Batch, Ground - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::Highlight, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::Rescue:
		{
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().DustStartScale, UI().DustEndScale, T);
			AddQuad(Batch, Ground - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::Good, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::Raise:
		{
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().DustStartScale, UI().DustEndScale, T);
			AddQuad(Batch, Ground - FVector2f(Size * Half, Size * UI().PlotAnchor), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxDust)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::PerkKeystone, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::Burn:
		{
			// 불꽃 몇 개가 원 안에서 일렁인다. 끝날 즈음 사그라진다
			const int32 Flames = FMath::RoundToInt(UI().BurnFlames);
			const float Fade = FMath::Clamp((1.f - T) / FMath::Max(UI().BurnFadeShare, KINDA_SMALL_NUMBER), 0.f, 1.f);
			for (int32 Index = 0; Index < Flames; ++Index)
			{
				const float Angle = Index * UE_TWO_PI / Flames + Index;
				const float Ring = Effect.Radius * UI().BurnSpread * ((Index % 2) ? 1.f : Half);
				const FVector2f Pos = Effect.Pos + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle) * UI().GroundSquash) * Ring;
				const float Flicker = 1.f + UI().BurnFlicker * FMath::Sin(Effect.Age * UI().BurnFlickerSpeed + Index * UI().BurnFlickerPhase);
				const float Size = Effect.Radius * UI().BurnFlameScale * Flicker * Zoom;
				AddQuad(Batch, WorldToScreen(Pos) - FVector2f(Size * Half, Size), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxFlame)), FxAtlas.CellUV, FLinearColor(1.f, 1.f, 1.f, Fade), (Index % 2) != 0);
			}
			break;
		}
		case EYJJEffectKind::Promote:
		{
			// 금빛 빛줄기가 돌며 떠올랐다 사라진다
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().DustStartScale, 1.f, T);
			const FVector2f Center = WorldToScreen(Effect.Pos - FVector2f(0.f, UI().PromoteRise * T));
			AddQuadRotated(Batch, Center, FVector2f(Size), T * UI().BoneSpin, FxAtlas.UV(FMath::RoundToInt(UI().FxPromote)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::Highlight, 1.f - T * T), false);
			break;
		}
		case EYJJEffectKind::SmackFall:
		{
			// 머리 위에서 국자가 휘둘러 내려온다 (끝에 쾅: 판이 먼지·고리를 만든다)
			const float Size = UI().SmackDrawSize * Zoom;
			const float Angle = FMath::Lerp(UI().SmackStartAngle, 0.f, T * T);
			const FVector2f Pivot = Ground - FVector2f(0.f, Size * Half);
			AddQuadRotated(Batch, Pivot, FVector2f(Size), Angle, FxAtlas.UV(FMath::RoundToInt(UI().FxLadle)), FxAtlas.CellUV, FLinearColor::White, false);
			break;
		}
		case EYJJEffectKind::Slash:
		{
			// 붓으로 휙 그은 초승달이 휘두른 쪽으로 펼쳐지고, 맞은 자리에 먹 튀김이 터진다
			const float Open = FMath::Sqrt(T);
			const float Size = Effect.Radius * UI().SlashScale * Zoom * FMath::Lerp(UI().SlashStartScale, 1.f, Open);
			const FVector2f Center = Ground - FVector2f(0.f, Effect.Radius * UI().SlashLift * Zoom);
			const float Angle = Effect.Power < 0.f ? UE_PI : 0.f;
			AddQuadRotated(Batch, Center, FVector2f(Size), Angle + (Open - Half) * UI().SlashSweep * Effect.Power, FxAtlas.UV(FMath::RoundToInt(UI().FxSlash)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, 1.f - T * T), false);
			const float Burst = Effect.Radius * UI().ImpactScale * Zoom * FMath::Lerp(UI().SlashStartScale, 1.f, Open);
			AddQuadRotated(Batch, Center, FVector2f(Burst), T * UI().ImpactSpin, FxAtlas.UV(FMath::RoundToInt(UI().FxImpact)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::SkillWarn:
		{
			// 쓸어 버릴 원: 바깥 원은 그대로, 안쪽 원이 차오른다 (다 차면 쾅). 보스는 빨강, 아군은 금색
			const FLinearColor Color = Effect.Side < 0.f ? YJJUI::Bad : YJJUI::Highlight;
			const float Full = Effect.Radius * 2.f * Zoom;
			AddQuad(Batch, Ground - FVector2f(Full * Half), FVector2f(Full), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(Color, UI().SkillWarnAlpha), false);
			const float Inner = Full * T;
			AddQuad(Batch, Ground - FVector2f(Inner * Half), FVector2f(Inner), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(Color, FMath::Lerp(UI().SkillWarnFillAlpha, 1.f, T)), false);
			break;
		}
		case EYJJEffectKind::SkillBlast:
		{
			const FLinearColor Color = Effect.Side < 0.f ? YJJUI::Bad : YJJUI::Highlight;
			const float Open = FMath::Sqrt(T);
			const float Size = Effect.Radius * 2.f * Zoom * FMath::Lerp(UI().SkillBlastStart, UI().SkillBlastEnd, Open);
			AddQuad(Batch, Ground - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(Color, 1.f - T), false);
			AddQuadRotated(Batch, Ground, FVector2f(Size * UI().SkillBlastImpact), T * UI().ImpactSpin, FxAtlas.UV(FMath::RoundToInt(UI().FxImpact)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, 1.f - T), false);
			break;
		}
		case EYJJEffectKind::Spark:
		{
			// 난전 불꽃: 자리마다 다른 각도로 툭 터졌다 사라진다
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().SparkStartScale, 1.f, FMath::Sqrt(T));
			const float Angle = FMath::Frac((Effect.Pos.X + Effect.Pos.Y) * SparkAngleSeed) * FullTurn;
			AddQuadRotated(Batch, Ground - FVector2f(0.f, Effect.Radius * UI().SparkLift * Zoom), FVector2f(Size), Angle, FxAtlas.UV(FMath::RoundToInt(UI().FxImpact)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, 1.f - T * T), false);
			break;
		}
		case EYJJEffectKind::Build:
		{
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().DustStartScale, UI().DustEndScale, T);
			AddQuad(Batch, Ground - FVector2f(Size * Half, Size * UI().PlotAnchor), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxDust)), FxAtlas.CellUV, FLinearColor(1.f, 1.f, 1.f, 1.f - T), false);
			break;
		}
		}
	}
}

void AYJJWaveHUD::DrawSpellAim()
{
	if (AimingSpell == INDEX_NONE || !FxAtlas.Texture || IsMouseOverUI())
	{
		return;
	}
	const EYJJSpell Spell = static_cast<EYJJSpell>(AimingSpell);
	const FYJJSpellStats Stats = Battle()->SpellNow(Spell);
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, EffectQuadsGuess);
	const FLinearColor Color = YJJUI::WithAlpha(YJJUI::Highlight, UI().AimAlpha);
	const float Size = Stats.Radius * 2.f * Zoom;
	AddQuad(Batch, MouseScreen() - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, Color, false);
}

// ---------------------------------------------------------------------------------------------
// 좌표

FVector2f AYJJWaveHUD::MouseScreen() const
{
	float X = 0.f;
	float Y = 0.f;
	if (PlayerOwner && PlayerOwner->GetMousePosition(X, Y))
	{
		return FVector2f(X, Y);
	}
	return ScreenSize * Half;
}

FVector2f AYJJWaveHUD::ToVirtual(const FVector2f& Screen) const
{
	return (Screen - ViewOffset) / ViewScale;
}

FVector2f AYJJWaveHUD::WorldToVirtual(const FVector2f& World) const
{
	return ToVirtual(WorldToScreen(World));
}

int32 AYJJWaveHUD::PlotAt(const FVector2f& World) const
{
	const FYJJBattle* Current = Battle();
	const float Pick = UI().PlotPickRadius;
	for (int32 PlotIndex = 0; PlotIndex < Current->Plots.Num(); ++PlotIndex)
	{
		// 칸 좌표는 건물 발밑. 눈에 보이는 건물 몸통 가운데로 판정한다
		const FVector2f Body = Current->Plots[PlotIndex].Pos - FVector2f(0.f, UI().PlotPickLift);
		if (FVector2f::DistSquared(Body, World) <= Pick * Pick)
		{
			return PlotIndex;
		}
	}
	return INDEX_NONE;
}
