#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

namespace NormalTwoCompute
{
static constexpr int32 THREADS_X = 16;
static constexpr int32 THREADS_Y = 16;
}

BEGIN_SHADER_PARAMETER_STRUCT(FNormalTwoComputeParams,)
    // Texture type is same as set in shader - for getting the unlit colour
    SHADER_PARAMETER(FVector2f, TextureSize)
    SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, NormalSourceTexture)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
END_SHADER_PARAMETER_STRUCT()
/**
 * Definition compute shader
 */
class SHADERDIRECTORY_API FNormalTwoCS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FNormalTwoCS, Global,);
    using FParameters = FNormalTwoComputeParams;
    SHADER_USE_PARAMETER_STRUCT(FNormalTwoCS, FGlobalShader);

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
                                             FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

        OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
        // Can set Macro
        /*SET_SHADER_DEFINE(OutEnvironment, USE_UNLIT_SCENE_COLOUR, 1);
        SET_SHADER_DEFINE(OutEnvironment, THREADS_X, NormalCompute::THREADS_X);
        SET_SHADER_DEFINE(OutEnvironment, THREADS_Y, NormalCompute::THREADS_Y);*/
        OutEnvironment.SetDefine(TEXT("THREADS_X"), NormalTwoCompute::THREADS_X);
        OutEnvironment.SetDefine(TEXT("THREADS_Y"), NormalTwoCompute::THREADS_Y);
    }
};
