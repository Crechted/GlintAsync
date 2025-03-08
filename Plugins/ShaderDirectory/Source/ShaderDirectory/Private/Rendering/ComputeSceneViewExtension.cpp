#include "Rendering/ComputeSceneViewExtension.h"

#include "RenderGraphEvent.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShaderPasses/NormalOneCS.h"

#include "RenderGraphUtils.h"
#include "PostProcess/PostProcessing.h"
#include "ShaderDirectory/GlintsSettings.h"
#include "ShaderPasses/GlintParametersCS.h"
#include "ShaderPasses/NormalTwoCS.h"
#include "ShaderPasses/SceneTextureCS.h"

DECLARE_GPU_DRAWCALL_STAT(NormalCompute); // Unreal Insights

FComputeSceneViewExtension::FComputeSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
}

void FComputeSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
    if (!OutputSomeTextureRT) return;

    FVector2D ViewportSize = FVector2D(OutputSomeTextureRT->SizeX, OutputSomeTextureRT->SizeY);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }

    if (!(ViewportSize.X == OutputSomeTextureRT->SizeX && ViewportSize.Y == OutputSomeTextureRT->SizeY))
    {
        if (PooledSomeTexturesRT.IsValid()) PooledSomeTexturesRT.SafeRelease();
        OutputSomeTextureRT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }
}

inline void FComputeSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    FSceneViewExtensionBase::PreRenderView_RenderThread(GraphBuilder, InView);
    if (!NormalOneRT || !NormalTwoRT || !NormalSource || !NormalSource->GetResource()) return;

    if (!PooledNormalOneRT.IsValid())
    {
        // Only needs to be done once
        // However, if you modify the render target asset, eg: change the resolution or pixel format, you may need to recreate the PooledNormalOneRT object
        PooledNormalOneRT = CreatePooledRenderTarget_RenderThread(NormalOneRT);
    }

    if (!PooledNormalTwoRT.IsValid())
    {
        // Only needs to be done once
        // However, if you modify the render target asset, eg: change the resolution or pixel format, you may need to recreate the PooledNormalOneRT object
        PooledNormalTwoRT = CreatePooledRenderTarget_RenderThread(NormalTwoRT);
    }

    if (!PooledGlintParametersRT.IsValid())
    {
        PooledGlintParametersRT = CreatePooledRenderTarget_RenderThread(GlintParametersRT);
    }

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    constexpr bool bUseAsyncCompute = true;
    const bool bAsyncCompute = /*GSupportsEfficientAsyncCompute &&*/ (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;

    CalcNormalOnePass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
    CalcNormalTwoPass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
    CalcGlintParametersPass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
}

void FComputeSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View,
    const FPostProcessingInputs& Inputs)
{
    FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);

    if (!OutputSomeTextureRT) return;

    if (!PooledSomeTexturesRT.IsValid())
    {
        PooledSomeTexturesRT = CreatePooledRenderTarget_RenderThread(OutputSomeTextureRT);
    }

    FSceneTextureShaderParameters SceneTextureShaderParameters = CreateSceneTextureShaderParameters(GraphBuilder, View,
        ESceneTextureSetupMode::All);

    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "SceneTextureCompute FOUR");

    // Needs to be registered every frame
    FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledSomeTexturesRT, TEXT("Bound Render Target"));

    // Since we're rendering to the render target, we're going to use the full size of the render target rather than the screen
    const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("Temp Texture"));
    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempTexture);
    FRDGTextureUAVRef TempUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FRDGTextureUAVRef SceneColourTextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->SceneColorTexture);
    //FRDGTextureUAVRef CustomDepthTextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->CustomDepthTexture);
    //FRDGTextureUAVRef SceneDepthTextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->SceneDepthTexture);
    //FRDGTextureUAVRef GBufferATextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->GBufferATexture);
    //FRDGTextureUAVRef GBufferBTextureUAV = GraphBuilder.CreateUAV((*Inputs.SceneTextures)->GBufferBTexture);

    FSomeTextureCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSomeTextureCS::FParameters>();
    Parameters->TextureSize = RenderTargetTexture->Desc.Extent;
    Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->View = View.ViewUniformBuffer;
    Parameters->SceneTextures = SceneTextureShaderParameters;
    //Parameters->SceneColorTexture = SceneColourTextureUAV;
    Parameters->InputCustomDepthTexture = (*Inputs.SceneTextures)->CustomDepthTexture;
    Parameters->InputSceneDepthTexture = (*Inputs.SceneTextures)->SceneDepthTexture;
    Parameters->InputGBufferATexture = (*Inputs.SceneTextures)->GBufferATexture;
    Parameters->InputGBufferDTexture = (*Inputs.SceneTextures)->GBufferDTexture;
    Parameters->OutputTexture = TempUAV;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(GlintParametersCompute::THREADS_X, GlintParametersCompute::THREADS_Y));

    constexpr bool bUseAsyncCompute = true;
    const bool bAsyncCompute = /*GSupportsEfficientAsyncCompute &&*/ (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;
    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("SomeSceneTextureCompute %u", 4221), PassFlags,
        TShaderMapRef<FSomeTextureCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcNormalOnePass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap,
    const bool bAsyncCompute)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "GlintCompute ONE");

    // Needs to be registered every frame
    FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledNormalOneRT, TEXT("Bound Render Target"));

    // Since we're rendering to the render target, we're going to use the full size of the render target rather than the screen
    const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("Temp Texture"));
    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempTexture);
    FRDGTextureUAVRef TempUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FNormalOneCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNormalOneCS::FParameters>();
    Parameters->TextureSize = RenderTargetTexture->Desc.Extent;
    Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->NormalSourceTexture = NormalSource->GetResource()->TextureRHI;
    Parameters->OutputTexture = TempUAV;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(NormalOneCompute::THREADS_X, NormalOneCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = /*bAsyncCompute ? ERDGPassFlags::AsyncCompute :*/ ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 2), PassFlags,
        TShaderMapRef<FNormalOneCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcNormalTwoPass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap, bool bAsyncCompute)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "GlintCompute TWO");

    // Needs to be registered every frame
    FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledNormalTwoRT, TEXT("Bound Render Target"));

    // Since we're rendering to the render target, we're going to use the full size of the render target rather than the screen
    const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("Temp Texture"));
    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempTexture);
    FRDGTextureUAVRef TempUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FNormalTwoCS::FParameters* Parameters = GraphBuilder.AllocParameters<FNormalTwoCS::FParameters>();
    Parameters->TextureSize = RenderTargetTexture->Desc.Extent;
    Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->NormalSourceTexture = NormalOneRT->GetResource()->TextureRHI;
    Parameters->OutputTexture = TempUAV;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(NormalTwoCompute::THREADS_X, NormalTwoCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = /*bAsyncCompute ? ERDGPassFlags::AsyncCompute :*/ ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 1), PassFlags,
        TShaderMapRef<FNormalTwoCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcGlintParametersPass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap,
    bool bAsyncCompute)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "GlintCompute THREE");

    // Needs to be registered every frame
    FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledGlintParametersRT, TEXT("Bound Render Target"));

    // Since we're rendering to the render target, we're going to use the full size of the render target rather than the screen
    const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("Temp Texture"));
    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempTexture);
    FRDGTextureUAVRef TempUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FGlintParametersCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGlintParametersCS::FParameters>();
    Parameters->TextureSize = RenderTargetTexture->Desc.Extent;
    Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->SigmasRho = GetDefault<UGlintsSettings>()->SigmasRho;
    Parameters->NormalTexture1 = NormalOneRT->GetResource()->TextureRHI;
    Parameters->NormalTexture2 = NormalTwoRT->GetResource()->TextureRHI;
    Parameters->GlintParametersTexture = TempUAV;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(GlintParametersCompute::THREADS_X, GlintParametersCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = /*bAsyncCompute ? ERDGPassFlags::AsyncCompute :*/ ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Glint Parameters Pass %u", 1), PassFlags,
        TShaderMapRef<FGlintParametersCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::SetOneRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
    NormalOneRT = RenderTarget;
}

void FComputeSceneViewExtension::SetTwoRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
    NormalTwoRT = RenderTarget;
}

void FComputeSceneViewExtension::SetGlintParametersTarget(UTextureRenderTarget2D* RenderTarget)
{
    GlintParametersRT = RenderTarget;
}

void FComputeSceneViewExtension::SetNormalSource(UTextureRenderTarget2D* RenderTarget)
{
    NormalSource = RenderTarget;
}

void FComputeSceneViewExtension::SetOutputSomeTextureTarget(UTextureRenderTarget2D* RenderTarget)
{
    OutputSomeTextureRT = RenderTarget;
}

TRefCountPtr<IPooledRenderTarget> FComputeSceneViewExtension::CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* RenderTarget)
{
    TRefCountPtr<IPooledRenderTarget> Result = nullptr;

    if (!RenderTarget) return Result;
    checkf(IsInRenderingThread() || IsInRHIThread(), TEXT("Cannot create from outside the rendering thread"));

    // Render target resources require the render thread
    const FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GetRenderTargetResource();
    if (RenderTargetResource == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Render Target Resource is null"));
    }

    const FTexture2DRHIRef RenderTargetRHI = RenderTargetResource->GetRenderTargetTexture();
    if (RenderTargetRHI.GetReference() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Render Target RHI is null"));
    }

    FSceneRenderTargetItem RenderTargetItem;
    RenderTargetItem.TargetableTexture = RenderTargetRHI;
    RenderTargetItem.ShaderResourceTexture = RenderTargetRHI;

    // Flags allow it to be used as a render target, shader resource, UAV 
    FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(RenderTargetResource->GetSizeXY(),
        RenderTargetRHI->GetDesc().Format, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV,
        TexCreate_None, false);

    GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, Result, RenderTargetItem);

    UE_LOG(LogTemp, Warning, TEXT("Created untracked Pooled Render Target resource"));
    return Result;
}