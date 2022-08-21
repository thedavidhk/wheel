#ifndef MESH_WHEEL_H

#include "math_wheel.h"

struct Vertex {
    v2 coord;
    v4 color;
    //v2 tex_coord;
};

#if 1
struct Vertexbuffer {
    Vertex *data;
    uint32 count, max_count;
};

#else

struct Vertexbuffer {
    v2 *data;
    uint32 count, max_count;
};
#endif

struct Indexbuffer {
    uint32 *indices;
    uint32 count, max_count;
};

struct Mesh {
    Vertex *v_buffer;
    uint32 *i;
    v2 por;
    uint32 index_count;
};

struct Transform {
    v2 pos;
    v2 scale;
    real32 rot;
};

Mesh
create_polygon(Vertexbuffer *vb, Indexbuffer *ib, const Vertex *verts, uint32 count_v, const uint32 *indices, uint32 count_i);

Mesh
create_rectangle(Vertexbuffer *vb, Indexbuffer *ib, v2 min, v2 max, v4 color);

v2
center_of_mass(Mesh mesh);

v2
transform(v2 v, Transform t);

bool
is_in_mesh(v2 p, Mesh mesh, Transform t);

#define MESH_WHEEL_H
#endif
