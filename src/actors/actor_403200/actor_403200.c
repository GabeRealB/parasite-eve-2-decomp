#include "common.h"

#include "actors/actor_403200.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

/// 0x14-byte scratchpad frame `func_actor_403200_801324D0` carves off
/// `G_SCRATCH_HEAD`: the `GpDeltaScratch` it hands `func_800E0C10` plus the
/// "did the coordinate actually move" flag it returns.
typedef struct Actor403200DeltaScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor403200DeltaScratch;
STATIC_ASSERT_SIZEOF(Actor403200DeltaScratch, 0x14);

/// Integer part of the last step `func_actor_403200_801324D0` applied.
extern SVECTOR D_actor_403200_8015F8E8;

/// Step `coord` by the movement the first `arg2` records of `rec` resolve to,
/// and latch the integer part of that delta into `D_actor_403200_8015F8E8`.
/// Returns whether it moved: set when the X or Z delta is nonzero, and also
/// when only its fractional half is, in which case the coordinate and the
/// latched step are nudged one unit further from zero.
s32 func_actor_403200_801324D0(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    Actor403200DeltaScratch* s;
    register void*           p asm("v1");
    s32                      val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]         += ((Actor403200DeltaScratch*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]         += s->delta.vz.h.hi;
        D_actor_403200_8015F8E8.vx = ((Actor403200DeltaScratch*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_403200_8015F8E8.vy = s->delta.vy.w >> 16;
        D_actor_403200_8015F8E8.vz = s->delta.vz.w >> 16;
        val                        = ((Actor403200DeltaScratch*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_403200_8015F8E8.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_403200_8015F8E8.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_403200_8015F8E8.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_403200_8015F8E8.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
