// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ShaderDirectory : ModuleRules
{
	public ShaderDirectory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.Combine(GetModuleDirectory("Renderer"), "Private"),
				Path.Combine(GetModuleDirectory("D3D12RHI"), "Private"),
				Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "D3DX12", "include"),
				Path.Combine(Target.UEThirdPartySourceDirectory, "Windows", "DirectX", "include"),
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "Engine"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"DeveloperSettings",
                "RenderCore", 
                "Renderer",
				"D3D12RHI", "RHI", "RHICore", "VulkanRHI", "Vulkan"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
