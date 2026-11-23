#pragma once
// standard lib
#include <array>
#include <cstddef>
// third party
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace clay {

// Per-instance data for GPU-instanced MODEL draws. The render system writes one of
// these per visible object into a per-frame vertex buffer bound at binding 1
// (instance rate), and draws every object sharing a mesh + material in ONE call.
//
// A pipeline opts in by adding getBindingDescription() to
// PipelineLayoutInfo::extraVertexBindings and appending getAttributeDescriptions() to
// its attribute list; its vertex shader then reads
//     layout(location = 6)  in mat4 iModel;   // occupies locations 6..9
//     layout(location = 10) in vec4 iColor;
// instead of the { mat4 model; vec4 color; } push constant. The material built on that
// pipeline is registered with Material::setInstancedVariant() on the ordinary one.
struct InstanceData {
    glm::mat4 model;
    glm::vec4 color;

    static constexpr uint32_t kBinding = 1;        // Mesh::Vertex uses binding 0
    static constexpr uint32_t kFirstLocation = 6;  // Mesh::Vertex uses locations 0..5

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {
            .binding = kBinding,
            .stride = sizeof(InstanceData),
            .inputRate = vk::VertexInputRate::eInstance
        };
    }

    // A mat4 attribute takes four consecutive locations (one vec4 column each).
    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 5> attributes{};
        for (uint32_t column = 0; column < 4; ++column) {
            attributes[column].binding = kBinding;
            attributes[column].location = kFirstLocation + column;
            attributes[column].format = vk::Format::eR32G32B32A32Sfloat;
            attributes[column].offset = static_cast<uint32_t>(offsetof(InstanceData, model) + sizeof(glm::vec4) * column);
        }
        attributes[4].binding = kBinding;
        attributes[4].location = kFirstLocation + 4;
        attributes[4].format = vk::Format::eR32G32B32A32Sfloat;
        attributes[4].offset = static_cast<uint32_t>(offsetof(InstanceData, color));
        return attributes;
    }
};

} // namespace clay
