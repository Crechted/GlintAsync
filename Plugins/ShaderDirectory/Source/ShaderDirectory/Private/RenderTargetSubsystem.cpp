// Fill out your copyright notice in the Description page of Project Settings.


#include "RenderTargetSubsystem.h"

#include "SceneViewExtension.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/ComputeSceneViewExtension.h"
#include "ShaderDirectory/GlintsSettings.h"

void URenderTargetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ComputeSceneViewExtension = FSceneViewExtensions::NewExtension<FComputeSceneViewExtension>();

    RenderTargetNormalOne = GetDefault<UGlintsSettings>()->NormalOneRenderTarget.LoadSynchronous();
    if (RenderTargetNormalOne)
    {
        ComputeSceneViewExtension->SetOneRenderTarget(RenderTargetNormalOne);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Render Target for NormalOne found"));
    }

    RenderTargetNormalTwo = GetDefault<UGlintsSettings>()->NormalTwoRenderTarget.LoadSynchronous();
    if (RenderTargetNormalTwo)
    {
        ComputeSceneViewExtension->SetTwoRenderTarget(RenderTargetNormalTwo);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Render Target for NormalTwo found"));
    }

    RenderTargetNormalSource = GetDefault<UGlintsSettings>()->NormalSource.LoadSynchronous();
    if (RenderTargetNormalSource)
    {
        ComputeSceneViewExtension->SetNormalSource(RenderTargetNormalSource);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Normal Source ONE found"));
    }

    RenderTargetSomeTexture = GetDefault<UGlintsSettings>()->OutputSomeTextureTarget.LoadSynchronous();
    if (RenderTargetSomeTexture)
    {
        ComputeSceneViewExtension->SetOutputSomeTextureTarget(RenderTargetSomeTexture);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Output Some Texture Target found"));
    }

    RenderTargetGlintParameters = GetDefault<UGlintsSettings>()->GlintParametersTarget.LoadSynchronous();
    if (RenderTargetGlintParameters)
    {
        ComputeSceneViewExtension->SetGlintParametersTarget(RenderTargetGlintParameters);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Glint Parameters RT found"));
    }
    
}

void URenderTargetSubsystem::Deinitialize()
{
    Super::Deinitialize();
    ComputeSceneViewExtension.Reset();
    ComputeSceneViewExtension = nullptr;
}