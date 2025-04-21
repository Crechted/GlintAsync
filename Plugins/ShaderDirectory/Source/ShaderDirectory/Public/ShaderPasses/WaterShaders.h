#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MeshMaterialShader.h"

// vertex shader class for CustomTerrainPass
class FWaterSimulatePassVS : public FMeshMaterialShader
{
public:
    DECLARE_SHADER_TYPE(FWaterSimulatePassVS, MeshMaterial);

    static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
    {
        // Compile if supported by the hardware.
        const bool bIsFeatureSupported = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);

        return bIsFeatureSupported && FMeshMaterialShader::ShouldCompilePermutation(Parameters);
    }

    FWaterSimulatePassVS() = default;

    FWaterSimulatePassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FMeshMaterialShader(Initializer)
    {
    }
};

// domain shader class for CustomTerrainPass
class FWaterSimulatePassPS : public FMeshMaterialShader
{
public:
    DECLARE_SHADER_TYPE(FWaterSimulatePassPS, MeshMaterial);

    static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
    {
        return true;
    }

    FWaterSimulatePassPS() = default;

    FWaterSimulatePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FMeshMaterialShader(Initializer)
    {
    }
};