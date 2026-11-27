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
	bool bUnits = false;      // true: 유닛 그림, false: Fx.png
	int32 BigRow = INDEX_NONE; // 큰 유닛 그림 (BigAtlases 번호), 아니면 Units.png
	bool bFlip = false;
	FLinearColor Color = FLinearColor::White;
};

/** 아틀라스 한 장 (정사각 칸) */
struct FYJJAtlas
{
	UTexture2D* Texture = nullptr;
	/** 논리 칸 수 (한 줄 = 한 유닛의 동작 칸들) */
	int32 Columns = 1;
	/** 그림 파일의 실제 칸 수 */
	int32 SheetColumns = 1;
	/** 논리 칸 하나를 몇 칸의 중간 프레임으로 쪼갰나 (큰 유닛 = 4, 나머지 1) */
	int32 FrameMul = 1;
	FVector2f CellUV = FVector2f::UnitVector;

	/** LogicalColumns가 0이면 그림 파일 칸 수 그대로 */
	void Init(UTexture2D* InTexture, float CellPixels, int32 InFrameMul = 1, int32 LogicalColumns = 0);
	FVector2f UV(int32 Cell) const;
	/** Sub = 중간 프레임 (0 ~ FrameMul-1) */
	FVector2f UV(int32 Column, int32 Row, int32 Sub = 0) const;
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

/** 지금 주고받는 말풍선 대화 (Dialogues.csv) */
struct FYJJTalkPlay
{
	int32 Talk = INDEX_NONE;
	int32 Line = 0;
	/** 이 줄이 뜬 지 (음수면 앞 줄 끝나고 기다리는 중) */
	float Age = 0.f;
	bool bVoiced = false;
};

/** 솥 숫자 옆에 튀어 오르는 +뼈 (모아서 한 번에, 지난 시간) */
struct FYJJPotPop
{
	int32 Value = 0;
	float Age = 0.f;
};

/** 처치 금화 튀기 (월드 자리, 금액, 지난 시간) */
struct FYJJCoinPop
{
	FVector2f Pos = FVector2f::ZeroVector;
	int32 Value = 0;
	float Age = 0.f;
	float Side = 0.f;
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
	/** 터치: 손가락 자리를 마우스 자리로 쓴다 (모바일엔 마우스가 없다) */
	void SetTouchPointer(const FVector2f& Screen) { TouchPos = Screen; bTouchPointer = true; }
	/** 두 번째 손가락이 닿으면: 누르던 클릭·끌기를 취소한다 */
	void CancelPointer() { bLeftDown = bLeftDragged = false; }
	/** 두 손가락 확대·이동: 시작 → (손가락 사이 비율, 가운데 자리) */
	void PinchBegin(const FVector2f& Mid);
	void PinchUpdate(float Ratio, const FVector2f& Mid);
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
	bool IsOptionsOpen() const { return bOptionsOpen; }
	void SetOptionsOpen(bool bOpen) { bOptionsOpen = bOpen; }
	void SetBuyPopup(int32 Building) { BuyPopupFor = Building; SelectedBuilding = Building == INDEX_NONE ? SelectedBuilding : Building; }
	/** 지난 프레임 DrawUnits 의 CPU 시간 (화면 안 유닛 고르기·정렬·사각형 넣기) */
	float LastUnitDrawMs() const { return UnitDrawMs; }
	int32 LastVisibleUnits() const { return Sorted.Num(); }
	void SetTreeOpen(bool bOpen) { bTreeOpen = bOpen; }
	/** 오토파일럿 스크린샷용: 마우스 대신 이 별의 설명창을 띄운다 (INDEX_NONE이면 마우스) */
	void SetForcedHoverPerk(int32 Perk) { ForcedHoverPerk = Perk; }
	/** 지금 말풍선이 뜬 지 (없으면 음수, 오토파일럿 스크린샷용) */
	float TalkAge() const { return TalkNow.Talk != INDEX_NONE ? TalkNow.Age : -1.f; }

private:
	enum class EButton : int32
	{
		BuildCard,
		Charge,
		HeroSkill,
		Spell,
		Tree,
		TreeClose,
		Speed,
		Restart,
		NextStage,
		Card,
		SkillPick,
		Buy,
		Option,
		OpenOptions,
		Pot,
		TitleStart,
		TitleQuit,
		WorldStage,
		WorldBack,
		ResultMap,
		Seasoning,
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
	/** Sorted[Begin, End) 작은 유닛을 인스턴싱 한 번으로 */
	void DrawSmallUnits(int32 Begin, int32 End);
	/** 큰 유닛 하나 (고해상도 아틀라스) */
	void DrawBigUnit(int32 Slot);
	/** 유닛 그림 칸: 공격 > 맞음 > 걷기 > 숨쉬기 (ParallelFor 안에서 부른다: 읽기만) */
	/** OutSub: 그 칸 안에서 얼마나 진행했나 (0~1, 큰 유닛 중간 프레임용) */
	int32 PickFrame(const FYJJHordeView& Horde, int32 Slot, float* OutSub = nullptr) const;
	void DrawDying();
	void DrawProjectiles();
	void DrawEffects();
	/** 조준 중인 스킬: 범위 고리 */
	void DrawSpellAim();
	// 왕국의 마지막 냄비 (솥 → 국물 면·거품·김 → 뚜껑 → 글자)
	/** 좌측 패널 솥 칸: 솥 그림 · 단계 · 뼈 숫자(+N) · 단계 눈금 게이지. 누르면(Q) 뚜껑을 연다. 높이를 돌려준다 */
	float DrawPotPanel(float X, float Y, float W);
	void DrawPotTooltip();
	/** 주문 칸(F1·F2·F3)에 마우스: 무엇을 하는지 + Lv 수치 */
	void DrawSpellTooltip();
	/** 냄비 뚜껑 열기 (클릭·Q) */
	void OpenPot();
	// 말풍선 대화 (Dialogues.csv): 판 사건을 듣고 조건이 맞는 대화를 한 판에 한 번
	void TakeEvents();
	bool TalkMatches(const FYJJTalk& Talk, const FYJJBattleEvent& Event) const;
	void UpdateTalks(float Dt);
	void DrawTalks();
	/** 그 유닛이 전장에 있으면 화면 가운데에 가장 가까운 슬롯 (없으면 INDEX_NONE) */
	int32 FindSpeaker(int32 Unit) const;

	// 사각형 배치 (캔버스 배치에 직접)
	FYJJQuadBatch BeginBatch(FCanvas* Target, UTexture2D* Texture, int32 ExpectedQuads) const;
	void AddQuad(const FYJJQuadBatch& Batch, const FVector2f& TopLeft, const FVector2f& Size, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const;
	void AddQuadRotated(const FYJJQuadBatch& Batch, const FVector2f& Center, const FVector2f& Size, float Angle, const FVector2f& UV0, const FVector2f& UVSize, const FLinearColor& Color, bool bFlipX) const;

	// UI (1920x1080 가상 좌표)
	/** 좌측 패널 하나에 모든 UI (물결·VS·보스·자원·병영·스펠·특성·성능) */
	void DrawSidePanel();
	/** 병영 카드 아래 즉시 구매 팝업 */
	void DrawBuyPopup(float X, float W);
	/** 화면 오른쪽 위 배속 버튼 */
	void DrawSpeedButton();
	/** 영웅·보스 머리 위 작은 체력 게이지 */
	void DrawUnitBars();
	/** 연속 처치 숫자 (화면 오른쪽, 처치할수록 커지고 붉어진다) */
	void DrawCombo();
	float BuyCountOf(int32 Index) const;
	void DrawPlotTooltip();
	void DrawTree();
	void DrawGameOver();
	/** 결과 별: 빈 별이 먼저, 얻은 만큼 하나씩 채워진다 (Start = 빈 별이 나타나는 때) */
	void DrawResultStars(float CenterX, float Y, float Start, int32 Stars);
	/** 결과 창 Start 초부터 0→1 (한 칸이 나타나는 진행) */
	float ResultStep(float Start) const;
	/** 성벽과 성문 (진지 오른쪽 끝, 세로로 길게) + 성문 위 체력 */
	void DrawWall();
	void DrawBlockers();
	/** 땅 위 전선 띠: 아군 앞줄·적 앞줄 자리, 아군 띠 위에 "전선 +N%" (밀어낸 만큼 금화·영혼) */
	void DrawFrontLines();
	/** 전장 아래 가운데: 영웅 돌격 버튼 (E, 쿨다운 막대) */
	void DrawChargeButton();
	void TryHeroCharge();
	/** 영웅 큰 스킬 버튼 (돌격 버튼 오른쪽) */
	void DrawHeroSkillButton();
	void TryHeroSkill();
	/** 싹쓸이 글자 (스킬 한 번에 많이 쓰러뜨렸을 때) */
	void DrawSkillTexts();
	/** 처치한 적 자리에서 금화가 튀어 오른다 */
	void DrawCoinPops();
	void UpdateCoinPops(float Dt);
	/** 솥 뼈가 늘면 +N을 모아 튀기고 숫자를 툭 키운다 */
	void UpdatePotPops(float Dt);
	/** 화면 위 가운데: 다음 웨이브에 무엇이 몇 마리 오는지 */
	void DrawWavePreview();
	// 타이틀 · 월드맵 (YJJWaveHUD_Menu.cpp)
	void DrawTitle();
	void DrawWorldMap();
	/** 메뉴 버튼 하나 (가운데 글자) */
	void DrawMenuButton(EButton Kind, int32 Index, float X, float Y, float W, float H, const FString& Text, bool bHighlight);
	/** 텍스처 한 장을 화면 영역에 (UV 전체) */
	void DrawTexture(UTexture2D* Texture, float X, float Y, float W, float H, const FLinearColor& Color);
	/** 유닛 숨쉬기 그림 한 장 (타이틀 장식) */
	void DrawUnitSprite(int32 Unit, float CenterX, float FootY, float Size, bool bFaceLeft);
	bool HoverButton(EButton Kind) const;
	/** 좌측 패널 냄비 칸 (높이를 돌려준다) */
	FLinearColor PotStatusColor() const;
	/** 지금 스테이지 바닥 그림 (Stages.csv Ground, 없으면 Ground.png) */
	UTexture2D* GroundFor(int32 Stage);
	/** 전장 바깥 둘레 배경 (그 바닥 이름_Outer.png) */
	UTexture2D* GroundOuterFor(int32 Stage);
	UTexture2D* LoadGroundFile(const FString& File);
	void DrawDebugLine();
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color, int32 Cell);
	void DrawBox(float X, float Y, float W, float H, const FLinearColor& Color);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color);
	/** 게이지 안 숫자: 게이지 높이에 맞춘 크기, 세로 가운데 */
	void DrawBarLabel(const FString& Text, float X, float Y, float W, float H);
	void DrawIcon(int32 Cell, float X, float Y, float Size, const FLinearColor& Color = FLinearColor::White);
	void DrawLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float AlignX = 0.f, float MaxWidth = 0.f);
	/** 글자 몸통의 가운데를 (CenterX, CenterY)에 (버튼·게이지 안 글자) */
	void DrawLabelCentered(const FString& Text, float CenterX, float CenterY, float Size, const FLinearColor& Color, float MaxWidth = 0.f);
	/** 세로만 가운데: 아이콘 옆 숫자 */
	void DrawLabelLeftMiddle(const FString& Text, float X, float CenterY, float Size, const FLinearColor& Color, float AlignX);
	FSlateFontInfo MakeFont(const FString& Text, float Size, float Alpha, float MaxWidth, float& OutWidth);
	/** 설명 글: 숫자(25, +8%, x1.6 …)만 YJJUI::Value 색으로 (색은 모든 설명에서 같다) */
	void DrawValueLabel(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float MaxWidth = 0.f);
	/** 숫자 조각인가: 숫자로 시작하거나 +·-·x 바로 뒤에 숫자 */
	static bool StartsNumber(const FString& Text, int32 Index);
	float MeasureWith(const FString& Text, const FSlateFontInfo& Font) const;
	void DrawText(const FString& Text, const FSlateFontInfo& Font, float Left, float Top, const FLinearColor& Color);
	/** 칸 너비에 맞춰 띄어쓰기 단위로 줄을 나눈다 (글자를 줄이지 않고) */
	TArray<FString> WrapText(const FString& Text, float Size, float MaxWidth);
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
	/** 주문 별 설명: 지금 Lv 수치, 다음 Lv, Lv별 특수 효과 */
	FString SpellText(EYJJSpell Spell) const;
	FString SpellStatText(EYJJSpell Spell, const FYJJSpellStats& Stats) const;
	int32 SpellIcon(EYJJSpell Spell) const;
	/** 주문 키·버튼: 조준이 필요한 주문은 커서가 전장 위면 바로, 아니면 조준 상태로 */
	void PressSpell(EYJJSpell Spell);
	static bool NeedsAim(EYJJSpell Spell) { return Spell != EYJJSpell::Rally; }
	void DrawCardPick();
	/** 판 시작: 영웅 스킬 후보 카드 (1·2·3) */
	void DrawSkillPick();
	void PickHeroSkill(int32 Choice);
	/** 양념 하나를 냄비에 (카드 고르기) + 배너 */
	void PickSeasoning(int32 Choice);
	/** 좌측 패널: 이번 판 넣은 양념 그림들 (높이를 돌려준다) */
	float DrawSeasonings(float X, float Y, float W);
	void DrawSeasoningTooltip();
	void DrawCutscene();
	/** 장 제목 카드: 전장을 어둡게 하고 가운데 제목 + 아래 한 줄이 서서히 나타난다 */
	void DrawChapterCard(const FYJJCutLine& Line, float Time);
	void UpdateCutsceneCamera(float Dt);
	void DrawOptions();
	void HandleOption(int32 Index);
	static float* OptionVolume(int32 Row);
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
	/** F1·F2·F3 → 주문 번호 */
	static int32 SpellIndexForKey(const FKey& Key);

	UPROPERTY() TObjectPtr<UTexture2D> UnitTexture;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> BigUnitTextures;
	UPROPERTY() TObjectPtr<UTexture2D> FxTexture;
	UPROPERTY() TObjectPtr<UTexture2D> BuildingTexture;
	UPROPERTY() TObjectPtr<UTexture2D> UiTexture;
	/** 길목 지형 그림 (Props.png) */
	UPROPERTY() TObjectPtr<UTexture2D> PropTexture;
	UPROPERTY() TObjectPtr<UTexture2D> GroundTexture;
	/** 스테이지별 바닥 (파일 이름 → 텍스처) */
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> GroundTextures;
	UPROPERTY() TObjectPtr<UTexture2D> WorldMapTexture;
	/** 바닥을 구운 스테이지 (바뀌면 다시 굽는다) */
	int32 GroundStage = INDEX_NONE;
	/** 결과 창이 뜬 지 (실제 시간, 별 연출) */
	float ResultAge = 0.f;
	int32 StarsSounded = 0;
	/** 글꼴을 만든 언어 (바뀌면 다시 만든다) */
	FString GameFontLanguage;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> GroundTarget;
	UPROPERTY() TObjectPtr<UFont> BaseFont;

	FYJJAtlas UnitAtlas;
	/** 큰 유닛마다 한 장 (Units.csv BigRow 번호, Big_이름.png) */
	TArray<FYJJAtlas> BigAtlases;
	/** 유닛 그림이 있는 아틀라스와 줄 (큰 유닛은 BigUnits.png) */
	const FYJJAtlas* BigAtlasFor(const FYJJUnitDef& Def) const { return BigAtlases.IsValidIndex(Def.BigRow) && BigAtlases[Def.BigRow].Texture && BigAtlases[Def.BigRow].Texture->GetResource() ? &BigAtlases[Def.BigRow] : nullptr; }
	const FYJJAtlas& AtlasFor(const FYJJUnitDef& Def) const { const FYJJAtlas* Big = BigAtlasFor(Def); return Big ? *Big : UnitAtlas; }
	/** 큰 유닛 그림은 한 장에 한 유닛이라 줄 0 */
	int32 RowFor(const FYJJUnitDef& Def) const { return BigAtlasFor(Def) ? 0 : Def.AtlasRow; }
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
	/** 왼쪽 버튼이 전장에서 눌려 있다 / 그사이 끌어서 화면을 옮겼다 */
	bool bLeftDown = false;
	bool bLeftDragged = false;
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
	/** 즉시 구매 팝업이 열린 병영 (INDEX_NONE이면 닫힘) */
	int32 BuyPopupFor = INDEX_NONE;
	float BuyPopupY = 0.f;
	bool bTreeOpen = false;
	bool bOptionsOpen = false;
	/** 조준 중인 주문 (EYJJSpell 번호, 없으면 INDEX_NONE) */
	int32 AimingSpell = INDEX_NONE;
	bool bShowDebug = false;   // F4: 성능 표시
	// 말풍선 대화
	TArray<int32> TalkQueue;
	TArray<uint8> TalkUsed;
	FYJJTalkPlay TalkNow;
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
	TArray<FYJJCoinPop> CoinPops;
	float CoinBudget = 0.f;
	int32 ComboCount = 0;
	float ComboTimer = 0.f;
	float ComboPop = 0.f;
	// 솥 숫자
	TArray<FYJJPotPop> PotPops;
	float LastPotBones = 0.f;
	float PotPending = 0.f;
	float PotPopTimer = 0.f;
	float PotNumberPop = 0.f;
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
	static constexpr int32 BakeFirstBigPass = 2;   // 피 → 시체 → 큰 유닛마다 큰 시체
	inline static const FName SoundClick = TEXT("Click");
	inline static const FName SoundStar = TEXT("Star");

	// 옵션 버튼 번호
	static constexpr int32 NumVolumeRows = 4;   // 배경음·효과음·보이스·카메라 진동 (막대 줄)
	static constexpr int32 OptionsExtraRows = 2;   // 언어, 해상도
	static constexpr int32 OptionVolumeDown = 0;
	static constexpr int32 OptionVolumeUp = 10;
	static constexpr int32 OptionLanguage = 20;
	static constexpr int32 OptionResolutionPrev = 30;
	static constexpr int32 OptionResolutionNext = 31;
	static constexpr int32 OptionClose = 40;
	static constexpr int32 OptionGiveUp = 41;
	static constexpr int32 NumLanguages = 2;
	/** 연출 하나가 그리는 사각형 수 어림 (대지 가르기 줄·불꽃이 여러 개) */
	static constexpr int32 EffectQuadsGuess = 12;
	/** 길목 하나를 채우는 바위 줄 수 어림 (배치 예약용) */
	static constexpr int32 BlockerRowsGuess = 16;
	/** 불꽃 각도를 자리로 정하는 씨앗 */
	static constexpr float SparkAngleSeed = 0.0137f;
	// 냄비 거품·김·뚜껑 모양 (자리를 번호로 흩는 씨앗과 퍼지는 비율)
	static constexpr float PotBubbleSeedX = 2.1f;
	static constexpr float PotBubbleSeedY = 1.7f;
	static constexpr float PotBubbleSpreadX = 0.35f;
	static constexpr float PotBubbleSpreadY = 0.25f;
	static constexpr float PotBubbleTempo = 0.25f;
	static constexpr float PotSteamSpread = 0.25f;
	static constexpr float PotLidAjarTilt = 0.6f;
	static constexpr float PotLidAjarX = -0.32f;
	static constexpr float PotLidAjarY = 0.14f;
	/** 잠긴 지역 구름을 몇 겹 (겹칠수록 짙다) */
	static constexpr int32 RegionCloudLayers = 3;
	static constexpr int32 BuyCountKeys[] = { 0, 1, 2 };   // 팝업 버튼 3개 (마리 수는 UILayout BuyCountA·B·C)
	inline static const TCHAR* const LanguageCodes[NumLanguages] = { TEXT("ko"), TEXT("en") };
	inline static const TCHAR* const VolumeKeys[NumVolumeRows] = { TEXT("Option.Music"), TEXT("Option.Sfx"), TEXT("Option.Voice"), TEXT("Option.Shake") };
	inline static const FName SoundBanner = TEXT("Banner");
	static constexpr const TCHAR* BaseFontPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");
	static constexpr const TCHAR* FontFolder = TEXT("YJJWave/Fonts");
	static constexpr const TCHAR* KoreanFontFile = TEXT("DoHyeon-Regular.ttf");
	static constexpr const TCHAR* FontFileKey = TEXT("Font.File");
	static constexpr int32 HangulFirst = 0x1100;
	static constexpr int32 HangulLast = 0xD7A3;
	static constexpr const TCHAR* DefaultGroundFile = TEXT("Ground.png");
	static constexpr const TCHAR* OuterGroundSuffix = TEXT("_Outer");
	// 터치
	FVector2f TouchPos = FVector2f::ZeroVector;
	bool bTouchPointer = false;
	float PinchStartZoom = 1.f;
	FVector2f PinchLastMid = FVector2f::ZeroVector;
	static constexpr const TCHAR* WorldMapFile = TEXT("WorldMap.png");
	static constexpr const TCHAR* GameFontFace = TEXT("Regular");
};
