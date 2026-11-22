#include "YJJSpriteShaders.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

void FYJJWaveShadersModule::StartupModule()
{
	AddShaderSourceDirectoryMapping(VirtualPath, FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), ShaderFolder));
}

IMPLEMENT_MODULE(FYJJWaveShadersModule, YJJWaveShaders);

IMPLEMENT_GLOBAL_SHADER(FYJJSpriteVS, "/YJJWave/YJJSprite.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FYJJSpritePS, "/YJJWave/YJJSprite.usf", "MainPS", SF_Pixel);
