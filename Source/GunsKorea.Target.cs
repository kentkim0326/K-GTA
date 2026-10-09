

using UnrealBuildTool;
using System.Collections.Generic;

public class GunsKoreaTarget : TargetRules
{
	public GunsKoreaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "GunsKorea" } );
	}
}
