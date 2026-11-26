#ifdef CLAY_PLATFORM_XR

// class
#include "clay/gui/xr/UiComponentXR.h"
// standard lib
#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>
// third party
#include <android/asset_manager.h>
#include <android/imagedecoder.h>
// clay
#include "clay/graphics/xr/PanelRenderTargetXR.h"
#include "clay/gui/desktop/UiBridge.h"
#include "clay/utils/common/Logger.h"

namespace clay {

namespace {

// Heap-held and never destroyed at process exit (the Vulkan device may already be gone
// then): finalize() releases it.
PanelRenderTargetXR& target() {
    static PanelRenderTargetXR* t = new PanelRenderTargetXR();
    return *t;
}
bool gAvailable = false;
bool gActive = true;
std::chrono::steady_clock::time_point gStart;

utils::ImageData decodeAsset(AAssetManager* assets, const std::string& path) {
    const std::unique_ptr<AAsset, void (*)(AAsset*)> asset(
        AAssetManager_open(assets, path.c_str(), AASSET_MODE_STREAMING), &AAsset_close);
    if (!asset) throw std::runtime_error("no such asset: " + path);
    AImageDecoder* rawDecoder = nullptr;
    if (AImageDecoder_createFromAAsset(asset.get(), &rawDecoder) != ANDROID_IMAGE_DECODER_SUCCESS) {
        throw std::runtime_error("cannot decode: " + path);
    }
    const std::unique_ptr<AImageDecoder, void (*)(AImageDecoder*)> decoder(rawDecoder, &AImageDecoder_delete);
    AImageDecoder_setAndroidBitmapFormat(decoder.get(), ANDROID_BITMAP_FORMAT_RGBA_8888);
    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder.get());
    utils::ImageData img{};
    img.width = AImageDecoderHeaderInfo_getWidth(info);
    img.height = AImageDecoderHeaderInfo_getHeight(info);
    img.channels = 4;
    const size_t stride = static_cast<size_t>(img.width) * 4;
    const size_t size = stride * static_cast<size_t>(img.height);
    img.pixels = std::make_unique<unsigned char[]>(size);
    if (AImageDecoder_decodeImage(decoder.get(), img.pixels.get(), stride, size) != ANDROID_IMAGE_DECODER_SUCCESS) {
        throw std::runtime_error("decode failed: " + path);
    }
    return img;
}

} // namespace

bool UiComponentXR::initialize(android_app* app, BaseGraphicsContext* graphics, const char* uiRoot,
                               uint32_t width, uint32_t height) {
    if (gAvailable) return true;
    AAssetManager* assets = app->activity->assetManager;
    // Fonts: the files in an asset folder.
    uibridge::setFileLister([assets](const std::string& dir) {
        std::vector<std::string> out;
        if (AAssetDir* d = AAssetManager_openDir(assets, dir.c_str())) {
            while (const char* name = AAssetDir_getNextFileName(d)) out.push_back(dir + "/" + name);
            AAssetDir_close(d);
        }
        return out;
    });
    // <img> sources: decoded from the assets as RGBA.
    uibridge::setImageLoader([assets](const std::string& path) { return decodeAsset(assets, path); });

    if (!uibridge::init(graphics, uiRoot)) {
        LOG_W("UiComponentXR: RmlUi not available (engine built without CLAY_ENABLE_RMLUI?)");
        return false;
    }
    target().create(*graphics, width, height, {0.0f, 0.0f, 0.0f, 0.0f}); // transparent background
    uibridge::setTargetSize(width, height);
    gStart = std::chrono::steady_clock::now();
    gAvailable = true;
    return true;
}

bool UiComponentXR::isAvailable() { return gAvailable; }

void UiComponentXR::finalize() {
    if (!gAvailable) return;
    uibridge::unloadView();
    uibridge::shutdown();
    target().destroy();
    gAvailable = false;
}

void UiComponentXR::setDpRatio(float ratio) { uibridge::setDpRatio(ratio); }

bool UiComponentXR::loadView(const char* viewFile) {
    if (!gAvailable) return false;
    return uibridge::loadView(viewFile, target().getWidth(), target().getHeight());
}
void UiComponentXR::unloadView() { uibridge::unloadView(); }
bool UiComponentXR::hasView() { return gAvailable && uibridge::hasView(); }

vk::ImageView UiComponentXR::getImageView() { return target().getSampledView(); }
uint32_t UiComponentXR::getWidth() { return target().getWidth(); }
uint32_t UiComponentXR::getHeight() { return target().getHeight(); }

void UiComponentXR::setActive(bool active) { gActive = active; }
bool UiComponentXR::isActive() { return gActive; }

void UiComponentXR::render(vk::CommandBuffer cmd) {
    if (!hasView() || !gActive) return;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();
    uibridge::update(now, target().getWidth(), target().getHeight());
    uibridge::setCommandBuffer(static_cast<VkCommandBuffer>(cmd));
    uibridge::renderOffscreen();               // app-filled images, outside the pass
    target().begin(cmd);
    uibridge::renderOnscreen(static_cast<VkRenderPass>(target().getRenderPass()), 1);
    target().end(cmd);
}

bool UiComponentXR::onMouseMove(int x, int y) { return uibridge::mouseMove(x, y); }
bool UiComponentXR::onMouseButton(int x, int y, int button, bool down) {
    return uibridge::mouseButton(x, y, button, down);
}
void UiComponentXR::onMouseLeave() {
    // Far outside the panel: nothing is hovered any more.
    uibridge::mouseMove(-100000, -100000);
}
bool UiComponentXR::scrollAt(int x, int y, float pixels) { return uibridge::scrollAt(x, y, pixels); }

bool UiComponentXR::popCommand(std::string& out) {
    char buffer[256];
    if (!uibridge::popCommand(buffer, sizeof(buffer))) return false;
    out = buffer;
    return true;
}

void UiComponentXR::setText(const char* id, const std::string& text) { uibridge::setText(id, text.c_str()); }
void UiComponentXR::setInnerRml(const char* id, const std::string& rml) { uibridge::setInnerRml(id, rml.c_str()); }
bool UiComponentXR::getToggle(const char* id, bool& out) { return uibridge::getToggle(id, &out); }
void UiComponentXR::setToggle(const char* id, bool value) { uibridge::setToggle(id, value); }
bool UiComponentXR::getRangeValue(const char* id, float& out) { return uibridge::getRangeValue(id, &out); }
void UiComponentXR::setRangeValue(const char* id, float value) { uibridge::setRangeValue(id, value); }
void UiComponentXR::setProperty(const char* id, const char* property, const std::string& value) {
    uibridge::setProperty(id, property, value.c_str());
}
void UiComponentXR::setVisible(const char* id, bool visible) { uibridge::setVisible(id, visible); }
void UiComponentXR::setEnabled(const char* id, bool enabled) { uibridge::setEnabled(id, enabled); }

} // namespace clay

#endif // CLAY_PLATFORM_XR
