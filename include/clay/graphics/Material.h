#pragma once
// standard lib
#include <vector>

// third party
#include <vulkan/vulkan.h>

/**
  own a VkDescriptorSetLayout and VkPipeline

  for now try to render multiple meshes tied to materials (some shared)

 */

class Material {

public:



    VkPipeline mGraphicsPipeline_;

    std::vector<VkDescriptorSet> mDescriptorSets_;


};