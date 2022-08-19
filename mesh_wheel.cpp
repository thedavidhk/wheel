#include <string.h>
#include "mesh_wheel.h"

Mesh
create_polygon(Vertexbuffer *vb, Indexbuffer *ib, const v2 *verts, uint32 count_v, const uint32 *indices, uint32 count_i) {
    Mesh mesh = {};
    mesh.v_buffer = vb->data;
    mesh.index_count = count_i;
    mesh.i = ib->indices + ib->count;
    memcpy(vb->data + vb->count, verts, count_v * sizeof(*verts));
    memcpy(mesh.i, indices, count_i * sizeof(*indices));
    for (uint32 i = 0; i < count_i; i++) {
        mesh.i[i] += vb->count;
    }
    vb->count += count_v;
    ib->count += count_i;
    return mesh;
};

Mesh
create_rectangle(Vertexbuffer *vb, Indexbuffer *ib, v2 min, v2 max) {
    v2 verts[4] = {min, v2{max.x, min.y}, max, v2{min.x, max.y}};
    uint32 indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(vb, ib, verts, 4, indices, 6);
};

v2
center_of_mass(Mesh mesh) {
    v2 sum = {};
    uint32 i;
    for (i = 0; i < mesh.index_count; i++) {
        sum += mesh.v_buffer[mesh.i[i]];
    }
    return sum / (real32)i;
}

v2
transform(v2 v, Transform t) {
    return rotate(v, {0, 0}, t.rot) + t.pos;
}

bool
is_in_mesh(v2 p, Mesh mesh, Transform t) {
    // TODO: We probably don't need to check every single triangle if mesh is
    // convex.
    bool result = false;
    for (uint32 i = 0; i < mesh.index_count - 2; i += 3) {
        result = is_in_triangle(p,
                transform(mesh.v_buffer[mesh.i[i]], t),
                transform(mesh.v_buffer[mesh.i[i + 1]], t),
                transform(mesh.v_buffer[mesh.i[i + 2]], t));
        if (result)
            return result;
    }
    return result;
}
