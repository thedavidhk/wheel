#include <stdio.h>
#include <float.h>

#include "physics_wheel.h"

void
accelerate(v2 direction, real32 force, real32 mass, Movement *m, real64 d_t) {
    direction = normalize(direction);
    m->a += (direction * force/mass);
}

// TODO Still very buggy!!
Collision
check_collision(Mesh a, Mesh b, Transform t_a, Transform t_b, Movement mov_a, Movement mov_b, real64 d_t) {
    // TODO: Probably allocate memory to allow for a larger number of lines.
    Collision result = {};
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

    // Extend colliders to include origin location
    Transform t_a_ext = t_a;
    t_a_ext.pos += mov_a.v * d_t;
    t_a_ext.rot += mov_a.rot_v * d_t;
    Transform t_b_ext = t_b;
    t_b_ext.pos += mov_b.v * d_t;
    t_b_ext.rot += mov_b.rot_v * d_t;

    bool a_has_face_col, b_has_face_col;
    v2 c_a, c_a2;
    v2 c_b, c_b2;

    // Project data onto axes
    for (uint32 i = 0; i < a.index_count + b.index_count; i++) {
        real32 min_a = FLT_MAX;
        real32 max_a = -FLT_MAX;
        real32 min_b = FLT_MAX;
        real32 max_b = -FLT_MAX;
        v2 v_min_a, v_min_a2, v_max_a, v_max_a2, v_min_b, v_min_b2, v_max_b, v_max_b2;
        normals[i] = normalize(normals[i]);

        for (uint32 a_i = 0; a_i < a.index_count; a_i++) {
            v2 v = transform(a.v_buffer[a.i[a_i]].coord, t_a);
            real32 proj = dot(v, normals[i]);
            v2 v_ext = transform(a.v_buffer[a.i[a_i]].coord, t_a_ext);
            real32 proj_ext = dot(v_ext, normals[i]);
            if (proj < min_a) {
                if (proj_ext < proj) {
                    min_a = proj_ext;
                    v_min_a2 = v;
                    v_min_a = v_ext;
                } else {
                    min_a = proj;
                    v_min_a2 = proj_ext < min_a ? v_ext : v_min_a;
                    v_min_a = v;
                }
            }
            else if (proj_ext < min_a) {
                assert(proj > proj_ext);
                min_a = proj_ext;
                v_min_a2 = v_min_a;
                v_min_a = v_ext;
            }
            if (proj > max_a) {
                if (proj_ext > proj) {
                    max_a = proj_ext;
                    v_max_a2 = v;
                    v_max_a = v_ext;
                } else {
                    max_a = proj;
                    v_max_a2 = proj_ext > max_a ? v_ext : v_max_a;
                    v_max_a = v;
                }
            }
            else if (proj_ext > max_a) {
                assert(proj < proj_ext);
                max_a = proj_ext;
                v_max_a2 = v_max_a;
                v_max_a = v_ext;
            }
        }
        for (uint32 b_i = 0; b_i < b.index_count; b_i++) {
            v2 v = transform(b.v_buffer[b.i[b_i]].coord, t_b);
            real32 proj = dot(v, normals[i]);
            v2 v_ext = transform(b.v_buffer[b.i[b_i]].coord, t_b_ext);
            real32 proj_ext = dot(v_ext, normals[i]);
            if (proj < min_b) {
                if (proj_ext < proj) {
                    min_b = proj_ext;
                    v_min_b2 = v_min_b;
                    v_min_b = v_ext;
                } else {
                    min_b = proj;
                    v_min_b2 = proj_ext < min_b ? v_ext : v_min_b;
                    v_min_b = v;
                }
            }
            else if (proj_ext < min_b) {
                assert(proj > proj_ext);
                min_b = proj_ext;
                v_min_b2 = v_min_b;
                v_min_b = v_ext;
            }
            if (proj > max_b) {
                if (proj_ext > proj) {
                    max_b = proj_ext;
                    v_max_b2 = v_max_b;
                    v_max_b = v_ext;
                } else {
                    max_b = proj;
                    v_max_b2 = proj_ext > max_b ? v_ext : v_max_b;
                    v_max_b = v;
                }
            }
            else if (proj_ext > max_b) {
                assert(proj < proj_ext);
                max_b = proj_ext;
                v_max_b2 = v_max_b;
                v_max_b = v_ext;
            }
        }
        if (min_a > max_b || max_a < min_b)
            return result; // Does not collide


        // Determine minimum translation vector
        real32 o1 = max_a - min_b;
        real32 o2 = max_b - min_a;
        real32 o = min(o1, o2);
        if (o < min_overlap) {
            min_overlap = o;
            min_overlap_axis = normals[i];
            a_has_face_col = (i < a.index_count);
            b_has_face_col = !a_has_face_col;
        }
        if (o1 < o2) {
            c_a = v_max_a;
            c_a2 = v_max_a2;
            c_b = v_min_b;
            c_b2 = v_min_b2;
            min_overlap_axis *= -1;                
        } else {
            c_a = v_min_a;
            c_a2 = v_min_a2;
            c_b = v_max_b;
            c_b2 = v_max_b2;
        }
    }
    // ONLY AT THIS POINT WE KNOW THAT WE HAVE A COLLISION
    // TODO: Find the collision point

    // Check for face-face collision
    if (a_has_face_col) {
        result.poi = c_b - d_t * mov_b.v;
        if (magnitude(c_b - c_b2) < EPSILON) {
            b_has_face_col = true;
        }
    }
    if (b_has_face_col) {
        result.poi = c_a - d_t * mov_a.v;
        if (magnitude(c_a - c_b2) < EPSILON) {
            a_has_face_col = true;
        }
    }
    if (a_has_face_col && b_has_face_col)
        printf("We have a face-face collision and don't know how to deal with it!\n");;
    
    result.collided = true;
    //if (abs(result.mtv.x) < EPSILON && abs(result.mtv.y) < EPSILON)
        //result.collided = false;
    assert(min_overlap > (-EPSILON));
    result.mtv = normalize(min_overlap_axis) * min_overlap;
    return result;
}

void
resolve_collision(Collision col, Mesh mesh_a, Mesh mesh_b, Transform *t_a, Transform *t_b, Movement *mov_a, Movement *mov_b,
        Mass m_a, Mass m_b)
{
    /*
    v2 dv_a = projection(mov_a->v, col.mtv);
    v2 dv_b = projection(mov_b->v, col.mtv);
    v2 dv = m_b.value * dv_b + m_a.value * dv_a;
    mov_a->v += dv / (m_a.value + m_b.value) - dv_a;
    mov_b->v += dv / (m_a.value + m_b.value) - dv_b;
    t_a->pos += col.mtv;
    t_b->pos -= col.mtv;
    */
}

void
resolve_collision(Collision col, Mesh mesh_a, Transform *t_a, Transform t_b, Movement *mov_a, Mass m_a) {
    v2 dv = projection(mov_a->v, col.mtv);
    //v2 com_a = transform(center_of_mass(mesh_a), *t_a);
    mov_a->v -= dv;
    //v2 r = com_a - col.poi;
    //real32 rel_torque = (r.x * mov_a->v.y - r.y * mov_a->v.x) / (r.x * r.x + mov_a->v.y * mov_a->v.y);
    //mov_a->rot_v -= rel_torque * magnitude(dv);
    //t_a->pos += col.mtv;
}

