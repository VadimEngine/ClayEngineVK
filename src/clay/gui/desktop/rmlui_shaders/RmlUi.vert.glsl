#version 450
// RmlUi geometry: 2D position (pixels) + premultiplied sRGB colour + texture coordinate.
layout(push_constant) uniform Push {
    mat4 transform;   // pixels -> clip space (with the element's CSS transform, if any)
    vec2 translate;   // per-draw translation, in pixels
} pc;
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
void main() {
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    gl_Position = pc.transform * vec4(inPosition + pc.translate, 0.0, 1.0);
}
