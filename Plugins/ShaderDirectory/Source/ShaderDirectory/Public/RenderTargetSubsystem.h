// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "RenderTargetSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class SHADERDIRECTORY_API URenderTargetSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()
private:
    TSharedPtr<class FComputeSceneViewExtension, ESPMode::ThreadSafe> ComputeSceneViewExtension;
    
    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> RenderTargetNormalOne = nullptr;
    TObjectPtr<UTextureRenderTarget2D> RenderTargetNormalTwo = nullptr;

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
};
