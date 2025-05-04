#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneTexturesConfig.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

namespace GlintCompose
{
static constexpr int32 THREADS_X = 16;
static constexpr int32 THREADS_Y = 16;
}

BEGIN_SHADER_PARAMETER_STRUCT(FGlintComposeParams,)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
    SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, SurfaceColorTexture)
    SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GlintResultTexture)
    // Only needed if we're outputting to a render target
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * Definition compute shader
 */
class SHADERDIRECTORY_API FGlintComposePS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FGlintComposePS, Global,);
    using FParameters = FGlintComposeParams;
    SHADER_USE_PARAMETER_STRUCT(FGlintComposePS, FGlobalShader);

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
    }
};