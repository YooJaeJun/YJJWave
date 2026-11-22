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
	DrawUnits();
	DrawDying();
	DrawProjectiles();
	DrawEffects();
	DrawMeteorAim();
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
		const FVector2f QuadSize(Size, Size * Bounce);
		const FVector2f TopLeft(Feet.X - QuadSize.X * Half, Feet.Y - QuadSize.Y * UI().PlotAnchor);
		AddQuad(Batch, TopLeft, QuadSize, BuildingAtlas.UV(Column, 0), BuildingAtlas.CellUV, bHover ? YJJUI::Highlight : FLinearColor::White, false);
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
				const float Size = Defs[Horde.Type[Slot]].DrawSize * Zoom;
				const FVector2f Feet = WorldToScreen(Horde.Pos[Slot]);
				FYJJSpriteInstance& Sprite = Shadows[Index];
				Sprite.Rect = FVector4f(Feet.X, Feet.Y, Size * L.ShadowWidth * Half, Size * L.ShadowHeight * Half);
				Sprite.UV = FVector4f(ShadowUV.X, ShadowUV.Y, FxAtlas.CellUV.X, FxAtlas.CellUV.Y);
				Sprite.Color = ShadowColor;
			}
		});
	}

	const bool bRally = Current->RallyLeft() > 0.f;
	TArray<FYJJSpriteInstance>& Sprites = FYJJSpriteRenderItem::Add(Canvas->Canvas, UnitAtlas.Texture->GetResource(), Count);
	ParallelFor(NumChunks, [&](int32 Chunk)
	{
		const int32 End = FMath::Min((Chunk + 1) * SpriteChunk, Count);
		for (int32 Index = Chunk * SpriteChunk; Index < End; ++Index)
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
			FYJJSpriteInstance& Sprite = Sprites[Index];
			Sprite.Rect = FVector4f(Feet.X, Feet.Y - Size * L.FootAnchor + Size * Half, Size * Half, Size * Half);
			Sprite.UV = bFlip
				? FVector4f(UV0.X + UnitAtlas.CellUV.X, UV0.Y, -UnitAtlas.CellUV.X, UnitAtlas.CellUV.Y)
				: FVector4f(UV0.X, UV0.Y, UnitAtlas.CellUV.X, UnitAtlas.CellUV.Y);
			Sprite.Color = Color;
		}
	});
	UnitDrawMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * MsPerSecond);
}

int32 AYJJWaveHUD::PickFrame(const FYJJHordeView& Horde, int32 Slot) const
{
	const FYJJUILayout& L = UI();
	const FYJJTuning& T = YJJData::Tune();
	auto Step = [](float Fraction, float Frames) { return FMath::Clamp(FMath::FloorToInt(Fraction * Frames), 0, FMath::Max(FMath::RoundToInt(Frames) - 1, 0)); };
	if (Horde.AttackAnim[Slot] > 0.f)
	{
		const float Done = 1.f - Horde.AttackAnim[Slot] / FMath::Max(T.AttackAnimTime, KINDA_SMALL_NUMBER);
		return FMath::RoundToInt(L.AnimAttackFirst) + Step(Done, L.AnimAttackFrames);
	}
	if (Horde.Flash[Slot] > 0.f)
	{
		const float Done = 1.f - Horde.Flash[Slot] / FMath::Max(T.HitFlashTime, KINDA_SMALL_NUMBER);
		return FMath::RoundToInt(L.AnimHitFirst) + Step(Done, L.AnimHitFrames);
	}
	if (Horde.MoveSpeed[Slot] >= L.AnimIdleSpeed)
	{
		return FMath::RoundToInt(L.AnimWalkFirst) + Step(FMath::Frac(Horde.WalkPhase[Slot]), L.AnimWalkFrames);
	}
	// 숨쉬기: 유닛마다 박자를 조금씩 어긋나게 (걷기 위상을 씨앗으로)
	const float Breath = FMath::Frac(AnimClock * L.AnimIdleFps / FMath::Max(L.AnimIdleFrames, 1.f) + Horde.WalkPhase[Slot]);
	return FMath::RoundToInt(L.AnimIdleFirst) + Step(Breath, L.AnimIdleFrames);
}

void AYJJWaveHUD::DrawDying()
{
	if (!UnitAtlas.Texture || Dying.Num() == 0)
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, UnitAtlas.Texture, Dying.Num());
	for (const FYJJDying& Body : Dying)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Body.Type);
		const float Size = Def.DrawSize * Zoom;
		const FVector2f Center = WorldToScreen(Body.Pos - FVector2f(0.f, Body.Height + Def.DrawSize * (UI().FootAnchor - Half)));
		AddQuadRotated(Batch, Center, FVector2f(Size), Body.Angle, UnitAtlas.UV(FMath::RoundToInt(UI().AnimHitFirst), Def.AtlasRow), UnitAtlas.CellUV, FLinearColor::White, Body.bFaceLeft);
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
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, Current->Effects.Num() * 2);
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
		case EYJJEffectKind::Build:
		{
			const float Size = Effect.Radius * Zoom * FMath::Lerp(UI().DustStartScale, UI().DustEndScale, T);
			AddQuad(Batch, Ground - FVector2f(Size * Half, Size * UI().PlotAnchor), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxDust)), FxAtlas.CellUV, FLinearColor(1.f, 1.f, 1.f, 1.f - T), false);
			break;
		}
		}
	}
}

void AYJJWaveHUD::DrawMeteorAim()
{
	if (!bAimingMeteor || !FxAtlas.Texture || IsMouseOverUI())
	{
		return;
	}
	const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, 1);
	const float Size = YJJData::Tune().MeteorRadius * 2.f * Zoom;
	AddQuad(Batch, MouseScreen() - FVector2f(Size * Half), FVector2f(Size), FxAtlas.UV(FMath::RoundToInt(UI().FxRing)), FxAtlas.CellUV, YJJUI::WithAlpha(YJJUI::Highlight, UI().AimAlpha), false);
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
