#ifndef PHYSICS_WHEEL_H

#define MAX_POLYGON_VERTICES 8
#define MAX_BODY_COUNT 64
#define MAX_SHAPE_COUNT 64
#define MAX_COLLISION_COUNT 64

#define NEW_PHYSICS_SYSTEM 0

#include "mesh_wheel.h"


typedef void *BodyHandle;

enum CollisionMask {
    CM_NONE,
    CM_EVERYTHING = 0xFFFFFFFF,
};

enum ShapeType {
    ST_NONE,
    ST_CIRCLE,
    ST_POLYGON,
    ST_COUNT
};

struct Mass {
    v2 com;
    real32 value;
};

struct Material {
    real32 friction_dynamic;
    real32 friction_static;
    real32 restitution;
};

struct BodyDef {
    v2 p;
    v2 v;
    real32 p_ang;
    real32 v_ang;
    uint32 collision_mask;
    real32 m;
};

struct Body {
    v2 p;
    v2 v;
    real32 p_ang;
    real32 v_ang;
    uint32 collision_mask;
    real32 inertia;
    real32 inertia_inv;
    real32 m;
    real32 m_inv;
};

struct ShapeCircle {
    v2 center;
    real32 radius;
};

struct ShapePolygon {
    uint32 count;
    v2 *vertices;
    v2 *normals;
};

struct Shape {
    ShapeType type;
    // TODO: Probably wanna use a hash table instead of "hard linking" a shape
    // to a body. Both for decoupling reasons and to allow for instanced
    // rendering.
    Body *body;
    union {
        ShapeCircle circle;
        ShapePolygon polygon;
    };
};

#if NEW_PHYSICS_SYSTEM
struct Collision {
    Body *a;
    Body *b;
    v2 poi_a;
    v2 poi_b;
    v2 normal;
    real32 overlap;
    bool collided;
};
#else
struct Collision {
    Mesh mesh_a;
    Mesh mesh_b;
    Material mat_a;
    Material mat_b;
    v2 *v_a;
    v2 *v_b;
    v2 poi_a;
    v2 poi_b;
    v2 p_a;
    v2 p_b;
    v2 normal;
    real32 overlap;
    real32 m_a;
    real32 m_b;
    bool collided;
};
#endif

struct Physics {
    // TODO: Memory management!
    uint32 body_count;
    Body bodies[MAX_BODY_COUNT];
    uint32 shape_count;
    Shape shapes[MAX_SHAPE_COUNT];
    uint32 collision_count;
    Collision collisions[MAX_COLLISION_COUNT];
};

struct Movement {
    v2 v;
    v2 a;
    real32 v_ang;
    real32 a_ang;
};

uint32
body_create(BodyDef def);

uint32
shape_create_circle(v2 center, real32 radius);

uint32
shape_create_polygon(uint32 count, const v2 *vertices);

void
handle_collisions(real64 d_t);

void
apply_external_forces(Movement *movements, Material *materials, Collision *collisions, uint32 *masks, uint32 count, real64 d_t);

void
update_velocities(Movement *movements, uint32 *masks, uint32 count, real64 d_t);

void
apply_impulse(Mesh mesh, real32 m, v2 com, v2 *v, v2 poi, v2 f);

#if NEW_PHYSICS_SYSTEM
void
check_collision(Collision *collision, Body a, Body b, real64 dt, bool sweep);
#else
void
check_collision(Collision *collision, Mesh a, Mesh b, Transform t_a, Transform t_b, v2 v_a, v2 v_b, real64 d_t, bool sweeping);
#endif

void
resolve_collision(Collision *col);


#define PHYSICS_WHEEL_H
#endif
