
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <talloc.h>

#define MIN(x,y) ({ \
    typeof(x) _x = (x);     \
    typeof(y) _y = (y);     \
    (void) (&_x == &_y);    \
    _x < _y ? _x : _y; })

#define MAX(x,y) ({ \
    typeof(x) _x = (x);     \
    typeof(y) _y = (y);     \
    (void) (&_x == &_y);    \
    _x > _y ? _x : _y; })

const int MAX_FRAMES_IN_FLIGHT = 2;

struct blobby {
	size_t len;
	const char *data;
};

/**
 * Allocates and returns a blobby from a file.
 *
 * @param path to file
 * @return blobby or NULL on error
 */
struct blobby *
blobby_from_file(const char *path) {
	struct stat fileinfo;
	struct blobby *blobby;

	int fd;
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return NULL;
	}

	if (fstat(fd, &fileinfo) != 0) {
		close(fd);
		perror("fstat");
		return NULL;
	}
	
	blobby = talloc(NULL, struct blobby);
	blobby->len = fileinfo.st_size;
	blobby->data = talloc_size(blobby, blobby->len);

	if (read(fd, (void*)(blobby->data), blobby->len) != blobby->len) {
		perror("read");
		close(fd);
		talloc_free(blobby);
		return NULL;
	}

	close(fd);
	return blobby;
}

/** End Blobby */


/** Generic */


int
error(const char *msg) {
	fputs(msg, stderr);
	exit(1);
	return 0;
}


/** End Generic */

/** Window stuff (glfw) */

GLFWwindow *window_init(void) {
	GLFWwindow *window;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);

	return window;
}

VkInstance createInstance(GLFWwindow *window) {
	VkInstance instance;

        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

       uint32_t glfwExtensionCount;
	const char **glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	printf("Need %d extensions\n", glfwExtensionCount);
	for (int i = 0; i < glfwExtensionCount; i++){
		printf("  - %s\n", glfwExtensions[i]);
	}

	VkInstanceCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

	createInfo.enabledLayerCount = 0;
	createInfo.pNext = NULL;

        if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
		error("Unable tp create instance");
		return 0;
	}
	return instance;
}

struct swap_chain_data {
	VkSwapchainKHR swap_chain;
	VkImage *images;
	uint32_t nimages;
	VkImageView *image_views; 
	VkFormat image_format; // Swap chain image formt
	VkExtent2D extent; // Extent of the images
};

struct queue_family_indices {
	uint32_t graphics_family;
	uint32_t present_family;
	bool has_graphics;
	bool has_present;
};

struct swap_chain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t nformats;
    uint32_t npresentmodes;
    VkSurfaceFormatKHR *formats;
    VkPresentModeKHR *presentModes;
};

/*bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
}*/

VkSurfaceKHR
create_surface(VkInstance instance, GLFWwindow *window) {
	VkSurfaceKHR surface;	
	VkResult result;
	printf("Instance: %lld, Winf %p\n", (uint64_t)instance, window);
	result = glfwCreateWindowSurface(instance, window, NULL, &surface);
	if (result != VK_SUCCESS) {
		printf("failed to create window surface! %d\n", result);
		error("bailing");
	}
	return surface;
}

VkPhysicalDevice
pickPhysicalDevice(VkInstance instance) {
	uint32_t deviceCount = 0;
	// Urgh; leaky leaky leak
	VkPhysicalDevice *devices;

	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	if (deviceCount == 0) {
		error("failed to find GPUs with Vulkan support!");
		return NULL;
	}

	devices = calloc(deviceCount, sizeof(VkPhysicalDevice)); // FIXME: talloc

	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
	printf("Found %d devices\n", deviceCount);

	/*
	for (i = 0 ; i < deviceCount ; i++) {
		if (isDeviceSuitable(device)) {
			physicalDevice = devicqueue_info;
			break;
		}
	}*/
	return devices[0];
}

struct queue_family_indices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
	struct queue_family_indices indices = { 0,0,false,false };
	VkQueueFamilyProperties *properties;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

	properties = talloc_array(NULL, VkQueueFamilyProperties, queueFamilyCount);
	memset(properties, 0, sizeof(VkQueueFamilyProperties) * queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, properties);

	for (int i = 0; i < queueFamilyCount ; i ++){
		printf("Loop %d %p %d %p\n",i, device, i, surface);
		VkQueueFamilyProperties *queue_family = properties + i;

		if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
			indices.has_graphics = true;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.present_family = i;
			indices.has_present = true;
		}

		if (indices.has_present && indices.has_graphics) {
			break;
		}
	}

	return indices;
}

VkDevice
createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue *graphicsQueue, VkQueue *presentQueue) {
	struct queue_family_indices queue_family_indices;
	float queue_priority = 1.0f;
	VkDevice device;
	VkDeviceQueueCreateInfo queue_info[2];
	const char *extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueCreateInfoCount = 2,
		.pQueueCreateInfos = queue_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = extensions,
		.pEnabledFeatures = NULL,
	};

	// Once for graphics, once for present
		
	queue_family_indices = find_queue_families(physicalDevice, surface);

	queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info[0].queueFamilyIndex = queue_family_indices.graphics_family;;
	queue_info[0].queueCount = 1;
	queue_info[0].pQueuePriorities = &queue_priority;

	queue_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info[1].queueFamilyIndex = queue_family_indices.present_family;;
	queue_info[1].queueCount = 1;
	queue_info[1].pQueuePriorities = &queue_priority;
		

	vkCreateDevice(physicalDevice, &device_info, NULL, &device);

	vkGetDeviceQueue(device, queue_family_indices.graphics_family, 0, graphicsQueue);
	vkGetDeviceQueue(device, queue_family_indices.present_family, 0, presentQueue);

	return device;
}




const VkSurfaceFormatKHR *chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats, uint32_t nformats) {
	for (uint32_t i = 0 ; i < nformats ; i ++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats + i;
		}
	}

	return availableFormats;
}


VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR* availablePresentModes, uint32_t npresentmodes) {
	for (uint32_t i = 0; i< npresentmodes ; i ++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}



VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities) {
        if (capabilities->currentExtent.width != UINT32_MAX) {
            return capabilities->currentExtent;
        } else {
            int width, height;
	    width = 800;
	    height = 600;
	    fprintf(stderr, "Hardcoded size you fools!\n");
           // glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
		    .width = width,
		    .height = height,
            };

            actualExtent.width = MAX(capabilities->minImageExtent.width, MIN(capabilities->maxImageExtent.width, actualExtent.width));
            actualExtent.height = MAX(capabilities->minImageExtent.height, MIN(capabilities->maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }


struct swap_chain_support_details *
querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
	struct swap_chain_support_details *details;
	details = talloc(NULL, struct swap_chain_support_details);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats,NULL);

	if (details->nformats != 0) {
		details->formats = talloc_array(details, VkSurfaceFormatKHR, details->nformats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details->nformats, details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes, NULL);

	if (details->npresentmodes != 0) {
		details->presentModes = talloc_array(details, VkPresentModeKHR, details->npresentmodes);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details->npresentmodes, details->presentModes);
	}

	return details;
}

struct swap_chain_data *create_swap_chain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	struct swap_chain_data *scd = talloc_zero(NULL, struct swap_chain_data);

	struct swap_chain_support_details *swapChainSupport = querySwapChainSupport(physical_device, surface);

        const VkSurfaceFormatKHR *surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport->formats, swapChainSupport->nformats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport->presentModes, swapChainSupport->npresentmodes);
        VkExtent2D extent = chooseSwapExtent(&swapChainSupport->capabilities);

	// FIXME: Why is this '[1]'??  That seems ... wrong
	uint32_t imageCount = swapChainSupport[0].capabilities.minImageCount + 1;
	if (swapChainSupport->capabilities.maxImageCount > 0 && imageCount > swapChainSupport->capabilities.maxImageCount) {
		imageCount = swapChainSupport->capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat->format;
	createInfo.imageColorSpace = surfaceFormat->colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	struct queue_family_indices queue_family_indices;
	queue_family_indices = find_queue_families(physical_device, surface);

	if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		uint32_t queueFamilyIndices[2];
		queueFamilyIndices[0] = queue_family_indices.graphics_family;
		queueFamilyIndices[1] = queue_family_indices.present_family;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport->capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, NULL, &scd->swap_chain) != VK_SUCCESS) {
		error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, scd->swap_chain, &scd->nimages, NULL);
	scd->images = talloc_array(scd, VkImage, scd->nimages);
	vkGetSwapchainImagesKHR(device, scd->swap_chain, &scd->nimages, scd->images);

	scd->image_format = surfaceFormat->format;
	scd->extent = extent;

	return scd;
}


void create_image_views(VkDevice device, struct swap_chain_data *scd) {
	scd->image_views = talloc_array(scd, VkImageView, scd->nimages);

        for (uint32_t i = 0; i < scd->nimages; i++) {
            VkImageViewCreateInfo createInfo = { 0 };
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = scd->images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = scd->image_format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, NULL, &scd->image_views[i]) != VK_SUCCESS) {
		    error("failed to create image views!");
            }
        }
    }

/** End Window */



VkShaderModule
create_shader(VkDevice device, struct blobby *blobby) {
	VkShaderModuleCreateInfo shader_info;
	VkShaderModule shader_module;
		
	if (!blobby) return 0;

	shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.codeSize = blobby->len;
	shader_info.pCode = (void*)blobby->data;

	if (vkCreateShaderModule(device, &shader_info, NULL, &shader_module) != VK_SUCCESS) {
		error("Create shader Module\n");
		return 0;
	}

	talloc_free(blobby);

	return shader_module;
}

VkRenderPass
create_render_pass(VkDevice device, struct swap_chain_data *scd) {
	VkRenderPass render_pass;

	VkAttachmentDescription colorAttachment = { 0 };
	colorAttachment.format = scd->image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = { 0 };
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency = { 0 };
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo = { 0 };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, NULL, &render_pass) != VK_SUCCESS) {
		error("failed to create render pass!");
	}

	return render_pass;
}

VkPipeline
create_graphics_pipeline(VkDevice device, struct swap_chain_data *scd, VkRenderPass render_pass) {
	VkPipeline graphics_pipeline;
	struct blobby *fragcode = blobby_from_file("shaders/frag.spv");
	struct blobby *vertcode = blobby_from_file("shaders/vert.spv");

	VkShaderModule frag = create_shader(device, fragcode);
	VkShaderModule vert = create_shader(device, vertcode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = { 0 };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vert;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = frag;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 0 };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = NULL; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = NULL; // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { 0 };
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0 };
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)scd->extent.width;
	viewport.height = (float)scd->extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { 0 };
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = scd->extent;

	VkPipelineViewportStateCreateInfo viewportState = { 0 };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = NULL; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending = { 0 };
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = { 0 };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { 0 };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = NULL; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) != VK_SUCCESS) {
		error("failed to create pipeline layout!");
	}

        VkGraphicsPipelineCreateInfo pipelineInfo = { 0 };
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = render_pass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphics_pipeline) != VK_SUCCESS) {
		error("failed to create graphics pipeline!");
        }

	return graphics_pipeline;
}

static VkFramebuffer *
create_frame_buffers(VkDevice device, struct swap_chain_data *scd, VkRenderPass render_pass) {
	VkFramebuffer *framebuffers;

	framebuffers = talloc_array(scd, VkFramebuffer, scd->nimages);

	for (uint32_t i = 0; i < scd->nimages ; i++) {
		VkImageView attachments[] = {
			scd->image_views[i]
		};

		VkFramebufferCreateInfo framebufferInfo = { 0 };
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = render_pass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = scd->extent.width;
		framebufferInfo.height = scd->extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]) != VK_SUCCESS) {
			error("failed to create framebuffer!");

		}
	}

	return framebuffers;
}


VkCommandPool create_command_pool(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	VkCommandPool command_pool;
	struct queue_family_indices qfi = find_queue_families(physical_device, surface);
	
	VkCommandPoolCreateInfo pool_info = { 0 };
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = qfi.graphics_family;
	pool_info.flags = 0;

	if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
		error("Failed to create command pool");
	}

	return command_pool;
}

VkCommandBuffer *create_command_buffers(VkDevice device, struct swap_chain_data *scd, VkRenderPass render_pass, VkCommandPool command_pool, VkFramebuffer *framebuffers, VkPipeline pipeline){
	VkCommandBuffer *buffers;
	
	buffers = talloc_array(scd, VkCommandBuffer, scd->nimages); 

        VkCommandBufferAllocateInfo allocInfo = { 0 };
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = command_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = scd->nimages;

        if (vkAllocateCommandBuffers(device, &allocInfo, buffers) != VK_SUCCESS) {
		error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < scd->nimages; i++) {
            VkCommandBufferBeginInfo beginInfo =  { 0 };
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(buffers[i], &beginInfo) != VK_SUCCESS) {
                error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = { 0 };
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = render_pass;
            renderPassInfo.framebuffer = framebuffers[i];
            renderPassInfo.renderArea.offset.x = 0;
            renderPassInfo.renderArea.offset.y = 0;
            renderPassInfo.renderArea.extent = scd->extent;

            VkClearValue clearColor = { .color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

	    vkCmdBeginRenderPass(buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	    {

		    vkCmdBindPipeline(buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		    vkCmdDraw(buffers[i], 3, 1, 0, 0);
		}
	    vkCmdEndRenderPass(buffers[i]);

            if (vkEndCommandBuffer(buffers[i]) != VK_SUCCESS) {
		    error("failed to record command buffer!");
            }
        }

	return buffers;
}

VkSemaphore
create_semaphores(VkDevice device) {
	VkSemaphoreCreateInfo sem_info = { 0 };
	VkSemaphore sem;
	sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(device, &sem_info, NULL, &sem)) {
		error("failed tocreate sem");
	}
	return sem;
}

VkFence
create_fences(VkDevice device) {
	VkFenceCreateInfo fenceInfo = { 0 };
	VkFence fence;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(device, &fenceInfo, NULL, &fence) != VK_SUCCESS) {
		error("Failed to create fence");
	}
	return fence;
}

void
draw_frame(VkDevice device, VkSwapchainKHR swapchain,
		VkSemaphore image_semaphore, VkSemaphore renderFinishedSemaphore,
		VkFence fence, VkFence *image_fences,
		VkCommandBuffer *buffers, VkQueue graphicsQueue, VkQueue presentQueue) {
	// Make sure this frame was completed
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_semaphore, VK_NULL_HANDLE, &imageIndex);

	// Check the system has finished with this image before we start
	// scribbling over the top of it.
	if (image_fences[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &image_fences[imageIndex], VK_TRUE, UINT64_MAX);
	}
	image_fences[imageIndex] = fence;

	VkSubmitInfo submitInfo = { 0 };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {image_semaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &buffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &fence);

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
		error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo = { 0 };
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(presentQueue, &presentInfo);

        //currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int
main_loop(GLFWwindow* window) {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		//draw_frame();
	}
	return 0;
}

int
main(int argc, char **argv) {
	GLFWwindow *window;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkSurfaceKHR surface;
	struct swap_chain_data *scd;
	VkRenderPass render_pass;
	VkFramebuffer *framebuffers;
	VkPipeline pipeline;
	VkCommandPool command_pool;
	VkSemaphore image_ready_sem[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_done_sem[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
	VkFence *images_in_flight;
	VkCommandBuffer *command_buffers;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	window = window_init();
	instance = createInstance(window);
	surface = create_surface(instance, window);
	physical_device = pickPhysicalDevice(instance);
	device = createLogicalDevice(physical_device, surface, &graphicsQueue, &presentQueue);

        scd = create_swap_chain(device, physical_device, surface);
        create_image_views(device, scd);
	render_pass = create_render_pass(device, scd);
	pipeline = create_graphics_pipeline(device, scd, render_pass);
	framebuffers = create_frame_buffers(device, scd, render_pass);

	command_pool = create_command_pool(device, physical_device, surface);
	command_buffers = create_command_buffers(
			device,
			scd,
			render_pass,
			command_pool,
			framebuffers,
			pipeline);
	for (int i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i ++){
		image_ready_sem[i] = create_semaphores(device);
		render_done_sem[i] = create_semaphores(device);
		in_flight_fences[i] = create_fences(device);
	}
	images_in_flight = talloc_array(scd, VkFence, scd->nimages);
	// FIXME: Should do this when creating the Scd structure
	for (int i = 0; i < scd->nimages; i++) {
		images_in_flight[i] = VK_NULL_HANDLE;
	}

	int currentFrame = 0;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		draw_frame(device, scd->swap_chain, image_ready_sem[currentFrame], render_done_sem[currentFrame], in_flight_fences[currentFrame], images_in_flight, command_buffers, graphicsQueue, presentQueue);
		currentFrame ++;
		currentFrame %= MAX_FRAMES_IN_FLIGHT;
	}
	vkDeviceWaitIdle(device);

//	main_loop(window);

	return 0;
}





