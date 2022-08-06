#pragma once

#include <math.h>

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a < b ? b : a)
#define abs(a) (a >= 0 ? a : -a)

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

union v3 {
    struct {
        float x, y, z;
    };
    float elements[3];
};

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
normal(Line l) {
    return v2{l.a.y - l.b.y, l.b.x - l.a.x};
}

inline v3
operator+(v3 a, v3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline v3
operator-(v3 a, v3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

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
intersection(Rectangle a, Rectangle b) {
    Rectangle result = {};
    result.min = {max(a.min.x, b.min.x), max(a.min.y, b.min.y)};
    result.max = {min(a.max.x, b.max.x), min(a.max.y, b.max.y)};
    return result;
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

