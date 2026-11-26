// The engine's UI bridge (clay::uibridge, see UiBridge.h), on RmlUi, with the engine's own
// Vulkan renderer for it. Games drive their permanent UI through UiComponentDesktop, which
// forwards here. A view is an .rml file in the game's UI folder; elements are found by id.
//
// Conventions the .rml / .rcss side must follow (see theme.rcss in a game's UI folder):
//   - a clickable control is a <button id="..."> : its id is queued for popCommand()
//   - setVisible() toggles the class "hidden"; setEnabled() toggles the class "disabled"
//   - setRangeValue() drives a <progress> (value) or an <input type="range">
//   - body has pointer-events: none so only real panels count as "the mouse is over the UI"
// Built for desktop, Android and XR (the folder name is historical).
#if (defined(CLAY_PLATFORM_DESKTOP) || defined(CLAY_PLATFORM_ANDROID) || defined(CLAY_PLATFORM_XR)) && defined(CLAY_ENABLE_RMLUI)

#include "clay/gui/desktop/UiBridge.h"
#include "clay/application/common/Resources.h"
#include "clay/graphics/common/BaseGraphicsContext.h"
#include "clay/utils/common/Logger.h"
#ifdef CLAY_PLATFORM_DESKTOP
#include "clay/utils/desktop/UtilsDesktop.h"
#endif

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace clay::uibridge {

namespace {

// Frames the GPU may still be working on (GraphicsContextDesktop / GraphicsContextAndroid
// both use 2): released resources wait this long (+1) before they are destroyed.
constexpr uint64_t kFramesInFlight = 2;

// Platform hooks (see UiBridge.h). Desktop: plain files; Android: the APK's assets.
std::function<utils::ImageData(const std::string&)> gImageLoader;
std::function<std::vector<std::string>(const std::string&)> gFileLister;
std::function<void(std::string&)> gViewTextFilter;

// ---- SPIR-V (compiled from rmlui_shaders/*.glsl with `glslc -mfmt=c`) -------------------------
const uint32_t kVertSpv[] =
#include "rmlui_shaders/RmlUi.vert.inc"
;
const uint32_t kFragSpv[] =
#include "rmlui_shaders/RmlUi.frag.inc"
;

struct PushConstants {
    glm::mat4 transform;
    glm::vec2 translate;
};

// =================================================================================================
// Vulkan renderer for RmlUi
// =================================================================================================
class Renderer final : public Rml::RenderInterface {
public:
    void init(BaseGraphicsContext* gc) {
        mGc_ = gc;
        vk::Device device = gc->getDevice();

        vk::DescriptorSetLayoutBinding binding = {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };
        mSetLayout_ = device.createDescriptorSetLayout({.bindingCount = 1, .pBindings = &binding});

        vk::DescriptorPoolSize poolSize = {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = kMaxTextures};
        mPool_ = device.createDescriptorPool({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = kMaxTextures,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        });

        mSampler_ = device.createSampler({
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .maxLod = 0.0f,
        });

        vk::PushConstantRange range = {.stageFlags = vk::ShaderStageFlagBits::eVertex, .offset = 0, .size = sizeof(PushConstants)};
        mPipelineLayout_ = device.createPipelineLayout({
            .setLayoutCount = 1, .pSetLayouts = &mSetLayout_,
            .pushConstantRangeCount = 1, .pPushConstantRanges = &range,
        });

        // 1x1 white texture for untextured geometry.
        const uint8_t white[4] = {255, 255, 255, 255};
        mWhite_ = createTexture(white, 1, 1);
    }

    void shutdown() {
        if (!mGc_) return;
        vk::Device device = mGc_->getDevice();
        device.waitIdle();
        clearDynamic();
        flushReleases(/*all*/ true);
        for (auto& [handle, tex] : mTextures_) destroyTexture(tex);
        mTextures_.clear();
        for (Chunk& c : mChunks_) {
            device.unmapMemory(c.memory);
            device.destroyBuffer(c.buffer, nullptr);
            device.freeMemory(c.memory, nullptr);
        }
        mChunks_.clear();
        if (mPipeline_) device.destroyPipeline(mPipeline_, nullptr);
        device.destroyPipelineLayout(mPipelineLayout_, nullptr);
        device.destroySampler(mSampler_, nullptr);
        device.destroyDescriptorPool(mPool_, nullptr);
        device.destroyDescriptorSetLayout(mSetLayout_, nullptr);
        mPipeline_ = nullptr;
        mGc_ = nullptr;
    }

    // Called inside the main render pass, before Rml::Context::Render().
    void beginFrame(vk::CommandBuffer cmd, vk::RenderPass renderPass, vk::SampleCountFlagBits samples) {
        flushReleases(false);
        ensurePipeline(renderPass, samples);
        mCmd_ = cmd;
        // The UI is drawn either straight onto the swapchain or into an offscreen target
        // (XR panels), whose size the owner sets with setTargetSize().
        if (targetWidth > 0 && targetHeight > 0) {
            mWidth_ = targetWidth;
            mHeight_ = targetHeight;
        } else {
            const auto dims = mGc_->getFrameDimensions();
            mWidth_ = static_cast<uint32_t>(dims.first);
            mHeight_ = static_cast<uint32_t>(dims.second);
        }
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline_);
        cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(mWidth_), static_cast<float>(mHeight_), 0.0f, 1.0f});
        mScissorEnabled_ = false;
        applyScissor();
        mHasTransform_ = false;
    }
    void endFrame() { mCmd_ = nullptr; }

    // A texture the app fills itself (e.g. a minimap); returned handle is usable as an <img> source
    // through the "dyn:<name>" scheme (see LoadTexture).
    Rml::TextureHandle createTexture(const uint8_t* rgba, int width, int height) {
        if (width <= 0 || height <= 0 || mTextures_.size() + 1 >= kMaxTextures) return 0;
        vk::Device device = mGc_->getDevice();
        const vk::DeviceSize size = static_cast<vk::DeviceSize>(width) * height * 4;

        vk::Buffer staging; vk::DeviceMemory stagingMemory;
        mGc_->createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                           staging, stagingMemory);
        void* mapped = device.mapMemory(stagingMemory, 0, size);
        std::memcpy(mapped, rgba, static_cast<size_t>(size));
        device.unmapMemory(stagingMemory);

        Texture tex;
        tex.width = width; tex.height = height;
        const vk::Format format = vk::Format::eR8G8B8A8Unorm;   // sRGB bytes; the shader linearises
        mGc_->createImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1, vk::SampleCountFlagBits::e1, format,
                          vk::ImageTiling::eOptimal,
                          vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                          vk::MemoryPropertyFlagBits::eDeviceLocal, tex.image, tex.memory);
        mGc_->transitionImageLayout(tex.image, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 1);
        mGc_->copyBufferToImage(staging, tex.image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        mGc_->transitionImageLayout(tex.image, format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 1);
        device.destroyBuffer(staging, nullptr);
        device.freeMemory(stagingMemory, nullptr);

        tex.view = mGc_->createImageView(tex.image, format, vk::ImageAspectFlagBits::eColor, 1);
        vk::DescriptorSetAllocateInfo alloc = {.descriptorPool = mPool_, .descriptorSetCount = 1, .pSetLayouts = &mSetLayout_};
        tex.set = device.allocateDescriptorSets(alloc)[0];
        vk::DescriptorImageInfo info = {.sampler = mSampler_, .imageView = tex.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
        vk::WriteDescriptorSet write = {
            .dstSet = tex.set, .dstBinding = 0, .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler, .pImageInfo = &info,
        };
        device.updateDescriptorSets(write, nullptr);

        const Rml::TextureHandle handle = ++mNextTexture_;
        mTextures_[handle] = tex;
        return handle;
    }

    // ---- app-filled images (e.g. a CPU-drawn minimap) -------------------------------------------
    // The image is shown through an <img src="dyn:<name>">. Each update goes into the next of
    // a few rotating GPU images (so one a frame in flight still samples is never written), and
    // is uploaded by flushDynamic() on the frame's command buffer, before the main pass.
    bool createDynamic(const std::string& name, int width, int height) {
        if (mDynamic_.count(name)) return true;
        Dynamic d;
        d.width = width; d.height = height;
        const vk::DeviceSize size = static_cast<vk::DeviceSize>(width) * height * 4;
        const std::vector<uint8_t> blank(static_cast<size_t>(size), 0);
        for (DynamicSlot& slot : d.slots) {
            slot.texture = createTexture(blank.data(), width, height);
            if (!slot.texture) return false;
            mGc_->createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
                               vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                               slot.staging, slot.stagingMemory);
            slot.mapped = static_cast<uint8_t*>(mGc_->getDevice().mapMemory(slot.stagingMemory, 0, size));
        }
        d.handle = ++mNextTexture_;
        mTextures_[d.handle] = mTextures_[d.slots[0].texture];   // what the <img> draws with (an alias of the current slot)
        d.pixels.resize(static_cast<size_t>(size));
        mDynamic_[name] = std::move(d);
        return true;
    }
    void updateDynamic(const std::string& name, const uint8_t* rgba, int width, int height) {
        auto it = mDynamic_.find(name);
        if (it == mDynamic_.end() || width != it->second.width || height != it->second.height) return;
        // Straight alpha in, premultiplied out.
        uint8_t* out = it->second.pixels.data();
        const size_t count = static_cast<size_t>(width) * height;
        for (size_t i = 0; i < count; ++i, rgba += 4, out += 4) {
            const unsigned a = rgba[3];
            out[0] = static_cast<uint8_t>(rgba[0] * a / 255);
            out[1] = static_cast<uint8_t>(rgba[1] * a / 255);
            out[2] = static_cast<uint8_t>(rgba[2] * a / 255);
            out[3] = static_cast<uint8_t>(a);
        }
        it->second.dirty = true;
    }
    // Outside any render pass, on the frame's command buffer.
    void flushDynamic(vk::CommandBuffer cmd) {
        for (auto& [name, d] : mDynamic_) {
            if (!d.dirty) continue;
            d.dirty = false;
            d.current = (d.current + 1) % kDynamicSlots;
            DynamicSlot& slot = d.slots[d.current];
            std::memcpy(slot.mapped, d.pixels.data(), d.pixels.size());
            const vk::Image image = mTextures_[slot.texture].image;
            const vk::ImageSubresourceRange range = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
            vk::ImageMemoryBarrier toWrite = {
                .srcAccessMask = vk::AccessFlagBits::eShaderRead, .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal, .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image, .subresourceRange = range,
            };
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, toWrite);
            vk::BufferImageCopy region = {
                .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
                .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                .imageOffset = vk::Offset3D{0, 0, 0},
                .imageExtent = vk::Extent3D{static_cast<uint32_t>(d.width), static_cast<uint32_t>(d.height), 1},
            };
            cmd.copyBufferToImage(slot.staging, image, vk::ImageLayout::eTransferDstOptimal, region);
            vk::ImageMemoryBarrier toRead = toWrite;
            toRead.srcAccessMask = vk::AccessFlagBits::eTransferWrite; toRead.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            toRead.oldLayout = vk::ImageLayout::eTransferDstOptimal; toRead.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, toRead);
            mTextures_[d.handle] = mTextures_[slot.texture];   // the <img> now draws this slot
        }
    }
    // Every dynamic image goes with the view that used it.
    void clearDynamic() {
        // (RmlUi keeps a texture per source name: make it forget these, or the next view's
        // "dyn:<name>" would get the handle of an image that is gone.)
        for (auto& [name, d] : mDynamic_) Rml::ReleaseTexture("dyn:" + name, this);
        for (auto& [name, d] : mDynamic_) destroyDynamic(d);
        mDynamic_.clear();
    }

    // ---- Rml::RenderInterface ----------------------------------------------------------------
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override {
        const uint32_t vbytes = static_cast<uint32_t>(vertices.size() * sizeof(Rml::Vertex));
        const uint32_t ibytes = static_cast<uint32_t>(indices.size() * sizeof(int));
        if (vbytes == 0 || ibytes == 0) return 0;
        auto* g = new Geometry();
        g->size = align4(vbytes) + align4(ibytes);
        if (!allocate(g->size, g->chunk, g->offset)) { delete g; return 0; }
        g->vertexOffset = g->offset;
        g->indexOffset = g->offset + align4(vbytes);
        g->indexCount = static_cast<uint32_t>(indices.size());
        uint8_t* base = mChunks_[g->chunk].mapped;
        std::memcpy(base + g->vertexOffset, vertices.data(), vbytes);
        std::memcpy(base + g->indexOffset, indices.data(), ibytes);
        return reinterpret_cast<Rml::CompiledGeometryHandle>(g);
    }

    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override {
        if (!mCmd_ || !geometry) return;
        const Geometry* g = reinterpret_cast<const Geometry*>(geometry);
        auto it = mTextures_.find(texture ? texture : mWhite_);
        if (it == mTextures_.end()) it = mTextures_.find(mWhite_);

        PushConstants pc;
        // Pixels -> Vulkan clip space (Y down), times the element's transform if it has one.
        glm::mat4 ortho(1.0f);
        ortho[0][0] = 2.0f / static_cast<float>(mWidth_);
        ortho[1][1] = 2.0f / static_cast<float>(mHeight_);
        ortho[2][2] = 0.0f;                       // UI is flat: everything at depth 0
        ortho[3] = glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f);
        pc.transform = mHasTransform_ ? ortho * mTransform_ : ortho;
        pc.translate = glm::vec2(translation.x, translation.y);

        const vk::Buffer buffer = mChunks_[g->chunk].buffer;
        const vk::DeviceSize vertexOffset = g->vertexOffset;
        mCmd_.bindVertexBuffers(0, 1, &buffer, &vertexOffset);
        mCmd_.bindIndexBuffer(buffer, g->indexOffset, vk::IndexType::eUint32);
        mCmd_.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout_, 0, 1, &it->second.set, 0, nullptr);
        mCmd_.pushConstants(mPipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(pc), &pc);
        mCmd_.drawIndexed(g->indexCount, 1, 0, 0, 0);
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
        // A command buffer still in flight may be drawing from it: free it a few frames on.
        if (geometry) mDeadGeometry_.push_back({mGc_->getFrameSerial(), reinterpret_cast<Geometry*>(geometry)});
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
        if (source.rfind("dyn:", 0) == 0) {   // an app-filled image (createDynamic)
            auto it = mDynamic_.find(source.substr(4));
            if (it == mDynamic_.end()) return 0;
            dimensions = Rml::Vector2i(it->second.width, it->second.height);
            return it->second.handle;
        }
        utils::ImageData img;
        try {
#ifdef CLAY_PLATFORM_DESKTOP
            img = gImageLoader ? gImageLoader(source) : utils::loadImageFileToMemory_desktop(std::filesystem::path(source));
#else
            if (gImageLoader) img = gImageLoader(source);
#endif
        } catch (const std::exception& e) {
            LOG_W("RmlUi: %s", e.what());
        }
        if (!img.pixels) { LOG_W("RmlUi: could not load image '%s'", source.c_str()); return 0; }
        if (img.channels == 3) utils::convertRGBtoRGBA(img);
        // Premultiply alpha (RmlUi's blending expects it).
        const size_t count = static_cast<size_t>(img.width) * img.height;
        uint8_t* p = img.pixels.get();
        for (size_t i = 0; i < count; ++i, p += 4) {
            const unsigned a = p[3];
            if (a == 255) continue;
            p[0] = static_cast<uint8_t>(p[0] * a / 255);
            p[1] = static_cast<uint8_t>(p[1] * a / 255);
            p[2] = static_cast<uint8_t>(p[2] * a / 255);
        }
        dimensions = Rml::Vector2i(img.width, img.height);
        return createTexture(img.pixels.get(), img.width, img.height);
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override {
        return createTexture(source.data(), dimensions.x, dimensions.y);
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        auto it = mTextures_.find(texture);
        if (it == mTextures_.end() || texture == mWhite_) return;
        for (auto& [name, d] : mDynamic_) if (d.handle == texture) return;   // owned by its dynamic image (clearDynamic)
        mDeadTextures_.push_back({mGc_->getFrameSerial(), it->second});
        mTextures_.erase(it);
    }

    void EnableScissorRegion(bool enable) override { mScissorEnabled_ = enable; applyScissor(); }
    void SetScissorRegion(Rml::Rectanglei region) override {
        mScissor_ = region;
        applyScissor();
    }
    void SetTransform(const Rml::Matrix4f* transform) override {
        mHasTransform_ = transform != nullptr;
        if (transform) std::memcpy(&mTransform_, transform->data(), sizeof(glm::mat4));
    }

private:
    static constexpr uint32_t kMaxTextures = 512;
    static constexpr uint32_t kChunkSize = 2u << 20;     // geometry lives in 2 MB host-visible buffers
    static constexpr uint64_t kReleaseDelay = kFramesInFlight + 1;

    struct Texture {
        vk::Image image; vk::DeviceMemory memory; vk::ImageView view; vk::DescriptorSet set;
        int width = 0, height = 0;
    };
    static constexpr int kDynamicSlots = kFramesInFlight + 1;
    struct DynamicSlot { Rml::TextureHandle texture = 0; vk::Buffer staging; vk::DeviceMemory stagingMemory; uint8_t* mapped = nullptr; };
    struct Dynamic {
        int width = 0, height = 0, current = 0;
        bool dirty = false;
        Rml::TextureHandle handle = 0;      // alias in mTextures_ of the current slot's texture
        DynamicSlot slots[kDynamicSlots];
        std::vector<uint8_t> pixels;        // the latest update, premultiplied, waiting for flushDynamic
    };
    void destroyDynamic(Dynamic& d) {
        vk::Device device = mGc_->getDevice();
        mTextures_.erase(d.handle);         // (an alias: the slots own the images)
        for (DynamicSlot& slot : d.slots) {
            auto it = mTextures_.find(slot.texture);
            if (it != mTextures_.end()) { mDeadTextures_.push_back({mGc_->getFrameSerial(), it->second}); mTextures_.erase(it); }
            if (slot.staging) {
                // (only written from the CPU and read by a copy recorded the same frame: wait that out)
                device.waitIdle();
                device.unmapMemory(slot.stagingMemory);
                device.destroyBuffer(slot.staging, nullptr);
                device.freeMemory(slot.stagingMemory, nullptr);
            }
        }
    }
    struct Geometry {
        int chunk = 0;
        uint32_t offset = 0, size = 0, vertexOffset = 0, indexOffset = 0, indexCount = 0;
    };
    struct Chunk {
        vk::Buffer buffer; vk::DeviceMemory memory; uint8_t* mapped = nullptr; uint32_t size = 0;
        std::vector<std::pair<uint32_t, uint32_t>> free;   // (offset, size), sorted by offset
    };

    static uint32_t align4(uint32_t n) { return (n + 3u) & ~3u; }

    bool allocate(uint32_t size, int& chunkOut, uint32_t& offsetOut) {
        for (int c = 0; c < static_cast<int>(mChunks_.size()); ++c) {
            auto& free = mChunks_[c].free;
            for (size_t i = 0; i < free.size(); ++i) {
                if (free[i].second < size) continue;
                chunkOut = c; offsetOut = free[i].first;
                free[i].first += size; free[i].second -= size;
                if (free[i].second == 0) free.erase(free.begin() + i);
                return true;
            }
        }
        Chunk chunk;
        chunk.size = std::max(kChunkSize, size);
        mGc_->createBuffer(chunk.size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                           chunk.buffer, chunk.memory);
        chunk.mapped = static_cast<uint8_t*>(mGc_->getDevice().mapMemory(chunk.memory, 0, chunk.size));
        if (chunk.size > size) chunk.free.push_back({size, chunk.size - size});
        mChunks_.push_back(std::move(chunk));
        chunkOut = static_cast<int>(mChunks_.size()) - 1; offsetOut = 0;
        return true;
    }
    void release(const Geometry& g) {
        auto& free = mChunks_[g.chunk].free;
        auto it = std::lower_bound(free.begin(), free.end(), std::make_pair(g.offset, 0u));
        it = free.insert(it, {g.offset, g.size});
        // merge with the ranges on either side
        if (it + 1 != free.end() && it->first + it->second == (it + 1)->first) { it->second += (it + 1)->second; free.erase(it + 1); }
        if (it != free.begin() && (it - 1)->first + (it - 1)->second == it->first) { (it - 1)->second += it->second; free.erase(it); }
    }
    void destroyTexture(Texture& tex) {
        vk::Device device = mGc_->getDevice();
        if (tex.set) device.freeDescriptorSets(mPool_, tex.set);
        device.destroyImageView(tex.view, nullptr);
        device.destroyImage(tex.image, nullptr);
        device.freeMemory(tex.memory, nullptr);
    }
    void flushReleases(bool all) {
        const uint64_t now = mGc_->getFrameSerial();
        while (!mDeadGeometry_.empty() && (all || now >= mDeadGeometry_.front().first + kReleaseDelay)) {
            release(*mDeadGeometry_.front().second);
            delete mDeadGeometry_.front().second;
            mDeadGeometry_.pop_front();
        }
        while (!mDeadTextures_.empty() && (all || now >= mDeadTextures_.front().first + kReleaseDelay)) {
            destroyTexture(mDeadTextures_.front().second);
            mDeadTextures_.pop_front();
        }
    }
    void applyScissor() {
        if (!mCmd_) return;
        vk::Rect2D rect;
        if (mScissorEnabled_) {
            const int x0 = std::clamp(mScissor_.Left(), 0, static_cast<int>(mWidth_));
            const int y0 = std::clamp(mScissor_.Top(), 0, static_cast<int>(mHeight_));
            const int x1 = std::clamp(mScissor_.Right(), x0, static_cast<int>(mWidth_));
            const int y1 = std::clamp(mScissor_.Bottom(), y0, static_cast<int>(mHeight_));
            rect.offset = vk::Offset2D{x0, y0};
            rect.extent = vk::Extent2D{static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)};
        } else {
            rect.offset = vk::Offset2D{0, 0};
            rect.extent = vk::Extent2D{mWidth_, mHeight_};
        }
        mCmd_.setScissor(0, rect);
    }

    void ensurePipeline(vk::RenderPass renderPass, vk::SampleCountFlagBits samples) {
        if (mPipeline_ && renderPass == mPipelinePass_ && samples == mPipelineSamples_) return;
        vk::Device device = mGc_->getDevice();
        if (mPipeline_) { device.waitIdle(); device.destroyPipeline(mPipeline_, nullptr); mPipeline_ = nullptr; }

        vk::ShaderModule vert = device.createShaderModule({.codeSize = sizeof(kVertSpv), .pCode = kVertSpv});
        vk::ShaderModule frag = device.createShaderModule({.codeSize = sizeof(kFragSpv), .pCode = kFragSpv});
        vk::PipelineShaderStageCreateInfo stages[2] = {
            {.stage = vk::ShaderStageFlagBits::eVertex, .module = vert, .pName = "main"},
            {.stage = vk::ShaderStageFlagBits::eFragment, .module = frag, .pName = "main"},
        };

        vk::VertexInputBindingDescription vbinding = {.binding = 0, .stride = sizeof(Rml::Vertex), .inputRate = vk::VertexInputRate::eVertex};
        vk::VertexInputAttributeDescription attrs[3] = {
            {.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = static_cast<uint32_t>(offsetof(Rml::Vertex, position))},
            {.location = 1, .binding = 0, .format = vk::Format::eR8G8B8A8Unorm, .offset = static_cast<uint32_t>(offsetof(Rml::Vertex, colour))},
            {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = static_cast<uint32_t>(offsetof(Rml::Vertex, tex_coord))},
        };
        vk::PipelineVertexInputStateCreateInfo vertexInput = {
            .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &vbinding,
            .vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attrs,
        };
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {.topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo viewport = {.viewportCount = 1, .scissorCount = 1};
        vk::PipelineRasterizationStateCreateInfo raster = {
            .polygonMode = vk::PolygonMode::eFill, .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eCounterClockwise, .lineWidth = 1.0f,
        };
        vk::PipelineMultisampleStateCreateInfo multisample = {.rasterizationSamples = samples};
        vk::PipelineDepthStencilStateCreateInfo depth = {.depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE};
        // Premultiplied alpha.
        vk::PipelineColorBlendAttachmentState blend = {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = vk::BlendFactor::eOne, .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne, .dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };
        vk::PipelineColorBlendStateCreateInfo blending = {.attachmentCount = 1, .pAttachments = &blend};
        vk::DynamicState dynamics[2] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamic = {.dynamicStateCount = 2, .pDynamicStates = dynamics};

        vk::GraphicsPipelineCreateInfo info = {
            .stageCount = 2, .pStages = stages,
            .pVertexInputState = &vertexInput, .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewport, .pRasterizationState = &raster,
            .pMultisampleState = &multisample, .pDepthStencilState = &depth,
            .pColorBlendState = &blending, .pDynamicState = &dynamic,
            .layout = mPipelineLayout_, .renderPass = renderPass, .subpass = 0,
        };
        mPipeline_ = device.createGraphicsPipeline(nullptr, info).value;
        mPipelinePass_ = renderPass; mPipelineSamples_ = samples;
        device.destroyShaderModule(vert, nullptr);
        device.destroyShaderModule(frag, nullptr);
    }

    BaseGraphicsContext* mGc_ = nullptr;
public:
    uint32_t targetWidth = 0;   // 0 = the swapchain size (see setTargetSize)
    uint32_t targetHeight = 0;
private:
    vk::DescriptorSetLayout mSetLayout_;
    vk::DescriptorPool mPool_;
    vk::Sampler mSampler_;
    vk::PipelineLayout mPipelineLayout_;
    vk::Pipeline mPipeline_;
    vk::RenderPass mPipelinePass_;
    vk::SampleCountFlagBits mPipelineSamples_ = vk::SampleCountFlagBits::e1;

    vk::CommandBuffer mCmd_;
    uint32_t mWidth_ = 1, mHeight_ = 1;
    bool mScissorEnabled_ = false;
    Rml::Rectanglei mScissor_;
    bool mHasTransform_ = false;
    glm::mat4 mTransform_{1.0f};

    std::vector<Chunk> mChunks_;
    std::unordered_map<Rml::TextureHandle, Texture> mTextures_;
    Rml::TextureHandle mNextTexture_ = 0;
    Rml::TextureHandle mWhite_ = 0;
    std::deque<std::pair<uint64_t, Geometry*>> mDeadGeometry_;
    std::deque<std::pair<uint64_t, Texture>> mDeadTextures_;
    std::unordered_map<std::string, Dynamic> mDynamic_;
};

// =================================================================================================
// System interface: the clock, the log, and absolute image paths
// =================================================================================================
class System final : public Rml::SystemInterface {
public:
    double time = 0.0;
    double GetElapsedTime() override { return time; }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT) { LOG_E("[RmlUi] %s", message.c_str()); }
        else if (type == Rml::Log::LT_WARNING) { LOG_W("[RmlUi] %s", message.c_str()); }
        else { LOG_I("[RmlUi] %s", message.c_str()); }
        return true;
    }
    // A path that is already absolute (C:/..., /...) is used as it is, not joined to the document's.
    void JoinPath(Rml::String& out, const Rml::String& documentPath, const Rml::String& path) override {
        if (path.rfind("dyn:", 0) == 0) { out = path; return; }   // an app-filled image, not a file
        if ((path.size() > 1 && path[1] == ':') || (!path.empty() && (path[0] == '/' || path[0] == '\\'))) { out = path; return; }
        Rml::SystemInterface::JoinPath(out, documentPath, path);
    }
};

// =================================================================================================
// File interface: views, style sheets and fonts read whole through the engine's file loader
// (clay::Resources) and served from memory. Always used on Android / XR, where they are APK
// assets; on desktop only when the app asks (setFilesFromLoader: resources packed into the
// executable), otherwise RmlUi opens the files itself.
// =================================================================================================
class Files final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override {
        try {
            utils::FileData data = Resources::getFileLoader()(path);
            auto* f = new File{std::move(data), 0};
            return reinterpret_cast<Rml::FileHandle>(f);
        } catch (const std::exception& e) {
            LOG_W("RmlUi: %s", e.what());
            return 0;
        }
    }
    void Close(Rml::FileHandle file) override { delete reinterpret_cast<File*>(file); }
    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override {
        auto* f = reinterpret_cast<File*>(file);
        const size_t n = std::min(size, f->data.size - f->pos);
        std::memcpy(buffer, f->data.data.get() + f->pos, n);
        f->pos += n;
        return n;
    }
    bool Seek(Rml::FileHandle file, long offset, int origin) override {
        auto* f = reinterpret_cast<File*>(file);
        long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? long(f->pos) : long(f->data.size);
        const long to = base + offset;
        if (to < 0 || size_t(to) > f->data.size) return false;
        f->pos = size_t(to);
        return true;
    }
    size_t Tell(Rml::FileHandle file) override { return reinterpret_cast<File*>(file)->pos; }

private:
    struct File {
        utils::FileData data;
        size_t pos;
    };
};
Files gFiles;
bool gFilesFromLoader = false;

// =================================================================================================
// Bridge state
// =================================================================================================
Renderer gRenderer;
System gSystem;
BaseGraphicsContext* gGc = nullptr;
bool gInitialized = false;
Rml::Context* gContext = nullptr;
Rml::ElementDocument* gDocument = nullptr;
std::string gUiRoot;          // <game>/res/ui
std::string gResourceRoot;    // <game>/res  (images are named relative to this, e.g. textures/icons.png)
vk::CommandBuffer gCmd;
std::deque<std::string> gCommands;
std::unordered_map<std::string, std::string> gCache;   // last value pushed per "<what>:<id>" (skips no-op updates)
void (*gHoverSfx)() = nullptr;
void (*gPressSfx)() = nullptr;
std::string gHoverButton;
std::string gPressedButton;   // id of the <button> the left mouse button went down on

std::string slashes(std::string s) { std::replace(s.begin(), s.end(), '\\', '/'); return s; }
std::string resourcePath(const char* relative) {
    const std::string r = relative ? relative : "";
    return gResourceRoot.empty() ? r : gResourceRoot + "/" + r;  // Android: asset paths are relative
}

Rml::Element* find(const char* id) {
    return (gDocument && id) ? gDocument->GetElementById(id) : nullptr;
}
// The nearest enabled <button> with an id at or above `e` (nullptr if there is none).
Rml::Element* buttonOf(Rml::Element* e) {
    for (; e; e = e->GetParentNode()) {
        if (e->GetTagName() == "button" && !e->GetId().empty()) return e->IsClassSet("disabled") ? nullptr : e;
    }
    return nullptr;
}
// True when the value for (what, id) is unchanged since it was last pushed.
bool unchanged(const char* what, const char* id, const std::string& value) {
    std::string key = std::string(what) + ":" + (id ? id : "");
    auto it = gCache.find(key);
    if (it != gCache.end() && it->second == value) return true;
    gCache[std::move(key)] = value;
    return false;
}
std::string rmlText(const char* utf8) {
    std::string out = Rml::StringUtilities::EncodeRml(utf8 ? utf8 : "");
    for (size_t at = 0; (at = out.find('\n', at)) != std::string::npos; ) out.replace(at, 1, "<br/>");
    return out;
}
std::string rgb(float r, float g, float b) {
    const auto c = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return "rgb(" + std::to_string(c(r)) + "," + std::to_string(c(g)) + "," + std::to_string(c(b)) + ")";
}
// <img> markup for one 128px tile of an atlas.
std::string tileImg(const char* atlasFile, int col, int row, const char* cls) {
    return std::string("<img class=\"") + cls + "\" src=\"" + resourcePath(atlasFile) + "\" rect=\""
         + std::to_string(col * 128) + " " + std::to_string(row * 128) + " 128 128\"/>";
}

// Clicks on buttons become commands; hovering / pressing one plays the UI sounds.
class Listener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        Rml::Element* button = buttonOf(event.GetTargetElement());
        switch (event.GetId()) {
            case Rml::EventId::Click:
                if (button) gCommands.push_back(button->GetId());
                break;
            case Rml::EventId::Mousedown:
                if (button && gPressSfx && event.GetParameter<int>("button", 0) == 0) gPressSfx();
                break;
            case Rml::EventId::Mouseover: {
                const std::string id = button ? button->GetId() : std::string();
                if (id != gHoverButton) { gHoverButton = id; if (!id.empty() && gHoverSfx) gHoverSfx(); }
                break;
            }
            default: break;
        }
    }
};
Listener gListener;

Rml::Input::KeyIdentifier keyFromGlfw(int key) {
    using namespace Rml::Input;
    switch (key) {
        case 257: case 335: return KI_RETURN;
        case 256: return KI_ESCAPE;
        case 258: return KI_TAB;
        case 259: return KI_BACK;
        case 261: return KI_DELETE;
        case 262: return KI_RIGHT;
        case 263: return KI_LEFT;
        case 264: return KI_DOWN;
        case 265: return KI_UP;
        case 268: return KI_HOME;
        case 269: return KI_END;
        default:  return KI_UNKNOWN;
    }
}

} // namespace

// =================================================================================================
// The bridge functions
// =================================================================================================
bool init(BaseGraphicsContext* graphics, const char* uiRoot) {
    if (gInitialized) return true;
    gGc = graphics;
    gUiRoot = slashes(uiRoot ? uiRoot : "");
    gResourceRoot = slashes(std::filesystem::path(gUiRoot).parent_path().string());

    gRenderer.init(gGc);
    Rml::SetSystemInterface(&gSystem);
    Rml::SetRenderInterface(&gRenderer);
#if defined(CLAY_PLATFORM_ANDROID) || defined(CLAY_PLATFORM_XR)
    gFilesFromLoader = true;
#endif
    if (gFilesFromLoader) Rml::SetFileInterface(&gFiles);
    if (!Rml::Initialise()) { LOG_E("RmlUi: Initialise failed"); return false; }

    const auto dims = graphics->getFrameDimensions();
    gContext = Rml::CreateContext("main", Rml::Vector2i(std::max(dims.first, 1), std::max(dims.second, 1)));
    if (!gContext) { LOG_E("RmlUi: could not create the context"); return false; }

    // Every .ttf / .otf in the UI folder.
    std::vector<std::string> files;
    if (gFileLister) {
        files = gFileLister(gUiRoot);
    } else {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(gUiRoot, ec)) files.push_back(slashes(entry.path().string()));
    }
    for (const std::string& file : files) {
        const std::string ext = std::filesystem::path(file).extension().string();
        if (ext != ".ttf" && ext != ".otf") continue;
        // Every font is also a fallback: it fills in the characters the font a text asks
        // for lacks (another alphabet's letters, or the Latin letters and digits inside a
        // text set in a font that only has another alphabet).
        Rml::LoadFontFace(file, true);
    }
    gInitialized = true;
    LOG_I("RmlUi %s: enabled (views from %s)", Rml::GetVersion().c_str(), gUiRoot.c_str());
    return true;
}

bool isInitialized() { return gInitialized; }

void setTargetSize(uint32_t width, uint32_t height) {
    gRenderer.targetWidth = width;
    gRenderer.targetHeight = height;
}

void setDpRatio(float ratio) {
    if (!gContext || ratio <= 0.0f) return;
    if (std::abs(gContext->GetDensityIndependentPixelRatio() - ratio) > 1e-4f) gContext->SetDensityIndependentPixelRatio(ratio);
}

void setImageLoader(std::function<utils::ImageData(const std::string&)> loader) { gImageLoader = std::move(loader); }
void setFileLister(std::function<std::vector<std::string>(const std::string&)> lister) { gFileLister = std::move(lister); }
void setViewTextFilter(std::function<void(std::string&)> filter) { gViewTextFilter = std::move(filter); }
void setFilesFromLoader(bool fromLoader) { gFilesFromLoader = fromLoader; }

namespace {

// A whole file through RmlUi's file interface (files on desktop, the APK's assets on Android).
bool readWholeFile(const std::string& path, std::string& out) {
    Rml::FileInterface* files = Rml::GetFileInterface();
    if (!files) return false;
    const Rml::FileHandle handle = files->Open(path);
    if (!handle) return false;
    out.resize(files->Length(handle));
    const size_t read = out.empty() ? 0 : files->Read(out.data(), out.size(), handle);
    files->Close(handle);
    out.resize(read);
    return true;
}

} // namespace

bool loadView(const char* viewFile, uint32_t width, uint32_t height) {
    if (!gInitialized || !viewFile) return false;
    unloadView();
    const std::string path = gUiRoot + "/" + viewFile;
#ifdef CLAY_PLATFORM_DESKTOP
    if (!gFilesFromLoader && !std::filesystem::exists(path)) { LOG_E("RmlUi: no such view %s", path.c_str()); return false; }
#endif
    gContext->SetDimensions(Rml::Vector2i(static_cast<int>(std::max(width, 1u)), static_cast<int>(std::max(height, 1u))));
    std::string rml;
    if (gViewTextFilter && readWholeFile(path, rml)) {
        gViewTextFilter(rml);
        // The view's address, for the relative paths in it. RmlUi reads a ':' there as the
        // end of a protocol name ("C:/..."); it writes drive letters with '|' itself.
        std::string url = path;
        std::replace(url.begin(), url.end(), ':', '|');
        gDocument = gContext->LoadDocumentFromMemory(rml, url);
    } else {
        gDocument = gContext->LoadDocument(path);
    }
    if (!gDocument) { LOG_E("RmlUi: failed to load %s", path.c_str()); return false; }
    gDocument->AddEventListener(Rml::EventId::Click, &gListener);
    gDocument->AddEventListener(Rml::EventId::Mousedown, &gListener);
    gDocument->AddEventListener(Rml::EventId::Mouseover, &gListener);
    gDocument->Show();
    gContext->Update();
    return true;
}

void unloadView() {
    gCache.clear();
    gCommands.clear();
    gHoverButton.clear();
    gPressedButton.clear();
    if (!gContext || !gDocument) return;
    gContext->UnloadDocument(gDocument);
    gDocument = nullptr;
    gContext->Update();   // the document is destroyed here
    gRenderer.clearDynamic();
}

bool hasView() { return gDocument != nullptr; }

void update(double timeSeconds, uint32_t width, uint32_t height) {
    gSystem.time = timeSeconds;
    if (!gContext) return;
    const Rml::Vector2i dims(static_cast<int>(std::max(width, 1u)), static_cast<int>(std::max(height, 1u)));
    if (gContext->GetDimensions() != dims) gContext->SetDimensions(dims);
    if (gDocument) gContext->Update();
}

void setCommandBuffer(void* commandBuffer) { gCmd = vk::CommandBuffer(static_cast<VkCommandBuffer>(commandBuffer)); }
void renderOffscreen() {
    if (gCmd && gDocument) gRenderer.flushDynamic(gCmd);   // app-filled images, before the main pass
}

void renderOnscreen(void* renderPass, uint32_t sampleCount) {
    if (!gContext || !gDocument || !gCmd) return;
    gRenderer.beginFrame(gCmd, vk::RenderPass(static_cast<VkRenderPass>(renderPass)),
                         static_cast<vk::SampleCountFlagBits>(sampleCount));
    gContext->Render();
    gRenderer.endFrame();
}

bool mouseMove(int x, int y) {
    if (!gContext || !gDocument) return false;
    gContext->ProcessMouseMove(x, y, 0);
    return gContext->IsMouseInteracting();
}
bool mouseButton(int x, int y, int button, bool down) {
    if (!gContext || !gDocument) return false;
    gContext->ProcessMouseMove(x, y, 0);
    // RmlUi only clicks when press and release land on the same *focus element*, and every
    // element (the label <span>s inside a button too) is its own focus element by default:
    // press on a button's text, drag a little onto its padding (or between its spans), and
    // the click was silently dropped. So a left press/release pair is matched here by the
    // <button> that owns each element instead, and the command is issued if RmlUi did not.
    if (button == 0 && down) {
        Rml::Element* pressed = buttonOf(gContext->GetHoverElement());
        gPressedButton = pressed ? pressed->GetId() : std::string();
    }
    const size_t commandsBefore = gCommands.size();
    if (down) gContext->ProcessMouseButtonDown(button, 0);
    else      gContext->ProcessMouseButtonUp(button, 0);
    if (button == 0 && !down) {
        // (The document can change during the release, e.g. a click handler loads a view:
        // only act when it is still the same press on a live document.)
        if (!gPressedButton.empty() && gDocument && gCommands.size() == commandsBefore) {
            Rml::Element* released = buttonOf(gContext->GetHoverElement());
            if (released && released->GetId() == gPressedButton) gCommands.push_back(gPressedButton);
        }
        gPressedButton.clear();
    }
    return gContext->IsMouseInteracting();
}
bool mouseWheel(int x, int y, int delta) {
    if (!gContext || !gDocument) return false;
    gContext->ProcessMouseMove(x, y, 0);
    // (RmlUi: positive scrolls down; the engine passes wheel deltas of +-120 per notch, positive = up.)
    return !gContext->ProcessMouseWheel(-static_cast<float>(delta) / 120.0f, 0);
}

bool scrollAt(int x, int y, float pixels) {
    if (!gContext || !gDocument) return false;
    Rml::Element* e = gContext->GetElementAtPoint(Rml::Vector2f(static_cast<float>(x), static_cast<float>(y)));
    // The nearest element at or above it that has more content than it shows.
    for (; e; e = e->GetParentNode()) {
        const float range = e->GetScrollHeight() - e->GetClientHeight();
        if (range <= 0.5f) continue;
        const Rml::Style::Overflow overflow = e->GetComputedValues().overflow_y();
        if (overflow != Rml::Style::Overflow::Auto && overflow != Rml::Style::Overflow::Scroll) continue;
        const float before = e->GetScrollTop();
        const float after = std::clamp(before + pixels, 0.0f, range);
        if (after == before) return false;  // at its end
        e->SetScrollTop(after);
        return true;
    }
    return false;
}

void setUiSoundCallbacks(void (*hover)(), void (*press)()) { gHoverSfx = hover; gPressSfx = press; }

bool popCommand(char* out, int cap) {
    if (gCommands.empty() || !out || cap <= 0) return false;
    std::strncpy(out, gCommands.front().c_str(), static_cast<size_t>(cap) - 1);
    out[cap - 1] = '\0';
    gCommands.pop_front();
    return true;
}

void setText(const char* name, const char* utf8) {
    Rml::Element* e = find(name);
    if (!e) return;
    std::string rml = rmlText(utf8);
    if (unchanged("text", name, rml)) return;
    e->SetInnerRML(rml);
}

void setInnerRml(const char* name, const char* rml) {
    Rml::Element* e = find(name);
    if (!e || !rml) return;
    if (unchanged("rml", name, rml)) return;
    e->SetInnerRML(rml);
}
bool getToggle(const char* name, bool* out) {
    Rml::Element* e = find(name);
    if (!e || !out) return false;
    *out = e->HasAttribute("checked");
    return true;
}
void setToggle(const char* name, bool value) {
    Rml::Element* e = find(name);
    if (!e) return;
    if (value) e->SetAttribute("checked", ""); else e->RemoveAttribute("checked");
}

bool getRangeValue(const char* name, float* out) {
    Rml::Element* e = find(name);
    if (!e || !out) return false;
    if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(e)) { *out = static_cast<float>(std::atof(control->GetValue().c_str())); return true; }
    *out = e->GetAttribute<float>("value", 0.0f);
    return true;
}
void setRangeValue(const char* name, float value) {
    Rml::Element* e = find(name);
    if (!e) return;
    if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(e)) {
        // A slider the user may have moved: only write a value that differs from what it shows.
        if (std::abs(static_cast<float>(std::atof(control->GetValue().c_str())) - value) > 0.0005f) control->SetValue(std::to_string(value));
        return;
    }
    char text[32]; std::snprintf(text, sizeof(text), "%.4f", value);
    if (unchanged("range", name, text)) return;
    e->SetAttribute("value", value);
}

void clearList(const char* panelName) {
    if (Rml::Element* panel = find(panelName)) panel->SetInnerRML("");
}
static void appendRow(const char* panelName, const char* cmdName, const std::string& classes, const std::string& inner) {
    Rml::Element* panel = find(panelName);
    if (!panel || !gDocument) return;
    Rml::ElementPtr row = gDocument->CreateElement("button");
    row->SetId(cmdName ? cmdName : "");
    row->SetClassNames(classes);
    row->SetInnerRML(inner);
    panel->AppendChild(std::move(row));
}
void addListRow(const char* panelName, const char* cmdName, const char* text,
                bool hasColor, float r, float g, float b, bool selected) {
    std::string inner;
    if (hasColor) inner += "<span class=\"swatch\" style=\"background-color: " + rgb(r, g, b) + ";\"></span>";
    inner += "<span class=\"row-text\">" + rmlText(text) + "</span>";
    appendRow(panelName, cmdName, selected ? "list-row selected" : "list-row", inner);
}
void addCraftRow(const char* panelName, const char* cmdName, const char* atlasFile,
                 int outCol, int outRow, const char* outName,
                 const int* ingCols, const int* ingRows, const int* ingCounts, int ingN, bool craftable) {
    std::string inner = tileImg(atlasFile, outCol, outRow, "row-icon")
                      + "<span class=\"row-text\">" + rmlText(outName) + "</span><span class=\"row-arrow\">&lt;-</span>";
    for (int i = 0; i < ingN; ++i)
        inner += tileImg(atlasFile, ingCols[i], ingRows[i], "row-icon small")
               + "<span class=\"row-count\">" + std::to_string(ingCounts[i]) + "</span>";
    appendRow(panelName, cmdName, craftable ? "craft-row" : "craft-row unavailable", inner);
}
void addIconRow(const char* panelName, const char* cmdName, const char* atlasFile, int col, int row, const char* text) {
    appendRow(panelName, cmdName, "icon-row",
              tileImg(atlasFile, col, row, "row-icon") + "<span class=\"row-text\">" + rmlText(text) + "</span>");
}

bool getEditText(const char* name, char* out, int cap) {
    auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(find(name));
    if (!control || !out || cap <= 0) return false;
    std::strncpy(out, control->GetValue().c_str(), static_cast<size_t>(cap) - 1);
    out[cap - 1] = '\0';
    return true;
}
void setEditText(const char* name, const char* text) {
    if (auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(find(name))) control->SetValue(text ? text : "");
}

void setProperty(const char* name, const char* property, const char* value) {
    Rml::Element* e = find(name);
    if (!e || !property) return;
    const std::string v = value ? value : "";
    if (unchanged(property, name, v)) return;
    if (v.empty()) e->RemoveProperty(property);
    else e->SetProperty(property, v);
}

void setElementColor(const char* name, float r, float g, float b) {
    Rml::Element* e = find(name);
    if (!e) return;
    const std::string colour = rgb(r, g, b);
    if (unchanged("colour", name, colour)) return;
    e->SetProperty("background-color", colour);
}

void setIconTile(const char* imageName, const char* atlasFile, int x, int y, int w, int h) {
    Rml::Element* e = find(imageName);
    if (!e) return;
    const std::string rect = std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(w) + " " + std::to_string(h);
    const std::string key = (w > 0 && atlasFile) ? std::string(atlasFile) + "|" + rect : std::string();
    if (unchanged("tile", imageName, key)) return;
    if (key.empty()) { e->SetClass("empty", true); return; }   // no tile: the theme hides an empty image
    e->SetClass("empty", false);
    e->SetAttribute("src", resourcePath(atlasFile));
    e->SetAttribute("rect", rect);
}

void setVisible(const char* name, bool visible) {
    Rml::Element* e = find(name);
    if (!e || unchanged("visible", name, visible ? "1" : "0")) return;
    e->SetClass("hidden", !visible);
}
void setEnabled(const char* name, bool enabled) {
    Rml::Element* e = find(name);
    if (!e || unchanged("enabled", name, enabled ? "1" : "0")) return;
    e->SetClass("disabled", !enabled);
}
void setMargin(const char* name, float l, float t, float /*r*/, float /*b*/) {
    Rml::Element* e = find(name);
    if (!e) return;
    const std::string left = std::to_string(static_cast<int>(l)) + "px", top = std::to_string(static_cast<int>(t)) + "px";
    if (unchanged("margin", name, left + "," + top)) return;
    e->SetProperty("margin-left", left);
    e->SetProperty("margin-top", top);
}

bool createDynamicImage(const char* name, uint32_t width, uint32_t height) {
    Rml::Element* e = find(name);
    if (!e || !name || !gRenderer.createDynamic(name, static_cast<int>(width), static_cast<int>(height))) return false;
    e->SetClass("empty", false);
    e->SetAttribute("src", std::string("dyn:") + name);
    return true;
}
void updateDynamicImage(const char* name, const void* rgba, uint32_t width, uint32_t height) {
    if (name && rgba) gRenderer.updateDynamic(name, static_cast<const uint8_t*>(rgba), static_cast<int>(width), static_cast<int>(height));
}

bool hitTestButton(int x, int y, char* out, int cap) {
    if (!gContext || !gDocument || !out || cap <= 0) return false;
    Rml::Element* button = buttonOf(gContext->GetElementAtPoint(Rml::Vector2f(static_cast<float>(x), static_cast<float>(y))));
    if (!button) return false;
    std::strncpy(out, button->GetId().c_str(), static_cast<size_t>(cap) - 1);
    out[cap - 1] = '\0';
    return true;
}

bool keyDown(int glfwKey) {
    const Rml::Input::KeyIdentifier key = keyFromGlfw(glfwKey);
    return gContext && gDocument && key != Rml::Input::KI_UNKNOWN && !gContext->ProcessKeyDown(key, 0);
}
bool keyUp(int glfwKey) {
    const Rml::Input::KeyIdentifier key = keyFromGlfw(glfwKey);
    return gContext && gDocument && key != Rml::Input::KI_UNKNOWN && !gContext->ProcessKeyUp(key, 0);
}
bool charInput(unsigned int codepoint) {
    if (!gContext || !gDocument || codepoint < 32 || codepoint == 127) return false;
    return !gContext->ProcessTextInput(static_cast<Rml::Character>(codepoint));
}
void focusElement(const char* name) {
    if (Rml::Element* e = find(name)) e->Focus();
}

void shutdown() {
    if (!gInitialized) return;
    if (gGc) gGc->getDevice().waitIdle();
    unloadView();
    Rml::Shutdown();
    gContext = nullptr;
    gRenderer.shutdown();
    gInitialized = false;
}

} // namespace clay::uibridge

#endif // (CLAY_PLATFORM_DESKTOP || CLAY_PLATFORM_ANDROID || CLAY_PLATFORM_XR) && CLAY_ENABLE_RMLUI
