#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform Camera {
    mat4 cameraTransform;
} camera;

layout(set = 1, binding = 1) uniform Constants {
    float scale;
} constants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragOnscreenPosition;

layout(location = 0) in vec2 vertPosition;

layout(location = 1) in vec2 vertOffset;

vec3 colors[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    vec4 position = camera.cameraTransform * vec4((vertPosition + vertOffset) * (1.0f + constants.scale), 0.0, 1.0);
    gl_Position = position;
    fragColor = colors[gl_VertexIndex];
    fragOnscreenPosition = position.xy;
}
