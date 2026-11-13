// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SmashKarts : ModuleRules
{
	public SmashKarts(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files in subfolders (Game/, UI/, Debug/, Tests/) include each other as "Game/KartGameMode.h"
		// and the flat kart/weapon headers as "KartCharacter.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// UI is built in C++ (UMG widget trees + Slate), FX through Niagara,
		// Sockets for showing the host's addresses, EngineSettings for the default maps
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "Niagara", "Sockets", "EngineSettings" });
	}
}
