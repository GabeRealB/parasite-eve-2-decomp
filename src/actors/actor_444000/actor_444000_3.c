#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern SVECTOR D_actor_444000_80161870;

s32 func_actor_444000_80132B14(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    Actor444000DeltaScratch* s;
    register void*           p asm("v1");
    s32                      val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]         += ((Actor444000DeltaScratch*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]         += s->delta.vz.h.hi;
        D_actor_444000_80161870.vx = ((Actor444000DeltaScratch*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_444000_80161870.vy = s->delta.vy.w >> 16;
        D_actor_444000_80161870.vz = s->delta.vz.w >> 16;
        val                        = ((Actor444000DeltaScratch*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_444000_80161870.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_444000_80161870.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_444000_80161870.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_444000_80161870.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
