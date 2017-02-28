#ifndef SOI_VFS_TYPES_H_
#define SOI_VFS_TYPES_H_

#include <stdint.h> // fixed size int types
#include <limits.h>		/* for CHAR_BIT */

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

typedef uint32_t size_type; // 4 bytes
typedef uint32_t block_index_type; // 4 bytes
typedef char* bitmap;

#endif
