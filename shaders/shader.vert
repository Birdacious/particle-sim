#version 450

//layout(set=0, binding=0) uniform UniformBufferObject... to bind multiple descriptor sets simultaneously.
  // If you had a more complex app where you might have some descriptors which vary per object and some that are shared,
	// you could put the shared/not-shared in separate descriptor sets.
	// Then you could avoid rebinding the not-shared descriptors across draw calls.
layout(binding=0) uniform UniformBufferObject {
	vec2 aligning_test;
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(location=0) in vec2 inPosition;
layout(location=1) in vec3 inColor;

layout(location=0) out vec3 fragColor;

vec2 positions[3] = vec2[] (
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[] (
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);


void main() {
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
}
