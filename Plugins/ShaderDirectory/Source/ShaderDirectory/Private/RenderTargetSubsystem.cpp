// Fill out your copyright notice in the Description page of Project Settings.


#include "RenderTargetSubsystem.h"

#include "SceneViewExtension.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/ComputeSceneViewExtension.h"
#include "ShaderDirectory/GlintsSettings.h"

void URenderTargetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    GSupportsEfficientAsyncCompute = true;
    ComputeSceneViewExtension = FSceneViewExtensions::NewExtension<FComputeSceneViewExtension>();
    bUseAsync = GetDefault<UGlintsSettings>()->bStartAsAsync;
}

void URenderTargetSubsystem::Deinitialize()
{
    Super::Deinitialize();
    ComputeSceneViewExtension.Reset();
    ComputeSceneViewExtension = nullptr;
}

void URenderTargetSubsystem::SwitchAsync()
{
    bUseAsync = !bUseAsync;
}