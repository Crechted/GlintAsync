// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GlintsSettings.generated.h"

/**
 * 
 */
UCLASS(Config=Game, defaultconfig, meta = (DisplayName="Glints"))
class SHADERDIRECTORY_API UGlintsSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RenderTargets")
    TSoftObjectPtr<UTextureRenderTarget2D> NormalOneRenderTarget;
    
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RenderTargets")
    TSoftObjectPtr<UTextureRenderTarget2D> NormalTwoRenderTarget;
    
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RenderTargets")
    TSoftObjectPtr<UTextureRenderTarget2D> NormalSource = nullptr;
    
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RenderTargets")
    TSoftObjectPtr<UTextureRenderTarget2D> GlintParametersTarget = nullptr;
    
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "RenderTargets")
    TSoftObjectPtr<UTextureRenderTarget2D> OutputSomeTextureTarget = nullptr;
    
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Parameters")
    FVector3f SigmasRho;
};
