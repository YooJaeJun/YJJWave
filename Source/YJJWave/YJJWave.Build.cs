using UnrealBuildTool;

public class YJJWave : ModuleRules
{
	public YJJWave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// "Sim/YJJHorde.h" 처럼 모듈 루트 기준으로 include
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		// ImageCore: 그림 PNG를 실행 중에 읽어 밉맵을 만든다 (Render/YJJTextureLoader)
		// RenderCore·RHI·YJJWaveShaders: 유닛 사각형을 캔버스 안에서 인스턴싱으로 그린다 (Render/YJJSpriteRenderItem)
		PrivateDependencyModuleNames.AddRange(new string[] { "ImageCore", "RenderCore", "RHI", "Renderer", "Slate", "SlateCore", "YJJWaveShaders" });
	}
}
