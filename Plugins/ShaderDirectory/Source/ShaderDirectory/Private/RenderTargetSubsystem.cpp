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

    if (const auto Normal = GetDefault<UGlintsSettings>()->NormalSource.LoadSynchronous())
    {
        ComputeSceneViewExtension->SetNormalSource(Normal);
    }
    else
    {
        UE_LOG(LogTexture, Error, TEXT("No Normal Source ONE found"));
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