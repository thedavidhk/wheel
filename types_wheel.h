#ifndef TYPES_WHEEL_H

#include <stdio.h>

#define int8 char
#define int16 short
#define int32 int
#define int64 long long

#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned long long

#define real32 float
#define real64 double

#define kilobytes(value) ((value) * 1024)
#define megabytes(value) (kilobytes(value) * 1024)
#define gigabytes(value) (megabytes(value) * 1024)

#define assert(b) do { if (!(b)) {printf("Assertion failed: %s in line %d of file %s\n", __STRING(b), __LINE__, __FILE__); exit(1);} } while (0)

// TODO: This should not be necessary in a data driven approach
typedef enum {
    CM_Pos              = 1 <<  0,
    CM_Rotation         = 1 <<  1,
    CM_Mesh             = 1 <<  2,
    CM_Color            = 1 <<  3,
    CM_Velocity         = 1 <<  4,
    CM_Force            = 1 <<  5,
    CM_Mass             = 1 <<  6,
    CM_Friction         = 1 <<  7,
    CM_Collision        = 1 <<  8,
    CM_BoxCollider      = 1 <<  9,
    CM_Texture          = 1 << 10,
    CM_Selectable       = 1 << 11
} ComponentMask; 

#define TYPES_WHEEL_H
#endif
