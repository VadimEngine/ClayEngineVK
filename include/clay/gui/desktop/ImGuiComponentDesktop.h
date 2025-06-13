#ifdef CLAY_PLATFORM_DESKTOP
// third party
#include "imgui.h"
// clay
#include "clay/gui/desktop/Window.h"
#include "clay/graphics/common/BaseGraphicsContext.h"

namespace clay
{
class ImGuiComponentDesktop {
public:
    static void initialize(Window& window, BaseGraphicsContext* graphics);
    
    static void beginRender();

    static void endRender(VkCommandBuffer cmdBuffer);

    static void finalize();
    
private:
    static BaseGraphicsContext* mpGraphics_;
    static VkDescriptorPool mImguiDescriptorPool_;
};

} // namespace clay


#endif // CLAY_PLATFORM_DESKTOP