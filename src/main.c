#define GLFW_INCLUDE_VULKAN
//#define GLM_FORCE_RADIANS cglm radians by default
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <error.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
// #include <shaderc/shaderc.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_VULKAN
#include "cimgui.h"
#include "cimgui_impl.h"
#include "kvec.h"

#include <gperftools/profiler.h>

// define "DEBUG" during compilation to use validation layers
// TODO: There's an extra part in the tut about Message Callbacks but idc rn
#define n_req_val_lyrs 1
const char* req_validation_layers[n_req_val_lyrs] = {
	"VK_LAYER_KHRONOS_validation"
};
bool checkValidationLayerSupport() {
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count,NULL);
	VkLayerProperties available_layers[layer_count];
	vkEnumerateInstanceLayerProperties(&layer_count,available_layers);

	for(  uint32_t i=0; i<n_req_val_lyrs ; i++) { bool layer_found=false;
		for(uint32_t j=0; j<layer_count; j++) {
			if(strcmp(req_validation_layers[i], available_layers[j].layerName) == 0) {
				layer_found=true; break;} }
		if(!layer_found) return false; }
	return true;
}

const uint32_t WIDTH  = 1000;
const uint32_t HEIGHT = 1000;
const uint32_t MAX_FRAMES_IN_FLIGHT = 2; uint32_t current_frame = 0;
GLFWwindow *window;
VkInstance instance;
VkPhysicalDevice physical_dev;
VkDevice dev;
VkQueue graphics_queue;
VkQueue present_queue;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
VkImage *swapchain_images; uint32_t n_image;
VkFormat swapchain_image_format;
VkExtent2D swapchain_extent;
VkImageView *swapchain_image_views;
VkRenderPass render_pass;
VkDescriptorSetLayout descriptor_set_layout;
VkDescriptorPool descriptor_pool;
VkDescriptorSet *descriptor_sets;
VkPipelineLayout pipeline_layout;
VkPipeline graphics_pipeline;
VkFramebuffer *swapchain_framebuffers;
VkCommandPool command_pool;
VkCommandBuffer *command_buffers;
VkSemaphore *image_available_semaphores, *render_finished_semaphores;
VkFence *in_flight_fences;
bool framebuffer_resized = false;
VkBuffer vertex_buffer;
VkDeviceMemory vertex_buffer_memory;
VkBuffer *uniform_buffers;
VkDeviceMemory *uniform_buffers_memory;

VkDescriptorPool imgui_descriptor_pool;

typedef struct { vec2 pos; vec3 color; } Vertex;
unsigned int n_vertices = 0;
Vertex *vertices;

typedef struct {
	vec2 aligning_test;
	_Alignas(16) mat4 model; // Seems cglm auto aligns by default. But w/ nested structures this could break down. So just in case _Alignas.
	_Alignas(16) mat4 view;
	_Alignas(16) mat4 proj;
} UniformBufferObject;

VkVertexInputBindingDescription *getBindingDescription() {
	VkVertexInputBindingDescription *binding_desc = malloc(sizeof(VkVertexInputBindingDescription));
	*binding_desc = (VkVertexInputBindingDescription){
		.binding = 0,
		.stride = sizeof(Vertex), // bytes from one entry to next
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX, // or ..._INSTANCE, for instanced rendering
	};
	return binding_desc;
}
const int n_attr_descs = 2;
VkVertexInputAttributeDescription *getAttributeDescriptions() {
	VkVertexInputAttributeDescription *attr_descs = malloc(n_attr_descs*sizeof(VkVertexInputAttributeDescription));
	attr_descs[0] = (VkVertexInputAttributeDescription){
		.binding = 0,
		.location = 0,
		.format = VK_FORMAT_R32G32_SFLOAT, // uses color format names. This really means vec3. Others: R32/R32G32/R32G32B32/R32G32B32A32 basically mean float/vec2/vec3/vec4. And _SINT/_UINT/_SFLOAT == ivec#/uvec#/dvec# (the _SFLOAT one is 64 bit unlike the others, takes up 2 "slots" per vec item).
		.offset = offsetof(Vertex,pos)
	};
	attr_descs[1] = (VkVertexInputAttributeDescription){
		.binding = 0,
		.location = 1,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex,color)
	};
	return attr_descs;
}


// does this actually need to be a static fn?
void framebufferResizeCallback(GLFWwindow *window, int w, int h) {
	framebuffer_resized = true;
}
void initWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(WIDTH,HEIGHT,"Particle Life Simulation", NULL,NULL);
	// don't need glfwSetWindowUserPointer(window,this) b/c bool framebuffer_resized is already in this file and globally accessible.
	    // The use of this function is so that you can access some things you might need from fn callbacks, w/o having to make those things totally global.
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void createInstance() {
	#ifdef DEBUG
	if(!checkValidationLayerSupport()) puts("Validation layers requested but not available!");
	#endif

	VkApplicationInfo app_info = (VkApplicationInfo){
	.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	.pApplicationName = "Particle Life Simulation",
	.applicationVersion = VK_MAKE_VERSION(1,0,0),
	.pEngineName = "No Engine",
	.engineVersion = VK_MAKE_VERSION(1,0,0),
	.apiVersion = VK_API_VERSION_1_0                };

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	// TODO: There's an extra part in the tut about asking for more extension details w/ vkEnumerateInstanceExtensionProperties or s/t but idc for now.

	VkInstanceCreateInfo create_info = (VkInstanceCreateInfo){
	.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	.pApplicationInfo = &app_info,
	.enabledExtensionCount = glfwExtensionCount,
	.ppEnabledExtensionNames = glfwExtensions,
	.enabledLayerCount = 0,
	.ppEnabledLayerNames = NULL                              };

	#ifdef DEBUG
	create_info.enabledLayerCount = n_req_val_lyrs;
	create_info.ppEnabledLayerNames = req_validation_layers;
	#endif

	if(vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) printf("ono");
}

void createSurface() {
	if(glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) printf("Failed to create window surface!");
}

typedef struct {
	uint32_t graphicsFamily;
  uint32_t presentFamily;
} QueueFamilyIndices;
void findQueueFamilies(VkPhysicalDevice phys_dev, QueueFamilyIndices* inds) {
	inds->graphicsFamily=UINT32_MAX;
	inds->presentFamily=UINT32_MAX; // If it's still UINT32_MAX after we ask for the index, the queue family probably doesn't exist (b/c I'm guessing the index won't be max int)

	uint32_t n_queue_fam=0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &n_queue_fam, NULL);
	VkQueueFamilyProperties queue_fams[n_queue_fam];
	vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &n_queue_fam, queue_fams);

	int graphics_ind=0;
	for(int i=0; i<n_queue_fam; i++) {
		if(queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) inds->graphicsFamily = graphics_ind;
		graphics_ind++;

		VkBool32 supports_present = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(phys_dev, i, surface, &supports_present);
		if(supports_present) inds->presentFamily=i;
	}
}

typedef struct {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;           uint32_t n_format;
	VkPresentModeKHR* presentModes;        uint32_t n_presentMode;
} SwapchainSupportDetails;
SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice phys_dev) {
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &details.n_format, NULL);
	if(details.n_format != 0) {
		details.formats = malloc(sizeof(VkSurfaceFormatKHR)*details.n_format); // MALLOC'D formats
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &details.n_format, details.formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &details.n_presentMode, NULL);
	if(details.n_presentMode != 0) {
		details.presentModes = malloc(sizeof(VkSurfaceFormatKHR)*details.n_presentMode); //MALLOC'D presentModes
		vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &details.n_presentMode, details.presentModes);
	}

	return details;
}
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR available_formats[], uint32_t n_formats) {
	for(uint32_t i=0; i<n_formats; i++) {
		if(available_formats[i].format     == VK_FORMAT_B8G8R8A8_SRGB &&
		   available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return available_formats[i];
	}
	return available_formats[0]; // If ideal B8G8R8A8 SRGB format not found, just w/e first one please.
}
VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR available_presentModes[], uint32_t n_presentModes) {
	for(uint32_t i=0; i<n_presentModes; i++) {
		if(available_presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) return available_presentModes[i];
	}
	return VK_PRESENT_MODE_FIFO_KHR;
	// See tut for more info about the 4 different present modes and which one for which situation! It's cool.
}
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR capabilities) {
	if(capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	} else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actual_extent = { (uint32_t)width, (uint32_t)height };

		printf("%d, %d\n", actual_extent.width, actual_extent.height);
		uint32_t w,h,miw,maw,mih,mah;
		  w = actual_extent.width;                   h = actual_extent.height;
		miw = capabilities.minImageExtent.width; mih = capabilities.minImageExtent.height;
		maw = capabilities.minImageExtent.width; mah = capabilities.minImageExtent.height;

		actual_extent.width  = w > miw ? (w < maw ? w : maw) : miw;
		actual_extent.height = h > mih ? (h < mah ? h : mah) : mih; // basically clamp width&height between min&max

		return actual_extent;
	}
}
void createSwapchain() {
	SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physical_dev);
	VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(swapchainSupport.formats, swapchainSupport.n_format);
	VkPresentModeKHR present_mode = chooseSwapPresentMode(swapchainSupport.presentModes, swapchainSupport.n_presentMode);
	VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

	n_image = swapchainSupport.capabilities.minImageCount + 1;
	if(swapchainSupport.capabilities.maxImageCount > 0 && n_image > swapchainSupport.capabilities.maxImageCount)
		n_image = swapchainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR create_info = (VkSwapchainCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = n_image,
		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // means will render directly to the images in the swap chain. VK_IMAGE_USAGE_TRANSFER_DST_BIT instead if you wanna do post-processing. See tut.
		.preTransform = swapchainSupport.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
		
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0, // optional, if graphics & present family index is the same
		.pQueueFamilyIndices=NULL // ^
	};
	QueueFamilyIndices inds; findQueueFamilies(physical_dev, &inds);
	uint32_t tmp[2] = {inds.graphicsFamily, inds.presentFamily};
	if(inds.graphicsFamily != inds.presentFamily) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = tmp;
	}

	if(vkCreateSwapchainKHR(dev, &create_info, NULL, &swapchain) != VK_SUCCESS) printf("Swapchain creation failed! :(");
	free(swapchainSupport.formats); free(swapchainSupport.presentModes); // FREE'D swapchainSupport

	vkGetSwapchainImagesKHR(dev, swapchain, &n_image, NULL);
	swapchain_images = malloc(sizeof(VkImage)*n_image); // MALLOC'D swapchain_images
	vkGetSwapchainImagesKHR(dev, swapchain, &n_image, swapchain_images);

	swapchain_image_format = surface_format.format;
	swapchain_extent = extent;
}

#define n_req_extensions 1
const char* req_device_extensions[n_req_extensions] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
bool checkDeviceExtensionSupport(VkPhysicalDevice phys_dev) {
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &extension_count, NULL);
	VkExtensionProperties available_extensions[extension_count];
	vkEnumerateDeviceExtensionProperties(phys_dev, NULL, &extension_count, available_extensions);

	for(  uint32_t i=0; i<n_req_extensions ; i++) { bool extension_found=false;
		for(uint32_t j=0; j<extension_count; j++) {
			if(strcmp(req_device_extensions[i], available_extensions[j].extensionName) == 0) {
				extension_found=true; break;} }
		if(!extension_found) return false; }
	return true;
}
bool isDeviceSuitable(VkPhysicalDevice phys_dev) {
	// There's plenty you could do here to select the device you want, but idc.
	//VkPhysicalDeviceProperties dev_properties;
	VkPhysicalDeviceFeatures   supported_features; 
	//vkGetPhysicalDeviceProperties(phys_dev, &dev_properties);
	vkGetPhysicalDeviceFeatures  (phys_dev, &supported_features);

	QueueFamilyIndices inds; findQueueFamilies(phys_dev, &inds);

	bool supports_extensions = checkDeviceExtensionSupport(phys_dev);

	bool swapchain_adequate = false;
	if(supports_extensions) {
		SwapchainSupportDetails swapchain_support = querySwapchainSupport(phys_dev);
		swapchain_adequate = swapchain_support.n_format>0 && swapchain_support.n_presentMode>0;
		free(swapchain_support.formats); free(swapchain_support.presentModes); // FREE'D swapchain_support
	}
	
	printf("%u, %u, %u\n", inds.graphicsFamily, inds.presentFamily, UINT32_MAX);
	return (inds.graphicsFamily != UINT32_MAX &&
			    inds.presentFamily != UINT32_MAX &&
					supports_extensions &&
					swapchain_adequate &&
					supported_features.samplerAnisotropy);
}
void pickPhysicalDevice() {
	uint32_t n_phys_dev=0;
	vkEnumeratePhysicalDevices(instance, &n_phys_dev, NULL);
	if(n_phys_dev==0) {printf("No Vulkan-compatible GPUs! :(");}
	VkPhysicalDevice phys_devs[n_phys_dev];
	vkEnumeratePhysicalDevices(instance, &n_phys_dev, phys_devs);
	printf("%u = ndev\n",n_phys_dev);

	for(uint32_t i=0; i<n_phys_dev; i++) {
		VkPhysicalDevice phys_dev = phys_devs[i];
		if(isDeviceSuitable(phys_dev)) {physical_dev = phys_dev; break;}
		if(physical_dev == VK_NULL_HANDLE) {printf("No suitable GPUs! :(");}
	}
}
void createLogicalDevice() {
	QueueFamilyIndices inds; findQueueFamilies(physical_dev, &inds);

	VkDeviceQueueCreateInfo queue_create_infos[1];

	float queuePriority = 1.0f;
	queue_create_infos[0] = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = inds.graphicsFamily,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority
	};
	//queue_create_infos[1] = queue_create_infos[0];
	//queue_create_infos[1].queueFamilyIndex = inds.presentFamily;

	VkPhysicalDeviceFeatures device_features = (VkPhysicalDeviceFeatures){.samplerAnisotropy=VK_TRUE};
	// Possible addition: if anisotropy not available, conditionally set sampler_info.anisoTropyEnable = VK_FALSE and .maxAnisotropy = 1.f (in createTextureSampler) if not available

	VkDeviceCreateInfo create_info = (VkDeviceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = queue_create_infos,
		.pEnabledFeatures = &device_features,
		.enabledExtensionCount = n_req_extensions,
		.ppEnabledExtensionNames = req_device_extensions
	};
	#ifdef DEBUG
	create_info.enabledLayerCount = n_req_val_lyrs; // Vulkan no longer makes distinction between instance and dev-specific validation layers, but for back-comp we include this anyway
	create_info.ppEnabledLayerNames = req_validation_layers; // ^
	#endif

	if(vkCreateDevice(physical_dev, &create_info, NULL, &dev)) printf("Failed to create logical device! :(");
	vkGetDeviceQueue(dev, inds.graphicsFamily, 0, &graphics_queue);
	// vkGetDeviceQueue(dev, inds.presentFamily , 0, &present_queue );
	present_queue = graphics_queue;
	// TODO: NOTE: I am doing s/t lazy and wrong here, where I commented out a bunch of stuff out of laziness and just assumed the graphics queue is the same as the present queue.
	  // And on my computer they ARE the same (graphicsFamily & presentFamily both == index 0), but it is NOT ALWAYS! So to be fully correct you'll need to come back and fix this.
		// In the tutorial they used a C++ set (I don't have in plain C) to deal w/ this elegantly but I'm lazy so I skipped it.
}

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
	VkImageViewCreateInfo create_info = (VkImageViewCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY, // try changing these for fun!
		.subresourceRange.aspectMask = aspect_flags,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.flags = 0,
		.pNext = NULL
	};
	VkImageView image_view;
	if(vkCreateImageView(dev, &create_info, NULL, &image_view) != VK_SUCCESS) {printf("Failed to create image view! :("); exit(1);}
	return image_view;
}
void createImageViews() {
	swapchain_image_views = malloc(sizeof(VkImageView) * n_image); // MALLOC'D swapchain_image_views
	for(uint32_t i=0; i < n_image; i++)
		swapchain_image_views[i] = createImageView(swapchain_images[i], swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
}

char* readFile(char* filenm, unsigned long* sz) {
	char* buf = NULL;
	FILE* f = fopen(filenm,"rb");
	if(f!=NULL) if(fseek(f,0,SEEK_END)==0) {
		size_t bufsz=ftell(f);  if(bufsz==-1) printf("AHHHH");
		buf = malloc(bufsz+1); if(!buf)     printf("Mem error!");  // MALLOC'D shader file contents
		if(fseek(f,0,SEEK_SET)!=0)          printf("AHHHH2");
		fread(buf,1,bufsz,f);
		*sz = bufsz;
		return buf;
	}
	return NULL;
}

VkShaderModule createShaderModule(const char* shdr_code, unsigned long code_sz) {
	VkShaderModuleCreateInfo create_info = (VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code_sz,
		.pCode = (uint32_t*)shdr_code
	};
	VkShaderModule shdr_module;
	if(vkCreateShaderModule(dev, &create_info, NULL, &shdr_module)) printf("Failed 2 create shader module");
	return shdr_module;
}

VkFormat findSupportedFormat(const VkFormat *candidate_formats, uint32_t n_candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for(uint32_t i=0; i<n_candidates; i++) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_dev, candidate_formats[i], &props);
		if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return candidate_formats[i];
		if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return candidate_formats[i];
	}
	printf("Failed to find supported format! :("); exit(1);
}

void createRenderPass() {
	// We just have a single color buffer attachment represented by one of the images from the swap chain
	VkAttachmentDescription color_attachment = (VkAttachmentDescription){
		.format = swapchain_image_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // What to do with the data in the attachment before rendering
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // ^ but ~after~ rendering
		// NOTE: see tut for more Op options
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // We're not using the stencil buffer
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	VkAttachmentReference color_attachment_ref = (VkAttachmentReference){
		.attachment = 0, // INDEX of attachment to reference (index in the attachment descriptions array)
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};


	VkSubpassDescription subpass = (VkSubpassDescription){
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref // index of the attachment in this array is directly reference from the frag shdr w/ "layout(location=0) out vec4 outColor"!
		// NOTE: See tut for the other types of attachments that can be referenced by a subpass.
	};
	VkSubpassDependency dependency = (VkSubpassDependency){
		.srcSubpass = VK_SUBPASS_EXTERNAL, //refers to implicit subpass before the render pass (or after, if in dst below v)
		.dstSubpass = 0, // refers to the index of our first and only subpass
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, // operations to wait on and stages in which the ops occur. So here we wait for color attachment output stage, so swapchain finishes reading image before we access it
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};
	
	VkAttachmentDescription attachments[1] = {color_attachment};
	VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};
	if(vkCreateRenderPass(dev, &render_pass_create_info, NULL, &render_pass) != VK_SUCCESS) printf("Failed to create render pass! :(");
}

void createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding ubo_layout_binding = (VkDescriptorSetLayoutBinding){
		.binding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = NULL // optional
	};
	VkDescriptorSetLayoutBinding bindings[1] = {ubo_layout_binding};
	VkDescriptorSetLayoutCreateInfo layout_info = (VkDescriptorSetLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1, // note to self: beware magic number
		.pBindings = bindings
	};
	if(vkCreateDescriptorSetLayout(dev, &layout_info, NULL, &descriptor_set_layout) != VK_SUCCESS) {printf("Failed to create descriptor set layout!"); exit(1);}
}
void createDescriptorPool() {
	VkDescriptorPoolSize uni_pool_size = (VkDescriptorPoolSize){
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT
	};
	VkDescriptorPoolSize pool_sizes[1] = {uni_pool_size};
	VkDescriptorPoolCreateInfo pool_info = (VkDescriptorPoolCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1, // beware magic number
		.pPoolSizes = pool_sizes,
		.maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT,
	};
	if(vkCreateDescriptorPool(dev, &pool_info, NULL, &descriptor_pool) != VK_SUCCESS) {printf("Failed to create descriptor pool! :("); exit(1);}
}
void createDescriptorSets() {
	VkDescriptorSetLayout *layouts = malloc(sizeof(VkDescriptorSetLayout) * MAX_FRAMES_IN_FLIGHT);
	for(size_t i=0; i<(uint32_t)MAX_FRAMES_IN_FLIGHT; i++) {
		//layouts[i]=descriptor_set_layout;
		memcpy(layouts+i, &descriptor_set_layout, sizeof(VkDescriptorSetLayout));
	}
	VkDescriptorSetAllocateInfo alloc_info = (VkDescriptorSetAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts
	};
	descriptor_sets = malloc(sizeof(VkDescriptorSet) * MAX_FRAMES_IN_FLIGHT);
	if(vkAllocateDescriptorSets(dev, &alloc_info, descriptor_sets) != VK_SUCCESS) {printf("Failed to allocate descriptor sets! :("); exit(1);}

	for(size_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo buf_info = (VkDescriptorBufferInfo){
			.buffer = uniform_buffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject), // or VK_WHOLE_SIZE b/c we are always overwriting the whole uniform buf e/ frame
		};

		VkWriteDescriptorSet uni_descriptor_write = (VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_sets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo = &buf_info,
			.pImageInfo = NULL, // optional
			.pTexelBufferView = NULL // optional
		};

		VkWriteDescriptorSet descriptor_writes[1] = {uni_descriptor_write};
		vkUpdateDescriptorSets(dev, 1, descriptor_writes, 0, NULL);
	}
}

VkDynamicState dynamic_states[] = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};
VkPipelineDynamicStateCreateInfo dynamic_state_create_info = (VkPipelineDynamicStateCreateInfo){
	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	.dynamicStateCount = 2, // see dynamic_states above
	.pDynamicStates = dynamic_states
};
void createGraphicsPipeline() {
	unsigned long vert_shdr_sz, frag_shdr_sz;
	char* vert_shdr_code = readFile("shaders/vert.spv",&vert_shdr_sz);
	char* frag_shdr_code = readFile("shaders/frag.spv",&frag_shdr_sz);

	VkShaderModule vert_shdr_module = createShaderModule(vert_shdr_code,vert_shdr_sz);
	VkShaderModule frag_shdr_module = createShaderModule(frag_shdr_code,frag_shdr_sz);

	VkPipelineShaderStageCreateInfo vert_shdr_stage_info = (VkPipelineShaderStageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert_shdr_module,
		.pName = "main",
		.pSpecializationInfo = NULL // allows specify values for shader constants, e.g. could use a single shader module where behavior configured @ pipeline creation by specifying different constants here, possibly more efficient
	};
	VkPipelineShaderStageCreateInfo frag_shdr_stage_info = (VkPipelineShaderStageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_shdr_module,
		.pName = "main",
		.pSpecializationInfo = NULL
	};

	VkPipelineShaderStageCreateInfo shdr_stages[] = {vert_shdr_stage_info, frag_shdr_stage_info};

	VkVertexInputBindingDescription *binding_desc = getBindingDescription();
	VkVertexInputAttributeDescription *attr_descs = getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = (VkPipelineVertexInputStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = binding_desc,
		.vertexAttributeDescriptionCount = n_attr_descs,
		.pVertexAttributeDescriptions = attr_descs
	};
	VkPipelineInputAssemblyStateCreateInfo input_asm_create_info = (VkPipelineInputAssemblyStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // plenty of ways to optimize vertices w/ different options here
		.primitiveRestartEnable = VK_FALSE
	};
//	VkViewport viewport = (VkViewport){
//		.x=0.f,
//		.y=0.f,
//    .width=(float)swapchain_extent.width,
//		.height=(float)swapchain_extent.height,
//		.minDepth = 0.f,
//		.maxDepth = 1.f
//	};
//	VkRect2D scissor = (VkRect2D){
//		.offset = {0,0},
//		.extent = swapchain_extent
//	};
	VkPipelineViewportStateCreateInfo viewport_state_create_info = (VkPipelineViewportStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		//.pViewports = &viewport, // To set up the viewport in the pipeline (not dynamic). To change this then, you'd need to make a new pipeline
		.scissorCount = 1,
		//.pScissors = &scissor // ^ see comment above
	};
	VkPipelineRasterizationStateCreateInfo rasterizer = (VkPipelineRasterizationStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE, // frags beyond near/far planes clamped to them instead of discarding. Useful for shadowmaps. Use reqs enabling GPU feature.
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.f, // thicker widths maybe not available on some hardware, reqs "wideLines" GPU feature.
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.f,
		.depthBiasClamp = 0.f,
		.depthBiasSlopeFactor = 0.f
	};
	VkPipelineMultisampleStateCreateInfo multisampling = (VkPipelineMultisampleStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = VK_FALSE,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1.f,
		.pSampleMask = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	// See tut for intuition about how this configures color blending
	// The config below (basically .blendEnable = VK_FALSE disables e/t) would mean the new color completely overwrites the old.
	// NOTE: this struct is per-framebuffer
	VkPipelineColorBlendAttachmentState color_blend_attachment = (VkPipelineColorBlendAttachmentState){
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = VK_FALSE, // None of the stuff below matters since we set this false
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD
	};
	// The config below implements alpha belnding
	  // i.e. final.rgb = new.a * new.rgb + (1-new.a) * old.rgb; final.a = new.a 
	//VkPipelineColorBlendAttachmentState color_blend_attachment = (VkPipelineColorBlendAttachmentState){
	//	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	//	.blendEnable = VK_TRUE,
	//	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	//	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	//	.colorBlendOp = VK_BLEND_OP_ADD,
	//	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	//	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
	//	.alphaBlendOp = VK_BLEND_OP_ADD
	//};
	
	VkPipelineColorBlendStateCreateInfo color_blending = (VkPipelineColorBlendStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE, // If you finna use bitwise combination blending instead, enable this. NOTE: enabling this overwites blendEnable to VK_FALSE! Can't use both.
		.logicOp = VK_LOGIC_OP_COPY, // ^
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
		.blendConstants[0] = 0.f,
		.blendConstants[1] = 0.f,
		.blendConstants[2] = 0.f,
		.blendConstants[3] = 0.f,
	};

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = (VkPipelineLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL
	};
	if(vkCreatePipelineLayout(dev, &pipeline_layout_create_info, NULL, &pipeline_layout) != VK_SUCCESS) printf("Failed to create pipeline layout! :(");

	VkGraphicsPipelineCreateInfo pipeline_create_info = (VkGraphicsPipelineCreateInfo){
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shdr_stages,
		.pVertexInputState = &vertex_input_create_info,
		.pInputAssemblyState = &input_asm_create_info,
		.pViewportState = &viewport_state_create_info,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = NULL,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state_create_info,
		.layout = pipeline_layout,
		.renderPass = render_pass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE, // See tut. If two very similar render passes, you can use one as the "base" for the other. Bit faster.
		.basePipelineIndex = -1
	};
	if(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &graphics_pipeline) != VK_SUCCESS) printf("Failed to create graphics pipeline! :(");
	// NOTE: this create function is special, can create multiple pipelines in one call.
	// the extra arg in this create function where we pass VK_NULL_HANDLE is a VkPipelineCache object to reuse data relevant to pipeline creation... for FASTness.
	  // Could even grab the cache stuff from a file!

	vkDestroyShaderModule(dev,vert_shdr_module,NULL);
	vkDestroyShaderModule(dev,frag_shdr_module,NULL);
	free(vert_shdr_code); free(frag_shdr_code);
	free(binding_desc); free(attr_descs);
}

void createFramebuffers() {
	swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * n_image);
	
	for(size_t i=0; i<n_image; i++) {
		VkImageView attachments[1] = {swapchain_image_views[i]};
		
		VkFramebufferCreateInfo framebuffer_create_info = (VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = 1, // beware magic number
			.pAttachments = attachments,
			.width = swapchain_extent.width,
			.height = swapchain_extent.height,
			.layers = 1
		};
		if(vkCreateFramebuffer(dev, &framebuffer_create_info, NULL, &swapchain_framebuffers[i]) != VK_SUCCESS) printf("Failed to create framebuffer! :(");
	}
}

void createCommandPool() {
	QueueFamilyIndices inds; findQueueFamilies(physical_dev, &inds);
	VkCommandPoolCreateInfo pool_info = (VkCommandPoolCreateInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = inds.graphicsFamily
	};
	if(vkCreateCommandPool(dev, &pool_info, NULL, &command_pool) != VK_SUCCESS) printf("Failed to create command pool! :(");
}

uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties mem_properties; // Contains 2 arrays: memoryTypes, memoryHeaps
	vkGetPhysicalDeviceMemoryProperties(physical_dev, &mem_properties);
	for(uint32_t i=0; i<mem_properties.memoryTypeCount; i++) {
		if((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) return i;
	}
	printf("Failed to find suitable memory type! :("); exit(1);
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *buffer_memory) {
	VkBufferCreateInfo buf_info = (VkBufferCreateInfo){
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	if(vkCreateBuffer(dev, &buf_info, NULL, buffer) != VK_SUCCESS) printf("Failed to create vertex buffer! :(");

	VkMemoryRequirements mem_reqs; // struct containing: size, alignment, memoryTypeBits
	vkGetBufferMemoryRequirements(dev, *buffer, &mem_reqs);
	
	VkMemoryAllocateInfo alloc_info = (VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, properties)
	};
	if(vkAllocateMemory(dev, &alloc_info, NULL, buffer_memory) != VK_SUCCESS) printf("Failed to allocate vertex buffer memory! :(");

	vkBindBufferMemory(dev, *buffer, *buffer_memory, 0);
}
// Simple way of submitting commands. To make sure things happen synchronously, just waits for queue to be idle (QueueWaitIdle).
  // However, for better performance, you could/should combine operations into a single cmd buffer and execute them asynchronously for better throughput.
    // (In the case of image texture stuff, especially the transitions and copy in createTextureImage() ).
		// To do that ^ for this ^ example, you could create a setup_cmd_buffer that the helper functions (transition and copy etc.) record into.
		// and a flushSetupCommands() to execute the commands that have been recorded so far.
VkCommandBuffer beginSingleTimeCommands(VkCommandPool cmd_pool) {
	VkCommandBufferAllocateInfo alloc_info = (VkCommandBufferAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = cmd_pool,
		.commandBufferCount = 1
	};
	VkCommandBuffer cb;
	vkAllocateCommandBuffers(dev, &alloc_info, &cb);

	VkCommandBufferBeginInfo begin_info = (VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(cb, &begin_info);
	return cb;
}
void endSingleTimeCommands(VkCommandBuffer cb, VkCommandPool cmd_pool) {
	vkEndCommandBuffer(cb);
	VkSubmitInfo submit_info = (VkSubmitInfo){
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cb
	};
	vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue);

	vkFreeCommandBuffers(dev, cmd_pool, 1, &cb);
}
void copyBuffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size, VkCommandPool cmd_pool) {
	VkCommandBuffer tmp_cmd_buf = beginSingleTimeCommands(cmd_pool);

	VkBufferCopy copy_region = (VkBufferCopy){
		.srcOffset = 0, // optional
		.dstOffset = 0, // optional
		.size = size
	};
	vkCmdCopyBuffer(tmp_cmd_buf, src_buf, dst_buf, 1, &copy_region);

	endSingleTimeCommands(tmp_cmd_buf, cmd_pool);
}

void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *image_memory) {
	VkImageCreateInfo image_info = (VkImageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = w,
		.extent.height = h,
		.extent.depth = 1,
		.mipLevels = 1,
		.arrayLayers = 1,
		.format = format,
		.tiling = tiling, // if you wanna directly access texels in the image's memory, use _LINEAR instead of _OPTIMAL.
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // i don't understant what dis measn
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.samples = VK_SAMPLE_COUNT_1_BIT, // SEE TUT, could be important to save allocating memory for "air blocks"
		.flags = 0 // optional
	};
	if(vkCreateImage(dev, &image_info, NULL, image) != VK_SUCCESS) {printf("Failed to create image! :("); exit(1);}

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(dev, *image, &mem_reqs);
	VkMemoryAllocateInfo alloc_info = (VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = findMemoryType(mem_reqs.memoryTypeBits, properties)
	};
	if(vkAllocateMemory(dev, &alloc_info, NULL, image_memory) != VK_SUCCESS) {printf("Failed to allocate image memory! :("); exit(1);}

	vkBindImageMemory(dev, *image, *image_memory, 0);
}

void createVertexBuffer() {
	VkDeviceSize buffer_size = sizeof(vertices[0]) * n_vertices;
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

	void* data;
	vkMapMemory(dev, staging_buffer_memory, 0, buffer_size, 0, &data);
	// s/t s/t tutorial says s/t about flushing memory explicitly vs ..._COHERENT_BIT, idk
	memcpy(data, vertices, (size_t)buffer_size);
	vkUnmapMemory(dev, staging_buffer_memory);

	// createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer, &vertex_buffer_memory);
  createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer, &vertex_buffer_memory);

	copyBuffer(staging_buffer, vertex_buffer, buffer_size, command_pool);

	vkDestroyBuffer(dev, staging_buffer, NULL);
	vkFreeMemory(dev, staging_buffer_memory, NULL);
}
void createUniformBuffers() {
	// We need multiple uniform buffers b/c w/ multiple frames in flight we don't want to update one uniform buffer in prep of next frame while the prev frame is still reading from it!
	// We don't do all that staging buffer stuff here b/c we're changing this pretty much e/ frame so it would actually probably add more overhead than it's worth.
	VkDeviceSize buffer_size = sizeof(UniformBufferObject);
	uniform_buffers = malloc(sizeof(VkBuffer)       * MAX_FRAMES_IN_FLIGHT);
	uniform_buffers_memory = malloc(sizeof(VkDeviceMemory) * MAX_FRAMES_IN_FLIGHT);
	for(size_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++)
		createBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniform_buffers[i], &uniform_buffers_memory[i]);
	// No vkMapMemory here, we'll do that thru a separate fn b/c we want more control
};

void createCommandBuffers() {
	command_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo alloc_info = (VkCommandBufferAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, // Specifies if primary or secondary command buffer. Secondary cannot be submitted to queue directly, but can be called from primary cmd buffers.
		.commandBufferCount = MAX_FRAMES_IN_FLIGHT
	};
	if(vkAllocateCommandBuffers(dev, &alloc_info, command_buffers) != VK_SUCCESS) printf("Failed to allocate command buffers! :(");
}
void recordCommandBuffer(VkCommandBuffer cb, uint32_t image_ind) {
	VkCommandBufferBeginInfo begin_info = (VkCommandBufferBeginInfo) {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0, // NOTE: see tut for possible flags and what they mean
		.pInheritanceInfo = NULL // For 2ndary cmd bufs. Says which state to inherit from the calling primary cmd bufs.
	};
	if(vkBeginCommandBuffer(cb,&begin_info) != VK_SUCCESS) printf("Failed to begin recording cmd buffer! :(");

	VkClearValue clear_values[1] = {
		{.color={{0.f,0.f,0.f,1.f}}},
	};
	VkRenderPassBeginInfo render_pass_begin_info = (VkRenderPassBeginInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = render_pass,
		.framebuffer = swapchain_framebuffers[image_ind],
		.renderArea.offset = {0,0},
		.renderArea.extent = swapchain_extent,
		.clearValueCount = 1,
		.pClearValues = clear_values // defines the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
	};

	vkCmdBeginRenderPass(command_buffers[current_frame], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(command_buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline); // Could specify a compute pipeline insead of graphics...

	VkBuffer vertex_buffers[] = {vertex_buffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffers[current_frame], 0,1, vertex_buffers, offsets);

	// We specified viewport and scissor to be dynamic so we set them here.
	VkViewport viewport = (VkViewport){
		.x=0.f,
		.y=0.f,
    .width=(float)swapchain_extent.width,
		.height=(float)swapchain_extent.height,
		.minDepth = 0.f,
		.maxDepth = 1.f
	};
	vkCmdSetViewport(command_buffers[current_frame], 0, 1, &viewport);
	VkRect2D scissor = (VkRect2D){
		.offset = {0,0},
		.extent = swapchain_extent
	};
	vkCmdSetScissor(command_buffers[current_frame], 0, 1, &scissor);

	vkCmdBindDescriptorSets(command_buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,1, &descriptor_sets[current_frame], 0,NULL);
	vkCmdDraw(command_buffers[current_frame], n_vertices, 1,0,0); // args: cb, vertexCount, instanceCount, firstVertex (lowest val of gl_VertexIndex), firstInstance

	ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), command_buffers[current_frame], VK_NULL_HANDLE);

	vkCmdEndRenderPass(command_buffers[current_frame]);
	if(vkEndCommandBuffer(command_buffers[current_frame]) != VK_SUCCESS) printf("Failed to record command buffer! :(");
}

void createSyncObjects() {
	image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
	
	VkSemaphoreCreateInfo semaphore_info = (VkSemaphoreCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VkFenceCreateInfo fence_info = (VkFenceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT // So that the fence is already signalled on start and we don't get stuck waiting on the first frame before it gets a chance to be vkResetFences()'d
	};

	for(uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
		if(vkCreateSemaphore(dev, &semaphore_info, NULL, &image_available_semaphores[i]) != VK_SUCCESS ||
			 vkCreateSemaphore(dev, &semaphore_info, NULL, &render_finished_semaphores[i]) != VK_SUCCESS ||
		 	 vkCreateFence(    dev, &fence_info,     NULL, &in_flight_fences[i])         != VK_SUCCESS   ) {

			printf("Failed to create sync objects for a frame! :(");
		}
	}
}

void cleanupSwapchain() {
	for(uint32_t i=0; i<n_image; i++) {
		vkDestroyFramebuffer(dev,swapchain_framebuffers[i],NULL);
	}

	//vkGetSwapchainImagesKHR(dev, swapchain, &n_image, NULL);
	for(uint32_t i=0; i<n_image; i++) {
		vkDestroyImageView(dev,swapchain_image_views[i],NULL);
	}

	vkDestroySwapchainKHR(dev,swapchain,NULL);
}
void recreateSwapchain() {
	int w=0; int h=0;
	//glfwGetFramebufferSize(window, &w,&h);
	while(w==0||h==0) {
		glfwGetFramebufferSize(window, &w,&h);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(dev);
	cleanupSwapchain();

	createSwapchain();
	createImageViews();
	createFramebuffers();
	// Could recreate renderpass too. Swapchain image format could change during prog's life, e.g. maybe useful if dragging a window to another monitor.
}

void updateUniformBuffer(uint32_t current_image) {
	static struct timespec ts_app_start = (struct timespec){.tv_sec=0};
	if(ts_app_start.tv_sec == 0) timespec_get(&ts_app_start,TIME_UTC); // To get around plain C not allowing static vars to be declared with an initial non-constant value
	struct timespec ts_current;  timespec_get(&ts_current,  TIME_UTC);
	float t_running = 0;
	t_running += (ts_current.tv_sec-ts_app_start.tv_sec) + ((ts_current.tv_nsec-ts_app_start.tv_nsec)/1000000000.0);
	//printf("%f\n",t_running);

	mat4 I; glm_mat4_identity(I);
	UniformBufferObject ubo;// = (UniformBufferObject){.model=I, .view=I, .proj=I}; // Can't assign vals to array types (which mat4 are *float[4]). Need use memcpy or in this case glm_mat4_copy().
	glm_mat4_copy(I,ubo.model); glm_mat4_copy(I,ubo.view); glm_mat4_copy(I,ubo.proj);
	glm_rotate(ubo.model, t_running*(3.141f/2.f), (vec3){0.f,0.f,1.f}), // rot 90°/s about z-axis
	glm_lookat((vec3){2.f,2.f,2.f}, (vec3){0.f,0.f,0.f}, (vec3){0.f,0.f,1.f}, ubo.view); 
	glm_perspective((3.131f/4.f), swapchain_extent.width / (float)swapchain_extent.height, .1f, 10.f, ubo.proj);
	ubo.proj[1][1] *= -1;

	void *data;
	vkMapMemory(dev, uniform_buffers_memory[current_image], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(dev, uniform_buffers_memory[current_image]);
}
void updateVertexBuffer(VkCommandPool cmd_pool) {
  // No staging buffer b/c doing this e/ frame, so all that staging business would probably be slower.
	VkDeviceSize buffer_size = sizeof(vertices[0]) * n_vertices;
  void* data;
  vkMapMemory(dev, vertex_buffer_memory, 0, buffer_size, 0, &data);
  memcpy(data, vertices, (size_t)buffer_size);
  vkUnmapMemory(dev, vertex_buffer_memory);
}

void drawFrame() {
  igGetIO();
  igRender();

	vkWaitForFences(dev, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	uint32_t image_index;
	VkResult result = vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
	if(result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain(); return;
	} else if (result != VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) { printf("Failed to acquire swap chain image!"); }
	vkResetFences(dev, 1, &in_flight_fences[current_frame]);
	vkResetCommandBuffer(command_buffers[current_frame],0);
	recordCommandBuffer(command_buffers[current_frame],image_index);

	updateUniformBuffer(current_frame);
  updateVertexBuffer(command_pool);

	VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};
	VkSubmitInfo submit_info = (VkSubmitInfo){
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = wait_semaphores,
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffers[current_frame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signal_semaphores
	};
	if(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS) printf("Failed to submit draw command buffer! :(");

	VkSwapchainKHR swapchains[] = {swapchain};
	VkPresentInfoKHR present_info = (VkPresentInfoKHR){
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signal_semaphores, // which semaphores to wait on before presentation can happen
		.swapchainCount = 1,
		.pSwapchains = swapchains,
		.pImageIndices = &image_index,
		.pResults = NULL
	};
	result = vkQueuePresentKHR(present_queue, &present_info);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
		framebuffer_resized = false;
		recreateSwapchain();
	} else if (result != VK_SUCCESS) { printf("Failed to present swap chain image!"); }
	// TODO later: resizing window quickly causes validation layers message about imageExtent = (,) slightly out of bounds of min/maxImageExtent = (,). See comment in "Swap Chain Recreation" section for possible fixes, it's quite far down. The GTX1660 guy's solution doesn't work for me, prolly need to do s/t on glfw side like just wait while user is resizing window, as Alexander suggests.

	current_frame = (current_frame+1) % MAX_FRAMES_IN_FLIGHT;
}

void initVulkan() {
	createInstance();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createFramebuffers();
	createVertexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}


// ACTUAL PARTICLE SIM STUFF
typedef struct { float x,y,vx,vy; vec3 color; } Particle;
#define n_particles 800
Particle particles[n_particles];
Particle particles2[n_particles];
// typedef struct {
//   struct GridCell *nw,*ne,*sw,*se;
//   float center_x,center_y,w,h;
//   unsigned int n_ps; kvec_t(unsigned int) particle_inds;
// } GridCell;
// GridCell grid;

// If a grid cell has a length of max_interact_distance then only need to check the surrounding 8. But that is for max_interact_dist.
// The point of this report is to have ALL particles interact no matter distance

int seed = 41;
bool paused = false;
bool do_d_sq = false;
bool show_particles2 = false;
float max_interaction_dist = 150.f;
float drag = 0.5;

float forces[16] = {0};

void createParticleGroup(int start, int n, vec3 color) {
  Particle p;
  for(int i=start; i<n+start; i++) {
		p.x = (rand()/(float)RAND_MAX)*90+5;
		p.y = (rand()/(float)RAND_MAX)*90+5;
    p.vx = p.vy = 0.f;
		glm_vec3_copy(color, p.color);
    particles[i] = p;
	}
  memcpy(particles2+start, particles+start, sizeof(Particle)*n);
  particles2[start].x = 27.f;
}
void updateParticleVertices(bool show_particles2) {
  Particle *ps = particles;
  if(show_particles2) { ps = particles2; }
  for(int i=0; i<n_particles; i++) {
    vertices[i].pos[0] = ps[i].x/50-1;
    vertices[i].pos[1] = ps[i].y/50-1;
    glm_vec3_copy(ps[i].color, vertices[i].color);
  }
}
void ParticleGroupInteraction1(int i_gr1, size_t len_gr1, int i_gr2, size_t len_gr2, float g, float dt) {
  for(int i=i_gr1; i<i_gr1+len_gr1; i++) { float fx=0.f; float fy=0.f;
  for(int j=i_gr2; j<i_gr2+len_gr2; j++) {
	  Particle a = particles[i];
    Particle b = particles[j];
    float dx = a.x-b.x;
    float dy = a.y-b.y;
    float d_sq = dx*dx+dy*dy;
    if(d_sq > 0.f && sqrtf(d_sq) < max_interaction_dist) {
      float F=0.f;
      if(do_d_sq) {
        F = g/d_sq;
      } else { F = g/sqrtf(d_sq); }

      fx += F*dx;
      fy += F*dy;
    }
    a.vx = (a.vx+fx)*drag;
    a.vy = (a.vy+fy)*drag;

    if(a.x <  0.f) a.vx = fabsf(a.vx)       ;
    if(a.x >100.f) a.vx = fabsf(a.vx) * -1.f; 
    if(a.y <  0.f) a.vy = fabsf(a.vy)       ;
    if(a.y >100.f) a.vy = fabsf(a.vy) * -1.f;

    a.x += a.vx * dt;
    a.y += a.vy * dt;
 
    particles[i] = a;
	}}
}
void ParticleGroupInteraction2(int i_gr1, size_t len_gr1, int i_gr2, size_t len_gr2, float g, float dt) {
  for(int i=i_gr1; i<i_gr1+len_gr1; i++) { float fx=0.f; float fy=0.f;
  for(int j=i_gr2; j<i_gr2+len_gr2; j++) {
	  Particle a = particles2[i];
    Particle b = particles2[j];
    float dx = a.x-b.x;
    float dy = a.y-b.y;
    float d_sq = dx*dx+dy*dy;
    if(d_sq > 0.f && sqrtf(d_sq) < max_interaction_dist) {
      float F=0.f;
      if(do_d_sq) {
        F = g/d_sq;
      } else { F = g/sqrtf(d_sq); }

      fx += F*dx;
      fy += F*dy;
    }
    a.vx = (a.vx+fx)*drag;
    a.vy = (a.vy+fy)*drag;

    if(a.x <  0.f) a.vx = fabsf(a.vx)       ;
    if(a.x >100.f) a.vx = fabsf(a.vx) * -1.f; 
    if(a.y <  0.f) a.vy = fabsf(a.vy)       ;
    if(a.y >100.f) a.vy = fabsf(a.vy) * -1.f;

    a.x += a.vx * dt;
    a.y += a.vy * dt;
 
    particles2[i] = a;
  }}
}

void reinitPositions(int seed) {
  srand(seed);
  for(int i=0; i<n_particles; i++) {
    particles[i].x = particles2[i].x = (rand()/(float)RAND_MAX)*90+5;
    particles[i].y = particles2[i].y = (rand()/(float)RAND_MAX)*90+5;
    particles[i].vx = particles[i].vy = particles2[i].vx = particles2[i].vy = 0.f;
	}
  memcpy(particles2, particles, sizeof(Particle)*n_particles);
  //particles2[10].x = 20.f;
}
void captureDistanceData(char *filename) { // TODO
  
}
void reconstructGrid() { // TODO
  // Traverse total "particles". Based on the particles' positions put in appropriate grid cell.
}
double calcAvgDistSq(Particle ps1[], Particle ps2[], int n) {
  double sum = 0.;
  for(int i=0; i<n; i++) {
    float dx = ps1[i].x - ps2[i].x;
    float dy = ps1[i].y - ps2[i].y;
    float d = sqrtf(dx*dx+dy*dy);
    sum+=d;
  }
  return sum/n;
}

// IMGUI STUFF
void initImgui() {
	// ImGUI descriptor pool
	VkDescriptorPoolSize pool_sizes[] =	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = (VkDescriptorPoolCreateInfo){
	  .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	  .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
	  .maxSets = 1000,
	  .poolSizeCount = 11,
	  .pPoolSizes = pool_sizes
  };

	if(vkCreateDescriptorPool(dev, &pool_info, NULL, &imgui_descriptor_pool) != VK_SUCCESS) printf("Failed to create ImGUI descriptor pool! :(");

	// 2: initialize core imgui stuff
	igCreateContext(NULL);

	ImGui_ImplGlfw_InitForVulkan(window,true);

  // IO
  ImGuiIO* ioptr = igGetIO();
  ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enables Keyboard Controls
  ioptr->IniFilename = NULL;
  ImFontAtlas_AddFontFromFileTTF(ioptr->Fonts, "./lib/Px437_IBM_BIOS.ttf", 8.0f, 0, 0); // Cool font :)

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = (ImGui_ImplVulkan_InitInfo){
	  .Instance = instance,
	  .PhysicalDevice = physical_dev,
	  .Device = dev,
	  .Queue = graphics_queue,
	  .DescriptorPool = imgui_descriptor_pool,
	  .MinImageCount = 3,
	  .ImageCount = 3,
	  .MSAASamples = VK_SAMPLE_COUNT_1_BIT
  };

	ImGui_ImplVulkan_Init(&init_info, render_pass); // TODO make new render pass for imgui?

	// GPU command to upload imgui font textures
  VkCommandBuffer cmd = beginSingleTimeCommands(command_pool); // TODO make new command pool for imgui?
  ImGui_ImplVulkan_CreateFontsTexture(cmd);
  endSingleTimeCommands(cmd, command_pool);

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}
void imguiFrame(float dt, float t_running, float *t, float *dropped_time) {
  bool my_bool = true;
  igSetNextWindowPos((ImVec2){0.f,0.f}, 0, (ImVec2){0.f,0.f}); igSetNextWindowSize((ImVec2){320.f, 280.f}, 0);
  igBegin("Controls!", &my_bool, 0);

  if(igButton("Pause", (ImVec2){60.f,14.f})) {paused = !paused;}
  igText("Running time: %03.0fs | FPS: %.0f", t_running, igGetIO()->Framerate);
  igText("Physics time: %03.0fs | TPS: %.0f/%.0f", *t, fminf(1.f/dt, igGetIO()->Framerate), 1/dt);
  igText("Delayed physics time: %.3f", *dropped_time);
  if(igButton("Reinit", (ImVec2){60.f,14.f})) { *dropped_time += *t; *t = 0; reinitPositions(seed);} igSameLine(60.f,20.f); igDragInt("##", &seed, .1f, 0, UINT32_MAX, "seed: %d", 0);

  // TODO: randomize forces button, followed by sliders for a bunch of forces
  igText("\nCONTROL INTER-PARTICLE FORCES");
  igCheckbox("Interact based on d^2?", &do_d_sq);
  igDragFloat("##a", &max_interaction_dist, 1.f, 0.f, 150.f, "Interaction distance: %.1f",0);
  igDragFloat("##d", &drag, .001f, 0.f, 1.f, "Drag: %.3f", 0);
  if(igButton("Randomize", (ImVec2){80.f,14.f})) { for(int i=0; i<16; i++) forces[i] = (rand()/(float)RAND_MAX)*1.f - .5f; }
  igDragFloat4("red", forces,      .001f, -.5f, .5f, "%.3f",0);
  igDragFloat4("yel", &forces[4],  .001f, -.5f, .5f, "%.3f",0);
  igDragFloat4("grn", &forces[8],  .001f, -.5f, .5f, "%.3f",0);
  igDragFloat4("blu", &forces[12], .001f, -.5f, .5f, "%.3f",0);

  igText("\nSECOND SET OF PARTICLES");
  igCheckbox("Show particles2 instead", &show_particles2);
  igText("Avg dist from particles1: %.3lf", calcAvgDistSq(particles, particles2, n_particles));

  igEnd();
}


void mainLoop() {
  float dt = 0.001f; // Fixed timestep
  float t = 0; // How old the PHYSICS SIMULATION is
  float t_running = 0; // How long the APPLCIATION has been running
  float total_dropped_time = 0.f; float drop_phys_time = 0.f;
  struct timespec ts_app_start; timespec_get(&ts_app_start,TIME_UTC);
	while(!glfwWindowShouldClose(window)) {
    struct timespec ts_current; timespec_get(&ts_current,  TIME_UTC);
    t_running = (ts_current.tv_sec-ts_app_start.tv_sec) + ((ts_current.tv_nsec-ts_app_start.tv_nsec)/1e9);
    printf("%f\r",t_running-t);

    // Fixed timestep. Physics not harmed by FPS fluctuations.
    // Can drop/delay physics ticks. I.e. If paused or FPS dips, don't go superspeed to try to catch back up.
    if(t_running-t-total_dropped_time > dt) {
      drop_phys_time = t_running-t-total_dropped_time-dt;
      drop_phys_time = drop_phys_time - fmodf(drop_phys_time,dt);
      if(drop_phys_time > 0.f) {total_dropped_time += drop_phys_time;}

      if(!paused) {
        t+=dt;

        // TODO bad code
        // (Well, ~especially~ bad code relative the rest of this)
        ParticleGroupInteraction1(  0,200,   0,200, forces[0],dt);
        ParticleGroupInteraction1(  0,200, 200,200, forces[1],dt);
        ParticleGroupInteraction1(  0,200, 400,200, forces[2],dt);
        ParticleGroupInteraction1(  0,200, 600,200, forces[3],dt);
        ParticleGroupInteraction1(200,200,   0,200, forces[4],dt);
        ParticleGroupInteraction1(200,200, 200,200, forces[5],dt);
        ParticleGroupInteraction1(200,200, 400,200, forces[6],dt);
        ParticleGroupInteraction1(200,200, 600,200, forces[7],dt);
        ParticleGroupInteraction1(400,200,   0,200, forces[8],dt);
        ParticleGroupInteraction1(400,200, 200,200, forces[9],dt);
        ParticleGroupInteraction1(400,200, 400,200, forces[10],dt);
        ParticleGroupInteraction1(400,200, 600,200, forces[11],dt);
        ParticleGroupInteraction1(600,200,   0,200, forces[12],dt);
        ParticleGroupInteraction1(600,200, 200,200, forces[13],dt);
        ParticleGroupInteraction1(600,200, 400,200, forces[14],dt);
        ParticleGroupInteraction1(600,200, 600,200, forces[15],dt);

        ParticleGroupInteraction2(  0,200,   0,200, forces[0],dt);
        ParticleGroupInteraction2(  0,200, 200,200, forces[1],dt);
        ParticleGroupInteraction2(  0,200, 400,200, forces[2],dt);
        ParticleGroupInteraction2(  0,200, 600,200, forces[3],dt);
        ParticleGroupInteraction2(200,200,   0,200, forces[4],dt);
        ParticleGroupInteraction2(200,200, 200,200, forces[5],dt);
        ParticleGroupInteraction2(200,200, 400,200, forces[6],dt);
        ParticleGroupInteraction2(200,200, 600,200, forces[7],dt);
        ParticleGroupInteraction2(400,200,   0,200, forces[8],dt);
        ParticleGroupInteraction2(400,200, 200,200, forces[9],dt);
        ParticleGroupInteraction2(400,200, 400,200, forces[10],dt);
        ParticleGroupInteraction2(400,200, 600,200, forces[11],dt);
        ParticleGroupInteraction2(600,200,   0,200, forces[12],dt);
        ParticleGroupInteraction2(600,200, 200,200, forces[13],dt);
        ParticleGroupInteraction2(600,200, 400,200, forces[14],dt);
        ParticleGroupInteraction2(600,200, 600,200, forces[15],dt);
      }

      updateParticleVertices(show_particles2);
    }
  
		glfwPollEvents();

    ImGui_ImplVulkan_NewFrame(); ImGui_ImplGlfw_NewFrame(); igNewFrame(); // Functions you just gotta call before new imgui frame
    imguiFrame(dt, t_running, &t, &total_dropped_time);

		drawFrame();
	}
	vkDeviceWaitIdle(dev);
}

void cleanup() {
	cleanupSwapchain();

	for(size_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(dev,uniform_buffers[i],NULL);
		vkFreeMemory(dev,uniform_buffers_memory[i],NULL);
	}

	vkDestroyDescriptorPool(dev,descriptor_pool,NULL); // automatically frees descriptor sets too
	//free(descriptor_sets);
	vkDestroyDescriptorSetLayout(dev,descriptor_set_layout,NULL);

  vkDestroyDescriptorPool(dev,imgui_descriptor_pool,NULL);

	vkDestroyBuffer(dev,vertex_buffer,NULL);
	vkFreeMemory(dev,vertex_buffer_memory,NULL);

	for(uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(dev,image_available_semaphores[i],NULL);
		vkDestroySemaphore(dev,render_finished_semaphores[i],NULL);
		vkDestroyFence(dev,in_flight_fences[i],NULL);
	}
	vkDestroyCommandPool(dev,command_pool,NULL);

	vkDestroyPipeline(dev,graphics_pipeline,NULL);
	vkDestroyPipelineLayout(dev,pipeline_layout,NULL);
	vkDestroyRenderPass(dev,render_pass,NULL);

	free(swapchain_image_views); swapchain_image_views = VK_NULL_HANDLE;

  ImGui_ImplVulkan_Shutdown();

	vkDestroyDevice(dev,NULL);
	vkDestroySurfaceKHR(instance,surface,NULL);
	vkDestroyInstance(instance,NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void run() {
  ProfilerStart("out.prof");
  srand(0);
  createParticleGroup(0  ,200, (vec3){.7f,0.f,0.f});
  createParticleGroup(200,200, (vec3){.5f,.5f,0.1f});
  createParticleGroup(400,200, (vec3){.1f,.6f,0.f});
  createParticleGroup(600,200, (vec3){.05f,.05f,.5f});

  n_vertices = n_particles;
  printf("%d\n",n_vertices);
  vertices = malloc(sizeof(Vertex)*n_vertices);

	initWindow();
	initVulkan();
  initImgui();

	mainLoop();

	cleanup();
  ProfilerStop();
}

int main() {
	run();
	return 0;
}