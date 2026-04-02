using UnrealBuildTool;

public class TeidoreArmoryTarget : TargetRules
{
	public TeidoreArmoryTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("TeidoreArmory");
	}
}
