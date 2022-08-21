#ifndef PHYSICS_WHEEL_H

#include "mesh_wheel.h"

struct Mass {
    v2 com;
    real32 value;
};

struct Collision {
    bool collided = false;
    v2 poi;
    v2 mtv;
};

struct Movement {
    v2 v;
    v2 a;
    real32 rot_v;
    real32 rot_a;
};

void
accelerate(v2 direction, real32 force, real32 mass, Movement *m, real64 d_t);

Collision
check_collision(Mesh a, Mesh b, Transform t_a, Transform t_b, Movement mov_a, Movement mov_b, real64 d_t);

void
resolve_collision(Collision col, Mesh mesh_a, Mesh mesh_b, Transform *t_a, Transform *t_b, Movement *mov_a, Movement *mov_b, Mass m_a, Mass m_b);

void
resolve_collision(Collision col, Mesh mesh_a, Transform *t_a, Transform t_b, Movement *mov_a, Mass m_a);


#define PHYSICS_WHEEL_H
#endif
