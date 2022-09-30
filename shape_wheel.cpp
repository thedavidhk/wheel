#include <float.h>

#include "shape_wheel.h"

static void
calculate_normals(uint32 count, v2 *vertices, v2 *normals);

uint32
shape_create_circle(ShapeList *list, v2 center, real32 radius) {
    Shape shape;
    shape.type = ST_CIRCLE;
    shape.circle.center = center;
    shape.circle.radius = radius;
    list->shapes[list->count] = shape;
    return list->count++;
}

uint32
shape_create_polygon(ShapeList *list, uint32 count, v2 *vertices, v2 *normals) {
    Shape shape;
    shape.type = ST_POLYGON;
    shape.polygon.count = count;
    calculate_normals(count, vertices, normals);
    shape.polygon.vertices = vertices;
    shape.polygon.normals = normals;
    assert(list->count < MAX_SHAPE_COUNT);
    list->shapes[list->count] = shape;
    return list->count++;
}

BoundingBox
shape_get_bounding_box(Shape shape, real32 ang) {
    // TODO: Implement this for other shape types
    BoundingBox b = {
        {FLT_MAX, FLT_MAX},
        {-FLT_MAX, -FLT_MAX}
    };
    for (uint32 i = 0; i < shape.polygon.count; i++) {
        v2 rotated = rotate(shape.polygon.vertices[i], {0, 0}, ang);
        b.min.x = min(b.min.x, rotated.x);
        b.min.y = min(b.min.y, rotated.y);
        b.max.x = max(b.max.x, rotated.x);
        b.max.y = max(b.max.y, rotated.y);
    }
    return b;
}

static void
calculate_normals(uint32 count, v2 *vertices, v2 *normals) {
    for (uint32 i = 0; i < count; i++) {
        uint32 i_next = i < count-1 ? i+1 : 0;
        normals[i] = rnormal(vertices[i_next] - vertices[i]);
    }
}
