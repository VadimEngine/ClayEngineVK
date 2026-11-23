#version 450
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;
void main() {
    // Texture and vertex colour are premultiplied and sRGB-encoded; the target is an sRGB
    // framebuffer, so hand it linear premultiplied colour.
    vec4 c = texture(tex, fragTexCoord) * fragColor;
    vec3 rgb = c.a > 0.0001 ? c.rgb / c.a : vec3(0.0);
    // The framebuffer blends in linear light, where a translucent DARK layer hides less than
    // the same layer does in the sRGB-space blend the UI colours were chosen for. Give dark
    // colours the coverage that reproduces the sRGB-space result; leave light ones (text) alone.
    float lum = dot(rgb, vec3(0.299, 0.587, 0.114));
    float a = mix(1.0 - pow(1.0 - c.a, 2.2), c.a, clamp(lum, 0.0, 1.0));
    rgb = pow(rgb, vec3(2.2));
    outColor = vec4(rgb * a, a);
}
