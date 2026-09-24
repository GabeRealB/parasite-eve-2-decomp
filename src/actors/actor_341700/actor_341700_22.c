#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

/// 0x14-byte `G_SCRATCH_HEAD` block `func_actor_341700_8016B804` gives
/// `func_800E0C10`: the `GpDeltaScratch` it fills plus the returned flag, set
/// when the X or Z delta is nonzero.
typedef struct Actor341700DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor341700DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor341700DeltaFlag, 0x14);

/// Whole-unit part of the last movement step `func_actor_341700_8016B804`
/// applied, rounded away from zero when the step had a fraction.
extern SVECTOR D_actor_341700_80176360;

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of
/// `movement` resolve to, and keeps the whole-unit part of that step in
/// `D_actor_341700_80176360`. Returns 1 when the X or Z step is nonzero; a
/// step with a fractional part moves the coordinate and the kept step one
/// unit further from zero.
s32 func_actor_341700_8016B804(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor341700DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_341700_80176360.vx = ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_341700_80176360.vy = s->delta.vy.w >> 16;
        D_actor_341700_80176360.vz = s->delta.vz.w >> 16;
        val                        = ((Actor341700DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_341700_80176360.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_341700_80176360.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_341700_80176360.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_341700_80176360.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
