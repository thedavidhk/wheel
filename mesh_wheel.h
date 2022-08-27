#ifndef MESH_WHEEL_H

#include <float.h>

#include "math_wheel.h"

// TODO: Probably don't want to hard-code the vertex attributes. Otherwise I
// have to change the create_mesh functions every time I add a new vertex
// attribute.
struct Vertex {
    v2 coord;
    v4 color;
    v2 tex_coord;
};

struct Vertexbuffer {
    Vertex *data;
    uint32 count, max_count;
};

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

inline real32
area_squared(Mesh mesh) {
    real32 x_min = FLT_MAX;
    real32 x_max = -FLT_MAX;
    real32 y_min = FLT_MAX;
    real32 y_max = -FLT_MAX;
    for (uint32 i = 0; i < mesh.index_count; i++) {
        x_min = min(x_min, mesh.v_buffer[i].coord.x);
        x_max = max(x_max, mesh.v_buffer[i].coord.x);
        y_min = min(y_min, mesh.v_buffer[i].coord.y);
        y_max = max(y_max, mesh.v_buffer[i].coord.y);
    }
    return (x_max - x_min) * (x_max - x_min) + (y_max - y_min) * (y_max - y_min);
}

v2
transform(v2 v, Transform t);

v2
world_to_object_space(v2 v, Transform t);

bool
is_in_mesh(v2 p, Mesh mesh, Transform t);

#define MESH_WHEEL_H
#endif
