// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PGActor : ModuleRules
{
	public PGActor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "PGShared", "PGData", "AIModule" });

		// Preserve existing cross-module references during the UE 5.8 migration.
		CircularlyReferencedDependentModules.AddRange(new string[] { "PGUI" });
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PrivateDependencyModuleNames.Add("AudioMixer"); // Isolated UI audio capture probe.
        
        // C++ 20 사용 설정
        CppStandard = CppStandardVersion.Cpp20;
		
		// 모듈 헤더 공개 설정
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameplayAbilities",
				"EnhancedInput",
				"UMG"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PGAbilitySystem", "IKRig", "AnimGraphRuntime",
				"MotionWarping",
				"PGMessage",
				"PGUI", 
				"Niagara",
				"NavigationSystem", "Slate", "SlateCore", "InputCore", "Json"
			});
	}
}
