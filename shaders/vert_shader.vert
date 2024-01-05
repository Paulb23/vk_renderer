#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec3 fragCamPos;

layout(binding = 0) uniform CameraBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

void main() {
	gl_Position = camera.proj * camera.view * camera.model * vec4(inPosition, 1.0);
    fragWorldPos = vec3(camera.model * vec4(inPosition, 1.0));
	fragColor = inColor;
    fragTexCoord = texCoord;
    fragNormal = mat3(transpose(inverse(camera.model))) * inNormal;
    fragCamPos = vec3(camera.view[3][0], camera.view[3][1], camera.view[0][2]);
}
