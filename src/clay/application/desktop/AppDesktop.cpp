#ifdef CLAY_PLATFORM_DESKTOP

// third party
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
// classAppDesktop
#include "clay/application/desktop/AppDesktop.h"

namespace clay {

AppDesktop::AppDesktop() :
    mWindow_(),
    mGraphicsContext_(mWindow_),
    mResources_(mGraphicsContext_), // TODO this needs a valid mGraphicsContext_?
    mSceneBuffer_{{nullptr, nullptr}} {
        
    initVulkan();
    initImgui();
}

AppDesktop::~AppDesktop() {}

void AppDesktop::run() {
    mainLoop();
    cleanup();
}

void AppDesktop::setScene(BaseScene* pScene) {
    //mpScene_ = pScene;
    // assign to back buffer TODO should check if back buffer is already populated
    mSceneBuffer_[1] = pScene;
}

Window& AppDesktop::getWindow() {
    return mWindow_;
}

Resources& AppDesktop::getResources() {
    return mResources_;
}

GraphicsContextDesktop& AppDesktop::getGraphicsContext() {
    return mGraphicsContext_;
}

void AppDesktop::quit() {
    glfwSetWindowShouldClose(mWindow_.getGLFWWindow(), true);
}

void AppDesktop::initVulkan() {
    createCommandBuffers();
}

void AppDesktop::initImgui() {
    VkDescriptorPoolSize pool_sizes[] =
    {
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

    vkCreateDescriptorPool(mGraphicsContext_.mDevice_, &pool_info, nullptr, &mImguiDescriptorPool_);

    //this initializes the core structures of imgui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //disable .ini file generations
    io.IniFilename = nullptr;

    //this initializes imgui for SDL
    ImGui_ImplGlfw_InitForVulkan(mWindow_.getGLFWWindow(), true);

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mGraphicsContext_.mInstance_;
    init_info.PhysicalDevice = mGraphicsContext_.mPhysicalDevice_;
    init_info.Device = mGraphicsContext_.mDevice_;
    init_info.Queue = mGraphicsContext_.mGraphicsQueue_;
    init_info.DescriptorPool = mImguiDescriptorPool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = mGraphicsContext_.mMSAASamples_;
    init_info.RenderPass = mGraphicsContext_.mRenderPass_;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}

void AppDesktop::mainLoop() {
    while (mWindow_.isRunning()) {
        std::chrono::duration<float> dt = (std::chrono::steady_clock::now() - mLastTime_);
        mLastTime_ = std::chrono::steady_clock::now();

        if (mSceneBuffer_[1] != nullptr) {
            // move back buffer to main buffer
            delete mSceneBuffer_[0];
            mSceneBuffer_[0] = mSceneBuffer_[1];
            mSceneBuffer_[1] = nullptr;
        }

        mWindow_.update(dt.count());
        // TODO check if null
        mSceneBuffer_[0]->update(dt.count());
        drawFrame();
    }

    vkDeviceWaitIdle(mGraphicsContext_.mDevice_);
}

void AppDesktop::createCommandBuffers() {
    mCommandBuffers_.resize(GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mGraphicsContext_.mCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)mCommandBuffers_.size();

    if (vkAllocateCommandBuffers(mGraphicsContext_.mDevice_, &allocInfo, mCommandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void AppDesktop::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mGraphicsContext_.mRenderPass_;
    renderPassInfo.framebuffer = mGraphicsContext_.mSwapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mGraphicsContext_.mSwapChainExtent_;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)mGraphicsContext_.mSwapChainExtent_.width;
        viewport.height = (float)mGraphicsContext_.mSwapChainExtent_.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = mGraphicsContext_.mSwapChainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        mSceneBuffer_[0]->render(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void AppDesktop::drawFrame() {
    vkWaitForFences(mGraphicsContext_.mDevice_, 1, &mGraphicsContext_.mInFlightFences_[mGraphicsContext_.mCurrentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        mGraphicsContext_.mDevice_, 
        mGraphicsContext_.mSwapChain_, 
        UINT64_MAX,
        mGraphicsContext_.mImageAvailableSemaphores_[mGraphicsContext_.mCurrentFrame_],
        VK_NULL_HANDLE, 
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        mGraphicsContext_.recreateSwapChain(mWindow_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(mGraphicsContext_.mDevice_, 1, &mGraphicsContext_.mInFlightFences_[mGraphicsContext_.mCurrentFrame_]);

    vkResetCommandBuffer(mCommandBuffers_[mGraphicsContext_.mCurrentFrame_], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(mCommandBuffers_[mGraphicsContext_.mCurrentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mGraphicsContext_.mImageAvailableSemaphores_[mGraphicsContext_.mCurrentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers_[mGraphicsContext_.mCurrentFrame_];

    VkSemaphore signalSemaphores[] = {mGraphicsContext_.mRenderFinishedSemaphores_[mGraphicsContext_.mCurrentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mGraphicsContext_.mGraphicsQueue_, 1, &submitInfo, mGraphicsContext_.mInFlightFences_[mGraphicsContext_.mCurrentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {mGraphicsContext_.mSwapChain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mGraphicsContext_.mPresentQueue_, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFramebufferResized_) {
        mFramebufferResized_ = false;
        mGraphicsContext_.recreateSwapChain(mWindow_);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    mGraphicsContext_.mCurrentFrame_ = (mGraphicsContext_.mCurrentFrame_ + 1) % GraphicsContextDesktop::MAX_FRAMES_IN_FLIGHT;
}

void AppDesktop::cleanup() {
    mGraphicsContext_.cleanUp();
}

AudioManager& AppDesktop::getAudioManager() {
    return mAudioManger_;
}


} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP