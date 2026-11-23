// The UI bridge without a UI library: built when CLAY_ENABLE_RMLUI is off, so games that
// have no RmlUi interface still link. Every function does nothing.
#if !defined(CLAY_ENABLE_RMLUI)
#include "clay/gui/desktop/UiBridge.h"

namespace clay { namespace uibridge {
bool init(BaseGraphicsContext*, const char*) { return false; }
void setImageLoader(std::function<utils::ImageData(const std::string&)>) {}
void setFileLister(std::function<std::vector<std::string>(const std::string&)>) {}
void setViewTextFilter(std::function<void(std::string&)>) {}
bool isInitialized() { return false; }
void setDpRatio(float) {}
bool loadView(const char*, uint32_t, uint32_t) { return false; }
void unloadView() {}
bool hasView() { return false; }
void update(double, uint32_t, uint32_t) {}
void setCommandBuffer(void*) {}
void renderOffscreen() {}
void renderOnscreen(void*, uint32_t) {}
bool mouseMove(int, int) { return false; }
bool mouseButton(int, int, int, bool) { return false; }
bool mouseWheel(int, int, int) { return false; }
bool scrollAt(int, int, float) { return false; }
void setUiSoundCallbacks(void (*)(), void (*)()) {}
bool popCommand(char*, int) { return false; }
void setText(const char*, const char*) {}
bool getToggle(const char*, bool*) { return false; }
void setToggle(const char*, bool) {}
bool getRangeValue(const char*, float*) { return false; }
void setRangeValue(const char*, float) {}
void clearList(const char*) {}
void addListRow(const char*, const char*, const char*, bool, float, float, float, bool) {}
void addCraftRow(const char*, const char*, const char*, int, int, const char*,
                 const int*, const int*, const int*, int, bool) {}
void addIconRow(const char*, const char*, const char*, int, int, const char*) {}
bool getEditText(const char*, char*, int) { return false; }
void setEditText(const char*, const char*) {}
void setElementColor(const char*, float, float, float) {}
void setProperty(const char*, const char*, const char*) {}
void setIconTile(const char*, const char*, int, int, int, int) {}
void setVisible(const char*, bool) {}
void setEnabled(const char*, bool) {}
void setMargin(const char*, float, float, float, float) {}
bool hitTestButton(int, int, char*, int) { return false; }
bool keyDown(int) { return false; }
bool keyUp(int) { return false; }
bool charInput(unsigned int) { return false; }
void focusElement(const char*) {}
void shutdown() {}
bool createDynamicImage(const char*, uint32_t, uint32_t) { return false; }
void updateDynamicImage(const char*, const void*, uint32_t, uint32_t) {}
} }

#endif // !CLAY_ENABLE_RMLUI
