#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/session.h"
#include "rooms/rooms_shared_8017d830.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern s32 func_800E0C10(GpRec18* rec, GpDeltaScratch* out, s16 arg2, void* arg3);

s32 RoomsShared8017d830(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2)
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
        coord->coord.t[0]          += ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          += s->delta.vz.h.hi;
        RoomsShared8017d830Delta.vx = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w >> 16;
        RoomsShared8017d830Delta.vy = s->delta.vy.w >> 16;
        RoomsShared8017d830Delta.vz = s->delta.vz.w >> 16;
        val                         = ((RoomsShared8017d830Scratch*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                RoomsShared8017d830Delta.vx++;
            } else {
                coord->coord.t[0]--;
                RoomsShared8017d830Delta.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                RoomsShared8017d830Delta.vz++;
            } else {
                coord->coord.t[2]--;
                RoomsShared8017d830Delta.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
