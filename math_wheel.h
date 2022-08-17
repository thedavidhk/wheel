#ifndef MATH_WHEEL_H
#define MATH_WHEEL_H

#include <math.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))
#define abs(a) ((a) >= 0 ? (a) : (-(a)))

#define round(a) (int)(a + 0.5)

#define EPSILON 0.001f

#define FINVALID (float)0xFFFFFFFF

union v2 {
    struct {
        float x, y;
    };
    float elements[2];
};

struct Rectangle {
    v2 min, max;

    Rectangle() {
        this->min = {};
        this->max = {};
    }

    Rectangle(v2 a, v2 b) {
        this->min.x = min(a.x, b.x);
        this->min.y = min(a.y, b.y);
        this->max.x = max(a.x, b.x);
        this->max.y = max(a.y, b.y);
    }
};

struct Circle {
    v2 center;
    float radius;
};

struct Line {
    v2 a, b;
};

struct MinkowskiSum {
    Rectangle rect;
    v2 point;
};

struct Intersection {
    bool exists;
    v2 coordinates;
};

struct M2 {
    float m11, m12, m21, m22;
};

struct Polygon {
    int vertex_count;
    v2 *vertices;
};

struct Triangle {
    v2 a, b, c;
};

inline int
v2_compare_x(const void *a, const void *b) {
    const v2 *va = (v2 *)a;
    const v2 *vb = (v2 *)b;
    if (va->x > vb->x)
        return 1;
    else if (va->x < vb->x)
        return -1;
    else
        return 0;
}

inline int
v2_compare_y(const void *a, const void *b) {
    const v2 *va = (v2 *)a;
    const v2 *vb = (v2 *)b;
    if (va->y > vb->y)
        return 1;
    else if (va->y < vb->y)
        return -1;
    else
        return 0;
}

inline v2
operator+(v2 a, v2 b) {
    return {a.x + b.x, a.y + b.y};
}

inline v2&
operator+=(v2& a, const v2& b) {
    a = a + b;
    return a;
}

inline v2
operator-(v2 a) {
    return {-a.x, -a.y};
}

inline v2
operator-(v2 a, v2 b) {
    return {a.x - b.x, a.y - b.y};
}

inline v2&
operator-=(v2& a, const v2& b) {
    a = a - b;
    return a;
}

inline v2
operator*(v2 a, float b) {
    return {a.x * b, a.y * b};
}

inline v2
operator*(float b, v2 a) {
    return {a.x * b, a.y * b};
}

inline v2&
operator*=(v2& a, float b) {
    a = a * b;
    return a;
}

inline v2
operator/(v2 a, float b) {
    return {a.x / b, a.y / b};
}

inline v2&
operator/=(v2& a, float b) {
    a = a / b;
    return a;
}

inline v2
operator/(float b, v2 a) {
    return {a.x / b, a.y / b};
}

inline float
dot(v2 a, v2 b) {
    return a.x * b.x + a.y * b.y;
}

inline float
magnitude(v2 v) {
    float square = dot(v, v);
    if (square < EPSILON)
        return 0;
    return sqrt(square);
}

inline v2
rnormal(Line l) {
    return v2{l.a.y - l.b.y, l.b.x - l.a.x};
}

inline v2
rnormal(v2 a, v2 b) {
    return v2{a.y - b.y, b.x - a.x};
}

inline v2
lnormal(Line l) {
    return -rnormal(l);
}

inline v2
lnormal(v2 a, v2 b) {
    return -rnormal(a, b);
}

inline v2
operator*(M2 m, v2 v) {
    return v2{m.m11 * v.x + m.m12 * v.y, m.m21 * v.x + m.m22 * v.y};
};

inline M2
operator*(M2 a, M2 b) {
    return M2{
        a.m11 * b.m11 + a.m12 * b.m21,
        a.m11 * b.m12 + a.m12 * b.m22,
        a.m21 * b.m11 + a.m22 * b.m21,
        a.m21 * b.m12 + a.m22 * b.m22
    };
};

inline Rectangle
operator+(Rectangle r, v2 offset) {
    return Rectangle(r.min + offset, r.max + offset);
}

inline Rectangle
operator-(Rectangle r, v2 offset) {
    return Rectangle(r.min - offset, r.max - offset);
}

inline bool
isPositive(Rectangle a) {
    if (a.min.x >= a.max.x || a.min.y >= a.max.y)
        return false;
    return true;
}

inline Rectangle
area_intersection(Rectangle a, Rectangle b) {
    a.min.x = max(a.min.x, b.min.x);
    a.min.y = max(a.min.y, b.min.y);
    a.max.x = min(a.max.x, b.max.x);
    a.max.y = min(a.max.y, b.max.y);
    return a;
}

inline void get_sides(Rectangle a, Line*sides) {
    sides[0] = Line{a.min, {a.min.x, a.max.y}};
    sides[1] = Line{{a.min.x, a.max.y}, a.max};
    sides[2] = Line{a.max, {a.max.x, a.min.y}};
    sides[3] = Line{{a.max.x, a.min.y}, a.min};
}

inline Intersection intersection(Line a, Line b) {
    Intersection intersect = {};
    float t_1 = (a.a.x - b.a.x) * (b.a.y - b.b.y) - (a.a.y - b.a.y) * (b.a.x - b.b.x);
    float t_2 = (a.a.x - a.b.x) * (b.a.y - b.b.y) - (a.a.y - a.b.y) * (b.a.x - b.b.x);
    float u_1 = (a.a.x - b.a.x) * (a.a.y - a.b.y) - (a.a.y - b.a.y) * (a.a.x - a.b.x);
    float u_2 = (a.a.x - a.b.x) * (b.a.y - b.b.y) - (a.a.y - a.b.y) * (b.a.x - b.b.x);
    intersect.exists = (t_1 * t_2 >= -EPSILON && abs(t_1) <= abs(t_2) + EPSILON && abs(t_2) > EPSILON &&
                        u_1 * u_2 >= -EPSILON && abs(u_1) <= abs(u_2) + EPSILON && abs(u_2) > EPSILON);
    if (intersect.exists) {
        intersect.coordinates = a.a + (t_1 / t_2) * (a.b - a.a);
    }
    return intersect;
}

inline MinkowskiSum
minkowski_sum(Rectangle a, Rectangle b) {
    MinkowskiSum m;
    v2 a_center = (a. max - a.min) / 2 + a.min;
    m.rect = Rectangle(b.min + (a.min - a_center), b.max + (a.max - a_center));
    m.point = a_center;
    return m;
}

// Check if polygon defined clockwise and convex
bool
validatePolygon(Polygon p) {
    if (p.vertex_count < 3)
        return false;
    v2 pprev = p.vertices[0];
    v2 prev = p.vertices[1];
    bool result = true;
    for (int i = 2; i < p.vertex_count; i++) {
        v2 prev_norm = rnormal(Line{pprev, prev});
        result = result && (dot(p.vertices[i] - pprev, prev_norm) >= 0);
        pprev = prev;
        prev = p.vertices[i];
    }
    return result;
}

bool
isInsidePolygon(v2 point, Polygon poly) {
    v2 a = poly.vertices[0];
    bool result = true;
    for (int i = 2; i < poly.vertex_count; i++) {
        v2 b = poly.vertices[i];
        v2 rnorm = rnormal(Line{a, b});
        result = result && (dot(point - a, rnorm) >= 0);
        a = b;
    }
    result = result && (dot(point - poly.vertices[poly.vertex_count], rnormal(Line{poly.vertices[poly.vertex_count], poly.vertices[0]})) >= 0);
    return result;
}

inline bool
isInsideTriangle(v2 point, Triangle t) {
    return (dot(point - t.a, rnormal(Line{t.a, t.b})) >= 0 &&
            dot(point - t.b, rnormal(Line{t.b, t.c})) >= 0 &&
            dot(point - t.c, rnormal(Line{t.c, t.a})) >= 0);
}

inline Rectangle
boundingBox(Triangle t) {
    Rectangle r = {};
    r.min = v2{min(min(t.a.x, t.b.x), t.c.x), min(min(t.a.y, t.b.y), t.c.y)};
    r.max = v2{max(max(t.a.x, t.b.x), t.c.x), max(max(t.a.y, t.b.y), t.c.y)};
    return r;
}

inline v2
normalize(v2 v) {
    float mag = magnitude(v);
    if (abs(mag) < EPSILON)
        return {0};
    if (abs(mag - 1) < EPSILON)
        return v;
    return v / mag;
}

inline v2
projection(v2 a, v2 b) {
    return b * dot(a, b) / dot(b, b);
}

inline v2
rotate(v2 v, v2 por, float angle) {
    if (abs(angle) < EPSILON)
        return v;
    M2 rot = {cos(angle), -sin(angle), sin(angle), cos(angle)};
    return rot * v;
    //v2 result;
    //result.x = ((v.x - por.x) * cos(angle)) - ((por.y - v.y) * sin(angle)) + por.x;
    //result.y = por.y - ((por.y - v.y) * cos(angle)) + ((v.x - por.x) * sin(angle));
    //return result;
}
#endif
