#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"
#include "ShaderParameterStruct.h"
#include "NormalOneCS.h"
#include "RenderGraphUtils.h"


/**
 * Definition compute shader
 */
class SHADERDIRECTORY_API FNormalTwoCS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FNormalTwoCS, Global,);
    using FParameters = FNormalComputeParams;
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
        OutEnvironment.SetDefine(TEXT("THREADS_X"), NormalCompute::THREADS_X);
        OutEnvironment.SetDefine(TEXT("THREADS_Y"), NormalCompute::THREADS_Y);
    }
};
