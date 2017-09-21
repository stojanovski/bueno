#ifndef __TYPES___H____
#define __TYPES___H____

#ifdef _MSC_VER
#include <windows.h>
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef __int64          int64_t;
typedef unsigned __int64 uint64_t;
typedef SSIZE_T          ssize_t;
#else  /* ! _MSC_VER */
#include <stddef.h>  /* for size_t */
#include <stdint.h>  /* for uint8_t, etc.) */
#endif  /* ! _MSC_VER */

#endif /* __TYPES___H____ */
