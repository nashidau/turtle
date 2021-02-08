

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "vertex.h"


// FIXME Tag as pure
VkVertexInputBindingDescription 
vertex_binding_description_get(const struct vertex *vertex) {
	VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(struct vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
}

// Tag as pure
VkVertexInputAttributeDescription *
get_attribute_description_pair(const struct vertex *vertex) {
	VkVertexInputAttributeDescription *descriptions;

	descriptions = talloc_zero_array(NULL, VkVertexInputAttributeDescription, 2);

        descriptions[0].binding = 0;
        descriptions[0].location = 0;
        descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        descriptions[0].offset = offsetof(struct vertex, pos);

        descriptions[1].binding = 0;
        descriptions[1].location = 1;
        descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        descriptions[1].offset = offsetof(struct vertex, color);

        return descriptions;
}
