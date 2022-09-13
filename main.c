#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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
GLFWwindow* window;
VkInstance instance;
VkPhysicalDevice physical_dev = VK_NULL_HANDLE;
VkDevice dev;
VkQueue graphics_queue;
VkQueue present_queue;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
VkImage* swapchain_images;
VkFormat swapchain_image_format;
VkExtent2D swapchain_extent;
VkImageView* swapchain_image_views;
VkRenderPass render_pass;
VkPipelineLayout pipeline_layout;


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

// NOTE NOTE NOTE NOTE: I believe this is an issue: "details" is malloc'd several times
// b/c querySwapchainSupport is used several times.
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
		free(swapchain_support.formats); free(swapchain_support.presentModes);
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

	uint32_t n_image = swapchainSupport.capabilities.minImageCount + 1;
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
	free(swapchainSupport.formats); free(swapchainSupport.presentModes);

	vkGetSwapchainImagesKHR(dev, swapchain, &n_image, NULL);
	swapchain_images = malloc(sizeof(VkImage)*n_image); // MALLOC'D swapchain_images
	vkGetSwapchainImagesKHR(dev, swapchain, &n_image, swapchain_images);

	swapchain_image_format = surface_format.format;
	swapchain_extent = extent;
}

// NOTE NOTE NOTE NOTE Destroy is giving validation layers error that the VkImageView handle is not valid.
// I tried only destroying swapchain_image_views[0], and it also has an error, so ALL the VkImageViews in the array are bad I guess.
// I tried messing with the VkImageViewCreateInfo and messing it up, and it still only complains on Destroy. So there could be s/t wrong when creating.
//   I.e. there is no check if the create_info is valid yet, it seems.
//   So there could be s/t wrong in the create info but it's hard to know.
//     I don't think swapchain_image_format is getting out of scope.
//     My guess is s/t in the swapchain_images[] is wrong?
//     or some simple insidious mispelling?
// Remember to uncomment the for loop to Destroy ALL imageViews when you do figure this out.
void createImageViews() {
	uint32_t n_image = vkGetSwapchainImagesKHR(dev, swapchain, &n_image, NULL);
	// ^ I should just make n_image a global at this point
	swapchain_image_views = malloc(sizeof(VkImageView) * n_image); // MALLOC'D swapchain_image_views
	for(size_t i=0; i < n_image; i++) {
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
		if(vkCreateImageView(dev, &create_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS) printf("Swapchain image view (index %lu) creation failed! :(",i);
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


	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = (VkPipelineVertexInputStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = NULL,
		.vertexAttributeDescriptionCount =0,
		.pVertexAttributeDescriptions = NULL
	};
	VkPipelineInputAssemblyStateCreateInfo input_asm_create_info = (VkPipelineInputAssemblyStateCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // plenty of ways to optimize vertices w/ different options here
		.primitiveRestartEnable = VK_FALSE
	};
	VkViewport viewport = (VkViewport){
		.x=0.f,
		.y=0.f,
    .width=(float)swapchain_extent.width,
		.height=(float)swapchain_extent.height,
		.minDepth = 0.f,
		.maxDepth = 1.f
	};
	VkRect2D scissor = (VkRect2D){
		.offset = {0,0},
		.extent = swapchain_extent
	};
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

	vkDestroyShaderModule(dev,vert_shdr_module,NULL);
	vkDestroyShaderModule(dev,frag_shdr_module,NULL);
	free(vert_shdr_code); free(frag_shdr_code);
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
	
	VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass
	};
	if(vkCreateRenderPass(dev, &render_pass_create_info, NULL, &render_pass) != VK_SUCCESS) printf("Failed to create render pass! :(");
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
}

void mainLoop() {
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void cleanup() {
	vkDestroyPipelineLayout(dev,pipeline_layout,NULL);
	vkDestroyRenderPass(dev,render_pass,NULL);

	//TODO ImageViews invalid accorind to validation layers when getting destroyed apparently? idk.
	uint32_t n_image; // Yep should probably just be a global
	vkGetSwapchainImagesKHR(dev, swapchain, &n_image, NULL);
//	/*for(uint32_t i=0; i<n_image; i++)*/ vkDestroyImageView(dev,swapchain_image_views[0],NULL);
	free(swapchain_image_views); swapchain_image_views = VK_NULL_HANDLE;

	vkDestroySwapchainKHR(dev,swapchain,NULL);
	vkDestroyDevice(dev,NULL);
	vkDestroySurfaceKHR(instance,surface,NULL);
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
