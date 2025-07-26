#ifdef CLAY_PLATFORM_DESKTOP
// third party
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// clay
#include "clay/graphics/desktop/GraphicsContextDesktop.h"
// class
#include "clay/gui/desktop/ImGuiComponentDesktop.h"

namespace clay {

BaseGraphicsContext* ImGuiComponentDesktop::mpGraphics_ = nullptr;
vk::DescriptorPool ImGuiComponentDesktop::mImguiDescriptorPool_ = nullptr;

void ImGuiComponentDesktop::initialize(Window& window, BaseGraphicsContext* graphicsContext) {
    mpGraphics_ = graphicsContext;

    std::array<vk::DescriptorPoolSize, 11> poolSizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000}
    };

    vk::DescriptorPoolCreateInfo pool_info = {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    mImguiDescriptorPool_ = mpGraphics_->getDevice().createDescriptorPool(pool_info);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //disable .ini file generations
    io.IniFilename = nullptr;

    ImGui_ImplGlfw_InitForVulkan(window.getGLFWWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = mpGraphics_->getInstance(),
        .PhysicalDevice = mpGraphics_->mPhysicalDevice_,
        .Device = mpGraphics_->getDevice(),
        .Queue = ((GraphicsContextDesktop*)mpGraphics_)->mGraphicsQueue_,
        .DescriptorPool = mImguiDescriptorPool_,
        .RenderPass = mpGraphics_->mRenderPass_,
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = (VkSampleCountFlagBits)((GraphicsContextDesktop*)mpGraphics_)->getMSAASamples(),
    };

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiComponentDesktop::beginRender() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiComponentDesktop::endRender(vk::CommandBuffer cmdBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

void ImGuiComponentDesktop::finalize() {
    // Wait for the device to be idle before destroying resources
    mpGraphics_->getDevice().waitIdle();

    // Destroy ImGui Vulkan resources
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Destroy the descriptor pool
    if (mImguiDescriptorPool_ != nullptr) {
        mpGraphics_->getDevice().destroyDescriptorPool(mImguiDescriptorPool_);
        mImguiDescriptorPool_ = nullptr;
    }

    mpGraphics_ = nullptr;
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP