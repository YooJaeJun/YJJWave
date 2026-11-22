#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "Modules/ModuleInterface.h"
#include "ShaderParameterStruct.h"

/** 프로젝트 Shaders 폴더를 /YJJWave 가상 경로로 연결한다 */
class FYJJWaveShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

private:
	static constexpr const TCHAR* VirtualPath = TEXT("/YJJWave");
	static constexpr const TCHAR* ShaderFolder = TEXT("Shaders");
};

/** 인스턴스 버퍼에서 사각형을 만드는 정점 셰이더 (Shaders/YJJSprite.usf) */
class YJJWAVESHADERS_API FYJJSpriteVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FYJJSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FYJJSpriteVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, Instances)
		SHADER_PARAMETER(FVector2f, InvViewSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }
};

/** 아틀라스 한 칸을 칠하는 픽셀 셰이더 */
class YJJWAVESHADERS_API FYJJSpritePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FYJJSpritePS);
	SHADER_USE_PARAMETER_STRUCT(FYJJSpritePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SpriteTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
		SHADER_PARAMETER(float, Gamma)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }
};
