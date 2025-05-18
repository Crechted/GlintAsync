#pragma once

#include "SceneViewExtension.h"

class SHADERDIRECTORY_API FComputeSceneViewExtension : public FSceneViewExtensionBase
{
public:
    FComputeSceneViewExtension(const FAutoRegister& AutoRegister);

    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
    {
    };

    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
    void TryResizeRenderTargets(const FVector2D& ViewportSizew);

    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
    {
    }

    virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView,
        const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

    virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
    {
    };

    virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

    virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

    virtual void PrePostProcessPass_RenderThread(
        FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
    {
    };

private:
    TObjectPtr<UTextureRenderTarget2D> NormalOneRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> NormalTwoRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> GlintParametersRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> GlintCameraVectorTextureRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> GlintWorldNormalTextureRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> WaterDDTexCoordRT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> CustomTexture1RT = nullptr;
    TObjectPtr<UTextureRenderTarget2D> CustomTexture2RT = nullptr;

    TRefCountPtr<IPooledRenderTarget> PooledNormalOneRT;
    TRefCountPtr<IPooledRenderTarget> PooledNormalTwoRT;
    TRefCountPtr<IPooledRenderTarget> PooledGlintParametersRT;
    TRefCountPtr<IPooledRenderTarget> PooledCameraVectorTexturesRT;
    TRefCountPtr<IPooledRenderTarget> PooledWorldNormalTexturesRT;
    TRefCountPtr<IPooledRenderTarget> PooledWaterDDTexCoordRT;
    TRefCountPtr<IPooledRenderTarget> PooledCustomTexture1RT;
    TRefCountPtr<IPooledRenderTarget> PooledCustomTexture2RT;

    TObjectPtr<UTextureRenderTarget2D> NormalSource = nullptr;
    // TObjectPtr<UTextureRenderTarget2D> NormalTwoSource = nullptr;

    // FRDGTextureUAVRef TempGlintUAV;
    FRDGTextureRef TempGlintTexture = nullptr;

    bool bWasCalcGlints = false;

    bool ShouldUseAsync() const;
    void CreateTempTexture(FRDGBuilder& GraphBuilder);
    void DrawWaterMesh(FRDGBuilder& GraphBuilder, const FSceneView& InView);
    void CalcNormalOnePass(FRDGBuilder& GraphBuilder, bool bAsyncCompute);
    void CalcNormalTwoPass(FRDGBuilder& GraphBuilder, bool bAsyncCompute);
    void CalcGlintParametersPass(FRDGBuilder& GraphBuilder, bool bAsyncCompute);
    bool CalcGlintWaterPass(FRDGBuilder& GraphBuilder, const FSceneView& InView, bool bAsyncCompute);
    void GlintCompose(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs);
    TRefCountPtr<IPooledRenderTarget> CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* RenderTarget,
        ETextureCreateFlags Flags = TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV);
};
