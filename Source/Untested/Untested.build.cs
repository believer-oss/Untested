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
			"SquidTasks",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ApplicationCore",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"WorkspaceMenuStructure",
		});
	}
}
