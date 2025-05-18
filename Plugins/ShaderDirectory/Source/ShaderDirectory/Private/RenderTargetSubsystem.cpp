// Fill out your copyright notice in the Description page of Project Settings.


#include "RenderTargetSubsystem.h"

#include <vulkan_core.h>

#include "ID3D12DynamicRHI.h"
#include "IVulkanDynamicRHI.h"
#include "SceneViewExtension.h"
#include "Rendering/ComputeSceneViewExtension.h"
#include "Runtime/VulkanRHI/Private/VulkanRHIPrivate.h"
#include "ShaderDirectory/GlintsSettings.h"

int32 GAllowAsyncComputeDif = 1;
static FAutoConsoleVariableRef CVarAllowAsyncComputeDif(
    TEXT("r.D3D12.AllowAsyncCompute"),
    GAllowAsyncComputeDif,
    TEXT("Allow usage of async compute"),
    ECVF_ReadOnly | ECVF_RenderThreadSafe
    );

void URenderTargetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    bUseAsync = GetDefault<UGlintsSettings>()->bStartAsAsync;
    GSupportsEfficientAsyncCompute = IsAsyncComputeSupported();

    ComputeSceneViewExtension = FSceneViewExtensions::NewExtension<FComputeSceneViewExtension>();
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

bool URenderTargetSubsystem::IsAsyncComputeSupported()
{
    if (GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12) return IsD3D12AsyncComputeSupported();
    if (GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Vulkan) return false;
    //IsVulkanAsyncComputeSupported(); - пока в UE5.3 async на Vulkan не поддерживается
    return false;
}

bool URenderTargetSubsystem::IsD3D12AsyncComputeSupported()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 options = {};
    ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
    if (!D3D12RHI) return false;

    ID3D12Device* D3DDevice = D3D12RHI->RHIGetDevice(0);
    if (!D3DDevice) return false;

    HRESULT Result = D3DDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &options, sizeof(options));

    bool bnVidiaAsyncComputeSupported = false;
    if (IsRHIDeviceNVIDIA() && Result == S_OK && options.CopyQueueTimestampQueriesSupported
        && options.WriteBufferImmediateSupportFlags != D3D12_COMMAND_LIST_SUPPORT_FLAG_NONE)
    {
        bnVidiaAsyncComputeSupported = true;
    }

    return GAllowAsyncComputeDif && (FParse::Param(FCommandLine::Get(), TEXT("ForceAsyncCompute"))
                                     || (GRHISupportsParallelRHIExecute && (IsRHIDeviceAMD() || bnVidiaAsyncComputeSupported)));
}

bool URenderTargetSubsystem::IsVulkanAsyncComputeSupported()
{
    IVulkanDynamicRHI* IVulkanRHI = GetDynamicRHI<IVulkanDynamicRHI>();
    if (!IVulkanRHI) return false;

    uint32_t count = 0;
    VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(IVulkanRHI->RHIGetVkPhysicalDevice(), &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(IVulkanRHI->RHIGetVkPhysicalDevice(), &count, properties.data());

    uint32_t GraphicsQueueFamilyIndex = 0;
    uint32_t ComputeQueueFamilyIndex = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            GraphicsQueueFamilyIndex = i;
        }
        if ((properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
        {
            ComputeQueueFamilyIndex = i;
        }
    }

    VkPhysicalDeviceProperties DeviceProperties;
    VulkanRHI::vkGetPhysicalDeviceProperties(IVulkanRHI->RHIGetVkPhysicalDevice(), &DeviceProperties);
    const bool bIsSupported =
        (RHIConvertToGpuVendorId(DeviceProperties.vendorID) == EGpuVendorId::Nvidia ||
         RHIConvertToGpuVendorId(DeviceProperties.vendorID) == EGpuVendorId::Amd) &&
        GraphicsQueueFamilyIndex != ComputeQueueFamilyIndex;

    return FParse::Param(FCommandLine::Get(), TEXT("ForceAsyncCompute")) || bIsSupported;
}