#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

/**
 * 
 */
class SHADERDIRECTORY_API FComputeSceneViewExtension : public FSceneViewExtensionBase
{
private:
    TObjectPtr<UTextureRenderTarget2D> NormalOneRTSource = nullptr;
    TObjectPtr<UTextureRenderTarget2D> NormalTwoRTSource = nullptr;

    TRefCountPtr<IPooledRenderTarget> PooledNormalOneRT;
    TRefCountPtr<IPooledRenderTarget> PooledNormalTwoRT;

    TObjectPtr<UTextureRenderTarget2D> NormalOneSource = nullptr;
    TObjectPtr<UTextureRenderTarget2D> NormalTwoSource = nullptr;

public:
    FComputeSceneViewExtension(const FAutoRegister& AutoRegister);

    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
    {
    };

    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
    {
    };

    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
    {
    };

    virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView,
        const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override
    {
    };

    virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
    {
    };

    virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override
    {
    };

    virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override
    {
    };
    virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View,
        const FPostProcessingInputs& Inputs) override;

    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override
    {
    };

    void SetRenderTarget(UTextureRenderTarget2D* RenderTarget);
    void SetNormalOne(UTextureRenderTarget2D* RenderTarget);
    void SetNormalTwo(UTextureRenderTarget2D* RenderTarget);

private:
    void CalcNormalOnePass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap, bool bAsyncCompute);
    void CalcNormalTwoPass(FRDGBuilder& GraphBuilder, const FGlobalShaderMap* GlobalShaderMap, bool bAsyncCompute);
    TRefCountPtr<IPooledRenderTarget> CreatePooledRenderTarget_RenderThread(UTextureRenderTarget2D* RenderTarget);
};
