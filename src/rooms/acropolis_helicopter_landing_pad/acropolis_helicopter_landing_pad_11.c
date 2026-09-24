#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/session.h"
#include "rooms/rooms_shared_8017d830.h"

extern s32 func_800E0C10(GpRec18* rec, GpDeltaScratch* out, s16 arg2, void* arg3);

/// The whole-unit delta of the last step
/// `func_acropolis_helicopter_landing_pad_801819C0` applied.
extern SVECTOR D_acropolis_helicopter_landing_pad_80187F88;

/// Moves a coordinate frame by the 16.16 delta `func_800E0C10` computes for
/// `rec`: adds its integer part to X and Z, rounds a fractional remainder of
/// X or Z one unit away from zero, and keeps the applied delta in
/// `D_acropolis_helicopter_landing_pad_80187F88`. Returns 1 when the delta's
/// X or Z is non-zero. Works in a 0x14 block from `G_SCRATCH_HEAD`.
s32 func_acropolis_helicopter_landing_pad_801819C0(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2)
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
        coord->coord.t[0]                             += ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                             += s->delta.vz.h.hi;
        D_acropolis_helicopter_landing_pad_80187F88.vx = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_helicopter_landing_pad_80187F88.vy = s->delta.vy.w >> 16;
        D_acropolis_helicopter_landing_pad_80187F88.vz = s->delta.vz.w >> 16;
        val                                            = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_helicopter_landing_pad_80187F88.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_helicopter_landing_pad_80187F88.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_helicopter_landing_pad_80187F88.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_helicopter_landing_pad_80187F88.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
