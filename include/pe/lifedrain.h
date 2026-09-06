#ifndef PE_LIFEDRAIN_H
#define PE_LIFEDRAIN_H

#include "common.h"

struct _GsCOORDINATE2;
struct _Task;

/// Per-level band row. `field_2` is the starting inner radius (also the per-frame
/// inner/outer step); `field_4` is the starting outer radius; `unk6` is the wedge
/// radius `func_lifedrain_8012FAF8` copies into `GpEffWork.field_26`. Indexed by
/// `(Gp_StateC08.field_0 % 10) - 1`.
typedef struct LifeDrainScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
} LifeDrainScale;
STATIC_ASSERT_SIZEOF(LifeDrainScale, 0xA);

extern LifeDrainScale D_lifedrain_80130AB4[];

/// The cast's collector task, published by `func_lifedrain_8012EF48`. Every
/// drain mote reparents itself onto it and adds its own `spawnArg1` to the
/// running total there.
extern struct _Task* D_lifedrain_80130B0C;

/// Draws one wedge of the drain funnel at frame `arg1` and radius `arg2`.
void func_lifedrain_801301AC(struct _GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

#endif /* PE_LIFEDRAIN_H */
