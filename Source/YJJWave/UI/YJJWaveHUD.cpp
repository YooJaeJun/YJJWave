#include "UI/YJJWaveHUD.h"
#include "BatchedElements.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"
#include "Render/YJJTextureLoader.h"
#include "Rendering/SlateRenderer.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "YJJWave.h"
#include "YJJWaveGameMode.h"

// ---------------------------------------------------------------------------------------------
// 아틀라스

void FYJJAtlas::Init(UTexture2D* InTexture, float CellPixels)
{
	Texture = InTexture;
	if (!Texture)
	{
		return;
	}
	Columns = FMath::Max(1, FMath::RoundToInt(Texture->GetSizeX() / CellPixels));
	CellUV = FVector2f(CellPixels / Texture->GetSizeX(), CellPixels / Texture->GetSizeY());
}

FVector2f FYJJAtlas::UV(int32 Cell) const
{
	return UV(Cell % Columns, Cell / Columns);
}

FVector2f FYJJAtlas::UV(int32 Column, int32 Row) const
{
	return FVector2f(Column * CellUV.X, Row * CellUV.Y);
}

// ---------------------------------------------------------------------------------------------
// 준비

AYJJWaveHUD::AYJJWaveHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

const FYJJUILayout& AYJJWaveHUD::UI() const
{
	return YJJData::Layout();
}

FYJJBattle* AYJJWaveHUD::Battle() const
{
	AYJJWaveGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AYJJWaveGameMode>() : nullptr;
	return Mode ? &Mode->BattleNoWait() : nullptr;
}

FYJJBattle* AYJJWaveHUD::EditBattle() const
{
	AYJJWaveGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AYJJWaveGameMode>() : nullptr;
	return Mode ? &Mode->GetBattle() : nullptr;
}

const FYJJHordeView& AYJJWaveHUD::View() const
{
	return GetWorld()->GetAuthGameMode<AYJJWaveGameMode>()->GetView();
}

void AYJJWaveHUD::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	LoadArt();
	ResetBattleView();
}

void AYJJWaveHUD::LoadArt()
{
	UnitTexture = FYJJTextureLoader::Load(TEXT("Units.png"), true);
	BigUnitTexture = FYJJTextureLoader::Load(TEXT("BigUnits.png"), true);
	FxTexture = FYJJTextureLoader::Load(TEXT("Fx.png"), true);
	BuildingTexture = FYJJTextureLoader::Load(TEXT("Buildings.png"), true);
	UiTexture = FYJJTextureLoader::Load(TEXT("UI.png"), true);
	GroundTexture = FYJJTextureLoader::Load(TEXT("Ground.png"), false);
	UnitAtlas.Init(UnitTexture, UI().UnitCell);
	BigAtlas.Init(BigUnitTexture, UI().BigUnitCell);
	FxAtlas.Init(FxTexture, UI().FxCell);
	BuildingAtlas.Init(BuildingTexture, UI().BuildingCell);
	UiAtlas.Init(UiTexture, UI().UiCell);
	BaseFont = LoadObject<UFont>(nullptr, BaseFontPath);
}

void AYJJWaveHUD::ResetBattleView()
{
	Dying.Reset();
	Stamps.Reset();
	Banners.Reset();
	TalkQueue.Reset();
	TalkUsed.Reset();
	TalkUsed.AddZeroed(YJJData::Talks().Num());
	TalkNow = FYJJTalkPlay();
	NextMilestone = UI().MilestoneFirst;
	KillWindow = 0.f;
	KillsAtWindow = 0;
	bBossSeen = false;
	ShakeLeft = 0.f;
	bGroundReady = false;
	bTreeOpen = false;
	AimingSpell = INDEX_NONE;
	if (const FYJJBattle* Current = Battle())
	{
		Zoom = UI().ZoomDefault;
		CamCenter = FVector2f(UI().CameraStartX, Current->FieldHeight * Half);
	}
}

void AYJJWaveHUD::InitGround()
{
	const FYJJBattle* Current = Battle();
	if (!Current || bGroundReady)
	{
		return;
	}
	const int32 Width = FMath::RoundToInt(UI().GroundTargetWidth);
	const int32 Height = FMath::RoundToInt(UI().GroundTargetWidth * Current->FieldHeight / Current->FieldWidth);
	if (!GroundTarget || GroundTarget->SizeX != Width || GroundTarget->SizeY != Height)
	{
		GroundTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Width, Height, RTF_RGBA8_SRGB, YJJUI::Field);
	}
	if (!GroundTarget)
	{
		return;
	}

	UCanvas* TargetCanvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GroundTarget, TargetCanvas, Size, Context);
	if (TargetCanvas && GroundTexture)
	{
		FCanvasTileItem Item(FVector2D::ZeroVector, GroundTexture->GetResource(), Size, FLinearColor::White);
		Item.BlendMode = SE_BLEND_Opaque;
		TargetCanvas->DrawItem(Item);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	bGroundReady = true;
}

TSharedPtr<const FCompositeFont> AYJJWaveHUD::GetGameFont()
{
	if (GameFont.IsValid())
	{
		return GameFont;
	}
	const FString FontPath = FPaths::ProjectContentDir() / GameFontFile;
	if (!FPaths::FileExists(FontPath))
	{
		return nullptr;
	}
	TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
	Composite->DefaultTypeface.Fonts.Add(FTypefaceEntry(GameFontFace, FontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
	if (const FCompositeFont* Fallback = BaseFont ? BaseFont->GetCompositeFont() : nullptr)
	{
		Composite->FallbackTypeface.Typeface = Fallback->DefaultTypeface;
	}
	GameFont = Composite;
	return GameFont;
}

// ---------------------------------------------------------------------------------------------
// 매 프레임

void AYJJWaveHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FrameMs = FMath::Lerp(FrameMs, DeltaSeconds * 1000.f, UI().FrameMsSmoothing);

	if (!Battle())
	{
		return;
	}
	InitGround();
	UpdateCamera(DeltaSeconds);
	UpdateCutsceneCamera(DeltaSeconds);
	TakeDeaths();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	UpdateDying(Mode && Mode->IsRunning() ? DeltaSeconds * Mode->GetSpeed() : 0.f);
	UpdateBanners(DeltaSeconds);
	TakeEvents();
	UpdateTalks(DeltaSeconds);
	BakeStamps();
}

void AYJJWaveHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !Battle())
	{
		return;
	}
	ScreenSize = FVector2f(Canvas->ClipX, Canvas->ClipY);
	AnimClock = GetWorld()->GetRealTimeSeconds();
	// UI는 화면 높이 1080 기준으로 늘리고 왼쪽에 붙인다. 가상 너비는 화면 비율대로
	ViewScale = ScreenSize.Y / VirtualHeight;
	ViewOffset = FVector2f::ZeroVector;
	VirtualW = ScreenSize.X / ViewScale;
	Buttons.Reset();

	// 화면 흔들림: 전장만 흔든다 (UI는 그대로)
	ShakeOffset = ShakeLeft > 0.f
		? FVector2f(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * UI().ShakePixels * ViewScale * (ShakeLeft / FMath::Max(UI().ShakeTime, KINDA_SMALL_NUMBER))
		: FVector2f::ZeroVector;
	DrawWorld();
	DrawTalks();
	DrawBanners();
	if (Battle()->bCardPending && !Battle()->bGameOver)
	{
		DrawCardPick();
	}

	if (bTreeOpen)
	{
		DrawTree();
	}
	else
	{
		DrawPlotTooltip();
		DrawPotTooltip();
	}
	DrawSidePanel();
	DrawCutscene();
	DrawGameOver();
	DrawDebugLine();
	if (bOptionsOpen)
	{
		DrawOptions();
	}
}

// ---------------------------------------------------------------------------------------------
// 카메라

FVector2f AYJJWaveHUD::WorldViewCenter() const
{
	// 전장은 좌측 패널 오른쪽 전부
	const float Left = UI().SidePanelW * ViewScale;
	return FVector2f(Left + (ScreenSize.X - Left) * Half, ScreenSize.Y * Half) + ShakeOffset;
}

FVector2f AYJJWaveHUD::WorldViewSize() const
{
	return FVector2f(ScreenSize.X - UI().SidePanelW * ViewScale, ScreenSize.Y);
}

float AYJJWaveHUD::WorldCenterVirtualX() const
{
	return (UI().SidePanelW + VirtualW) * Half;
}

FVector2f AYJJWaveHUD::WorldToScreen(const FVector2f& World) const
{
	return (World - CamCenter) * Zoom + WorldViewCenter();
}

FVector2f AYJJWaveHUD::ScreenToWorld(const FVector2f& Screen) const
{
	return (Screen - WorldViewCenter()) / Zoom + CamCenter;
}

void AYJJWaveHUD::FocusWorld(const FVector2f& World, float NewZoom)
{
	CamCenter = World;
	Zoom = FMath::Clamp(NewZoom, UI().ZoomMin, UI().ZoomMax);
	ClampCamera();
}

void AYJJWaveHUD::ClampCamera()
{
	const FYJJBattle* Current = Battle();
	if (!Current)
	{
		return;
	}
	// 전장이 전장 화면 영역을 빈틈없이 채우도록: 이보다 멀리 볼 수 없다 (빈 띠가 안 보이게)
	const FVector2f ViewSize = WorldViewSize();
	if (ViewSize.Y > 0.f && Current->FieldHeight > 0.f)
	{
		const float FillZoom = FMath::Max(ViewSize.Y / Current->FieldHeight, ViewSize.X / Current->FieldWidth);
		Zoom = FMath::Max(Zoom, FillZoom);
	}
	const FVector2f HalfView = ViewSize * (Half / Zoom);
	const float Margin = 0.f;
	const float MinX = HalfView.X - Margin;
	const float MaxX = Current->FieldWidth - HalfView.X + Margin;
	const float MinY = HalfView.Y - Margin;
	const float MaxY = Current->FieldHeight - HalfView.Y + Margin;
	CamCenter.X = MinX > MaxX ? Current->FieldWidth * Half : FMath::Clamp(CamCenter.X, MinX, MaxX);
	CamCenter.Y = MinY > MaxY ? Current->FieldHeight * Half : FMath::Clamp(CamCenter.Y, MinY, MaxY);
}

void AYJJWaveHUD::UpdateCamera(float Dt)
{
	FVector2f Pan = PanInput;
	// 화면 가장자리에 마우스를 대면 그쪽으로
	const FVector2f Mouse = MouseScreen();
	const float Edge = UI().EdgePanPixels;
	if (!bTreeOpen && UI().EdgePan > 0.f && GEngine && GEngine->GameViewport && GEngine->GameViewport->GetGameViewport() && GEngine->GameViewport->GetGameViewport()->HasMouseCapture())
	{
		const float Left = UI().SidePanelW * ViewScale;
		Pan.X += (Mouse.X >= Left && Mouse.X < Left + Edge) ? -1.f : (Mouse.X > ScreenSize.X - Edge ? 1.f : 0.f);
		Pan.Y += Mouse.Y < Edge ? -1.f : (Mouse.Y > ScreenSize.Y - Edge ? 1.f : 0.f);
	}
	if (!Pan.IsNearlyZero())
	{
		CamCenter += Pan.GetSafeNormal() * (UI().PanSpeed * Dt / Zoom);
	}
	if (bDragging)
	{
		CamCenter -= (Mouse - DragLast) / Zoom;
		DragLast = Mouse;
	}
	ClampCamera();
}

// ---------------------------------------------------------------------------------------------
// 시체와 바닥

void AYJJWaveHUD::TakeDeaths()
{
	FYJJBattle* Current = Battle();
	const int32 MaxDying = FMath::RoundToInt(UI().MaxDying);
	for (const FYJJDeath& Death : Current->RecentDeaths)
	{
		const FYJJUnitDef& Def = YJJData::Unit(Death.Type);
		if (Dying.Num() >= MaxDying)
		{
			// 너무 많으면 날지 않고 바로 눕는다
			FYJJStamp& Stamp = Stamps.AddDefaulted_GetRef();
			Stamp.Pos = Death.Pos;
			Stamp.Size = Def.DrawSize;
			Stamp.Cell = RowFor(Def) * AtlasFor(Def).Columns + FMath::RoundToInt(UI().AnimCorpse);
			Stamp.bUnits = true;
			Stamp.bBig = &AtlasFor(Def) == &BigAtlas;
			Stamp.bFlip = Death.bFaceLeft;
			Stamp.Color = FLinearColor::White;
			AddBlood(Death.Pos, Def.DrawSize);
			continue;
		}
		FYJJDying& Body = Dying.AddDefaulted_GetRef();
		Body.Pos = Death.Pos;
		Body.Vel = Death.Fling + FVector2f(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * UI().DeathJitter;
		// 세게 맞아 날아갈수록 높이 튄다 (망치·유성)
		Body.Rise = (UI().DeathRise * Random.FRandRange(UI().DeathRiseMin, 1.f) + Death.Fling.Size() * UI().DeathRiseFromFling) / FMath::Max(FMath::Sqrt(Def.Mass), 1.f);
		Body.Spin = Random.FRandRange(-1.f, 1.f) * UI().DeathSpin;
		Body.Type = Death.Type;
		Body.bFaceLeft = Death.bFaceLeft;
	}
	Current->RecentDeaths.Reset();

	if (Current->RecentCraters.Num() > 0)
	{
		Shake();
	}
	for (const FVector3f& Crater : Current->RecentCraters)
	{
		FYJJStamp& Stamp = Stamps.AddDefaulted_GetRef();
		Stamp.Pos = FVector2f(Crater.X, Crater.Y);
		Stamp.Size = Crater.Z * UI().CraterScale;
		Stamp.Cell = FMath::RoundToInt(UI().FxScorch);
		Stamp.Angle = Random.FRand() * FullTurn;
		Stamp.Color = FLinearColor::White;
	}
	Current->RecentCraters.Reset();
}

void AYJJWaveHUD::AddBlood(const FVector2f& Pos, float Size)
{
	FYJJStamp& Blood = Stamps.AddDefaulted_GetRef();
	Blood.Pos = Pos + FVector2f(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Size * UI().BloodScatter;
	Blood.Size = Size * Random.FRandRange(UI().BloodSizeMin, UI().BloodSizeMax);
	Blood.Cell = FMath::RoundToInt(UI().FxBloodFirst) + Random.RandHelper(FMath::Max(FMath::RoundToInt(UI().FxBloodCount), 1));
	Blood.Angle = Random.FRand() * FullTurn;
	Blood.Color = FLinearColor(1.f, 1.f, 1.f, Random.FRandRange(UI().BloodAlphaMin, 1.f));
}

void AYJJWaveHUD::UpdateDying(float Dt)
{
	if (Dt <= 0.f)
	{
		return;
	}
	const float Gravity = UI().DeathGravity;
	for (int32 Index = Dying.Num() - 1; Index >= 0; --Index)
	{
		FYJJDying& Body = Dying[Index];
		Body.Pos += Body.Vel * Dt;
		Body.Vel *= FMath::Max(1.f - UI().DeathDrag * Dt, 0.f);
		Body.Height += Body.Rise * Dt;
		Body.Rise -= Gravity * Dt;
		Body.Angle += Body.Spin * Dt;
		if (Body.Height > 0.f || Body.Rise > 0.f)
		{
			continue;
		}
		// 땅에 닿았다 → 바닥에 굽는다
		const FYJJUnitDef& Def = YJJData::Unit(Body.Type);
		AddBlood(Body.Pos, Def.DrawSize);
		FYJJStamp& Stamp = Stamps.AddDefaulted_GetRef();
		Stamp.Pos = Body.Pos;
		Stamp.Size = Def.DrawSize;
		Stamp.Cell = RowFor(Def) * AtlasFor(Def).Columns + FMath::RoundToInt(UI().AnimCorpse);
		Stamp.bUnits = true;
		Stamp.bBig = &AtlasFor(Def) == &BigAtlas;
		Stamp.bFlip = Body.bFaceLeft;
		Stamp.Color = FLinearColor(UI().CorpseShade, UI().CorpseShade, UI().CorpseShade, 1.f);
		Dying.RemoveAtSwap(Index, EAllowShrinking::No);
	}
}

void AYJJWaveHUD::BakeStamps()
{
	const FYJJBattle* Current = Battle();
	if (Stamps.Num() == 0 || !GroundTarget || !bGroundReady || !Current || !UnitAtlas.Texture || !FxAtlas.Texture)
	{
		return;
	}
	UCanvas* TargetCanvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, GroundTarget, TargetCanvas, Size, Context);
	if (TargetCanvas && TargetCanvas->Canvas)
	{
		const float Scale = Size.X / Current->FieldWidth;
		// 피(Fx)를 먼저, 시체(Units·BigUnits)를 위에
		for (int32 Pass = 0; Pass < BakePasses; ++Pass)
		{
			const bool bUnits = Pass > 0;
			const bool bBig = Pass == BakePasses - 1;
			const FYJJAtlas& Atlas = !bUnits ? FxAtlas : (bBig ? BigAtlas : UnitAtlas);
			if (!Atlas.Texture)
			{
				continue;
			}
			const FYJJQuadBatch Batch = BeginBatch(TargetCanvas->Canvas, Atlas.Texture, Stamps.Num());
			for (const FYJJStamp& Stamp : Stamps)
			{
				if (Stamp.bUnits != bUnits || (bUnits && Stamp.bBig != bBig))
				{
					continue;
				}
				const FVector2f Center = Stamp.Pos * Scale;
				const FVector2f QuadSize(Stamp.Size * Scale);
				AddQuadRotated(Batch, Center, QuadSize, Stamp.Angle, Atlas.UV(Stamp.Cell), Atlas.CellUV, Stamp.Color, Stamp.bFlip);
			}
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	Stamps.Reset();
}

// ---------------------------------------------------------------------------------------------
// 사각형 배치

FYJJQuadBatch AYJJWaveHUD::BeginBatch(FCanvas* Target, UTexture2D* Texture, int32 ExpectedQuads) const
{
	FYJJQuadBatch Batch;
	Batch.Texture = Texture->GetResource();
	Batch.Elements = Target->GetBatchedElements(FCanvas::ET_Triangle, nullptr, Batch.Texture, SE_BLEND_Translucent);
	Batch.Elements->AddReserveVertices(ExpectedQuads * QuadVertices);
	Batch.Elements->AddReserveTriangles(ExpectedQuads * QuadTriangles, Batch.Texture, SE_BLEND_Translucent);
	return Batch;
}

void AYJJWaveHUD::AddQuad(const FYJJQuadBatch& Batch, const FVector2f& TopLeft, const FVector2f& Size, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const
{
	const float U0 = bFlipX ? UV0.X + UVSize.X : UV0.X;
	const float U1 = bFlipX ? UV0.X : UV0.X + UVSize.X;
	const FVector2f BR = TopLeft + Size;
	const FHitProxyId Proxy;
	const int32 V0 = Batch.Elements->AddVertexf(FVector4f(TopLeft.X, TopLeft.Y, 0.f, 1.f), FVector2f(U0, UV0.Y), Color, Proxy);
	const int32 V1 = Batch.Elements->AddVertexf(FVector4f(BR.X, TopLeft.Y, 0.f, 1.f), FVector2f(U1, UV0.Y), Color, Proxy);
	const int32 V2 = Batch.Elements->AddVertexf(FVector4f(BR.X, BR.Y, 0.f, 1.f), FVector2f(U1, UV0.Y + UVSize.Y), Color, Proxy);
	const int32 V3 = Batch.Elements->AddVertexf(FVector4f(TopLeft.X, BR.Y, 0.f, 1.f), FVector2f(U0, UV0.Y + UVSize.Y), Color, Proxy);
	Batch.Elements->AddTriangle(V0, V1, V2, Batch.Texture, SE_BLEND_Translucent);
	Batch.Elements->AddTriangle(V0, V2, V3, Batch.Texture, SE_BLEND_Translucent);
}

void AYJJWaveHUD::AddQuadRotated(const FYJJQuadBatch& Batch, const FVector2f& Center, const FVector2f& Size, float Angle, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const
{
	const float U0 = bFlipX ? UV0.X + UVSize.X : UV0.X;
	const float U1 = bFlipX ? UV0.X : UV0.X + UVSize.X;
	float Sin = 0.f;
	float Cos = 1.f;
	FMath::SinCos(&Sin, &Cos, Angle);
	const FVector2f AxisX = FVector2f(Cos, Sin) * (Size.X * Half);
	const FVector2f AxisY = FVector2f(-Sin, Cos) * (Size.Y * Half);
	const FHitProxyId Proxy;
	auto Corner = [&](float SX, float SY) { const FVector2f P = Center + AxisX * SX + AxisY * SY; return FVector4f(P.X, P.Y, 0.f, 1.f); };
	const int32 V0 = Batch.Elements->AddVertexf(Corner(-1.f, -1.f), FVector2f(U0, UV0.Y), Color, Proxy);
	const int32 V1 = Batch.Elements->AddVertexf(Corner(1.f, -1.f), FVector2f(U1, UV0.Y), Color, Proxy);
	const int32 V2 = Batch.Elements->AddVertexf(Corner(1.f, 1.f), FVector2f(U1, UV0.Y + UVSize.Y), Color, Proxy);
	const int32 V3 = Batch.Elements->AddVertexf(Corner(-1.f, 1.f), FVector2f(U0, UV0.Y + UVSize.Y), Color, Proxy);
	Batch.Elements->AddTriangle(V0, V1, V2, Batch.Texture, SE_BLEND_Translucent);
	Batch.Elements->AddTriangle(V0, V2, V3, Batch.Texture, SE_BLEND_Translucent);
}
