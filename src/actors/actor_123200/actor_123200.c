#include "common.h"

#include "actors/actor_123200.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void func_actor_123200_801332E0(Task* task);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_80072729;

extern u8 D_801153F4;

/// 0x14-byte block the movement step `func_actor_123200_801329F0` takes from
/// the scratchpad head: the delta `func_800E0C10` resolves from the movement
/// records, plus the "moved" flag the function returns.
typedef struct Actor123200DeltaFlag {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ s32            field_10;
} Actor123200DeltaFlag;
STATIC_ASSERT_SIZEOF(Actor123200DeltaFlag, 0x14);

/// Integer part of the last movement step `func_actor_123200_801329F0`
/// applied.
extern SVECTOR D_actor_123200_80137240;

/// Steps `coord` by the movement the first `arg2` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `D_actor_123200_80137240`. Returns the "moved" flag: set when the X or Z
/// delta is nonzero; where a delta also has a fractional part, the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 func_actor_123200_801329F0(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**                scratch;
    u8*                   head;
    Actor123200DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor123200DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_123200_80137240.vx = ((Actor123200DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_123200_80137240.vy = s->delta.vy.w >> 16;
        D_actor_123200_80137240.vz = s->delta.vz.w >> 16;
        val                        = ((Actor123200DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_123200_80137240.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_123200_80137240.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_123200_80137240.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_123200_80137240.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
