#ifndef ACTOR_405800_ANIM_H
#define ACTOR_405800_ANIM_H

#include "common.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "actors/actor_405800.h"
#include "actors/actors_shared_8016a538.h"

/* Inline bodies shared by `actor_405800.c` callers of the rotation rebuild
 * and pending-anim tick. Same shape as `actor_400600_anim.h`. */

void func_actor_405800_80138224(Task* arg0);
void func_actor_405800_80138294(Task* arg0);
s16  func_actor_405800_8013836C(Task* arg0, s16 arg1);

/// `ActorsShared80139948`'s body, inlined: wrap the three angles to 12 bits and
/// rebuild the model root's rotation from them. Inlining is what keeps each
/// `G_SCRATCH_HEAD` access in the absolute `lui`/`lw` form instead of a
/// register CSE would otherwise hoist the address into.
static __inline__ void Actor405800_RebuildRotation(Task* arg0)
{
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
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

/// `func_actor_405800_8013A0F4`'s body, inlined: advance the pending animation
/// request, then tick every model slot at the current rate.
static __inline__ void Actor405800_TickAnim(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    s32              i;

    if (work->field_86E == 1) {
        if (work->field_870 != work->field_872) {
            work->field_874 = 0;
        } else {
            work->field_874 = func_actor_405800_8013836C(arg0, work->field_874);
        }
        func_actor_405800_80138294(arg0);
        work->field_86E = 3;
    } else if (work->field_86E == 2) {
        func_actor_405800_80138224(arg0);
        work->field_86E = 3;
        work->field_874 = 0;
    } else if (work->field_86E == 3) {
        work->field_874++;
    }
    i = 1;
    do {
        work->slots[i].field_9 = work->field_850;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
static __inline__ void Actor405800_UpdateColor(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)arg0->extra)->field_8[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

#endif
