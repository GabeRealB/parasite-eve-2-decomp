#ifndef GAMEPLAY_PRIVATE_LIGHTING_WORK_H
#define GAMEPLAY_PRIVATE_LIGHTING_WORK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// 0x14-byte scratch from the scratch stack used by `Gp_ProjectToSxy`.
/// `vec` is the packed `SVECTOR` fed to RTPS. `p` / `flag` / `otz` hold
/// IR0, FLAG, and `SZ3 >> 2`.
typedef struct _GpPerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
} GpPerspScratch;
STATIC_ASSERT_SIZEOF(GpPerspScratch, 0x14);

#endif // GAMEPLAY_PRIVATE_LIGHTING_WORK_H
