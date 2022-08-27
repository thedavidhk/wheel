#include <string.h>
#include "mesh_wheel.h"

Mesh
create_polygon(Vertexbuffer *vb, Indexbuffer *ib, const Vertex *verts, uint32 count_v, const uint32 *indices, uint32 count_i) {
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
create_rectangle(Vertexbuffer *vb, Indexbuffer *ib, v2 min, v2 max, v4 color) {
    // DEBUG: Take care of texture coordinates
    v2 uv[4];
    real32 width = max.x - min.x;
    real32 height = max.y - min.y;
    real32 shortest_side = min(width, height);
    uv[0] = {};
    uv[1] = v2{width / shortest_side, 0};
    uv[2] = v2{width / shortest_side, height / shortest_side};
    uv[3] = v2{0, height / shortest_side};

    Vertex verts[4] = {
        Vertex{min, color, uv[0]},
        Vertex{v2{max.x, min.y}, color, uv[1]},
        Vertex{max, color, uv[2]},
        Vertex{v2{min.x, max.y}, color, uv[3]}
    };
    uint32 indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(vb, ib, verts, 4, indices, 6);
};

v2
center_of_mass(Mesh mesh) {
// TODO: This is wrong! We should only count every vertex once!
    v2 sum = {};
    uint32 i;
    for (i = 0; i < mesh.index_count; i++) {
        sum += mesh.v_buffer[mesh.i[i]].coord;
    }
    return sum / (real32)i;
}

v2
transform(v2 v, Transform t) {
    return rotate(hadamard(v, t.scale), {0, 0}, t.rot) + t.pos;
}

v2
world_to_object_space(v2 v, Transform t) {
    return hadamard(rotate(v - t.pos, {0, 0}, -t.rot), 1/t.scale);
}

bool
is_in_mesh(v2 p, Mesh mesh, Transform t) {
    // TODO: We probably don't need to check every single triangle if mesh is
    // convex.
    bool result = false;
    for (uint32 i = 0; i < mesh.index_count - 2; i += 3) {
        result = is_in_triangle(p,
                transform(mesh.v_buffer[mesh.i[i]].coord, t),
                transform(mesh.v_buffer[mesh.i[i + 1]].coord, t),
                transform(mesh.v_buffer[mesh.i[i + 2]].coord, t));
        if (result)
            return result;
    }
    return result;
}
