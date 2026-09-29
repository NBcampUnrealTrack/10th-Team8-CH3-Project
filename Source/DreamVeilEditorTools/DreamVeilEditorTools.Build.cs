using UnrealBuildTool;

public class DreamVeilEditorTools : ModuleRules
{
    public DreamVeilEditorTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "DreamVeil", "UnrealEd", "UMG", "UMGEditor",
            "Slate", "SlateCore", "BlueprintGraph", "Kismet", "KismetCompiler", "AssetRegistry", "AssetTools",
            "RenderCore", "RHI", "SlateRHIRenderer"
        });
    }
}
