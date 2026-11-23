#ifdef CLAY_PLATFORM_ANDROID
#include "clay/gui/android/UiComponentAndroid.h"
#include "clay/gui/desktop/UiBridge.h"
#include "clay/graphics/android/GraphicsContextAndroid.h"
#include <android/asset_manager.h>
#include <android/imagedecoder.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace clay {

BaseGraphicsContext* UiComponentAndroid::mpGraphics_ = nullptr;

void UiComponentAndroid::initialize(android_app* app, BaseGraphicsContext* graphics,
                                    const char* uiRoot) {
    mpGraphics_ = graphics;
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
    uibridge::setImageLoader([assets](const std::string& path) {
        // Closed / deleted on every way out (the errors below throw).
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
    });
    uibridge::init(graphics, uiRoot);
}

bool UiComponentAndroid::isAvailable() { return uibridge::isInitialized(); }
void UiComponentAndroid::setDpRatio(float ratio) { uibridge::setDpRatio(ratio); }

bool UiComponentAndroid::loadView(const char* viewFile) {
    if (!mpGraphics_) return false;
    auto dims = mpGraphics_->getFrameDimensions();
    return uibridge::loadView(viewFile, static_cast<uint32_t>(dims.first), static_cast<uint32_t>(dims.second));
}

void UiComponentAndroid::unloadView() { uibridge::unloadView(); }
bool UiComponentAndroid::hasView() { return uibridge::hasView(); }

void UiComponentAndroid::update(double timeSeconds) {
    if (!mpGraphics_) return;
    auto dims = mpGraphics_->getFrameDimensions();
    uibridge::update(timeSeconds, static_cast<uint32_t>(dims.first), static_cast<uint32_t>(dims.second));
}

void UiComponentAndroid::renderOffscreen(vk::CommandBuffer cmdBuffer) {
    if (!uibridge::hasView()) return;
    uibridge::setCommandBuffer(static_cast<VkCommandBuffer>(cmdBuffer));
    uibridge::renderOffscreen();
}

void UiComponentAndroid::render(vk::CommandBuffer cmdBuffer) {
    if (!uibridge::hasView() || !mpGraphics_) return;
    auto* gc = static_cast<GraphicsContextAndroid*>(mpGraphics_);
    uint32_t samples = static_cast<uint32_t>(gc->getMSAASamples());
    uibridge::setCommandBuffer(static_cast<VkCommandBuffer>(cmdBuffer));   // the UI draws straight into this pass
    uibridge::renderOnscreen(static_cast<VkRenderPass>(mpGraphics_->mRenderPass_), samples);
}

bool UiComponentAndroid::onMouseMove(int x, int y) { return uibridge::mouseMove(x, y); }
bool UiComponentAndroid::onMouseButton(int x, int y, int button, bool down) {
    return uibridge::mouseButton(x, y, button, down);
}
bool UiComponentAndroid::onMouseWheel(int x, int y, int delta) {
    return uibridge::mouseWheel(x, y, delta);
}
bool UiComponentAndroid::scrollAt(int x, int y, float pixels) { return uibridge::scrollAt(x, y, pixels); }
void UiComponentAndroid::setViewTextFilter(std::function<void(std::string&)> filter) { uibridge::setViewTextFilter(std::move(filter)); }
void UiComponentAndroid::setUiSoundCallbacks(void (*hover)(), void (*press)()) {
    uibridge::setUiSoundCallbacks(hover, press);
}

bool UiComponentAndroid::popCommand(std::string& out) {
    char buf[128];
    if (!uibridge::popCommand(buf, sizeof(buf))) return false;
    out = buf;
    return true;
}

void UiComponentAndroid::setText(const char* name, const std::string& text) {
    uibridge::setText(name, text.c_str());
}
bool UiComponentAndroid::getToggle(const char* name, bool& out) {
    return uibridge::getToggle(name, &out);
}
void UiComponentAndroid::setToggle(const char* name, bool value) {
    uibridge::setToggle(name, value);
}
bool UiComponentAndroid::getRangeValue(const char* name, float& out) {
    return uibridge::getRangeValue(name, &out);
}
void UiComponentAndroid::setRangeValue(const char* name, float value) {
    uibridge::setRangeValue(name, value);
}
void UiComponentAndroid::clearList(const char* panelName) {
    uibridge::clearList(panelName);
}
void UiComponentAndroid::addListRow(const char* panelName, const char* cmdName,
                                        const char* text, bool hasColor,
                                        float r, float g, float b, bool selected) {
    uibridge::addListRow(panelName, cmdName, text, hasColor, r, g, b, selected);
}
void UiComponentAndroid::addCraftRow(const char* panelName, const char* cmdName,
                                         const char* atlasFile, int outCol, int outRow,
                                         const char* outName, const int* ingCols,
                                         const int* ingRows, const int* ingCounts,
                                         int ingN, bool craftable) {
    uibridge::addCraftRow(panelName, cmdName, atlasFile, outCol, outRow, outName,
                              ingCols, ingRows, ingCounts, ingN, craftable);
}
void UiComponentAndroid::addIconRow(const char* panelName, const char* cmdName,
                                        const char* atlasFile, int col, int row, const char* text) {
    uibridge::addIconRow(panelName, cmdName, atlasFile, col, row, text);
}
bool UiComponentAndroid::getEditText(const char* name, std::string& out) {
    char buf[256];
    if (!uibridge::getEditText(name, buf, sizeof(buf))) return false;
    out = buf;
    return true;
}
void UiComponentAndroid::setEditText(const char* name, const std::string& text) {
    uibridge::setEditText(name, text.c_str());
}
void UiComponentAndroid::setProperty(const char* name, const char* property, const std::string& value) {
    uibridge::setProperty(name, property, value.c_str());
}
void UiComponentAndroid::setElementColor(const char* name, float r, float g, float b) {
    uibridge::setElementColor(name, r, g, b);
}
void UiComponentAndroid::setIconTile(const char* imageName, const char* atlasFile,
                                         int x, int y, int w, int h) {
    uibridge::setIconTile(imageName, atlasFile, x, y, w, h);
}
void UiComponentAndroid::setVisible(const char* name, bool visible) {
    uibridge::setVisible(name, visible);
}
void UiComponentAndroid::setEnabled(const char* name, bool enabled) {
    uibridge::setEnabled(name, enabled);
}
void UiComponentAndroid::setMargin(const char* name, float l, float t, float r, float b) {
    uibridge::setMargin(name, l, t, r, b);
}
bool UiComponentAndroid::createDynamicImage(const char* name, uint32_t width, uint32_t height) {
    return uibridge::createDynamicImage(name, width, height);
}
void UiComponentAndroid::updateDynamicImage(const char* name, const void* rgba,
                                                uint32_t width, uint32_t height) {
    uibridge::updateDynamicImage(name, rgba, width, height);
}
bool UiComponentAndroid::hitTestButton(int x, int y, std::string& out) {
    char buf[64];
    if (!uibridge::hitTestButton(x, y, buf, sizeof(buf))) return false;
    out = buf;
    return true;
}
bool UiComponentAndroid::keyDown(int glfwKey) { return uibridge::keyDown(glfwKey); }
bool UiComponentAndroid::keyUp(int glfwKey) { return uibridge::keyUp(glfwKey); }
bool UiComponentAndroid::charInput(unsigned int codepoint) { return uibridge::charInput(codepoint); }
void UiComponentAndroid::focusElement(const char* name) { uibridge::focusElement(name); }

void UiComponentAndroid::finalize() {
    uibridge::shutdown();
    mpGraphics_ = nullptr;
}

} // namespace clay

#endif // CLAY_PLATFORM_ANDROID
