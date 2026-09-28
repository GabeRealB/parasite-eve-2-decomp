#ifndef GAMEPLAY_PRIVATE_DIRECTION_H
#define GAMEPLAY_PRIVATE_DIRECTION_H

#include "common.h"

/// Direction-action handlers copied to the stack by func_800AD6BC.
typedef struct _GpDirActionTable {
    void (*funcs[7])(void);
} GpDirActionTable;
STATIC_ASSERT_SIZEOF(GpDirActionTable, 0x1C);

#endif // GAMEPLAY_PRIVATE_DIRECTION_H
