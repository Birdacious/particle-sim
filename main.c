#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <error.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// define "DEBUG" during compilation to use validation layers
// TODO: There's an extra part in the tut about Message Callbacks but idc rn
#define n_val_lyrs 1
const char* validation_layers[n_val_lyrs] = {
	"VK_LAYER_KHRONOS_validation"
};

bool checkValidationLayerSupport() {
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count,NULL);
	VkLayerProperties available_layers[layer_count];
	vkEnumerateInstanceLayerProperties(&layer_count,available_layers);

	for(  uint32_t i=0; i<n_val_lyrs ; i++) { bool layer_found=false;
		for(uint32_t j=0; j<layer_count; j++) {
			if(strcmp(validation_layers[i], available_layers[j].layerName) == 0) {
				layer_found=true; break;} }
		if(!layer_found) return false; }
	return true;
}

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
GLFWwindow* window;
VkInstance instance;


GLFWwindow* initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	return glfwCreateWindow(WIDTH,HEIGHT,"Vulkan", NULL,NULL);
}

void createInstance() {
	#ifdef DEBUG
	if(!checkValidationLayerSupport()) puts("Validation layers requested but not available!");
	#endif

	VkApplicationInfo app_info = (VkApplicationInfo){
	.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	.pApplicationName = "Hello Triangle",
	.applicationVersion = VK_MAKE_VERSION(1,0,0),
	.pEngineName = "No Engine",
	.engineVersion = VK_MAKE_VERSION(1,0,0),
	.apiVersion = VK_API_VERSION_1_0            };

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	// TODO: There's an extra part in the tut about asking for more extension details w/ vkEnumerateInstanceExtensionProperties or s/t but idc for now.

	VkInstanceCreateInfo create_info = (VkInstanceCreateInfo){
	.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	.pApplicationInfo = &app_info,
	.enabledExtensionCount = glfwExtensionCount,
	.ppEnabledExtensionNames = glfwExtensions,
	.enabledLayerCount = 0,
	.ppEnabledLayerNames = NULL                    };

	#ifdef DEBUG
	create_info.enabledLayerCount = n_val_lyrs;
	create_info.ppEnabledLayerNames = validation_layers;
	#endif

	if(vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) printf("ono");
}

void initVulkan() {
	createInstance();
}

void mainLoop() {
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void cleanup() {
	vkDestroyInstance(instance,NULL);
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
