#pragma once

#include <cstddef>
#include <cstdint>

// unreal's basic types, sized the same on every platform except PTRINT
typedef uint8_t  BYTE;
typedef uint16_t _WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef int8_t   SBYTE;
typedef int16_t  SWORD;
typedef int32_t  INT;
typedef int64_t  SQWORD;
typedef float    FLOAT;
typedef double   DOUBLE;

enum { INDEX_NONE = -1 };
enum { MAXWORD = 0xffff };
typedef INT      UBOOL;
// storage for bool properties, one bit each
typedef DWORD    BITFIELD;
typedef char     ANSICHAR;

// the xbox build uses ansi strings throughout
typedef ANSICHAR TCHAR;
#define TEXT(s) s

// an integer big enough to hold a pointer: 4 bytes on xbox and vita, 8 on 64-bit hosts
typedef intptr_t  PTRINT;
typedef uintptr_t UPTRINT;

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#else
    #define FORCEINLINE inline __attribute__((always_inline))
#endif

// marks a function whose body hasn't been decompiled yet. reaching one at runtime is a fatal error.
#define NOT_DECOMPILED(address) appErrorf(TEXT("%s (%s) is not decompiled yet"), __func__, #address)
