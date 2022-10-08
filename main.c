#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <error.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
// #include <shaderc/shaderc.h>

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

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
const uint32_t MAX_FRAMES_IN_FLIGHT = 2; uint32_t current_frame = 0;
GLFWwindow *window;
VkInstance instance;
VkPhysicalDevice physical_dev = VK_NULL_HANDLE;
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

typedef struct { vec2 pos; vec3 color; } Vertex;
#define n_vertices 3
const Vertex vertices[n_vertices] = {
	{{ .0f,-.5f}, {1.f,0.f,0.f}},
	{{ .5f, .5f}, {0.f,1.f,0.f}},
	{{-.5f, .5f}, {1.f,1.f,0.f}}
};
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
		.format = VK_FORMAT_R32G32_SFLOAT, // uses color format names. This really means vec2. Others: R32/R32G32/R32G32B32/R32G32B32A32 basically mean float/vec2/vec3/vec4. And _SINT/_UINT/_SFLOAT == ivec#/uvec#/dvec# (the _SFLOAT one is 64 bit unlike the others, takes up 2 "slots" per vec item).
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
	window = glfwCreateWindow(WIDTH,HEIGHT,"Vulkan", NULL,NULL);
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

bool isDeviceSuitable(VkPhysicalDevice phys_dev) {
	// There's plenty you could do here to select the device you want, but idc.
	//VkPhysicalDeviceProperties          dev_properties;
	//VkPhysicalDeviceFeatures            dev_features;
	//vkGetPhysicalDeviceProperties(dev, &dev_properties);
	//vkGetPhysicalDeviceFeatures  (dev, &dev_features);

	QueueFamilyIndices inds; findQueueFamilies(phys_dev, &inds);

	bool supports_extensions = checkDeviceExtensionSupport(phys_dev);

	bool swapchain_adequate = false;
	if(supports_extensions) {
		SwapchainSupportDetails swapchain_support = querySwapchainSupport(phys_dev);
		swapchain_adequate = swapchain_support.n_format>0 && swapchain_support.n_presentMode>0;
		free(swapchain_support.formats); free(swapchain_support.presentModes); // FREE'D swapchain_support
	}
	
	printf("%u, %u, %u\n", inds.graphicsFamily, inds.presentFamily, UINT32_MAX);
	puts(inds.graphicsFamily == UINT32_MAX ? "no g" : "yes g");
	puts(inds.presentFamily  == UINT32_MAX ? "no p" : "yes p");
	puts(inds.presentFamily != UINT32_MAX && inds.graphicsFamily != UINT32_MAX ? "yay" : "nay");
	return (inds.graphicsFamily != UINT32_MAX &&
			    inds.presentFamily != UINT32_MAX &&
					supports_extensions &&
					swapchain_adequate);
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
		printf("is this the prob");
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

	// VkPhysicalDeviceFeatures dev_features;

	VkDeviceCreateInfo create_info = (VkDeviceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = queue_create_infos,
		.pEnabledFeatures = NULL,
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

void createImageViews() {
	swapchain_image_views = malloc(sizeof(VkImageView) * n_image); // MALLOC'D swapchain_image_views
	for(uint32_t i=0; i < n_image; i++) {
		VkImageViewCreateInfo create_info = (VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain_image_format,
			.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.a = VK_COMPONENT_SWIZZLE_IDENTITY, // try changing these for fun!
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel = 0,
			.subresourceRange.levelCount = 1,
			.subresourceRange.baseArrayLayer = 0,
			.subresourceRange.layerCount = 1,
			.flags = 0,
			.pNext = NULL
		};
		if(vkCreateImageView(dev, &create_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS) printf("Swapchain image view (index %u) creation failed! :(",i);
	}
}


// TODO: Failed attempt to get shaderc working (integrated glsl compilation)
//void compileGlsl2Svp(char* source, shaderc_shader_kind kind) {
//	const shaderc_compiler_t compiler;// = shaderc_compiler_initialize();
//	shaderc_compile_options_t options;// = shaderc_compile_options_initialize();
//	shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_size); // there's also "_performance" instead of "_size"
//	
//}

char* readFile(char* filenm, unsigned long* sz) {
	char* buf = NULL;
	FILE* f = fopen(filenm,"rb");
	if(f!=NULL) if(fseek(f,0,SEEK_END)==0) {
		size_t bufsz=ftell(f);  if(bufsz==-1) printf("AHHHH");
		buf = malloc(bufsz+1); if(!buf)     printf("Mem error!");  // MALLOC'D shader file contents
		if(fseek(f,0,SEEK_SET)!=0)          printf("AHHHH2");
		fread(buf,1,bufsz,f);
		//printf("BUFSZ: %ld",bufsz);
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
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // operations to wait on and stages in which the ops occur. So here we wait for color attachment output stage, so swapchain finishes reading image before we access it
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};
	
	VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};
	if(vkCreateRenderPass(dev, &render_pass_create_info, NULL, &render_pass) != VK_SUCCESS) printf("Failed to create render pass! :(");
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
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // plenty of ways to optimize vertices w/ different options here
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
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
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
		.setLayoutCount = 0,
		.pSetLayouts = NULL,
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
}


void createFramebuffers() {
	swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * n_image);
	
	for(size_t i=0; i<n_image; i++) {
		VkImageView attachments[] = {swapchain_image_views[i]};
		
		VkFramebufferCreateInfo framebuffer_create_info = (VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = 1,
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

void copyBuffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) {
	VkCommandBufferAllocateInfo alloc_info = (VkCommandBufferAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = command_pool,
		.commandBufferCount = 1
	};
	VkCommandBuffer tmp_cmd_buf;
	vkAllocateCommandBuffers(dev, &alloc_info, &tmp_cmd_buf);

	VkCommandBufferBeginInfo begin_info = (VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(tmp_cmd_buf, &begin_info);

	VkBufferCopy copy_region = (VkBufferCopy){
		.srcOffset = 0, // optional
		.dstOffset = 0, // optional
		.size = size
	};
	vkCmdCopyBuffer(tmp_cmd_buf, src_buf, dst_buf, 1, &copy_region);
	vkEndCommandBuffer(tmp_cmd_buf);

	VkSubmitInfo submit_info = (VkSubmitInfo){
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &tmp_cmd_buf
	};
	vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue); // Alternatively use a fence, which could allow schedule multiple transfers at same time and wait for all to complete. Driver might be able to optimize for you a little better that way.
	vkFreeCommandBuffers(dev, command_pool, 1, &tmp_cmd_buf);
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

	createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer, &vertex_buffer_memory);

	copyBuffer(staging_buffer, vertex_buffer, buffer_size);

	vkDestroyBuffer(dev, staging_buffer, NULL);
	vkFreeMemory(dev, staging_buffer_memory, NULL);
}

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

	VkClearValue clear_color = {{{0.f,0.f,0.f,1.f}}};
	VkRenderPassBeginInfo render_pass_begin_info = (VkRenderPassBeginInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = render_pass,
		.framebuffer = swapchain_framebuffers[image_ind],
		.renderArea.offset = {0,0},
		.renderArea.extent = swapchain_extent,
		.clearValueCount = 1,
		.pClearValues = &clear_color // defines the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
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

	vkCmdDraw(command_buffers[current_frame], (uint32_t)n_vertices, 1,0,0); // args: cb, vertexCount, instanceCount, firstVertex (lowest val of gl_VertexIndex), firstInstance

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
	glfwGetFramebufferSize(window, &w,&h);
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


void drawFrame() {
	vkWaitForFences(dev, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	uint32_t image_index;
	VkResult result = vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
	if(result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain(); return;
	} else if (result != VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) { printf("Failed to acquire swap chain image!"); }
	vkResetFences(dev, 1, &in_flight_fences[current_frame]);
	vkResetCommandBuffer(command_buffers[current_frame],0);
	recordCommandBuffer(command_buffers[current_frame],image_index);

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
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createVertexBuffer();
	createCommandBuffers();
	createSyncObjects();
}

void mainLoop() {
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		drawFrame();
	}
	vkDeviceWaitIdle(dev);
}

void cleanup() {
	cleanupSwapchain();

	vkDestroyBuffer(dev,vertex_buffer,NULL);
	vkFreeMemory(dev, vertex_buffer_memory, NULL);

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

	vkDestroyDevice(dev,NULL);
	vkDestroySurfaceKHR(instance,surface,NULL);
	vkDestroyInstance(instance,NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void run() {
	initWindow();
	initVulkan();
	mainLoop(window);
	cleanup(window);
}

int main() {
	run();
	return 0;
}
