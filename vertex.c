
#include <assert.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "../../tinyobjloader-c/tinyobj_loader_c.h"
#include "blobby.h"
#include "helpers.h"
#include "vertex.h"

struct vhash;
static int32_t vhash_find(struct vhash *vhash, uint32_t vertex_index, uint32_t texture_index,
			  int32_t material_idx, bool *new);
static struct vhash *vhash_init(int32_t size);
static int vhash_netries(struct vhash *vhash, int *lookups);

#define DEBUGTHIS 1

// FIXME: This doesn't need to a function
trtl_pure VkVertexInputBindingDescription
vertex_binding_description_get(void)
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(struct vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

VkVertexInputAttributeDescription *
get_attribute_description_pair(uint32_t *nentries)
{
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
tinyobj_file_reader(void *ctx, const char *filename, int is_mtl, const char *obj_filename,
		    char **buf, size_t *len)
{
	struct blobby *blobby;
	char *extra_path = NULL;

	printf("**** File Reader: %s %d %s\n", filename, is_mtl, obj_filename);

	if (is_mtl) {
		// Need to build the path:
		char *lastsep = rindex(obj_filename, '/');
		if (!lastsep) {
			// Not relative - just use filename.
		} else {
			asprintf(&extra_path, "%.*s/%s", (int)(lastsep - obj_filename),
				 obj_filename, filename);
			filename = extra_path;
		}

		printf("Is MTL: %s\n", extra_path);
	}

	blobby = blobby_from_file_ctx(ctx, filename);

	if (!blobby) {
		*len = 0;
		*buf = 0;
		return;
	}

	// FIXME: tinyobj file reader should take const char
	*buf = (char *)(uintptr_t)blobby->data;
	*len = blobby->len;

	if (extra_path) {
		free(extra_path);
	}
}

// extern int tinyobj_parse_obj(tinyobj_attrib_t *attrib, tinyobj_shape_t **shapes,
//                           size_t *num_shapes, tinyobj_material_t **materials,
//                         size_t *num_materials, const char *file_name, file_reader_callback
//                         file_reader,
//                       unsigned int flags);

/*
 * Loads the given model from the given path.
 *
 * Returns a model structure, that should be freed with talloc_free.
 */
struct trtl_model *
load_model(const char *basename)
{
	void *ctx;
	struct trtl_model *model;
	tinyobj_attrib_t attrib;
	tinyobj_shape_t *shapes;
	tinyobj_material_t *materials;
	struct pos3d *vertices;
	struct pos2d *texcoords;
	size_t num_shapes;
	size_t num_materials;

	ctx = talloc_init("Model Data buffer: %s", basename);

	int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials,
				    basename, tinyobj_file_reader, ctx, TINYOBJ_FLAG_TRIANGULATE);
	if (ret != 0) {
		printf("parse object failed\n");
		exit(1);
	}

	// Release the file buffer (blobby) allocated in the reader
	talloc_free(ctx);

	if (DEBUGTHIS) {
		printf("We have %d Vertices\n", attrib.num_vertices);
		printf("Model Details:\n Vertices: %d\n Faces: %d\n Face N Verts: %d\n",
		       attrib.num_vertices, attrib.num_faces, attrib.num_face_num_verts);
	}
	if (DEBUGTHIS > 1) {
		for (uint32_t i = 0; i < num_shapes; i++) {
			printf("Shape %d %d %d %s\n", i, shapes[i].face_offset, shapes[i].length,
			       shapes[i].name);
		}
	}

	// Some typed helpers
	vertices = (struct pos3d *)attrib.vertices;
	texcoords = (struct pos2d *)attrib.texcoords;

	// First; work out our scale factor.  Run through the array, generate the bounding,
	// and work the scale factor
	struct boundingbox3d bbox = BOUNDINGBOX_INIT;
	for (size_t i = 0 ; i < attrib.num_vertices ; i ++){
		bbox.min.x = MIN(bbox.min.x, vertices[i].x);
		bbox.min.y = MIN(bbox.min.y, vertices[i].y);
		bbox.min.z = MIN(bbox.min.z, vertices[i].z);
		bbox.max.x = MAX(bbox.max.x, vertices[i].x);
		bbox.max.y = MAX(bbox.max.y, vertices[i].y);
		bbox.max.z = MAX(bbox.max.z, vertices[i].z);
	}
	// Now work out scale factor x/y
	printf("Min: %f %f %f\n", bbox.min.x, bbox.min.y, bbox.min.z);
	printf("Max: %f %f %f\n", bbox.max.x, bbox.max.y, bbox.max.z);
	float scale = bbox.max.x - bbox.min.x;
	scale = MAX(scale, bbox.max.y - bbox.min.y);
	scale = MAX(scale, bbox.max.z - bbox.min.z);
	scale = 1 / scale;
	printf("Scaling by %f\n", scale);

	model = talloc_zero(NULL, struct trtl_model);

	model->vertices = talloc_array(model, struct vertex, attrib.num_faces);
	model->nvertices = attrib.num_faces;

	// Load our indices
	model->indices = talloc_array(model, uint32_t, attrib.num_faces);
	model->nindices = attrib.num_faces;


	struct vhash *vhash = vhash_init(attrib.num_faces);
	int maxn = -1;
	for (uint32_t i = 0; i < attrib.num_faces; i++) {
		bool new = true;
		tinyobj_vertex_index_t idx = attrib.faces[i];
		int n = vhash_find(vhash, idx.v_idx, idx.vt_idx, attrib.material_ids[i / 3], &new);
		if (n > maxn) maxn = n;
		// Already seen it
		model->indices[i] = n;
		if (!new) {
			continue;
		}

		struct vertex *v = model->vertices + n;

		v->pos.x = vertices[idx.v_idx].x * scale;
		v->pos.y = vertices[idx.v_idx].y * scale;
		v->pos.z = vertices[idx.v_idx].z * scale;
		// FIXME: Number of text coords shold be range checked.
		if (attrib.num_texcoords) {
			v->tex_coord.x = texcoords[idx.vt_idx].x;
			v->tex_coord.y = 1.0f - texcoords[idx.vt_idx].y;
		}

		// Note the '3' is assuming all triangles here
		int material_idx = attrib.material_ids[i / 3];
		assert(material_idx < (int)num_materials);
		if (material_idx >= 0) {
			v->color.r = materials[material_idx].diffuse[0];
			v->color.g = materials[material_idx].diffuse[1];
			v->color.b = materials[material_idx].diffuse[2];
		}
	}
	int lookups;
	vhash_netries(vhash, &lookups);
	talloc_free(vhash);

	printf("Max N is %d (of %d)\n", maxn, model->nvertices);

	if (DEBUGTHIS > 1) {
		for (uint32_t j = 0; j < model->nindices; j++) {
			struct vertex *v = model->vertices + model->indices[j];
			printf("Vertex %4d: %lf %lf %lf  / %lf %lf\n", j, v->pos.x, v->pos.y,
			       v->pos.z, v->tex_coord.x, v->tex_coord.y);
		}
	}

	return model;
}

struct vhash {
	int32_t size;
	int32_t next;
	struct vhash_node **nodes;

	// stats:
	int lookups;
};

struct vhash_node {
	struct vhash_node *next;
	uint32_t vertex_index;
	uint32_t texture_index;
	int32_t material_idx;

	int32_t vindex;
};

static uint32_t
hash(uint32_t vertex_index, uint32_t texture_index, int32_t material_idx, uint32_t size)
{
	return (vertex_index + texture_index ^ material_idx) % size;
}

// Returns the index to use for this vertex.
static int32_t
vhash_find(struct vhash *vhash, uint32_t vertex_index, uint32_t texture_index, int32_t material_idx,
	   bool *created)
{
	struct vhash_node *node;
	uint32_t key = hash(vertex_index, texture_index, material_idx, vhash->size);

	vhash->lookups++;

	node = vhash->nodes[key];
	while (node) {
		if (node->vertex_index == vertex_index && node->texture_index == texture_index &&
				node->material_idx == material_idx) {
			if (created) *created = false;
			return node->vindex;
		}
		node = node->next;
	}

	// Didn't find it
	node = talloc(vhash, struct vhash_node);
	node->texture_index = texture_index;
	node->vertex_index = vertex_index;
	node->vindex = vhash->next++;
	node->material_idx = material_idx;

	node->next = vhash->nodes[key];
	vhash->nodes[key] = node;

	if (created) *created = true;

	return node->vindex;
}

static struct vhash *
vhash_init(int32_t size)
{
	struct vhash *vhash;

	vhash = talloc(NULL, struct vhash);
	vhash->size = size;
	vhash->next = 0;
	vhash->nodes = talloc_zero_array(NULL, struct vhash_node *, size);
	vhash->lookups = 0;

	return vhash;
}

static int
vhash_netries(struct vhash *vhash, int *lookups)
{
	if (lookups) *lookups = vhash->lookups;
	return vhash->next;
}
