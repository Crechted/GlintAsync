#include "MeshPassProcess/WaterMeshPassProcess.h"

#include "MeshPassProcessor.inl"
#include "ScenePrivate.h"
#include "Materials/MaterialRenderProxy.h"
#include "ShaderPasses/WaterShaders.h"

void FCustomWaterMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
    uint32 VisibilityMap, FMeshElementCollector& Collector) const

{
    FStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

    /*for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
    {
        const FSceneView* View = Views[ViewIndex];
        if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)))
        {
            if (auto ViewInfo = const_cast<FViewInfo*>(static_cast<const FViewInfo*>(View)))
            {
                FLODMask LODMask = GetLODMask(View);
                for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++)
                {
                    if (LODMask.ContainsLOD(LODIndex) && LODIndex >= ClampedMinLOD)
                    {
                        // Создание FMeshBatch
                        FMeshBatch& Mesh = Collector.AllocateMesh();
                        auto MaterialRenderProxy = new FColoredMaterialRenderProxy(
                            GEngine->WireframeMaterial->GetRenderProxy(),
                            FLinearColor::White);

                        Collector.RegisterOneFrameMaterialProxy(MaterialRenderProxy);
                        if (SetupMesh(LODIndex, MaterialRenderProxy, SDPG_World, true, Mesh))
                        {
                            Mesh.bCanApplyViewModeOverrides = false;
                            Collector.AddMesh(ViewIndex, Mesh);
                            FMeshBatchAndRelevance BatchAndRelevance(Mesh, this, Collector.GetFeatureLevel());
                            ViewInfo->DynamicMeshElements.Add(BatchAndRelevance);
                            UE_LOG(LogTemp, Display, TEXT("Dynamic Mesh Added %d"), ViewInfo->DynamicMeshElements.Num());
                        }

                    }
                }
            }
        }
    }*/
}

SIZE_T FCustomWaterMeshSceneProxy::GetTypeHash() const
{
    return WaterTypeHash();
}

SIZE_T FCustomWaterMeshSceneProxy::WaterTypeHash()
{
    return 4221;
}

FPrimitiveViewRelevance FCustomWaterMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
    FPrimitiveViewRelevance Result = FStaticMeshSceneProxy::GetViewRelevance(View);

    /*Result.bDrawRelevance = IsShown(View);
    Result.bDynamicRelevance = true;
    Result.bRenderInMainPass = false;*/
    return Result;
}

bool FCustomWaterMeshSceneProxy::SetupMesh(int32 LODIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup,
    bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
    const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
    const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
    const FLODInfo& ProxyLODInfo = LODs[LODIndex];
    const FVertexFactory* VertexFactory = nullptr;

    FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];

    if (ProxyLODInfo.OverrideColorVertexBuffer)
    {
        VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;

        OutBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
    }
    else
    {
        VertexFactory = &VFs.VertexFactory;

        OutBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
    }

    const bool bWireframe = false;
    const bool bUseReversedIndices = false;
    const bool bDitheredLODTransition = false;

    OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
    OutMeshBatch.CastShadow = false;
    OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
    OutMeshBatch.MaterialRenderProxy = WireframeRenderProxy;

    OutBatchElement.MinVertexIndex = 0;
    OutBatchElement.MaxVertexIndex = LODModel.GetNumVertices() - 1;

    const uint32_t NumPrimitives = SetMeshElementGeometrySource(LODIndex, 0, bWireframe, bUseReversedIndices, bAllowPreCulledIndices,
        VertexFactory, OutMeshBatch);
    SetMeshElementScreenSize(LODIndex, bDitheredLODTransition, OutMeshBatch);

    return NumPrimitives > 0;
}

// FMeshPassProcessor interface
FWaterMeshPassProcessor::FWaterMeshPassProcessor(const FScene* InScene
    , ERHIFeatureLevel::Type InFeatureLevel
    , const FSceneView* InViewIfDynamicMeshCommand
    , const FMeshPassProcessorRenderState& InDrawRenderState
    , FMeshPassDrawListContext* InDrawListContext)
    : FMeshPassProcessor(InScene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
    PassDrawRenderState = InDrawRenderState;
}

void FWaterMeshPassProcessor::AddMeshBatch(const FMeshBatch& MeshBatch, uint64 BatchElementMask,
    const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 StaticMeshId)
{
    // add mesh batch for custom terrain pass

    // check fallback material, and decide the material render proxy
    const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
    const FMaterial& MaterialResource = MeshBatch.MaterialRenderProxy->
                                                  GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);
    const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr
                                                          ? *FallbackMaterialRenderProxyPtr
                                                          : *MeshBatch.MaterialRenderProxy;

    // get render state
    FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

    // setup pass shaders
    const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
    TMeshProcessorShaders<
        FWaterSimulatePassVS,
        FWaterSimulatePassPS> CustomTerrainPassShaders;

    CustomTerrainPassShaders.VertexShader = MaterialResource.GetShader<FWaterSimulatePassVS>(VertexFactory->GetType());
    CustomTerrainPassShaders.PixelShader = MaterialResource.GetShader<FWaterSimulatePassPS>(VertexFactory->GetType());

    // fill mode and cull mode
    const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
    const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MaterialResource, OverrideSettings);
    const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MaterialResource, OverrideSettings);

    // sort key, for now just use default one
    FMeshDrawCommandSortKey SortKey = FMeshDrawCommandSortKey::Default;

    // init shader element data, for now just use default one
    FMeshMaterialShaderElementData ShaderElementData;
    ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

    BuildMeshDrawCommands(
        MeshBatch,
        BatchElementMask,
        PrimitiveSceneProxy,
        MaterialRenderProxy,
        MaterialResource,
        DrawRenderState,
        CustomTerrainPassShaders,
        MeshFillMode,
        MeshCullMode,
        SortKey,
        EMeshPassFeatures::Default,
        ShaderElementData);
}


/*
// this create terrain pass processor
FMeshPassProcessor* CreateCustomTerrainPassProcessor(ERHIFeatureLevel::Type FeatureLevel,const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
    // for now, just init like base pass
    // but enable depth pass
    FMeshPassProcessorRenderState PassDrawRenderState;
    PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
    PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
    PassDrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
 
    return new(FMemStack::Get()) FWaterMeshPassProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}
 
// register custom terrain pass
FRegisterPassProcessorCreateFunction RegisterCustomTerrainPass(&CreateCustomTerrainPassProcessor, EShadingPath::Deferred, EMeshPass::BasePass, EMeshPassFlags::MainView);
*/