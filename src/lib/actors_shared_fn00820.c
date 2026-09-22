#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "psyq/abs.h"
#include <psyq/inline_c.h>

static __inline__ void Actor204000_ResetSlots(Actor104000Work* arg0)
{
    Actor104000Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

static __inline__ void Actor204000_TickSlots(Actor104000* arg0)
{
    Actor104000Work* work;
    s32              i;

    work = arg0->field_1C;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

void ActorsSharedFn00820(Actor104000* arg0)
{
    Actor104000Work* work;

    work = arg0->field_1C;
    if (work->field_170 == 1) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor204000_TickSlots(arg0);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}
