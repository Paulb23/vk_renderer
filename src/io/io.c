#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "SDL2/SDL.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "thirdparty/tinyobj_loader_c/tinyobj_loader_c.h"
#include "src/data_structures/hash_map.h"

#include "src/io/memory.h"
#include "src/surface.h"
#include "src/error/error.h"


void get_resource_path(char r_dest[512], const char *p_file) {
    char *base_path = SDL_GetBasePath();
    snprintf(r_dest, 512, "%s%s", base_path, p_file);
}

char *read_file(const char *p_path, size_t *r_file_size) {
    FILE *file = fopen(p_path, "rb");
    ERR_FAIL_COND_V(!file, NULL);

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = mmalloc(file_size + 1);
    fread(buffer, file_size, 1, file);
    fclose(file);

    *r_file_size = file_size;
    return buffer;
}

static char* mmap_file(size_t* len, const char* filename) {
  struct stat sb;
  char* p;
  int fd;

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror("open");
    return NULL;
  }

  if (fstat(fd, &sb) == -1) {
    perror("fstat");
    return NULL;
  }

  if (!S_ISREG(sb.st_mode)) {
    fprintf(stderr, "%s is not a file\n", filename);
    return NULL;
  }

  p = (char*)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (p == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  if (close(fd) == -1) {
    perror("close");
    return NULL;
  }

  (*len) = sb.st_size;

  return p;
}

static void get_file_data(void* ctx, const char* filename, const int is_mtl,
                          const char* obj_filename, char** data, size_t* len) {
  // NOTE: If you allocate the buffer with malloc(),
  // You can define your own memory management struct and pass it through `ctx`
  // to store the pointer and free memories at clean up stage(when you quit an
  // app)
  // This example uses mmap(), so no free() required.
  (void)ctx;

  if (is_mtl && obj_filename) {}
  if (!filename) {
    fprintf(stderr, "null filename\n");
    (*data) = NULL;
    (*len) = 0;
    return;
  }

  size_t data_len = 0;

  *data = mmap_file(&data_len, filename);
  (*len) = data_len;
}

void load_obj(const char *p_path, Vector *r_vertexes, Vector *r_indexes) {
    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes = NULL;
    size_t num_shapes;
    tinyobj_material_t* materials = NULL;
    size_t num_materials;

    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials, &num_materials, p_path, get_file_data, NULL, flags);
    CRASH_COND_MSG(ret != TINYOBJ_SUCCESS, "FATAL: Failed to load model %s.", p_path);

    HashMap *unique_vertices = hashmap_create(sizeof(uint32_t));
    Vector vertexes = {0, 0, sizeof(Vertex), NULL};
    Vector indices = {0, 0, sizeof(uint32_t), NULL};
    for (u_int32_t i = 0; i < attrib.num_faces; i++) {
        Vertex vertex = {
            .pos = {
                attrib.vertices[3 * attrib.faces[i].v_idx + 0],
                attrib.vertices[3 * attrib.faces[i].v_idx + 1],
                attrib.vertices[3 * attrib.faces[i].v_idx + 2],
            },
            .color = {
                {1.0f},
                {1.0f},
                {1.0f},
                {1.0f},
            },
            .tex_coord = {
                attrib.texcoords[2 * attrib.faces[i].vt_idx + 0],
                1.0f - attrib.texcoords[2 * attrib.faces[i].vt_idx + 1]
            }
        };

        char key[128];
        snprintf(key, 128, "%f%f%f%f%f%f%f%f%f", vertex.pos.x, vertex.pos.y, vertex.pos.z, vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a, vertex.tex_coord.x,vertex.tex_coord.y );
        void *has = hashmap_get(unique_vertices, key, 128);
        if (has) {
            vector_push_back(&indices, &(*(uint32_t *)has));
            continue;
        }

        hashmap_insert(unique_vertices, key, 128, &vertexes.size);
        vector_push_back(&indices, &vertexes.size);
        vector_push_back(&vertexes, &vertex);
    }

    hashmap_free(unique_vertices);
    *r_vertexes = vertexes;
    *r_indexes = indices;
}
