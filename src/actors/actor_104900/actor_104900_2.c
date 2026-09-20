#include "common.h"

#include "actors/actor_104900.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_801388e8.h"
#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include <psyq/inline_c.h>

extern u32       Gp_LcgState;
extern u8        D_actor_104900_80147480[];
extern GpU16Pair D_actor_104900_801392F0[];

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
s32  func_actor_104900_80132D78(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_801359CC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80135FDC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_80137498);

/// Countdown handler built around the halfword at 0xB8C.
///
/// The first frame arms the motion pair: `field_BA4` takes 0x13, or 0x14 while
/// the flag at 0xBAE is set, `field_BA5` and `field_BAF` both take 1 and the
/// countdown is zeroed, with the run-once latch at 0xBA8 stepped either way.
/// Every later frame moves the countdown up by one and, on the frame it reaches
/// 5, cues the 0x400B0003 event - the actor's id byte at 0xB88 in bits 8..15
/// and the variant at 0xBB8 in bit 22, pan and depth from the frame block -
/// then parks the countdown at -0x7FFF so it fires only once. The frame block's
/// scratch byte at 0x64 takes 3 either way, and the trigger at 0xBA9 ends the
/// sub-state: while `field_B92` still counts it keeps the state on the 0x17
/// motion with the 0x10 pair when the enemy is not carrying flag 0x2 in
/// `field_4C`, and stages the 0x14 motion through `field_BA6` when it is; once
/// that count has run out it hands the frame to the shared routine at
/// 0x80137498 on state 0x18 instead.
///
/// Same body as the four twins - `func_actor_101100_80137B1C` at the same
/// address, `func_actor_201100_8014FB1C` / `func_actor_204900_8014FB1C` 0x18000
/// past it and `func_actor_301100_80167B1C` 0x30000 past - but the last call
/// reaches this overlay's own `func_actor_104900_80137498`, so the body cannot
/// move into `src/actors/lib/`.
void func_actor_104900_80137B1C(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    u16 time;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x13;
        } else {
            work->field_BA4 = 0x14;
        }
        work->field_BA5 = 1;
        work->field_BAF = 1;
        work->field_B8C = 0;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    time            = (u16)work->field_B8C + 1;
    work->field_B8C = time;
    if ((s16)time >= 5) {
        SndEvt_EnqueueType6((work->field_BB8 << 22) | ((work->field_B88 << 8) | 0x400B0003), arg->pan, arg->depth);
        work->field_B8C = -0x7FFF;
    }
    arg->field_64 = 3;
    if (work->field_BA9 != 0) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->reactionFlags & 2)) {
                work->field_BAB = 0x10;
                work->field_BAF = 2;
                work->state     = 0x17;
            } else {
                work->field_BA6 = 2;
                work->field_BAB = 5;
                work->state     = 0x14;
            }
            work->field_BA8 = 0;
            return;
        }
        work->state     = 0x18;
        work->field_BA8 = 0;
        func_actor_104900_80137498(enemy, task, work, arg);
    }
}

/// Spawns the effect this actor's next state rides on and re-homes the actor.
///
/// The 0x58-byte work block goes in `Task::work` and the effect task comes back
/// from `Gp_SpawnEff` as `0x60081` parented to the model's trailing coordinate;
/// that task becomes `Task::spawnArg2` and the actor's parent, and the actor arms
/// its own 0x5A kill countdown.
///
/// The effect's velocity is a random direction in the actor's frame: an SVECTOR
/// is built 8 bytes into the scratchpad pool below its published head, with X
/// and Z from `rsin` / `rcos` of the spawn argument and Y a 9-bit draw hung below
/// 0xE000, rotated through the actor's current `coord` and then scaled by
/// `((Gp_LcgState >> 16) & 0x1F) + 0x28` of 0x1000, which the work block keeps.
/// The coordinate is reset to the identity first - a 0x1000 diagonal, the
/// off-diagonal pairs written as zeroed words - then the velocity's X and Z are
/// added to its translation and a 7-bit draw to the Y, and `flg` is cleared.
///
/// The display node is linked as kind 3 pointing at the coordinate and at the
/// 0x28 record, which takes 0x96 for `end0Radius` / `end1Radius` and points
/// `recs` at the one-entry collision table `Gp_InitRec18Table` zeroes, and
/// its `0xC000` flag pair is ORed in on top of `Gp_LinkObj`'s `flags = 3`. The
/// actor takes `ActorsShared801511c8` as its exit callback and steps on to the
/// next state, which it also runs immediately.
///
/// The stack copy of the vector is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist, which this body's schedule does not.
void func_actor_104900_80137C88(Task* task)
{
    Actor104900Work* work;
    GpActorD4Rec*    rec;
    GsCOORDINATE2*   coord;
    GpEffWork*       eff;
    GpObj*           obj;
    MATRIX*          mtx;
    SVECTOR          local;
    SVECTOR*         vec;
    u8*              head;
    s32              angle;

    coord = ((TmdObject*)task->extra)->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->work = (TaskIdMap*)work;
    eff        = Gp_SpawnEff(0x60081, coord, 0, 0);
    if (eff == NULL) {
        Task_CallExit(task);
        return;
    }
    task->spawnArg2 = eff->task;
    Task_Reparent(task, eff->task);
    angle               = task->spawnArg1;
    task->killCountdown = 0x5A;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 8;
    vec                   = (SVECTOR*)(head - 8);
    Gp_LcgState           = Gp_LcgState * 5 + 0x71357911;
    vec->vy               = 0xE000 - ((Gp_LcgState >> 16) & 0x1FF);
    vec->vx               = rsin(angle);
    vec->vz               = rcos(angle);

    mtx   = &coord->coord;
    local = *(SVECTOR*)(head - 8);
    gte_SetRotMatrix(mtx);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0_real();
    gte_stsv(vec);

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    gte_lddp(((Gp_LcgState >> 16) & 0x1F) + 0x28);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(&work->vel);

    *(s32*)&mtx->m[0][0] = 0x1000;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = 0x1000;

    coord->coord.t[0] += work->vel.vx;
    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
    coord->coord.t[1] += (Gp_LcgState >> 16) & 0x7F;
    coord->coord.t[2] += work->vel.vz;
    coord->flg         = 0;

    obj            = &work->obj;
    rec            = &work->rec;
    obj->coord     = coord;
    obj->ctx.d4rec = rec;
    obj->pos.vx    = 0;
    obj->pos.vy    = 0;
    obj->pos.vz    = 0;
    obj->radius    = 0;
    obj->key       = Gp_PackPair(&D_actor_104900_801392F0[0], 5);
    obj->flags     = 3;

    rec->recs       = work->rec18;
    rec->end1.vx    = 0;
    rec->end1.vy    = 0;
    rec->end1.vz    = 0;
    rec->end0.vx    = 0;
    rec->end0.vy    = 0;
    rec->end0.vz    = 0;
    rec->end0Radius = 0x96;
    rec->end1Radius = 0x96;
    Gp_InitRec18Table(work->rec18, 1, 0);
    Gp_LinkObj(3, obj);
    obj->flags |= 0xC000;

    task->exitCallback     = ActorsShared801511c8;
    *(u8**)G_SCRATCH_HEAD += 8;
    task->state           += 1;
    ActorsShared80137fb8(task);
}
