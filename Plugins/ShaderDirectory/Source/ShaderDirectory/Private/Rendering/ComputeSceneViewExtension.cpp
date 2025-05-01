#include "Rendering/ComputeSceneViewExtension.h"

#include "PixelShaderUtils.h"
#include "RenderGraphEvent.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShaderPasses/NormalOneCS.h"

#include "RenderGraphUtils.h"
#include "MeshPassProcess/WaterMeshPassProcess.h"
#include "PostProcess/PostProcessing.h"
#include "ShaderDirectory/GlintsSettings.h"
#include "ShaderPasses/GlintParametersCS.h"
#include "ShaderPasses/NormalTwoCS.h"
#include "ShaderPasses/SceneTextureCS.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "BasePassRendering.h"
#include "BasePassRendering.inl"
//#include "BasePassRendering.cpp"

#include "ScenePrivate.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "InstanceCulling/InstanceCullingContext.h"
//#include "WaterGlintHelper.h"


BEGIN_SHADER_PARAMETER_STRUCT(FWaterPassParameters,)
    //SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
    //SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
    SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FInstanceCullingGlobalUniforms, InstanceCulling)
    SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, SceneBuffer)
    RENDER_TARGET_BINDING_SLOTS() // OMSetRenderTarget
END_SHADER_PARAMETER_STRUCT()

//IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FForwardLightData, "ForwardLightData");


DECLARE_GPU_DRAWCALL_STAT(NormalCompute); // Unreal Insights


FComputeSceneViewExtension::FComputeSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
}

void FComputeSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
    if (!NormalSource)
        NormalSource = GetDefault<UGlintsSettings>()->NormalSource.LoadSynchronous();
    if (!NormalSource)
        UE_LOG(LogTexture, Error, TEXT("No Normal Source ONE found"));

    if (!NormalOneRT)
        NormalOneRT = GetDefault<UGlintsSettings>()->NormalOneRenderTarget.LoadSynchronous();
    if (!NormalOneRT)
        UE_LOG(LogTexture, Error, TEXT("No Render Target for NormalOne found"));

    if (!NormalTwoRT)
        NormalTwoRT = GetDefault<UGlintsSettings>()->NormalTwoRenderTarget.LoadSynchronous();
    if (!NormalTwoRT)
        UE_LOG(LogTexture, Error, TEXT("No Render Target for NormalTwo found"));

    if (!GlintCameraVectorTextureRT)
        GlintCameraVectorTextureRT = GetDefault<UGlintsSettings>()->CameraVectorTextureTarget.LoadSynchronous();
    if (!GlintCameraVectorTextureRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint World Position Texture Target found"));

    if (!GlintWorldNormalTextureRT)
        GlintWorldNormalTextureRT = GetDefault<UGlintsSettings>()->WorldNormalTextureTarget.LoadSynchronous();
    if (!GlintWorldNormalTextureRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint World Normal Texture Target found"));

    if (!GlintResultRT)
        GlintResultRT = GetDefault<UGlintsSettings>()->ResultGlintTarget.LoadSynchronous();
    if (!GlintResultRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint Result Target found"));

    if (!GlintParametersRT)
        GlintParametersRT = GetDefault<UGlintsSettings>()->GlintParametersTarget.LoadSynchronous();
    if (!GlintParametersRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint Parameters RT found"));

    FVector2D ViewportSize = FVector2D(InView.UnscaledViewRect.Width(), InView.UnscaledViewRect.Height());
    TryResizeRenderTargets(ViewportSize);
}

void FComputeSceneViewExtension::TryResizeRenderTargets(const FVector2D& ViewportSize)
{
    if (GlintCameraVectorTextureRT && !(ViewportSize.X == GlintCameraVectorTextureRT->SizeX && ViewportSize.Y == GlintCameraVectorTextureRT
                                        ->SizeY))
    {
        if (PooledCameraVectorTexturesRT.IsValid()) PooledCameraVectorTexturesRT.SafeRelease();
        GlintCameraVectorTextureRT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }

    if (GlintWorldNormalTextureRT && !(ViewportSize.X == GlintWorldNormalTextureRT->SizeX && ViewportSize.Y == GlintWorldNormalTextureRT->
                                       SizeY))
    {
        if (PooledWorldNormalTexturesRT.IsValid()) PooledWorldNormalTexturesRT.SafeRelease();
        GlintWorldNormalTextureRT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }

    if (GlintResultRT && !(ViewportSize.X == GlintResultRT->SizeX && ViewportSize.Y == GlintResultRT->SizeY))
    {
        if (PooledGlintResultRT.IsValid()) PooledGlintResultRT.SafeRelease();
        GlintResultRT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
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

    constexpr bool bUseAsyncCompute = false;
    const bool bAsyncCompute = /*GSupportsEfficientAsyncCompute &&*/ (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;

    CalcNormalOnePass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
    CalcNormalTwoPass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
    CalcGlintParametersPass(GraphBuilder, GlobalShaderMap, bAsyncCompute);
}

void FComputeSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    FSceneViewExtensionBase::PostRenderView_RenderThread(GraphBuilder, InView);
    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    constexpr bool bUseAsyncCompute = false;
    const bool bAsyncCompute = /*GSupportsEfficientAsyncCompute &&*/ (GNumExplicitGPUsForRendering == 1) && bUseAsyncCompute;

    DrawWaterMesh(GraphBuilder, InView);
    CalcGlintWaterPass(GraphBuilder, GlobalShaderMap, InView, bAsyncCompute);
}

void FComputeSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
    const FPostProcessingInputs& Inputs)
{
    FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, InView, Inputs);
}

void FComputeSceneViewExtension::CalcGlintWaterPass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap,
    FSceneView& InView, bool bAsyncCompute)
{
    if (!InView.ViewUniformBuffer.IsValid() && InView.bIsViewInfo)
    {
        if (auto View = static_cast<const FViewInfo*>(&InView))
        {
            InView.ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(
                *View->CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);
        }
        if (!InView.ViewUniformBuffer.IsValid()) return;
    }

    if (!GlintResultRT) return;

    if (!PooledGlintResultRT.IsValid())
    {
        PooledGlintResultRT = CreatePooledRenderTarget_RenderThread(GlintResultRT);
    }

    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "SceneTextureCompute FOUR");

    FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledGlintResultRT, TEXT("Bound Render Target"));

    const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

    FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(RenderTargetTexture->Desc, TEXT("Temp Texture"));
    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempTexture);
    FRDGTextureUAVRef TempUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FGlintWaterTextureCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGlintWaterTextureCS::FParameters>();
    Parameters->View = InView.ViewUniformBuffer;
    Parameters->SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->NormalTexture1 = NormalOneRT->GetResource()->GetTextureRHI();
    Parameters->NormalTexture2 = NormalTwoRT->GetResource()->GetTextureRHI();
    Parameters->WorldNormalTexture = GlintWorldNormalTextureRT->GetResource()->GetTextureRHI();
    Parameters->CameraVectorTexture = GlintCameraVectorTextureRT->GetResource()->GetTextureRHI();
    Parameters->GlintParamsTexture = GlintParametersRT->GetResource()->GetTextureRHI();
    Parameters->GlintsResultTexture = TempUAV;
    Parameters->LightVector = GetDefault<UGlintsSettings>()->LightVector;
    Parameters->TextureSize = RenderTargetTexture->Desc.Extent;
    Parameters->SigmasRho = GetDefault<UGlintsSettings>()->SigmasRho;
    Parameters->density = GetDefault<UGlintsSettings>()->Density;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(GlintParametersCompute::THREADS_X, GlintParametersCompute::THREADS_Y));

    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("GlintResultTextureCompute %u", 4221), PassFlags,
        TShaderMapRef<FGlintWaterTextureCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::DrawWaterMesh(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "Vertex and Pixel Water");
    if (!GlintCameraVectorTextureRT || !GlintWorldNormalTextureRT) return;

    if (!PooledCameraVectorTexturesRT.IsValid() || !PooledWorldNormalTexturesRT.IsValid())
    {
        PooledCameraVectorTexturesRT = CreatePooledRenderTarget_RenderThread(GlintCameraVectorTextureRT);
        PooledWorldNormalTexturesRT = CreatePooledRenderTarget_RenderThread(GlintWorldNormalTextureRT);
        if (!PooledCameraVectorTexturesRT || !PooledWorldNormalTexturesRT) return;
    }

    if (InView.bIsViewInfo)
    {
        auto View = static_cast<const FViewInfo*>(&InView);
        RDG_GPU_MASK_SCOPE(GraphBuilder, View->GPUMask);

        FRDGTextureRef CameraVectorTexture = GraphBuilder.RegisterExternalTexture(PooledCameraVectorTexturesRT, TEXT("Bound CameraVector"));
        FRDGTextureRef NormalTexture = GraphBuilder.RegisterExternalTexture(PooledWorldNormalTexturesRT, TEXT("Bound WorldNormal"));

        TStaticArray<FRenderTargetBinding, MaxSimultaneousRenderTargets> Output;

        Output[0] = FRenderTargetBinding(CameraVectorTexture, ERenderTargetLoadAction::EClear);
        Output[1] = FRenderTargetBinding(NormalTexture, ERenderTargetLoadAction::EClear);

        FViewShaderParameters Parameters;
        Parameters.View = View->ViewUniformBuffer;
        Parameters.InstancedView = View->GetInstancedViewUniformBuffer();
        // if we're a part of the stereo pair, make sure that the pointer isn't bogus

        FWaterPassParameters* PassParameters = GraphBuilder.AllocParameters<FWaterPassParameters>();
        //PassParameters->View = Parameters;
        PassParameters->RenderTargets.Output = Output;
        PassParameters->InstanceCulling = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
        PassParameters->SceneBuffer = GetSceneUniformBufferRef(GraphBuilder, *View);

        // Настройка глубины
        /*PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
            DepthTexture,
            ERenderTargetLoadAction::ELoad,
            ERenderTargetLoadAction::ENoAction,
            FExclusiveDepthStencil::DepthWrite_StencilNop);*/

        FMeshPassProcessorRenderState PassDrawRenderState;
        PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());

        //if (PassDrawRenderState.GetDepthStencilAccess() & FExclusiveDepthStencil::DepthWrite)
        //PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthFartherOrEqual>::GetRHI());
        //else
        PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthFartherOrEqual>::GetRHI());

        AddDrawDynamicMeshPass(GraphBuilder, RDG_EVENT_NAME("Vertex and Pixel Water Shader%u", 1), PassParameters, InView, View->ViewRect,
            [View, PassDrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
            {
                FWaterMeshPassProcessor PassProcessor(
                    View->Family->Scene->GetRenderScene(),
                    View->FeatureLevel,
                    View,
                    PassDrawRenderState,
                    DynamicMeshPassContext);
                if (const FScene* Scene = View->Family->Scene->GetRenderScene())
                {
                    auto WaterProxies = Scene->GetPrimitiveSceneProxies().FilterByPredicate([&](const FPrimitiveSceneProxy* Proxy)
                    {
                        return Proxy->GetTypeHash() == FCustomWaterMeshSceneProxy::WaterTypeHash();
                    });
                    for (auto SceneProxy : WaterProxies)
                    {
                        TArray<FMeshBatch> MeshElements;
                        SceneProxy->GetMeshDescription(0, MeshElements);
                        if (MeshElements.IsEmpty()) continue;
                        const FMeshBatch& Mesh = MeshElements[0];
                        const uint64 DefaultBatchElementMask = ~0ull;
                        PassProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, SceneProxy, 0);
                    }
                }
            });
    }
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

TRefCountPtr<IPooledRenderTarget> FComputeSceneViewExtension::CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* RenderTarget,
    ETextureCreateFlags Flags)
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
        RenderTargetRHI->GetDesc().Format, FClearValueBinding::Black, Flags,
        TexCreate_None, false);

    GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, Result, RenderTargetItem);

    UE_LOG(LogTemp, Warning, TEXT("Created untracked Pooled Render Target resource"));
    return Result;
}