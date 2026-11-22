#include "Render/YJJSpriteRenderItem.h"
#include "CanvasRender.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "RenderResource.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "YJJSpriteShaders.h"

TArray<FYJJSpriteInstance>& FYJJSpriteRenderItem::Add(FCanvas* Canvas, const FTexture* Texture, int32 Count)
{
	TSharedRef<FYJJSpriteBatchData> Batch = MakeShared<FYJJSpriteBatchData>();
	Batch->Texture = Texture;
	Batch->Instances.SetNumUninitialized(Count);
	const FRenderTarget* Target = Canvas->GetRenderTarget();
	Batch->ViewSize = Target ? Target->GetSizeXY() : FIntPoint(1, 1);
	Batch->Gamma = Target ? 1.f / Target->GetDisplayGamma() : 1.f;
	Batch->FeatureLevel = Canvas->GetFeatureLevel();
	FCanvas::FCanvasSortElement& SortElement = Canvas->GetSortElement(Canvas->TopDepthSortKey());
	SortElement.RenderBatchArray.Add(new FYJJSpriteRenderItem(Batch));
	return Batch->Instances;
}

bool FYJJSpriteRenderItem::Render_RenderThread(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FCanvas* Canvas)
{
	if (Data->Instances.Num() == 0 || !Data->Texture)
	{
		return false;
	}
	RenderContext.AddPass(RDG_EVENT_NAME("YJJSprites"), [Batch = Data](FRHICommandList& RHICmdList)
	{
		Draw(RHICmdList, *Batch);
	});
	return true;
}

bool FYJJSpriteRenderItem::Render_GameThread(const FCanvas* Canvas, FCanvasRenderThreadScope& RenderScope)
{
	if (Data->Instances.Num() == 0 || !Data->Texture)
	{
		return false;
	}
	RenderScope.AddPass(PassName, [Batch = Data](FRHICommandList& RHICmdList)
	{
		Draw(RHICmdList, *Batch);
	});
	return true;
}

void FYJJSpriteRenderItem::Draw(FRHICommandList& RHICmdList, const FYJJSpriteBatchData& Batch)
{
	if (!Batch.Texture->TextureRHI || Batch.ViewSize.X <= 0 || Batch.ViewSize.Y <= 0)
	{
		return;
	}

	// 인스턴스 버퍼: 매 프레임 새로 만들어 올린다 (4만 마리 = 1.9MB)
	const uint32 Bytes = Batch.Instances.Num() * sizeof(FYJJSpriteInstance);
	const FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::CreateStructured(PassName, Bytes, sizeof(FVector4f))
		.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static)
		.DetermineInitialState();
	FBufferRHIRef Buffer = RHICmdList.CreateBuffer(Desc);
	void* Dest = RHICmdList.LockBuffer(Buffer, 0, Bytes, RLM_WriteOnly);
	FMemory::Memcpy(Dest, Batch.Instances.GetData(), Bytes);
	RHICmdList.UnlockBuffer(Buffer);
	FShaderResourceViewRHIRef View = RHICmdList.CreateShaderResourceView(Buffer, FRHIViewDesc::CreateBufferSRV().SetTypeFromBuffer(Buffer));

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(Batch.FeatureLevel);
	TShaderMapRef<FYJJSpriteVS> VertexShader(ShaderMap);
	TShaderMapRef<FYJJSpritePS> PixelShader(ShaderMap);

	FGraphicsPipelineStateInitializer Pipeline;
	RHICmdList.ApplyCachedRenderTargets(Pipeline);
	Pipeline.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
	Pipeline.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	Pipeline.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	Pipeline.PrimitiveType = PT_TriangleList;
	Pipeline.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
	Pipeline.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	Pipeline.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	SetGraphicsPipelineState(RHICmdList, Pipeline, 0);

	FYJJSpriteVS::FParameters VertexParameters;
	VertexParameters.Instances = View;
	VertexParameters.InvViewSize = FVector2f(1.f / Batch.ViewSize.X, 1.f / Batch.ViewSize.Y);
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);

	FYJJSpritePS::FParameters PixelParameters;
	PixelParameters.SpriteTexture = Batch.Texture->TextureRHI;
	PixelParameters.SpriteSampler = Batch.Texture->SamplerStateRHI;
	PixelParameters.Gamma = Batch.Gamma;
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PixelParameters);

	RHICmdList.DrawPrimitive(0, TrianglesPerSprite, Batch.Instances.Num());
}
