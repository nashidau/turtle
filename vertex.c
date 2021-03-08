
#include <assert.h>

#include <vulkan/vulkan.h>

#include <talloc.h>

#include "vertex.h"
#include "blobby.h"
#include "../../tinyobjloader-c/tinyobj_loader_c.h"

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


// FIXME: WHen the hell does 'buf' get freed.
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

	printf("We have %d Vertices\n", attrib.num_vertices);
	for (int i = 0; i < num_shapes; i ++) {
		printf("Shape %d %d %d %s\n", i, shapes[i].face_offset, shapes[i].length, shapes[i].name);
	}

	printf("Model Details:\n Vertices: %d\n Faces: %d\n Face N Verts: %d\n",
			attrib.num_vertices, attrib.num_faces, attrib.num_face_num_verts);



	model = talloc_zero(NULL, struct trtl_model);
	
	// FIXME: This is the dumb way; each index just once
	
	// Load our vertices
	/* 
	model->vertices = talloc_array(model, struct vertex, attrib.num_vertices);
	model->nvertices = attrib.num_vertices;
	for (int i = 0 ; i < attrib.num_vertices ; i++) {
		struct vertex  *v = model->vertices + i;
		v->pos.x = attrib.vertices[i * 3];
		v->pos.y = attrib.vertices[i * 3 + 1];
		v->pos.z = attrib.vertices[i * 3 + 2];
		v->tex_coord.x = attrib.texcoords[i * 2];
		v->tex_coord.y = 1.0f - attrib.texcoords[i * 2 + 1];
		v->color = (struct color){1.0f,1.0f,1.0f};
	}
	*/

	// dumb
	model->vertices = talloc_array(model, struct vertex, attrib.num_faces);
	model->nvertices = attrib.num_faces;

	// Load our indices
	model->indices = talloc_array(model, uint32_t, attrib.num_faces);
	model->nindices = attrib.num_faces;

	int	mi = 0;
	for (int i = 0 ; i < attrib.num_faces ; i ++ ) {
		model->indices[i] = i;
		struct vertex *v = model->vertices + i;
		tinyobj_vertex_index_t idx = attrib.faces[i];

		v->pos.x = attrib.vertices[idx.v_idx * 3];
		v->pos.y = attrib.vertices[idx.v_idx * 3+ 1];
		v->pos.z = attrib.vertices[idx.v_idx * 3+ 2];
		v->tex_coord.x = attrib.texcoords[idx.vt_idx * 2];
		v->tex_coord.y = 1.0f - attrib.texcoords[idx.vt_idx * 2 + 1];

		
		//strut vertex *v = model->vertices + i;
		//v->posx.
	}

      /*size_t f;
      for (f = 0; f < (size_t)attrib.faces[i] / 3; f++) {

        tinyobj_vertex_index_t idx0 = attrib.faces[face_offset + 3 * f + 0];
        tinyobj_vertex_index_t idx1 = attrib.faces[face_offset + 3 * f + 1];
        tinyobj_vertex_index_t idx2 = attrib.faces[face_offset + 3 * f + 2];

	model->indices[mi++] = attrib.faces[f * 3.v_idx;
	model->indices[mi++] = idx1.v_idx;
	model->indices[mi++] = idx2.v_idx;

      }
    }

}*/

	return model;
	/*
	for (size_t s = 0; s < num_shapes ; s ++) {
		shape = shapes[s];
		for (size_t

	}
for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        vertices.push_back(vertex);
        indices.push_back(indices.size());
    }

vertex.pos = {
    attrib.vertices[3 * index.vertex_index + 0],
    attrib.vertices[3 * index.vertex_index + 1],
    attrib.vertices[3 * index.vertex_index + 2]
};

vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
   attrib.texcoords[2 * index.texcoord_index + 1]
};

vertex.color = {1.0f, 1.0f, 1.0f}; 
*/
	return NULL;
}
