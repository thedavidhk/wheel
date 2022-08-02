#pragma once

#include <math.h>

#define min(a, b) a < b ? a : b
#define max(a, b) a < b ? b : a

union v2 {
    struct {
        float x1, x2;
    };
    float x[2];
};

struct rect {
    v2 min, max;

    rect() {
        this->min = {};
        this->max = {};
    }

    rect(v2 a, v2 b) {
        this->min.x1 = min(a.x1, b.x1);
        this->min.x2 = min(a.x2, b.x2);
        this->max.x1 = max(a.x1, b.x1);
        this->max.x2 = max(a.x2, b.x2);
    }
};

union v3 {
    struct {
        float x1, x2, x3;
    };
    float x[3];
};

inline v2
operator+(v2 a, v2 b) {
    return {a.x1 + b.x1, a.x2 + b.x2};
}

inline v2
operator-(v2 a, v2 b) {
    return {a.x1 - b.x1, a.x2 - b.x2};
}

inline v2
operator*(v2 a, float b) {
    return {a.x1 * b, a.x2 * b};
}

inline v2
operator/(v2 a, float b) {
    return {a.x1 / b, a.x2 / b};
}

inline float
magnitude(v2 v) {
    float square = v.x1 * v.x1 + v.x2 * v.x2;
    if (square < 0.0001f)
        return 0;
    return sqrt(v.x1 * v.x1 + v.x2 * v.x2);
}

inline v3
operator+(v3 a, v3 b) {
    return {a.x1 + b.x1, a.x2 + b.x2, a.x3 + b.x3};
}

inline v3
operator-(v3 a, v3 b) {
    return {a.x1 - b.x1, a.x2 - b.x2, a.x3 - b.x3};
}

inline bool
isPositive(rect a) {
    if (a.min.x1 >= a.max.x1 || a.min.x2 >= a.max.x2)
        return false;
    return true;
}

inline rect
intersection(rect a, rect b) {
    rect result = {};
    result.min = {max(a.min.x1, b.min.x1), max(a.min.x2, b.min.x2)};
    result.max = {min(a.max.x1, b.max.x1), min(a.max.x2, b.max.x2)};
    return result;
}

