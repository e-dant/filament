/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_DRIVER_VULKANCONTEXT_H
#define TNT_FILAMENT_DRIVER_VULKANCONTEXT_H

#include "VulkanPipelineCache.h"
#include "VulkanCommands.h"
#include "VulkanConstants.h"
#include "VulkanDisposer.h"

#include <backend/DriverEnums.h>

#include <utils/Condition.h>
#include <utils/Slice.h>
#include <utils/Mutex.h>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaPool)

namespace filament {
namespace backend {

struct VulkanRenderTarget;
struct VulkanSwapChain;
struct VulkanTexture;
class VulkanStagePool;

struct VulkanAttachment {
    VulkanTexture* texture = nullptr;
    uint8_t level = 0;
    uint16_t layer = 0;
    VkImage getImage() const;
    VkFormat getFormat() const;
    VkImageLayout getLayout() const;
    VkImageView getImageView(VkImageAspectFlags aspect);
};

struct VulkanTimestamps {
    VkQueryPool pool;
    utils::bitset32 used;
    utils::Mutex mutex;
};

struct VulkanRenderPass {
    VkRenderPass renderPass;
    RenderPassParams params;
    int currentSubpass;
};

// For now we only support a single-device, single-instance scenario. Our concept of "context" is a
// bundle of state containing the Device, the Instance, and various globally-useful Vulkan objects.
struct VulkanContext {
    void selectPhysicalDevice();
    void createLogicalDevice();
    uint32_t selectMemoryType(uint32_t flags, VkFlags reqs);
    VkFormat findSupportedFormat(utils::Slice<VkFormat> candidates, VkImageTiling tiling,
            VkFormatFeatureFlags features);
    void createEmptyTexture(VulkanStagePool& stagePool);

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkDevice device;
    VkCommandPool commandPool;
    VulkanTimestamps timestamps;
    uint32_t graphicsQueueFamilyIndex;
    VkQueue graphicsQueue;
    bool debugMarkersSupported = false;
    bool debugUtilsSupported = false;
    bool portabilitySubsetSupported = false;
    bool maintenanceSupported[3] = {};
    VulkanPipelineCache::RasterState rasterState;
    VulkanSwapChain* currentSurface;
    Handle<HwRenderTarget> defaultRenderTarget;
    VulkanRenderPass currentRenderPass;
    VkViewport viewport;
    VkFormat finalDepthFormat;
    VmaAllocator allocator;
    VmaPool vmaPoolGPU;
    VmaPool vmaPoolCPU;
    VulkanTexture* emptyTexture = nullptr;
    VulkanCommands* commands = nullptr;
    std::string currentDebugMarker;
};

} // namespace filament
} // namespace backend

#endif // TNT_FILAMENT_DRIVER_VULKANCONTEXT_H
