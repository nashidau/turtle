
#include <assert.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "vertex.h"
#include "blobby.h"
#include "../../tinyobjloader-c/tinyobj_loader_c.h"

struct vhash;
static int32_t vhash_find(struct vhash *vhash, uint32_t vertex_index, uint32_t texture_index, bool *new);
static struct vhash * vhash_init(int32_t size);
static int vhash_netries(struct vhash *vhash, int *lookups);

#define DEBUGTHIS 0

// FIXME Tag as pure
VkVertexInputBindingDescription 
vertex_binding_description_get(const struct trtl_model *model) {
	VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(struct vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
}

// Tag as pure
VkVertexInputAttributeDescription *
get_attribute_description_pair(const struct trtl_model *model, uint32_t *nentries) {
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


static void
tinyobj_file_reader(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len) {
	struct blobby *blobby;

	printf("**** File Reader: %s %d %s\n", filename, is_mtl, obj_filename);

	if (is_mtl) {
		printf("Is MTL\n");
	}

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
	void *ctx;
	struct trtl_model *model;
   // tinyobj::attrib_t attrib;
   // std::vector<tinyobj::shape_t> shapes;
    //std::vector<tinyobj::material_t> materials;
    //std::string warn, err;
    	tinyobj_attrib_t attrib;
	tinyobj_shape_t *shapes;
	tinyobj_material_t *materials;
	size_t num_shapes;
	size_t num_materials;

	ctx = talloc_init("Model Data buffer: %s", basename);

   // if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
    //    throw std::runtime_error(warn + err);
	int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials, basename, tinyobj_file_reader, ctx, TINYOBJ_FLAG_TRIANGULATE);
	if (ret != 0) {
		printf("parse object failed\n");
		exit(1);
	}

	// Release the file buffer (blobby) allocated in the reader
   	talloc_free(ctx);

	if (DEBUGTHIS) {
		printf("We have %d Vertices\n", attrib.num_vertices);
		for (int i = 0; i < num_shapes; i ++) {
			printf("Shape %d %d %d %s\n", i, shapes[i].face_offset,
					shapes[i].length, shapes[i].name);
		}

		printf("Model Details:\n Vertices: %d\n Faces: %d\n Face N Verts: %d\n",
				attrib.num_vertices, attrib.num_faces, attrib.num_face_num_verts);
	}

	model = talloc_zero(NULL, struct trtl_model);

	model->vertices = talloc_array(model, struct vertex, attrib.num_faces);
	model->nvertices = attrib.num_faces;

	// Load our indices
	model->indices = talloc_array(model, uint32_t, attrib.num_faces);
	model->nindices = attrib.num_faces;

	struct vhash *vhash = vhash_init(attrib.num_faces);
	for (int i = 0 ; i < attrib.num_faces ; i ++) {
		bool new = true;
		tinyobj_vertex_index_t idx = attrib.faces[i];
		int n = vhash_find(vhash, idx.v_idx, idx.vt_idx, &new);
		// Already seen it
		model->indices[i] = n;
		if (!new) {
			continue;
		}

		struct vertex *v = model->vertices + n;

		v->pos.x = attrib.vertices[idx.v_idx * 3];
		v->pos.y = attrib.vertices[idx.v_idx * 3+ 1];
		v->pos.z = attrib.vertices[idx.v_idx * 3+ 2];
		v->tex_coord.x = attrib.texcoords[idx.vt_idx * 2];
		v->tex_coord.y = 1.0f - attrib.texcoords[idx.vt_idx * 2 + 1];
	}
	int lookups;
	vhash_netries(vhash, &lookups);
	talloc_free(vhash);

	if (DEBUGTHIS) {
		for (int j = 0 ; j < model->nindices ; j ++) {
			struct vertex *v = model->vertices + model->indices[j];
			printf("Vertex %4d: %lf %lf %lf  / %lf %lf\n",
					j,
					v->pos.x, v->pos.y, v->pos.z,
					v->tex_coord.x, v->tex_coord.y);
		}
	}

	return model;
}

struct vhash {
	int32_t size;
	int32_t next;
	// Could use a 0 length array to save an allocation here
	struct vhash_node **nodes;

	// stats:
	int lookups;
};

struct vhash_node {
	struct vhash_node *next;
	uint32_t vertex_index;
	uint32_t texture_index;

	int32_t vindex;
};

static uint32_t 
hash(uint32_t vertex_index, uint32_t texture_index, uint32_t size) {
	return (vertex_index + texture_index) % size;
}

// Returns the index to use for this vertex.
static int32_t
vhash_find(struct vhash *vhash, uint32_t vertex_index, uint32_t texture_index, bool *created) {
	struct vhash_node *node;
	uint32_t key = hash(vertex_index, texture_index, vhash->size);

	vhash->lookups ++;

	node = vhash->nodes[key];
	while (node) {
		if (node->vertex_index == vertex_index && node->texture_index == texture_index) {
			if (created) *created = false;
			return node->vindex;
		}
		node = node->next;
	}

	// Didn't find it
	node = talloc(vhash, struct vhash_node);
	node->texture_index = texture_index;
	node->vertex_index = vertex_index;
	node->vindex = vhash->next ++;

	node->next = vhash->nodes[key];
	vhash->nodes[key] = node;
			
	if (created) *created = true;

	return node->vindex;
}

static struct vhash *
vhash_init(int32_t size) {
	struct vhash *vhash;

	vhash = talloc(NULL, struct vhash);
	vhash->size = size;
	vhash->next = 0;
	vhash->nodes = talloc_zero_array(NULL, struct vhash_node *, size);
	vhash->lookups = 0;
	
	return vhash;
}

static int
vhash_netries(struct vhash *vhash, int *lookups) {
	if (lookups) *lookups = vhash->lookups;
	return vhash->next;
}


