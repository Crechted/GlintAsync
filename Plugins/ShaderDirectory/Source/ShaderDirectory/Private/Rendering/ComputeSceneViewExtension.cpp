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
#include "ShaderPasses/GlintWaterCS.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "BasePassRendering.h"
#include "BasePassRendering.inl"
//#include "BasePassRendering.cpp"

//#include "AssetDefinition.h"
#include "RenderTargetSubsystem.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "SceneRendering.h"
#include "Engine/Texture2DArray.h"
#include "InstanceCulling/InstanceCullingContext.h"
#include "ShaderPasses/GlintComposePS.h"
//#include "WaterGlintHelper.h"


BEGIN_SHADER_PARAMETER_STRUCT(FWaterPassParameters,)
    //SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
    //SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
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

    if (!GlintParametersRT)
        GlintParametersRT = GetDefault<UGlintsSettings>()->GlintParametersTarget.LoadSynchronous();
    if (!GlintParametersRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint Parameters RT found"));

    if (!WaterDDTexCoordRT)
        WaterDDTexCoordRT = GetDefault<UGlintsSettings>()->WaterUVTarget.LoadSynchronous();
    if (!WaterDDTexCoordRT)
        UE_LOG(LogTexture, Error, TEXT("No Glint Result Target found"));

    if (!CustomTexture1RT)
        CustomTexture1RT = GetDefault<UGlintsSettings>()->CustomDataTarget.LoadSynchronous();
    if (!CustomTexture1RT)
        UE_LOG(LogTexture, Error, TEXT("No CustomTexture1 Target found"));

    if (!CustomTexture2RT)
        CustomTexture2RT = GetDefault<UGlintsSettings>()->ResultGlintTarget.LoadSynchronous();
    if (!CustomTexture2RT)
        UE_LOG(LogTexture, Error, TEXT("No Glint Result Target found"));

    FVector2D ViewportSize = FVector2D(InView.UnscaledViewRect.Width(), InView.UnscaledViewRect.Height());
    TryResizeRenderTargets(ViewportSize);

    GEngine->LoadBlueNoiseTexture();
    GEngine->LoadGlintTextures();
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

    if (WaterDDTexCoordRT && !(ViewportSize.X == WaterDDTexCoordRT->SizeX && ViewportSize.Y == WaterDDTexCoordRT->SizeY))
    {
        if (PooledWaterDDTexCoordRT.IsValid()) PooledWaterDDTexCoordRT.SafeRelease();
        WaterDDTexCoordRT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }

    if (CustomTexture1RT && !(ViewportSize.X == CustomTexture1RT->SizeX && ViewportSize.Y == CustomTexture1RT->SizeY))
    {
        if (PooledCustomTexture1RT.IsValid()) PooledCustomTexture1RT.SafeRelease();
        CustomTexture1RT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }

    if (CustomTexture2RT && !(ViewportSize.X == CustomTexture2RT->SizeX && ViewportSize.Y == CustomTexture2RT->SizeY))
    {
        if (PooledCustomTexture2RT.IsValid()) PooledCustomTexture2RT.SafeRelease();
        CustomTexture2RT->ResizeTarget(ViewportSize.X, ViewportSize.Y);
    }
}

bool FComputeSceneViewExtension::ShouldUseAsync() const
{
    return GEngine->GetEngineSubsystem<URenderTargetSubsystem>()->IsUseAsync()
           && GSupportsEfficientAsyncCompute
           && GNumExplicitGPUsForRendering == 1;
}

inline void FComputeSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    FSceneViewExtensionBase::PreRenderView_RenderThread(GraphBuilder, InView);
    if (!NormalOneRT || !NormalTwoRT || !NormalSource || !NormalSource->GetResource()) return;

    if (!InView.ViewUniformBuffer.IsValid() && InView.bIsViewInfo)
    {
        if (auto View = static_cast<FViewInfo*>(&InView))
        {
            FViewUniformShaderParameters UniformShaderParameters;
            FIntPoint BufferSize = FIntPoint(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height());
            View->SetupCommonViewUniformBufferParameters(UniformShaderParameters, BufferSize, 0, View->ViewRect, View->ViewMatrices,
                View->PrevViewInfo.ViewMatrices);

            if (GEngine->GlintTexture2)
                UniformShaderParameters.GlintTexture = GEngine->GlintTexture2->GetResource()->GetTexture2DArrayRHI()
                                                              ->GetTexture2DArray();

            View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(UniformShaderParameters,
                UniformBuffer_SingleFrame);
            //CreateViewUniformBuffers(*View, UniformShaderParameters);
        }

    }

    if (!PooledNormalOneRT.IsValid())
    {
        PooledNormalOneRT = CreatePooledRenderTarget_RenderThread(NormalOneRT);
        if (!PooledNormalOneRT.IsValid()) return;
    }

    if (!PooledNormalTwoRT.IsValid())
    {
        PooledNormalTwoRT = CreatePooledRenderTarget_RenderThread(NormalTwoRT);
        if (!PooledNormalTwoRT.IsValid()) return;
    }

    if (!PooledGlintParametersRT.IsValid())
    {
        PooledGlintParametersRT = CreatePooledRenderTarget_RenderThread(GlintParametersRT);
        if (!PooledGlintParametersRT.IsValid()) return;
    }

    CreateTempTexture(GraphBuilder);

    CalcNormalOnePass(GraphBuilder, false);
    CalcNormalTwoPass(GraphBuilder, false);
    CalcGlintParametersPass(GraphBuilder, false);
}

void FComputeSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView,
    const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
    bWasCalcGlints = CalcGlintWaterPass(GraphBuilder, InView, ShouldUseAsync());
}

void FComputeSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
    FSceneViewExtensionBase::PostRenderView_RenderThread(GraphBuilder, InView);
    DrawWaterMesh(GraphBuilder, InView);
}

void FComputeSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
    const FPostProcessingInputs& Inputs)
{
    FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, InView, Inputs);

    GlintCompose(GraphBuilder, InView, Inputs);
}

bool FComputeSceneViewExtension::CalcGlintWaterPass(FRDGBuilder& GraphBuilder, const FSceneView& InView, bool bAsyncCompute)
{
    if (!GetDefault<UGlintsSettings>()->bCalcGlints) return false;
    if (InView.bIsViewInfo)
    {
        auto View = static_cast<const FViewInfo*>(&InView);
        if (const FScene* Scene = View->Family->Scene->GetRenderScene())
        {
            auto WaterProxies = Scene->GetPrimitiveSceneProxies().FilterByPredicate([&](const FPrimitiveSceneProxy* Proxy)
            {
                return Proxy->GetTypeHash() == FCustomWaterMeshSceneProxy::WaterTypeHash();
            });
            if (WaterProxies.IsEmpty()) return false;
        }
    }
    if (!CustomTexture2RT || !NormalOneRT || !CustomTexture1RT || !GlintWorldNormalTextureRT || !GlintCameraVectorTextureRT || !
        WaterDDTexCoordRT || !GlintParametersRT)
        return false;

    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "Glints Compute FOUR");

    const FIntRect RenderViewport = FIntRect(0, 0, CustomTexture2RT->SizeX, CustomTexture2RT->SizeY);

    FRDGTextureUAVDesc TempUAVDesc = FRDGTextureUAVDesc(TempGlintTexture);
    FRDGTextureUAVRef TempGlintUAV = GraphBuilder.CreateUAV(TempUAVDesc);

    FGlintWaterTextureCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGlintWaterTextureCS::FParameters>();

    Parameters->View = InView.ViewUniformBuffer;
    Parameters->ClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->NormalSampler = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

    Parameters->NormalTexture1 = NormalOneRT->GetResource()->GetTextureRHI();
    Parameters->Custom1Texture = CustomTexture1RT->GetResource()->GetTextureRHI();
    Parameters->Custom2Texture = CustomTexture2RT->GetResource()->GetTextureRHI();
    Parameters->WorldNormalTexture = GlintWorldNormalTextureRT->GetResource()->GetTextureRHI();
    Parameters->CameraVectorTexture = GlintCameraVectorTextureRT->GetResource()->GetTextureRHI();
    Parameters->GlintParamsTexture = GlintParametersRT->GetResource()->GetTextureRHI();
    Parameters->WaterUVTexture = WaterDDTexCoordRT->GetResource()->GetTextureRHI();

    Parameters->GlintsResultTexture = TempGlintUAV;
    Parameters->LightVector = GetDefault<UGlintsSettings>()->LightVector;
    Parameters->TextureSize = FIntPoint(CustomTexture2RT->SizeX, CustomTexture2RT->SizeY);
    Parameters->SigmasRho = GetDefault<UGlintsSettings>()->SigmasRho;
    Parameters->density = GetDefault<UGlintsSettings>()->Density;

    const FIntPoint ThreadCount = RenderViewport.Size();
    const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ThreadCount,
        FIntPoint(GlintParametersCompute::THREADS_X, GlintParametersCompute::THREADS_Y));

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("GlintResultTextureCompute %u", 4221), PassFlags,
        TShaderMapRef<FGlintWaterTextureCS>(GlobalShaderMap), Parameters, GroupCount);

    return true;
}

void FComputeSceneViewExtension::DrawWaterMesh(FRDGBuilder& GraphBuilder, const FSceneView& InView)
{
    if (!GetDefault<UGlintsSettings>()->bCalcGlints) return;
    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "Vertex and Pixel Water");
    if (!GlintCameraVectorTextureRT || !GlintWorldNormalTextureRT || !WaterDDTexCoordRT || !CustomTexture1RT || !CustomTexture2RT) return;

    PooledCameraVectorTexturesRT = PooledCameraVectorTexturesRT.IsValid()
                                       ? PooledCameraVectorTexturesRT
                                       : CreatePooledRenderTarget_RenderThread(GlintCameraVectorTextureRT);
    PooledWorldNormalTexturesRT = PooledWorldNormalTexturesRT.IsValid()
                                      ? PooledWorldNormalTexturesRT
                                      : CreatePooledRenderTarget_RenderThread(GlintWorldNormalTextureRT);
    PooledWaterDDTexCoordRT = PooledWaterDDTexCoordRT.IsValid()
                                  ? PooledWaterDDTexCoordRT
                                  : CreatePooledRenderTarget_RenderThread(WaterDDTexCoordRT);
    PooledCustomTexture1RT = PooledCustomTexture1RT.IsValid()
                                 ? PooledCustomTexture1RT
                                 : CreatePooledRenderTarget_RenderThread(CustomTexture1RT);
    PooledCustomTexture2RT = PooledCustomTexture2RT.IsValid()
                                 ? PooledCustomTexture2RT
                                 : CreatePooledRenderTarget_RenderThread(CustomTexture2RT);

    if (!PooledCameraVectorTexturesRT.IsValid() || !PooledWorldNormalTexturesRT.IsValid()
        || !PooledWaterDDTexCoordRT.IsValid() || !PooledCustomTexture1RT.IsValid() || !PooledCustomTexture2RT.IsValid()
        /*|| !PooledCameraVectorTexturesRT->GetTransientTexture() || !PooledWorldNormalTexturesRT->GetTransientTexture()
        || !PooledWaterDDTexCoordRT->GetTransientTexture() || !PooledCustomTexture1RT->GetTransientTexture()*/)
        return;

    if (InView.bIsViewInfo && InView.ViewUniformBuffer.IsValid())
    {
        auto View = static_cast<const FViewInfo*>(&InView);
        RDG_GPU_MASK_SCOPE(GraphBuilder, View->GPUMask);

        FRDGTextureRef CameraVectorTexture = GraphBuilder.RegisterExternalTexture(PooledCameraVectorTexturesRT, TEXT("Bound CameraVector"));
        FRDGTextureRef NormalTexture = GraphBuilder.RegisterExternalTexture(PooledWorldNormalTexturesRT, TEXT("Bound WorldNormal"));
        FRDGTextureRef UVTexture = GraphBuilder.RegisterExternalTexture(PooledWaterDDTexCoordRT, TEXT("Bound Water UVs Data"));
        FRDGTextureRef CustomTexture1 = GraphBuilder.RegisterExternalTexture(PooledCustomTexture1RT, TEXT("Bound Custom Data 1"));
        FRDGTextureRef CustomTexture2 = GraphBuilder.RegisterExternalTexture(PooledCustomTexture2RT, TEXT("Bound Custom Data 2"));

        TStaticArray<FRenderTargetBinding, MaxSimultaneousRenderTargets> Output;

        Output[0] = FRenderTargetBinding(CameraVectorTexture, ERenderTargetLoadAction::EClear);
        Output[1] = FRenderTargetBinding(NormalTexture, ERenderTargetLoadAction::EClear);
        Output[2] = FRenderTargetBinding(UVTexture, ERenderTargetLoadAction::EClear);
        Output[3] = FRenderTargetBinding(CustomTexture1, ERenderTargetLoadAction::EClear);
        Output[4] = FRenderTargetBinding(CustomTexture2, ERenderTargetLoadAction::EClear);

        /*FViewShaderParameters Parameters;
        Parameters.View = View->ViewUniformBuffer;
        Parameters.InstancedView = View->GetInstancedViewUniformBuffer();*/
        // if we're a part of the stereo pair, make sure that the pointer isn't bogus

        FWaterPassParameters* PassParameters = GraphBuilder.AllocParameters<FWaterPassParameters>();
        PassParameters->View = View->ViewUniformBuffer;
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

void FComputeSceneViewExtension::CalcNormalOnePass(FRDGBuilder& GraphBuilder, const bool bAsyncCompute)
{
    if (!NormalSource) return;
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

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 2), PassFlags,
        TShaderMapRef<FNormalOneCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcNormalTwoPass(FRDGBuilder& GraphBuilder, bool bAsyncCompute)
{
    if (!NormalOneRT) return;
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

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Normal Compute Pass %u", 1), PassFlags,
        TShaderMapRef<FNormalTwoCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::CalcGlintParametersPass(FRDGBuilder& GraphBuilder, bool bAsyncCompute)
{
    if (!NormalOneRT || !NormalTwoRT)
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

    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const ERDGPassFlags PassFlags = bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Glint Parameters Pass %u", 1), PassFlags,
        TShaderMapRef<FGlintParametersCS>(GlobalShaderMap), Parameters, GroupCount);

    AddCopyTexturePass(GraphBuilder, TempTexture, RenderTargetTexture);
}

void FComputeSceneViewExtension::GlintCompose(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
    if (!bWasCalcGlints || !GetDefault<UGlintsSettings>()->bComposeGlints || !CustomTexture2RT || !CustomTexture1RT)
        return;

    RDG_GPU_STAT_SCOPE(GraphBuilder, NormalCompute);
    RDG_EVENT_SCOPE(GraphBuilder, "GlintComposeSceneTexture");

    const FIntRect Viewport = static_cast<const FViewInfo&>(InView).ViewRect;
    const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

    const FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, InView,
        ESceneTextureSetupMode::SceneColor | ESceneTextureSetupMode::GBuffers);

    const FScreenPassTexture SceneColourTexture((*Inputs.SceneTextures)->SceneColorTexture, Viewport);

    FRDGTextureSRVDesc TempSRVDesc = FRDGTextureSRVDesc(TempGlintTexture);
    FRDGTextureSRVRef TempGlintSRV = GraphBuilder.CreateSRV(TempSRVDesc);

    FGlintComposePS::FParameters* Parameters = GraphBuilder.AllocParameters<FGlintComposePS::FParameters>();
    Parameters->SceneColorTexture = SceneColourTexture.Texture;
    Parameters->SceneTextures = SceneTextures;
    Parameters->View = InView.ViewUniformBuffer;
    Parameters->CustomTexture = CustomTexture1RT->GetResource()->GetTextureRHI();
    Parameters->GlintResultTexture = TempGlintSRV;
    Parameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ELoad);

    TShaderMapRef<FGlintComposePS> PixelShader(GlobalShaderMap);
    FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("Glint Compose Pass")), PixelShader, Parameters,
        Viewport);
}

void FComputeSceneViewExtension::CreateTempTexture(FRDGBuilder& GraphBuilder)
{
    FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(
        FIntPoint(CustomTexture2RT->SizeX, CustomTexture2RT->SizeY),
        CustomTexture2RT->GetFormat(),
        FClearValueBinding::Black,
        TexCreate_ShaderResource | TexCreate_UAV
        );

    TempGlintTexture = GraphBuilder.CreateTexture(TempDesc, TEXT("Temp Glint Texture"), ERDGTextureFlags::None);
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