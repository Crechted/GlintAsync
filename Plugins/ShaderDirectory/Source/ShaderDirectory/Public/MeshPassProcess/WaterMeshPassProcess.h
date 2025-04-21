#pragma once
#include "CoreMinimal.h"
#include "MeshPassProcessor.h"
#include "StaticMeshSceneProxy.h"
#include "WaterMeshPassProcess.generated.h"

class UStaticMeshComponent;
class FStaticMeshSceneProxy;
class UCustomWaterMeshComponent;

class FCustomWaterMeshSceneProxy : public FStaticMeshSceneProxy
{
public:
    // simply inherits from static mesh scene proxy
    FCustomWaterMeshSceneProxy(UCustomWaterMeshComponent* InComponent)
        : FStaticMeshSceneProxy(Cast<UStaticMeshComponent>(InComponent), false)
    {
    }

    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap,
        FMeshElementCollector& Collector) const override;


    virtual FPrimitiveViewRelevance GetViewRelevance(
        const FSceneView* View) const override;


    virtual SIZE_T GetTypeHash() const override;

    static SIZE_T WaterTypeHash();

    //virtual uint32 GetMemoryFootprint(void) const override { return (sizeof(*this) + GetAllocatedSize()); }
protected:
    bool SetupMesh(int32 LODIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup,
        bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const;
};

UCLASS(Blueprintable, ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility),
    editinlinenew, meta=(BlueprintSpawnableComponent))
class UCustomWaterMeshComponent : public UStaticMeshComponent
{
public:
    GENERATED_BODY()

    // create proxy for custom terrain mesh component
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override
    {
        // null static mesh check
        if (GetStaticMesh() == nullptr || GetStaticMesh()->GetRenderData() == nullptr)
        {
            return nullptr;
        }

        // LOD check
        const FStaticMeshLODResourcesArray& LODResources = GetStaticMesh()->GetRenderData()->LODResources;
        if (LODResources.Num() == 0 || LODResources[FMath::Clamp<int32>(GetStaticMesh()->GetMinLOD().Default, 0, LODResources.Num() - 1)].
                                       VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
        {
            return nullptr;
        }
        LLM_SCOPE(ELLMTag::StaticMesh);

        FCustomWaterMeshSceneProxy* Proxy = ::new FCustomWaterMeshSceneProxy(this);
        return Proxy;
    }
};


class FWaterMeshPassProcessor : public FMeshPassProcessor
{
public:
    FWaterMeshPassProcessor(const FScene* InScene
        , ERHIFeatureLevel::Type InFeatureLevel
        , const FSceneView* InViewIfDynamicMeshCommand
        , const FMeshPassProcessorRenderState& InDrawRenderState
        , FMeshPassDrawListContext* InDrawListContext);

    virtual void AddMeshBatch(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* PrimitiveSceneProxy,
        int32 StaticMeshId) override;

    FMeshPassProcessorRenderState PassDrawRenderState;
};