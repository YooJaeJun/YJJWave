using UnrealBuildTool;

// 전역 셰이더는 엔진이 셰이더를 모으기 전에 등록돼야 해서 PostConfigInit 에 올라오는 별도 모듈에 둔다
public class YJJWaveShaders : ModuleRules
{
	public YJJWaveShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "RHI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects" });
	}
}
