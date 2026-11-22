using UnrealBuildTool;
using System.Collections.Generic;

public class YJJWaveTarget : TargetRules
{
	public YJJWaveTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "YJJWave", "YJJWaveShaders" });
	}
}
