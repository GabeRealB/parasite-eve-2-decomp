#ifndef PE_LIFEDRAIN_H
#define PE_LIFEDRAIN_H

#include "main/task_types.h"

#include "common.h"

struct GpCoord;
struct Task;

/// Per-level band row. `field_2` is the starting inner radius (also the per-frame
/// inner/outer step); `field_4` is the starting outer radius; `unk6` is the wedge
/// radius `func_lifedrain_8012FAF8` copies into `GpEffWork.angle`. Indexed by
/// `(Gp_StateC08.field_0 % 10) - 1`.
typedef struct LifeDrainScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
} LifeDrainScale;
STATIC_ASSERT_SIZEOF(LifeDrainScale, 0xA);

void func_lifedrain_8012F9A8(Task* arg0);

void func_lifedrain_801308C0(Task* arg0);

void func_lifedrain_8012EF48(Task* arg0);

void func_lifedrain_8012FAF8(Task* arg0);

#endif /* PE_LIFEDRAIN_H */
