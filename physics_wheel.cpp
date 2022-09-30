#include <stdio.h>
#include <float.h>

#include "physics_wheel.h"

struct AxisProjections {
    real32 min, max;
    v2 v_min, v_max, v_min2, v_max2;
};

static void
get_points_on_axis_sweeping(Mesh mesh, v2 axis, Transform t, Transform sweep_offset, AxisProjections *out);

static void
get_points_on_axis(Mesh mesh, v2 axis, Transform t, AxisProjections *out);

uint32
physics_create_body(BodyList *list, BodyDef def) {
    Body body;
    body.p = def.p;
    body.v = def.v;
    body.p_ang = def.p_ang;
    body.v_ang = def.v_ang;
    body.collision_mask = def.collision_mask;
    body.m = def.m;
    if (abs(body.m) > EPSILON) {
        body.m_inv = 1 / body.m;
    }
    else {
        body.m_inv = 0;
    }
    assert(list->count < MAX_BODY_COUNT);
    list->bodies[list->count] = body;
    return list->count++;
}

#if NEW_PHYSICS_SYSTEM
void
check_collision(Collision *collision, Body a, Body b, real64 dt, bool sweep) {
    
}
#else
void
check_collision(Collision *collision, Mesh a, Mesh b, Transform t_a, Transform t_b, v2 v_a, v2 v_b, real64 d_t, bool sweeping) {
    // TODO: Probably allocate memory to allow for a larger number of lines.
    constexpr int32 MAX_LINES = 64;
    v2 normals[MAX_LINES];
    assert(!(a.index_count > MAX_LINES || b.index_count > MAX_LINES));

    // Get line normals scene axes
    for (uint32 i = 0; i < a.index_count - 1; i++) {
        normals[i] = lnormal(transform(a.v_buffer[a.i[i]].coord, t_a), transform(a.v_buffer[a.i[i + 1]].coord, t_a));
    }
    normals[a.index_count - 1] = lnormal(transform(a.v_buffer[a.i[a.index_count - 1]].coord, t_a), transform(a.v_buffer[a.i[0]].coord, t_a));
    for (uint32 i = 0; i < b.index_count - 1; i++) {
        normals[i + a.index_count] = lnormal(transform(b.v_buffer[b.i[i]].coord, t_b), transform(b.v_buffer[b.i[i + 1]].coord, t_b));
    }
    normals[a.index_count + b.index_count - 1] = lnormal(transform(b.v_buffer[b.i[b.index_count - 1]].coord, t_b), transform(b.v_buffer[b.i[0]].coord, t_b));

    real32 min_overlap = FLT_MAX;
    v2 min_overlap_axis;

    bool a_has_face_col = 0, b_has_face_col = 0;
    v2 c_a, c_a2;
    v2 c_b, c_b2;

    // Extend colliders to include origin location
    Transform t_a_ext = t_a;
    t_a_ext.pos += v_a * d_t;
    //t_a_ext.rot += *v_a_ang * d_t;
    Transform t_b_ext = t_b;
    t_b_ext.pos += v_b * d_t;
    //t_b_ext.rot += *v_b_ang * d_t;

    // Project data onto axes
    for (uint32 i = 0; i < a.index_count + b.index_count; i++) {
        normals[i] = normalize(normals[i]); // TODO: Change rnormal() and lnormal() in math to always return normalized vectors

        AxisProjections proj_a, proj_b;

        if (sweeping) {
            get_points_on_axis_sweeping(a, normals[i], t_a, t_a_ext, &proj_a);
            get_points_on_axis_sweeping(b, normals[i], t_b, t_b_ext, &proj_b);
        }
        else {
            get_points_on_axis(a, normals[i], t_a, &proj_a);
            get_points_on_axis(b, normals[i], t_b, &proj_b);
        }

        if (proj_a.min > proj_b.max || proj_a.max < proj_b.min) {

            return; // Does not collide
        }


        // Determine minimum translation vector
        real32 o1 = proj_a.max - proj_b.min;
        real32 o2 = proj_b.max - proj_a.min;
        real32 o = min(o1, o2);
        if (o < min_overlap) {
            min_overlap = o;
            min_overlap_axis = normals[i];
            a_has_face_col = (i < a.index_count);
            b_has_face_col = !a_has_face_col;
            if (o1 < o2) {
                min_overlap_axis *= -1;
                c_a = proj_a.v_max;
                c_a2 = proj_a.v_max2;
                c_b = proj_b.v_min;
                c_b2 = proj_b.v_min2;
            }
            else {
                c_a = proj_a.v_min;
                c_a2 = proj_a.v_min2;
                c_b = proj_b.v_max;
                c_b2 = proj_b.v_max2;
            }
        }
    }

    // Check for face-face collision
    if (a_has_face_col) {
        collision->poi_a = transform(c_b - d_t * v_b, t_a);
        collision->poi_b = transform(c_b, t_b);
        // TODO: This computation might be unneccessary. Check again which variables in this whole function are really needed!
        if (dot(c_b, min_overlap_axis) - dot(c_b2, min_overlap_axis) < EPSILON) {
            b_has_face_col = true;
        }
    }
    if (b_has_face_col) {
        collision->poi_b = transform(c_a - d_t * v_a, t_b);
        collision->poi_a = transform(c_a, t_a);
        if (dot(c_a, min_overlap_axis) - dot(c_a2, min_overlap_axis) < EPSILON) {
            a_has_face_col = true;
        }
    }
    if (a_has_face_col && b_has_face_col) {
        //printf("We have a face-face collision and don't know how to deal with it!\n");
        
        // TODO:
        // - Project points onto normal of normal (i.e. collision line)
        // - Check if center of mass is inside bounds of other object.
        // - If it is, that is your collision point
        // - If it is not, check the min/max point that is closest to the com
        v2 collision_line = rnormal(min_overlap_axis);
        v2 vcom_a = center_of_mass(a);
        v2 vcom_b = center_of_mass(b);
        real32 a1 = dot(c_a, collision_line);
        real32 a2 = dot(c_a2, collision_line);
        real32 b1 = dot(c_b, collision_line);
        real32 b2 = dot(c_b2, collision_line);
        real32 a_min, b_min, a_max, b_max, a_com, b_com;
        v2 a_min_v, b_min_v, a_max_v, b_max_v;
        if (a1 < a2) {
            a_min = a1;
            a_max = a2;
            a_min_v = c_a;
            a_max_v = c_a2;
        }
        else {
            a_min = a2;
            a_max = a1;
            a_min_v = c_a2;
            a_max_v = c_a;
        }
        if (b1 < b2) {
            b_min = b1;
            b_max = b2;
            b_min_v = c_b;
            b_max_v = c_b2;
        }
        else {
            b_min = b2;
            b_max = b1;
            b_min_v = c_b2;
            b_max_v = c_b;
        }
        a_com = dot(transform(vcom_a, t_a), collision_line);
        b_com = dot(transform(vcom_b, t_b), collision_line);
        if (a_com >= b_min && a_com <= b_max) {
            collision->poi_a = vcom_a + projection(vcom_a - c_a, min_overlap_axis);
        }
        else if (abs(a_com - b_min) < abs(a_com - b_max)) {
            collision->poi_a = b_min_v;
        }
        else {
            collision->poi_a = b_max_v;
        }
        if (b_com >= b_min && b_com <= b_max) {
            collision->poi_b = vcom_b + projection(vcom_b - c_b, min_overlap_axis);
        }
        else if (abs(b_com - a_min) < abs(b_com - a_max)) {
            collision->poi_b = b_min_v;
        }
        else {
            collision->poi_b = b_max_v;
        }
    }
    collision->collided = true;
    collision->p_a = t_a.pos;
    collision->p_b = t_b.pos;
    collision->mesh_a = a;
    collision->mesh_b = b;
    collision->normal = min_overlap_axis;
    collision->overlap = min_overlap;
}
#endif

void
apply_external_forces(Movement *movements, Material *materials, Collision *collisions, uint32 *masks, uint32 count, real64 d_t) {
    static constexpr uint32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 0; i < count; i++) {
        if ((masks[i] & MASK) == MASK) {
            movements[i].a = 9.81 * v2{0, 1}; // GRAVITY
            if (collisions[i].collided) {
                movements[i].a -= materials[i].friction_dynamic * movements[i].v;
            }
        }
    }
}

void
apply_impulse(Mesh mesh, real32 m, v2 com, v2 *v, v2 poi, v2 f) {
    v2 p_to_m = com - poi;
    real32 factor_lin = 1.0f;
    //real32 factor_ang = 0.0f;
    if (magnitude( p_to_m) >= EPSILON) {
        real32 cosine = dot(f, p_to_m) / (magnitude(f) * magnitude(p_to_m));
        factor_lin = 0.5 + 0.5 * cosine;
        //factor_ang = 1 - cosine;
    }
    v2 dv_lin = f * factor_lin / m;
    // TODO: Assume rectangle for now!
    //real32 m_inertia = area_squared(mesh) * mass.value / 12;
    //real32 dv_ang = magnitude(force) * factor_ang / m_inertia;
    *v += dv_lin;
    //mov->v_ang += dv_ang;
}

void
update_velocities(Movement *movements, uint32 *masks, uint32 count, real64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < count; i++) {
        if ((masks[i] & MASK) == MASK) {
            movements[i].v += movements[i].a * d_t;
        }
    }
}

static void
get_points_on_axis_sweeping(Mesh mesh, v2 axis, Transform t, Transform sweep_offset, AxisProjections *out) {
    out->min = FLT_MAX;
    out->max = -FLT_MAX;
    for (uint32 a_i = 0; a_i < mesh.index_count; a_i++) {
        v2 v = transform(mesh.v_buffer[mesh.i[a_i]].coord, t);
        real32 proj = dot(v, axis);
        v2 v_ext = transform(mesh.v_buffer[mesh.i[a_i]].coord, sweep_offset);
        real32 proj_ext = dot(v_ext, axis);
        if (proj < out->min) {
            if (proj_ext < proj) {
                out->min = proj_ext;
                out->v_min2 = v;
                out->v_min = v_ext;
            } else {
                out->min = proj;
                out->v_min2 = proj_ext < out->min ? v_ext : out->v_min;
                out->v_min = v;
            }
        }
        else if (proj_ext < out->min) {
            assert(proj > proj_ext);
            out->min = proj_ext;
            out->v_min2 = out->v_min;
            out->v_min = v_ext;
        }
        if (proj > out->max) {
            if (proj_ext > proj) {
                out->max = proj_ext;
                out->v_max2 = v;
                out->v_max = v_ext;
            } else {
                out->max = proj;
                out->v_max2 = proj_ext > out->max ? v_ext : out->v_max;
                out->v_max = v;
            }
        }
        else if (proj_ext > out->max) {
            assert(proj < proj_ext);
            out->max = proj_ext;
            out->v_max2 = out->v_max;
            out->v_max = v_ext;
        }
    }
}

static void
get_points_on_axis(Mesh mesh, v2 axis, Transform t, AxisProjections *out) {
    out->min = FLT_MAX;
    out->max = -FLT_MAX;
    for (uint32 a_i = 0; a_i < mesh.index_count; a_i++) {
        v2 v = transform(mesh.v_buffer[mesh.i[a_i]].coord, t);
        real32 proj = dot(v, axis);
        if (proj < out->min) {
            out->min = proj;
            out->v_min2 = out->v_min;
            out->v_min = v;
        }
        if (proj > out->max) {
            out->max = proj;
            out->v_max2 = out->v_max;
            out->v_max = v;
        }
    }
}

