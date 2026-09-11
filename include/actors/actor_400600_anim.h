#ifndef ACTOR_400600_ANIM_H
#define ACTOR_400600_ANIM_H

#include "common.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "actors/actor_400600.h"
#include "actors/actors_shared_8016a538.h"

/* Inline bodies shared by `actor_400600.c` and `actor_400600_2.c`. */

void func_actor_400600_80139A78(Task* arg0);
void func_actor_400600_80139AE8(Task* arg0);
s16  func_actor_400600_80139BA0(Task* arg0, s16 arg1);

/// `ActorsShared80139948`'s body, inlined: wrap the three angles to 12 bits and
/// rebuild the model root's rotation from them. Inlining is what keeps each
/// `G_SCRATCH_HEAD` access in the absolute `lui`/`lw` form instead of a
/// register CSE would otherwise hoist the address into.
static __inline__ void Actor400600_RebuildRotation(Task* arg0)
{
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80           &= 0xFFF;
    work->field_82           &= 0xFFF;
    work->field_84           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// `func_actor_400600_80139CAC`'s body, inlined: advance the pending animation
/// request, then tick every model slot at the current rate.
static __inline__ void Actor400600_TickAnim(Task* arg0)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;
    s32              i;

    if (work->field_742 == 1) {
        if (work->field_744 != work->field_746) {
            work->field_748 = 0;
        } else {
            work->field_748 = func_actor_400600_80139BA0(arg0, work->field_748);
        }
        func_actor_400600_80139AE8(arg0);
        work->field_742 = 3;
    } else if (work->field_742 == 2) {
        func_actor_400600_80139A78(arg0);
        work->field_742 = 3;
        work->field_748 = 0;
    } else if (work->field_742 == 3) {
        work->field_748++;
    }
    i = 1;
    do {
        work->slots[i].field_9 = work->field_726;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

#endif
