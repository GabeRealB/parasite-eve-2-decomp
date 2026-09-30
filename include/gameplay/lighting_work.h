#ifndef GAMEPLAY_LIGHTING_WORK_H
#define GAMEPLAY_LIGHTING_WORK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// 0x30-byte scratch from the scratch stack used by `Gp_UpdateActorColor`.
/// `mtx` holds the previous-mode 3x3 copy. `col0` / `col1` are the
/// current and previous columns packed for GPF/GPL.
typedef struct _GpColorScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ SVECTOR col0;
    /* 0x28 */ SVECTOR col1;
} GpColorScratch;
STATIC_ASSERT_SIZEOF(GpColorScratch, 0x30);

#endif // GAMEPLAY_LIGHTING_WORK_H
