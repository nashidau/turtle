
#include <assert.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "vertex.h"
#include "blobby.h"
#include "../../tinyobjloader-c/tinyobj_loader_c.h"

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
get_attribute_description_pair(const struct vertex *vertex, uint32_t *nentries) {
	VkVertexInputAttributeDescription *descriptions;

	descriptions = talloc_zero_array(NULL, VkVertexInputAttributeDescription, 3);

        descriptions[0].binding = 0;
        descriptions[0].location = 0;
        descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        descriptions[0].offset = offsetof(struct vertex, pos);

        descriptions[1].binding = 0;
        descriptions[1].location = 1;
        descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        descriptions[1].offset = offsetof(struct vertex, color);

        descriptions[2].binding = 0;
        descriptions[2].location = 2;
        descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        descriptions[2].offset = offsetof(struct vertex, tex_coord);

	if (nentries) {
		*nentries = 3;
	}

        return descriptions;
}


// FIXME: WHen the hell does 'buf' get freed.
static void
tinyobj_file_reader(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len) {
	struct blobby *blobby;

	assert(is_mtl == 0);

	blobby = blobby_from_file_ctx(ctx, obj_filename);

	if (!blobby) {
		*len = 0;
		*buf = 0;
		return;
	}

	*buf = (char *)blobby->data;
	*len = blobby->len;

}

//extern int tinyobj_parse_obj(tinyobj_attrib_t *attrib, tinyobj_shape_t **shapes,
  //                           size_t *num_shapes, tinyobj_material_t **materials,
    //                         size_t *num_materials, const char *file_name, file_reader_callback file_reader,
      //                       unsigned int flags);

/*
 * Loads the given model from the given path.
 * 
 * Returns a model structure, that should be freed with talloc_free.
 */
struct trtl_model *
load_model(const char *basename) {
   // tinyobj::attrib_t attrib;
   // std::vector<tinyobj::shape_t> shapes;
    //std::vector<tinyobj::material_t> materials;
    //std::string warn, err;

   // if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
    //    throw std::runtime_error(warn + err);
   // }
   return NULL;
}
