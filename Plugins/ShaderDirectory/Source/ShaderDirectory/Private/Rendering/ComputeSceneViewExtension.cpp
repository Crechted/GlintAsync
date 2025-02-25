#include "Rendering/ComputeSceneViewExtension.h"

#include "RenderGraphEvent.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShaderPasses/NormalOneCS.h"

#include "RenderGraphUtils.h"
#include "PostProcess/PostProcessing.h"
#include "ShaderPasses/NormalTwoCS.h"

DECLARE_GPU_DRAWCALL_STAT(NormalCompute); // Unreal Insights

FComputeSceneViewExtension::FComputeSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
}

inline void FComputeSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    FSceneViewExtensionBase::PreRenderView_RenderThread(GraphBuilder, InView);
    if (!NormalOneRT || !NormalTwoRT || !NormalOneSource) return;

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

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    constexpr bool bUseAsyncCompute = false;
    const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;

    CalcNormalOnePass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
    CalcNormalTwoPass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
}

void FComputeSceneViewExtension::CalcNormalOnePass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap,
    const bool bAsyncCompute)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "NormalCompute ONE");

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
    Parameters->NormalSourceTexture = NormalOneSource->GetResource()->TextureRHI;
    Parameters->OutputTexture = TempUAV;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(NormalOneCompute::THREADS_X, NormalOneCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 1), PassFlags,
        TShaderMapRef<FNormalOneCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcNormalTwoPass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap, bool bAsyncCompute)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "NormalCompute TWO");

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

    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 1), PassFlags,
        TShaderMapRef<FNormalTwoCS>(GlobalShaderMap), Parameters, GroupCount);

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

void FComputeSceneViewExtension::SetNormalOne(UTextureRenderTarget2D* RenderTarget)
{
    NormalOneSource = RenderTarget;
}

void FComputeSceneViewExtension::SetNormalTwo(UTextureRenderTarget2D* RenderTarget)
{
    //NormalTwoSource = RenderTarget;
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