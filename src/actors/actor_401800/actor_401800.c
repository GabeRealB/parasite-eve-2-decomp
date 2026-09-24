#include "common.h"

#include <psyq/inline_c.h>
#include "psyq/abs.h"

#include "actors/actor_401800.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// 0x14-byte scratch `func_actor_401800_80132C68` takes from `G_SCRATCH_HEAD`:
/// the `GpDeltaScratch` `func_800E0C10` fills, then the returned flag.
typedef struct Actor401800DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor401800DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor401800DeltaFlag, 0x14);

/// Integer part of the last movement step `func_actor_401800_80132C68`
/// applied, nudged one unit outward where the step had a fractional part.
extern SVECTOR D_actor_401800_80155AD0;

/// Moves `coord` by the step the first `arg2` records of `movement` resolve to
/// (`func_800E0C10`) and latches that step's integer part into
/// `D_actor_401800_80155AD0`. Where the X or Z step has a fractional part,
/// the coordinate and the latched step go one unit further from zero. Returns
/// 1 when the X or Z step is nonzero.
s32 func_actor_401800_80132C68(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor401800DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor401800DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_401800_80155AD0.vx = ((Actor401800DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_401800_80155AD0.vy = s->delta.vy.w >> 16;
        D_actor_401800_80155AD0.vz = s->delta.vz.w >> 16;
        val                        = ((Actor401800DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_401800_80155AD0.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_401800_80155AD0.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_401800_80155AD0.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_401800_80155AD0.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
