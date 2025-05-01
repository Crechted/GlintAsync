#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

namespace GlintCompute
{
static constexpr int32 THREADS_X = 16;
static constexpr int32 THREADS_Y = 16;
}

BEGIN_SHADER_PARAMETER_STRUCT(FGlintTextureComputeParams,)
    // Texture type is same as set in shader - for getting the unlit colour
    SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
    //SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

    //SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorTexture)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, NormalTexture1)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, NormalTexture2)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GlintParamsTexture)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, WorldNormalTexture)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraVectorTexture)
    SHADER_PARAMETER(FVector3f, LightVector)
    SHADER_PARAMETER(FVector2f, TextureSize)
    SHADER_PARAMETER(FVector3f, SigmasRho)
    SHADER_PARAMETER(float, density)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, GlintsResultTexture)
END_SHADER_PARAMETER_STRUCT()

/**
 * Definition compute shader
 */
class SHADERDIRECTORY_API FGlintWaterTextureCS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FGlintWaterTextureCS, Global,);
    using FParameters = FGlintTextureComputeParams;
    SHADER_USE_PARAMETER_STRUCT(FGlintWaterTextureCS, FGlobalShader);

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
        FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

        OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
        OutEnvironment.SetDefine(TEXT("THREADS_X"), GlintCompute::THREADS_X);
        OutEnvironment.SetDefine(TEXT("THREADS_Y"), GlintCompute::THREADS_Y);
    }
};