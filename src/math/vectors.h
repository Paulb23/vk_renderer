#ifndef VECTORS_H_
#define VECTORS_H_

typedef struct Vect2 {
    float x;
    float y;
} Vect2;

///

typedef struct Vect3 {
    float x;
    float y;
    float z;
} Vect3;

Vect3 vect3_normalize(const Vect3 p_a);

float vect3_dot(const Vect3 p_a, const Vect3 p_b);

Vect3 vect3_cross(const Vect3 p_a, const Vect3 p_b);

Vect3 vect3_add(const Vect3 p_a, const Vect3 p_b);

Vect3 vect3_sub(const Vect3 p_a, const Vect3 p_b);

Vect3 vect3_multi(const Vect3 p_a, const float p_factor);

///

typedef struct Vect4 {
    union {
        float r;
        float x;
    };
    union {
        float g;
        float y;
    };
    union {
        float b;
        float z;
    };
    union {
        float a;
        float w;
    };
} Vect4;

#endif
