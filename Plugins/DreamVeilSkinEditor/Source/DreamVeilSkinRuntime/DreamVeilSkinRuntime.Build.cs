using UnrealBuildTool;
public class DreamVeilSkinRuntime : ModuleRules {
 public DreamVeilSkinRuntime(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","UMG","SlateCore"});
  PrivateDependencyModuleNames.Add("Slate");
 }
}
