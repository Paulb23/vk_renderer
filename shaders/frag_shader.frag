#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in vec3 fragCamPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushBlock {
	bool lighting_enabled;
} pushBlock;

void main() {
	if (pushBlock.lighting_enabled == false) {
		outColor = texture(texSampler, fragTexCoord);
		return;
	}
	vec3 lightPos = vec3(0, 50, 0);
	vec3 lightColor = vec3(1, 1, 1);

	float ambientStrength = 0.5;
	float specularStrength = 0.5;
    vec3 ambient = ambientStrength * lightColor;

	vec3 norm = normalize(fragNormal);
	vec3 lightDir = normalize(lightPos - fragWorldPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	vec3 viewDir = normalize(fragCamPos - fragWorldPos);
	vec3 reflectDir = reflect(-lightDir, norm);

	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;


	vec4 result = vec4(ambient + diffuse + specular, 1.0) * texture(texSampler, fragTexCoord);
	outColor = vec4(result);

	//outColor = fragColor;
	//outColor = texture(texSampler, fragTexCoord);
}
