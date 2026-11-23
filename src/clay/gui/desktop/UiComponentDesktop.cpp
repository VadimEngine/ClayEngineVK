#ifdef CLAY_PLATFORM_DESKTOP
#include "clay/gui/desktop/UiComponentDesktop.h"
#include "clay/gui/desktop/UiBridge.h"
#include "clay/graphics/desktop/GraphicsContextDesktop.h"

namespace clay {

BaseGraphicsContext* UiComponentDesktop::mpGraphics_ = nullptr;

void UiComponentDesktop::initialize(Window& /*window*/, BaseGraphicsContext* graphics,
                                        const char* uiRoot) {
    mpGraphics_ = graphics;
    uibridge::init(graphics, uiRoot);
}

bool UiComponentDesktop::isAvailable() { return uibridge::isInitialized(); }
void UiComponentDesktop::setDpRatio(float ratio) { uibridge::setDpRatio(ratio); }

bool UiComponentDesktop::loadView(const char* viewFile) {
    if (!mpGraphics_) return false;
    auto dims = mpGraphics_->getFrameDimensions();
    return uibridge::loadView(viewFile, (uint32_t)dims.first, (uint32_t)dims.second);
}

void UiComponentDesktop::unloadView() { uibridge::unloadView(); }
bool UiComponentDesktop::hasView() { return uibridge::hasView(); }

void UiComponentDesktop::update(double timeSeconds) {
    if (!mpGraphics_) return;
    auto dims = mpGraphics_->getFrameDimensions();
    uibridge::update(timeSeconds, (uint32_t)dims.first, (uint32_t)dims.second);
}

void UiComponentDesktop::renderOffscreen(vk::CommandBuffer cmdBuffer) {
    if (!uibridge::hasView()) return;
    uibridge::setCommandBuffer((void*)(VkCommandBuffer)cmdBuffer);
    uibridge::renderOffscreen();
}

void UiComponentDesktop::render(vk::CommandBuffer cmdBuffer) {
    if (!uibridge::hasView() || !mpGraphics_) return;
    auto* gc = static_cast<GraphicsContextDesktop*>(mpGraphics_);
    uint32_t samples = static_cast<uint32_t>(gc->getMSAASamples());
    uibridge::setCommandBuffer((void*)(VkCommandBuffer)cmdBuffer);   // the UI draws straight into this pass
    uibridge::renderOnscreen((void*)(VkRenderPass)mpGraphics_->mRenderPass_, samples);
}

bool UiComponentDesktop::onMouseMove(int x, int y) { return uibridge::mouseMove(x, y); }
bool UiComponentDesktop::onMouseButton(int x, int y, int button, bool down) {
    return uibridge::mouseButton(x, y, button, down);
}
bool UiComponentDesktop::onMouseWheel(int x, int y, int delta) {
    return uibridge::mouseWheel(x, y, delta);
}
bool UiComponentDesktop::scrollAt(int x, int y, float pixels) { return uibridge::scrollAt(x, y, pixels); }
void UiComponentDesktop::setViewTextFilter(std::function<void(std::string&)> filter) { uibridge::setViewTextFilter(std::move(filter)); }
void UiComponentDesktop::setUiSoundCallbacks(void (*hover)(), void (*press)()) {
    uibridge::setUiSoundCallbacks(hover, press);
}

bool UiComponentDesktop::popCommand(std::string& out) {
    char buf[128];
    if (!uibridge::popCommand(buf, sizeof(buf))) return false;
    out = buf;
    return true;
}

void UiComponentDesktop::setText(const char* name, const std::string& text) {
    uibridge::setText(name, text.c_str());
}
bool UiComponentDesktop::getToggle(const char* name, bool& out) {
    return uibridge::getToggle(name, &out);
}
void UiComponentDesktop::setToggle(const char* name, bool value) {
    uibridge::setToggle(name, value);
}
bool UiComponentDesktop::getRangeValue(const char* name, float& out) {
    return uibridge::getRangeValue(name, &out);
}
void UiComponentDesktop::setRangeValue(const char* name, float value) {
    uibridge::setRangeValue(name, value);
}
void UiComponentDesktop::clearList(const char* panelName) {
    uibridge::clearList(panelName);
}
void UiComponentDesktop::addListRow(const char* panelName, const char* cmdName,
                                        const char* text, bool hasColor,
                                        float r, float g, float b, bool selected) {
    uibridge::addListRow(panelName, cmdName, text, hasColor, r, g, b, selected);
}
void UiComponentDesktop::addCraftRow(const char* panelName, const char* cmdName,
                                         const char* atlasFile, int outCol, int outRow,
                                         const char* outName, const int* ingCols,
                                         const int* ingRows, const int* ingCounts,
                                         int ingN, bool craftable) {
    uibridge::addCraftRow(panelName, cmdName, atlasFile, outCol, outRow, outName,
                              ingCols, ingRows, ingCounts, ingN, craftable);
}
void UiComponentDesktop::addIconRow(const char* panelName, const char* cmdName,
                                        const char* atlasFile, int col, int row, const char* text) {
    uibridge::addIconRow(panelName, cmdName, atlasFile, col, row, text);
}
bool UiComponentDesktop::getEditText(const char* name, std::string& out) {
    char buf[256];
    if (!uibridge::getEditText(name, buf, sizeof(buf))) return false;
    out = buf;
    return true;
}
void UiComponentDesktop::setEditText(const char* name, const std::string& text) {
    uibridge::setEditText(name, text.c_str());
}
void UiComponentDesktop::setProperty(const char* name, const char* property, const std::string& value) {
    uibridge::setProperty(name, property, value.c_str());
}
void UiComponentDesktop::setElementColor(const char* name, float r, float g, float b) {
    uibridge::setElementColor(name, r, g, b);
}
void UiComponentDesktop::setIconTile(const char* imageName, const char* atlasFile,
                                         int x, int y, int w, int h) {
    uibridge::setIconTile(imageName, atlasFile, x, y, w, h);
}
void UiComponentDesktop::setVisible(const char* name, bool visible) {
    uibridge::setVisible(name, visible);
}
void UiComponentDesktop::setEnabled(const char* name, bool enabled) {
    uibridge::setEnabled(name, enabled);
}
void UiComponentDesktop::setMargin(const char* name, float l, float t, float r, float b) {
    uibridge::setMargin(name, l, t, r, b);
}
bool UiComponentDesktop::createDynamicImage(const char* name, uint32_t width, uint32_t height) {
    return uibridge::createDynamicImage(name, width, height);
}
void UiComponentDesktop::updateDynamicImage(const char* name, const void* rgba,
                                                uint32_t width, uint32_t height) {
    uibridge::updateDynamicImage(name, rgba, width, height);
}
bool UiComponentDesktop::hitTestButton(int x, int y, std::string& out) {
    char buf[64];
    if (!uibridge::hitTestButton(x, y, buf, sizeof(buf))) return false;
    out = buf;
    return true;
}
bool UiComponentDesktop::keyDown(int glfwKey) { return uibridge::keyDown(glfwKey); }
bool UiComponentDesktop::keyUp(int glfwKey) { return uibridge::keyUp(glfwKey); }
bool UiComponentDesktop::charInput(unsigned int codepoint) { return uibridge::charInput(codepoint); }
void UiComponentDesktop::focusElement(const char* name) { uibridge::focusElement(name); }

void UiComponentDesktop::finalize() {
    uibridge::shutdown();
    mpGraphics_ = nullptr;
}

} // namespace clay

#endif // CLAY_PLATFORM_DESKTOP
