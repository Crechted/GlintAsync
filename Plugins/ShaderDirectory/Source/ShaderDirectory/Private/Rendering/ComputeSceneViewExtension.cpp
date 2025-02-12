#include "Rendering/ComputeSceneViewExtension.h"

#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "ShaderPasses/NormalComputeCS.h"

DECLARE_GPU_DRAWCALL_STAT(NormalCompute); // Unreal Insights

FComputeSceneViewExtension::FComputeSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
    Readback = new FRHIGPUBufferReadback(TEXT("Colour Replacement Count Readback"));
}

FComputeSceneViewExtension::~FComputeSceneViewExtension()
{
    delete Readback;
    Readback = nullptr;
}

void FComputeSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
    FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);

    checkSlow(View.bIsViewInfo);
    const FIntRect Viewport = static_cast<const FViewInfo&>(View).ViewRect;

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    constexpr bool bUseAsyncCompute = false;
    const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;

    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "NormalCompute");

    const FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, View, ESceneTextureSetupMode::SceneColor | ESceneTextureSetupMode::GBuffers);
    FRDGTextureUAVRef SceneColourTextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->SceneColorTexture);

    FNormalComputeCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNormalComputeCS::FParameters>();
    Parameters->SceneColorTexture = SceneColourTextureUAV;
    Parameters->View = View.ViewUniformBuffer;
    Parameters->SceneTextures = SceneTextures;

    const FIntPoint ThreadCount = Viewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount, FIntPoint(NormalCompute::THREADS_X, NormalCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 1), PassFlags, TShaderMapRef<FNormalComputeCS>(GlobalShaderMap), Parameters, GroupCount);
    
}
