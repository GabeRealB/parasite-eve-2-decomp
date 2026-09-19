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
void func_actor_104900_80137FB8(Task* task);
s32  func_actor_104900_80132D78(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80131F08);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132250);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", D_actor_104900_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", D_actor_104900_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_8013279C);

/// Arms the enemy's four display nodes the first time the state handler runs
/// with the CD command queue idle: the enemy's own link node is put back on the
/// list, node 0 takes the model's root coordinate and node 3 the pose 3 slots
/// along it, both linked as kind 2 with their `flags` halves ORed in and a
/// three-entry collision table each, and nodes 1 and 2 are linked as kind 3
/// with a `Gp_PackObjPair` payload, the first of the two taking pose 0xC and
/// the second pose 8 of the model's 0x50-byte coordinate records. The task then
/// takes `ActorsShared801384ac` as its
/// exit callback, the model's hidden bit is lifted, `msgTable` is pointed at
/// this overlay's message table and the state advances.
void func_actor_104900_80132B10(GpEnemy* enemy, Task* task, ActorShared801384acWork* work)
{
    GpObj* obj;
    s32    reach;
    s32    recOff;
    s32    i;
    s32    idx;

    if (CdCmd_IsIdle() & 0xFFFF) {
        Gp_LinkNode(&enemy->node);
        obj           = &work->field_9A8[0];
        obj->coord    = ((TmdObject*)task->extra)->coords;
        obj->ctx.recs = &work->field_A28[0][0];
        obj->pos.vx   = 0;
        obj->pos.vy   = -0x1D8;
        obj->pos.vz   = 0;
        obj->key      = 0x30000;
        obj->radius   = 0x258;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x4000;
        Gp_InitRec18Table(obj->ctx.recs, 3, 0);

        obj           = &work->field_9A8[3];
        obj->coord    = &((TmdObject*)task->extra)->coords[3];
        obj->ctx.recs = &work->field_A28[3][0];
        obj->pos.vx   = 0;
        obj->pos.vy   = 0;
        obj->pos.vz   = 0;
        obj->key      = 0x3000B;
        obj->radius   = 0x1C2;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x8000;
        Gp_InitRec18Table(obj->ctx.recs, 3, 0);
        obj->pos.vx = 0;
        obj->pos.vy = -0xC8;
        obj->pos.vz = 0xC8;

        i      = 0;
        reach  = 0x12C;
        recOff = 0xA70;
        obj    = &work->field_9A8[1];
        do {
            idx = 8;
            if (i == 0) {
                idx = 0xC;
            }
            obj->coord    = &((TmdObject*)task->extra)->coords[idx];
            obj->ctx.recs = (GpRec18*)((u8*)work + recOff);
            do {
                if (i == 0) {
                    obj->pos.vx = -0x12C;
                } else {
                    obj->pos.vx = reach;
                }
                obj->pos.vy = 0;
                obj->pos.vz = 0;
                obj->radius = reach;
                obj->key    = Gp_PackObjPair((GpObj50*)enemy, 1);
                obj->flags  = 1;
                Gp_LinkObj(3, obj);
                obj->flags &= 0x3FFF;
                Gp_InitRec18Table(obj->ctx.recs, 3, 0);
                recOff += 0x48;
                i++;
                obj = &work->field_9A8[i + 1];
            } while (0);
        } while (i < 2);

        enemy->recs                      = &work->field_A28[3][0];
        task->exitCallback               = ActorsShared801384ac;
        ((TmdObject*)task->extra)->flags = (u16)(((TmdObject*)task->extra)->flags & 0xFF7F);
        task->msgTable                   = &D_actor_104900_80147480;
        task->state++;
        enemy->reactionFlags = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132D78);

/// First of the 0xA pair the dispatcher at 0x80134780 runs while the latch at
/// 0xBA6 is still clear: it re-arms the link transform and decides from the
/// squared distance `ActorsShared801388e8` measures to the model's part-3
/// coordinate whether the actor closes in this frame.
///
/// The walk offset at 0xB8E steps back toward zero - 0x10 off either end of the
/// +-0x10 band, or straight to zero inside it - and the counter at 0xBAA is
/// cleared. `Task::spawnArg1` then picks the threshold: 0 takes 0x5F5E0F
/// outright, 0x20000 takes 0x3D08FF, and anything else 0xF423FF while the
/// player's `GameActor::field_958` reads 3 and 0xF423F otherwise; the 0x20000
/// case also closes in whenever the player flag at `D_801153F2` reads 1
/// without measuring at all. Either way the link transform is re-armed exactly
/// as its siblings arm it - model part 3 through `TmdObject::coords[3]`, the
/// 0xC8-box local offset through `src` - and the state machine at 0x80132D78
/// runs last; its nonzero answer also closes the actor in.
///
/// Closing in while `field_B92` still counts masks the 0xC000 pair back out of
/// the two `GpObj` nodes in the motion block and, the first time only, stages
/// the 0xA state through `field_BA6`: that is what hands the next frame to the
/// 0x80133BB8 body.
///
/// Each arm declares its own player and actor locals: the two arms must reach
/// the compiler as distinct quantities, since one of them is live across the
/// flag byte's address and cannot share the call's result register.
///
/// Same body as the four twins - `func_actor_101100_801339B0` at the same
/// address and `func_actor_201100_8014B9B0` / `func_actor_204900_8014B9B0` /
/// `func_actor_301100_801639B0` 0x18000 past it - but the last call reaches
/// this overlay's own `func_actor_104900_80132D78`, so the body cannot move
/// into `src/actors/lib/`.
void func_actor_104900_801339B0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg)
{
    GpLinkXform* xform;
    s32          flag;
    s32          off;
    s32          i;
    u32          dist;
    s16          walk;

    flag = 0;
    dist = ActorsShared801388e8(((TmdObject*)task->extra)->coords);
    walk = work->field_B8E;
    if (walk >= 0x11) {
        work->field_B8E = (s16)((u16)work->field_B8E - 0x10);
    } else if (walk < -0x10) {
        work->field_B8E = (s16)((u16)work->field_B8E + 0x10);
    } else {
        work->field_B8E = 0;
    }
    work->field_BAA = 0;

    if (task->spawnArg1 == 0) {
        if (dist <= 0x5F5E0F) {
            flag = 1;
        }
    } else if (task->spawnArg1 == 0x20000) {
        Task*      player = (Task*)Game_GetPtrSlot(3);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if (((D_801153F2 ^ 1) == 0) || (((u16)actor->field_958 == 3) && dist <= 0x3D08FF)) {
                flag = 1;
            }
        }
    } else {
        Task*      player = (Task*)Game_GetPtrSlot(3);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if ((((u16)actor->field_958 == 3) && dist <= 0xF423FF) || dist <= 0xF423F) {
                flag = 1;
            }
        }
    }

    xform             = (GpLinkXform*)&enemy->node;
    enemy->node.flags = 0;
    xform->coord      = &((TmdObject*)task->extra)->coords[3];
    xform->src.vx     = 0;
    xform->src.vy     = -0xC8;
    xform->src.vz     = 0xC8;
    if (func_actor_104900_80132D78(enemy, task, work, arg) != 0) {
        flag = 1;
    }
    if (flag && (work->field_B92 > 0)) {
        work->field_BAA = 0;
        i               = 0;
        off             = 0x9C8;
        do {
            ((GpObj*)((u8*)work + off))->flags &= 0x3FFF;
            off                                += 0x20;
            i++;
        } while (i < 2);
        if (work->field_BA6 == 0) {
            work->field_BA6 = 1;
            work->state     = 0xA;
            work->field_BA8 = 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80133BB8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80134780);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135404);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135560);

/// Spin-about handler: on the frame the latch at 0xBA8 is still clear it draws
/// a nibble from `Gp_LcgState` and arms one of the six spin rates - the
/// 0x200 / 0x400 / 0x600 triple, negative on odd draws - into the countdown at
/// 0xB8C, then acts its motion 4. Every later frame turns the model's yaw at
/// 0x46 by 0x10 towards that countdown, rebuilds the Y rotation over the pose
/// and clears `flg`, and when the countdown reaches zero it drops the state at
/// 0xBA7 and the latch, ending the spin about.
void func_actor_104900_801356BC(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    GpCoordPose* pose;
    s32          idx;
    u32          rng;
    u16          angle;

    pose = (GpCoordPose*)((TmdObject*)task->extra)->coords;
    if (work->field_BA8 == 0) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        idx         = (rng >> 0x10) & 0xF;
        if (idx < 3) {
            work->field_B8C = 0x200;
        } else if (idx < 6) {
            work->field_B8C = -0x200;
        } else if (idx < 9) {
            work->field_B8C = 0x400;
        } else if (idx < 0xC) {
            work->field_B8C = -0x400;
        } else if (idx < 0xE) {
            work->field_B8C = 0x600;
        } else {
            work->field_B8C = -0x600;
        }
        work->field_BA4 = 4;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (work->field_B8C > 0) {
        angle          = ((u16)pose->field_46 - 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C - 0x10;
    } else {
        angle          = ((u16)pose->field_46 + 0x10) & 0xFFF;
        pose->field_46 = angle;
        Gfx_RotMatrixY(&pose->coord, angle, 1);
        pose->flg       = 0;
        work->field_B8C = (u16)work->field_B8C + 0x10;
    }
    if (work->field_B8C == 0) {
        work->state     = 0;
        work->field_BA8 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801357F0);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801359CC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80135FDC);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137498);

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
    task->spawnArg2 = eff->field_0;
    Task_Reparent(task, eff->field_0);
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
    func_actor_104900_80137FB8(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137FB8);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", ActorsShared8013845cSub0);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80138374);

INCLUDE_RODATA("actors/nonmatchings/actor_104900/actor_104900", ActorsShared80138404Table);
