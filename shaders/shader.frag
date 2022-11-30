#version 450

layout(location=0) in vec3 fragColor;

layout(location=0) out vec4 outColor;

void main() {
	//outColor = vec4(1.0, 0.0, 0.0, 1.0); // Just red
	outColor = vec4(fragColor, 1.0); // Vertex colors
	//outColor = vec4(fragTexCoord, 0.0,1.0); // Texture coordinates
	//outColor = texture(texSampler, fragTexCoord); // Texture
	//outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0); // Texture mixed with vertex colors
}
