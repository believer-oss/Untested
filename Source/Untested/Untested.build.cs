using UnrealBuildTool;

public class Untested : ModuleRules
{
	public Untested(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"CommonGame",
			"SquidTasks",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ApplicationCore",
				"InputCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
		}
	}
}
