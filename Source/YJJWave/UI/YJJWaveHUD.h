#pragma once

#include "CoreMinimal.h"
#include "Data/YJJTuning.h"
#include "GameFramework/HUD.h"
#include "Sim/YJJHorde.h"
#include "Sim/YJJWaveTypes.h"
#include "YJJWaveHUD.generated.h"

class FBatchedElements;
class FCanvas;
class FTexture;
class FYJJBattle;
class UFont;
class UTexture2D;
class UTextureRenderTarget2D;
struct FCompositeFont;

/** 죽어서 날아가는 중인 몸 (땅에 닿으면 바닥에 구워진다) */
struct FYJJDying
{
	FVector2f Pos = FVector2f::ZeroVector;
	FVector2f Vel = FVector2f::ZeroVector;
	float Height = 0.f;
	float Rise = 0.f;
	float Spin = 0.f;
	float Angle = 0.f;
	int32 Type = 0;
	bool bFaceLeft = false;
};

/** 바닥 렌더 타깃에 구울 것 하나 */
struct FYJJStamp
{
	FVector2f Pos = FVector2f::ZeroVector;
	float Size = 0.f;
	float Angle = 0.f;
	int32 Cell = 0;           // 아틀라스 칸 번호
	bool bUnits = false;      // true: Units.png(또는 BigUnits.png), false: Fx.png
	bool bBig = false;        // BigUnits.png
	bool bFlip = false;
	FLinearColor Color = FLinearColor::White;
};

/** 아틀라스 한 장 (정사각 칸) */
struct FYJJAtlas
{
	UTexture2D* Texture = nullptr;
	int32 Columns = 1;
	FVector2f CellUV = FVector2f::UnitVector;

	void Init(UTexture2D* InTexture, float CellPixels);
	FVector2f UV(int32 Cell) const;
	FVector2f UV(int32 Column, int32 Row) const;
};

/** 캔버스 배치 하나 (텍스처 하나 + 반투명) 에 사각형을 넣는 곳 */
struct FYJJQuadBatch
{
	FBatchedElements* Elements = nullptr;
	const FTexture* Texture = nullptr;
};

/** 화면 위쪽 큰 글자 (물량 돌파·학살·보스 처치) */
struct FYJJBanner
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
};

/** 화면 버튼 영역 (마우스 판정) */
struct FYJJButton
{
	FBox2f Rect = FBox2f(ForceInit);
	int32 Kind = 0;
	int32 Index = 0;
};

/**
 * 화면 전부를 그린다: 바닥(피·시체가 쌓이는 렌더 타깃) → 진지·성벽 → 유닛 수만 마리 → 연출 → UI → 특성 성좌.
 * 유닛은 아틀라스 한 장으로 캔버스 배치에 사각형을 직접 넣어 드로우 콜 하나로 그린다.
 * UI 좌표는 1920x1080 가상 화면, 전장은 카메라(가운데·확대)로 월드 → 화면.
 */
UCLASS()
class YJJWAVE_API AYJJWaveHUD : public AHUD
{
	GENERATED_BODY()

public:
	AYJJWaveHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

	// ---- 입력 (플레이어 컨트롤러가 넘긴다) ----
	void HandleMouseDown(bool bRight);
	void HandleMouseUp(bool bRight);
	void HandleWheel(float Delta);
	void HandleKey(const FKey& Key);
	void SetPanInput(const FVector2f& Axis) { PanInput = Axis; }

	/** 판을 새로 시작할 때 (바닥·시체 초기화) */
	void ResetBattleView();

	// 카메라
	FVector2f WorldToScreen(const FVector2f& World) const;
	/** 전장이 그려지는 화면 영역 (좌측 패널 오른쪽) */
	FVector2f WorldViewCenter() const;
	FVector2f WorldViewSize() const;
	float WorldCenterVirtualX() const;
	FVector2f ScreenToWorld(const FVector2f& Screen) const;
	void FocusWorld(const FVector2f& World, float NewZoom);
	FVector2f CameraCenter() const { return CamCenter; }

	bool IsTreeOpen() const { return bTreeOpen; }
	/** 지난 프레임 DrawUnits 의 CPU 시간 (화면 안 유닛 고르기·정렬·사각형 넣기) */
	float LastUnitDrawMs() const { return UnitDrawMs; }
	int32 LastVisibleUnits() const { return Sorted.Num(); }
	void SetTreeOpen(bool bOpen) { bTreeOpen = bOpen; }
	/** 오토파일럿 스크린샷용: 마우스 대신 이 별의 설명창을 띄운다 (INDEX_NONE이면 마우스) */
	void SetForcedHoverPerk(int32 Perk) { ForcedHoverPerk = Perk; }

private:
	enum class EButton : int32
	{
		BuildCard,
		Spell,
		Tree,
		TreeClose,
		Speed,
		Restart,
		NextStage,
		Card,
	};

	/** 기다리지 않는 판 (Horde 밖 필드만). 유닛은 View() */
	FYJJBattle* Battle() const;
	/** 판을 바꾸는 입력용: 작업 스레드 시뮬이 끝날 때까지 기다린다 */
	FYJJBattle* EditBattle() const;
	/** 이번 프레임 유닛 사본 */
	const FYJJHordeView& View() const;
	const FYJJUILayout& UI() const;

	// 준비
	void LoadArt();
	void InitGround();
	TSharedPtr<const FCompositeFont> GetGameFont();

	// 시체·바닥
	void TakeDeaths();
	void UpdateDying(float Dt);
	void BakeStamps();
	void AddBlood(const FVector2f& Pos, float Size);

	// 전장
	void UpdateCamera(float Dt);
	void ClampCamera();
	void DrawWorld();
	void DrawGround();
	void DrawCamp();
	void DrawUnits();
	/** 유닛 그림 칸: 공격 > 맞음 > 걷기 > 숨쉬기 (ParallelFor 안에서 부른다: 읽기만) */
	int32 PickFrame(const FYJJHordeView& Horde, int32 Slot) const;
	void DrawDying();
	void DrawProjectiles();
	void DrawEffects();
	void DrawMeteorAim();

	// 사각형 배치 (캔버스 배치에 직접)
	FYJJQuadBatch BeginBatch(FCanvas* Target, UTexture2D* Texture, int32 ExpectedQuads) const;
	void AddQuad(const FYJJQuadBatch& Batch, const FVector2f& TopLeft, const FVector2f& Size, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const;
	void AddQuadRotated(const FYJJQuadBatch& Batch, const FVector2f& Center, const FVector2f& Size, float Angle, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const;

	// UI (1920x1080 가상 좌표)
	/** 좌측 패널 하나에 모든 UI (물결·VS·보스·자원·병영·스펠·특성·성능) */
	void DrawSidePanel();
	void DrawPlotTooltip();
	void DrawTree();
	void DrawGameOver();
	void DrawDebugLine();
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color, int32 Cell);
	void DrawBox(float X, float Y, float W, float H, const FLinearColor& Color);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
	/** 게이지 안 숫자: 게이지 높이에 맞춘 크기, 세로 가운데 */
	void DrawBarLabel(const FString& Text, float X, float Y, float W, float H);
	void DrawIcon(int32 Cell, float X, float Y, float Size, const FLinearColor& Color = FLinearColor::White);
	void DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float AlignX = 0.f, float MaxWidth = 0.f);
	void DrawLine(const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness);
	void AddButton(EButton Kind, int32 Index, float X, float Y, float W, float H);
	const FYJJButton* ButtonAt(const FVector2f& Virtual) const;
	FVector2f MouseScreen() const;
	FVector2f ToVirtual(const FVector2f& Screen) const;
	bool IsMouseOverUI() const;

	// 특성 성좌
	FVector2f PerkToVirtual(const FVector2f& PerkPos) const;
	int32 PerkAt(const FVector2f& Virtual) const;
	FString PerkText(int32 Perk) const;
	/** 특성·카드 공통: 능력치 줄 + 특수 효과 줄 */
	FString EffectText(const FYJJPerkDef& Def) const;
	void DrawCardPick();
	void DrawBanners();
	void UpdateBanners(float Dt);
	void AddBanner(const FString& Text, const FLinearColor& Color);
	void Shake();
	float PerkRadius(EYJJPerkKind Kind) const;
	FVector2f WorldToVirtual(const FVector2f& World) const;

	// 조작
	void ClickWorld();
	int32 PlotAt(const FVector2f& World) const;

	static FString FormatCount(double Value);
	/** 1~6 키 → 병영 번호 */
	static int32 BuildingIndexForKey(const FKey& Key);

	UPROPERTY() TObjectPtr<UTexture2D> UnitTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BigUnitTexture;
	UPROPERTY() TObjectPtr<UTexture2D> FxTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BuildingTexture;
	UPROPERTY() TObjectPtr<UTexture2D> UiTexture;
	UPROPERTY() TObjectPtr<UTexture2D> GroundTexture;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> GroundTarget;
	UPROPERTY() TObjectPtr<UFont> BaseFont;

	FYJJAtlas UnitAtlas;
	FYJJAtlas BigAtlas;
	/** 유닛 그림이 있는 아틀라스와 줄 (큰 유닛은 BigUnits.png) */
	const FYJJAtlas& AtlasFor(const FYJJUnitDef& Def) const { return Def.BigRow != INDEX_NONE && BigAtlas.Texture ? BigAtlas : UnitAtlas; }
	int32 RowFor(const FYJJUnitDef& Def) const { return Def.BigRow != INDEX_NONE && BigAtlas.Texture ? Def.BigRow : Def.AtlasRow; }
	/** 큰 유닛만 따로 그리는 목록 */
	TArray<int32> BigSorted;
	FYJJAtlas FxAtlas;
	FYJJAtlas BuildingAtlas;
	FYJJAtlas UiAtlas;
	TSharedPtr<const FCompositeFont> GameFont;

	TArray<FYJJDying> Dying;
	TArray<FYJJStamp> Stamps;
	bool bGroundReady = false;
	FRandomStream Random;

	// 카메라
	FVector2f CamCenter = FVector2f::ZeroVector;
	float Zoom = 1.f;
	FVector2f PanInput = FVector2f::ZeroVector;
	bool bDragging = false;
	FVector2f DragLast = FVector2f::ZeroVector;
	FVector2f DragStart = FVector2f::ZeroVector;

	// 화면
	FVector2f ScreenSize = FVector2f(1.f, 1.f);
	float ViewScale = 1.f;
	/** 가상 화면 너비 (높이 1080 기준, 화면 비율대로) */
	float VirtualW = 1920.f;
	FVector2f ViewOffset = FVector2f::ZeroVector;
	TArray<FYJJButton> Buttons;

	// 상태
	int32 SelectedBuilding = 0;
	bool bTreeOpen = false;
	bool bAimingMeteor = false;
	bool bShowDebug = true;
	int32 HoverPlot = INDEX_NONE;
	int32 HoverPerk = INDEX_NONE;
	int32 ForcedHoverPerk = INDEX_NONE;
	float FrameMs = 0.f;
	float UnitDrawMs = 0.f;
	/** 숨쉬기 그림 시계 (실제 시간) */
	float AnimClock = 0.f;

	// 물량 연출
	TArray<FYJJBanner> Banners;
	float NextMilestone = 0.f;
	float KillWindow = 0.f;
	int32 KillsAtWindow = 0;
	bool bBossSeen = false;
	float ShakeLeft = 0.f;
	FVector2f ShakeOffset = FVector2f::ZeroVector;

	// 그리기용 임시 배열 (매 프레임 다시 쓴다)
	TArray<int32> Visible;
	TArray<int32> Sorted;
	TArray<int32> BucketStart;


	static constexpr float VirtualHeight = 1080.f;
	static constexpr float Half = 0.5f;
	static constexpr float FullTurn = 2.f * UE_PI;
	static constexpr float Percent = 100.f;
	static constexpr double MsPerSecond = 1000.0;
	static constexpr int32 NineSlice = 3;
	static constexpr int32 QuadVertices = 4;
	static constexpr int32 QuadTriangles = 2;
	static constexpr int32 SpriteChunk = 1024;
	static constexpr int32 BakePasses = 3;   // 피 → 시체 → 큰 시체
	static constexpr const TCHAR* BaseFontPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");
	static constexpr const TCHAR* GameFontFile = TEXT("YJJWave/Fonts/Jua-Regular.ttf");
	static constexpr const TCHAR* GameFontFace = TEXT("Regular");
};
