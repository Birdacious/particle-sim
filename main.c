#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <error.h>
#include <stdlib.h>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
GLFWwindow* window;

GLFWwindow* initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	return glfwCreateWindow(WIDTH,HEIGHT,"Vulkan", NULL,NULL);
}

void initVulkan() {
}

void mainLoop() {
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void cleanup() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void run() {
	window = initWindow();
	initVulkan();
	mainLoop(window);
	cleanup(window);
}

int main() {
	run();
	return 0;
}
