#ifdef CLAY_PLATFORM_XR

#include "clay/gui/xr/ImGuiComponentXR.h"
#include "clay/graphics/xr/GraphicsContextXR.h"

// class

namespace clay {

IGraphicsContext* ImGuiComponentXR::gGraphicsContext_ = nullptr;
VkDescriptorPool ImGuiComponentXR::gImguiDescriptorPool_{};



ImGuiComponentXR::ImGuiComponentXR() {}

void ImGuiComponentXR::initialize(ANativeWindow* pWindow) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(gGraphicsContext_->mDevice_, &pool_info, nullptr, &gImguiDescriptorPool_);

    //this initializes the core structures of imgui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //disable .ini file generations
    io.IniFilename = nullptr;

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = gGraphicsContext_->mInstance_;
    init_info.PhysicalDevice = gGraphicsContext_->mPhysicalDevice_;
    init_info.Device = gGraphicsContext_->mDevice_;
    init_info.Queue = gGraphicsContext_->mQueue_;
    init_info.DescriptorPool = gImguiDescriptorPool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.RenderPass = ((clay::GraphicsContextXR*)gGraphicsContext_)->imguiRenderPass;
    // init_info.QueueFamily = gGraphicsContext_->mGraphicsQueueFamilyIndex_;

    ImGui_ImplAndroid_Init(pWindow);
    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiComponentXR::deinitialize() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiComponentXR::startRender() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
}

void ImGuiComponentXR::buildImGui() {}

void ImGuiComponentXR::endRender() {
    ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiComponentXR::render() {
    startRender();
    buildImGui();
    endRender();
}

} // namespace clay

#endif // CLAY_PLATFORM_XR