#ifdef CLAY_PLATFORM_ANDROID
// third party
#include "imgui_impl_android.h"
#include "imgui_impl_vulkan.h"
// clay
#include "clay/graphics/android/GraphicsContextAndroid.h"
// class
#include "clay/gui/android/ImGuiComponentAndroid.h"

namespace clay {

BaseGraphicsContext* ImGuiComponentAndroid::mpGraphics_ = nullptr;
VkDescriptorPool ImGuiComponentAndroid::mImguiDescriptorPool_ = VK_NULL_HANDLE;


void ImGuiComponentAndroid::initialize(android_app* app, BaseGraphicsContext* graphicsContext) {
    mpGraphics_ = graphicsContext;

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
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(mpGraphics_->getDevice(), &pool_info, nullptr, &mImguiDescriptorPool_);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //disable .ini file generations
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&font_cfg);
    io.IniFilename = nullptr;

    ImGui_ImplAndroid_Init(app->window);
    //ImGui_ImplGlfw_InitForVulkan(window.getGLFWWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mpGraphics_->getInstance();
    init_info.PhysicalDevice = mpGraphics_->mPhysicalDevice_;
    init_info.Device = mpGraphics_->getDevice();
    init_info.Queue = mpGraphics_->mGraphicsQueue_;
    init_info.DescriptorPool = mImguiDescriptorPool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = ((GraphicsContextAndroid*)mpGraphics_)->getMSAASamples();
    init_info.RenderPass = mpGraphics_->mRenderPass_;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
    ImGui::GetStyle().ScaleAllSizes(4.0f);
}

void ImGuiComponentAndroid::beginRender() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
}

void ImGuiComponentAndroid::endRender(VkCommandBuffer cmdBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

void ImGuiComponentAndroid::finalize() {
    // Wait for the device to be idle before destroying resources
    vkDeviceWaitIdle(mpGraphics_->getDevice());

    // Destroy ImGui Vulkan resources
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    // Destroy the descriptor pool
    if (mImguiDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(mpGraphics_->getDevice(), mImguiDescriptorPool_, nullptr);
        mImguiDescriptorPool_ = VK_NULL_HANDLE;
    }

    mpGraphics_ = nullptr;
}


} // namespace clay

#endif // CLAY_PLATFORM_ANDROID