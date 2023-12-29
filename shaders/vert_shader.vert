#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(binding = 0) uniform CameraBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

void main() {
	gl_Position = camera.proj * camera.view * camera.model * vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
    fragTexCoord = texCoord;
}
