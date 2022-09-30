#ifndef SHAPE_WHEEL_H

#include "math_wheel.h"

#define MAX_SHAPE_COUNT 64
#define MAX_VERTICES_PER_SHAPE 8

enum ShapeType {
    ST_NONE,
    ST_CIRCLE,
    ST_POLYGON,
    ST_COUNT
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
    union {
        ShapeCircle circle;
        ShapePolygon polygon;
    };
};

struct ShapeList {
    Shape shapes[MAX_SHAPE_COUNT];
    uint32 count;
};

struct BoundingBox {
    v2 min, max;
};

uint32
shape_create_circle(ShapeList *list, v2 center, real32 radius);

uint32
shape_create_polygon(ShapeList *list, uint32 count, v2 *vertices, v2 *normals);

BoundingBox
shape_get_bounding_box(Shape shape, real32 ang);


#define SHAPE_WHEEL_H
#endif
