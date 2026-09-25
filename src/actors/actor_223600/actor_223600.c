#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"

/// 0x14-byte block the movement step `func_actor_223600_8014AA04` takes from
/// the scratchpad head: the delta `func_800E0C10` resolves from the movement
/// records, plus the "moved" flag the function returns.
typedef struct Actor223600DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor223600DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor223600DeltaFlag, 0x14);

/// Integer part of the last movement step `func_actor_223600_8014AA04`
/// applied.
extern SVECTOR D_actor_223600_80150B54;

/// Steps `coord` by the movement the first `arg2` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `D_actor_223600_80150B54`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_223600_8014AA04(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor223600DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor223600DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_223600_80150B54.vx = ((Actor223600DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_223600_80150B54.vy = s->delta.vy.w >> 16;
        D_actor_223600_80150B54.vz = s->delta.vz.w >> 16;
        val                        = ((Actor223600DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_223600_80150B54.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_223600_80150B54.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_223600_80150B54.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_223600_80150B54.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
