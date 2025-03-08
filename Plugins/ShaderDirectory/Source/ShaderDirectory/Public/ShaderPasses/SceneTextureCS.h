#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

BEGIN_SHADER_PARAMETER_STRUCT(FSomeTextureComputeParams,)
    // Texture type is same as set in shader - for getting the unlit colour
    SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
    SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

    //SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputCustomDepthTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputSceneDepthTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputGBufferATexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputGBufferDTexture)
    SHADER_PARAMETER(FVector2f, TextureSize)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
END_SHADER_PARAMETER_STRUCT()

/**
 * Definition compute shader
 */
class SHADERDIRECTORY_API FSomeTextureCS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FSomeTextureCS, Global,);
    using FParameters = FSomeTextureComputeParams;
    SHADER_USE_PARAMETER_STRUCT(FSomeTextureCS, FGlobalShader);

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
        FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

        OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
    }
};