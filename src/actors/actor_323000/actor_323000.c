#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor_323000.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/mem.h"

/// 0x14-byte `G_SCRATCH_HEAD` block `func_actor_323000_80162A2C` gives
/// `func_800E0C10`: the `GpDeltaScratch` it fills plus the returned flag, set
/// when the X or Z delta is nonzero.
typedef struct Actor323000DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor323000DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor323000DeltaFlag, 0x14);

/// Whole-unit part of the last movement step `func_actor_323000_80162A2C`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_323000_80173A14;

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_323000_80173A14`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_323000_80162A2C(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor323000DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor323000DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_323000_80173A14.vx = ((Actor323000DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_323000_80173A14.vy = s->delta.vy.w >> 16;
        D_actor_323000_80173A14.vz = s->delta.vz.w >> 16;
        val                        = ((Actor323000DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_323000_80173A14.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_323000_80173A14.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_323000_80173A14.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_323000_80173A14.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

INCLUDE_RODATA("actors/nonmatchings/actor_323000/actor_323000", D_actor_323000_80161E24);

INCLUDE_RODATA("actors/nonmatchings/actor_323000/actor_323000", D_actor_323000_80161E34);
