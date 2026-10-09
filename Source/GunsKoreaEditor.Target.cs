

using UnrealBuildTool;
using System.Collections.Generic;

public class GunsKoreaEditorTarget : TargetRules
{
	public GunsKoreaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "GunsKorea" } );
	}
}
