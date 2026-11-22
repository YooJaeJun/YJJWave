#pragma once

#include "CoreMinimal.h"
#include "CanvasTypes.h"
#include "RHIFeatureLevel.h"

class FRHICommandList;
class FTexture;

/** 인스턴스 하나. 셰이더(Shaders/YJJSprite.usf)와 같은 배치: float4 3개 */
struct FYJJSpriteInstance
{
	FVector4f Rect;     // 가운데 x, y, 반너비, 반높이 (화면 픽셀)
	FVector4f UV;       // u0, v0, du, dv (du < 0 이면 좌우 뒤집힘)
	FLinearColor Color;
};

/** 렌더 스레드로 넘기는 한 덩어리 */
struct FYJJSpriteBatchData
{
	TArray<FYJJSpriteInstance> Instances;
	const FTexture* Texture = nullptr;
	FIntPoint ViewSize = FIntPoint::ZeroValue;
	float Gamma = 1.f;
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::SM5;
};

/**
 * 캔버스 안에서 사각형 수만 개를 인스턴싱 한 번으로 그리는 렌더 아이템.
 * CPU는 유닛당 48바이트만 채우고(ParallelFor로), 모서리 4개는 정점 셰이더가 만든다.
 * 캔버스 정렬 목록에 끼워 넣으므로 앞뒤 순서(바닥 → 유닛 → UI)가 그대로 지켜진다.
 */
class FYJJSpriteRenderItem : public FCanvasBaseRenderItem
{
public:
	explicit FYJJSpriteRenderItem(const TSharedRef<FYJJSpriteBatchData>& InData) : Data(InData) {}

	virtual bool Render_RenderThread(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FCanvas* Canvas) override;
	virtual bool Render_GameThread(const FCanvas* Canvas, FCanvasRenderThreadScope& RenderScope) override;

	/** 캔버스 지금 순서 자리에 덩어리 하나를 붙이고, Count 칸짜리 채울 배열을 돌려준다 */
	static TArray<FYJJSpriteInstance>& Add(FCanvas* Canvas, const FTexture* Texture, int32 Count);

private:
	static void Draw(FRHICommandList& RHICmdList, const FYJJSpriteBatchData& Batch);

	TSharedRef<FYJJSpriteBatchData> Data;

	static constexpr int32 TrianglesPerSprite = 2;
	static constexpr const TCHAR* PassName = TEXT("YJJSprites");
};
