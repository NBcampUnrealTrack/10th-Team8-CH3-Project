using UnrealBuildTool;

public class DreamVeilSkinEditor : ModuleRules
{
    public DreamVeilSkinEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "UMG", "UMGEditor", "Slate", "SlateCore", "Json", "JsonUtilities", "KismetCompiler", "DreamVeilSkinRuntime"
        });
    }
}
