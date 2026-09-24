#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/session.h"
#include "rooms/rooms_shared_8017d830.h"

extern SVECTOR D_acropolis_cafeteria_8018D6AC;

extern s32 func_800E0C10(GpRec18* rec, GpDeltaScratch* out, s16 arg2, void* arg3);

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_cafeteria_8018D6AC`. Returns non-zero when the X or Z
/// displacement is non-zero.
s32 func_acropolis_cafeteria_80181ED4(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2)
{
    void**                      scratch;
    u8*                         head;
    RoomsShared8017d830Scratch* s;
    register void*              p asm("v1");
    s32                         val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                += ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                += s->delta.vz.h.hi;
        D_acropolis_cafeteria_8018D6AC.vx = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vy = s->delta.vy.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vz = s->delta.vz.w >> 16;
        val                               = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_cafeteria_8018D6AC.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_cafeteria_8018D6AC.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_cafeteria_8018D6AC.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_cafeteria_8018D6AC.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
