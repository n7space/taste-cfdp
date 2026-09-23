#ifndef FLASH_H
#define FLASH_H

#include "component_efc.h"

#if defined(N7S_TARGET_SAMV71Q21)
#define EFC ((Efc*)0x400E0C00U)
#elif defined(N7S_TARGET_SAMRH71F20)
#define EFC ((Efc*)0x40004000U)
#else
#error "Unknown platform"
#endif

#define IFLASH_MAX_PAGE_SIZE (1024u)

typedef struct {
    Efc* efc;
    uint32_t address;
    uint32_t size;
    uint32_t page_size;
    uint32_t page_count;
    uint32_t lock_bits;
    uint32_t lock_region_size;
} FlashAccess;

#endif // FLASH_H
