#ifndef TYPES_WHEEL_H

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

#define TYPES_WHEEL_H
#endif
