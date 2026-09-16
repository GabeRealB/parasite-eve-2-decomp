#include "common.h"

#include "actors/actor_104900.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801511c8.h"
#include "main/gfx.h"
#include "main/mem.h"

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

void func_actor_104900_80137FB8(Task* task);

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
/// exit callback, the model's hidden bit is lifted, `field_24` is pointed at
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
        obj->field_8  = ((TmdObject*)task->extra)->field_8;
        obj->field_C  = &work->field_A28[0][0];
        obj->field_10 = 0;
        obj->field_12 = -0x1D8;
        obj->field_14 = 0;
        obj->field_18 = 0x30000;
        obj->field_1C = 0x258;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x4000;
        Gp_InitRec18Table(obj->field_C, 3, 0);

        obj           = &work->field_9A8[3];
        obj->field_8  = &((TmdObject*)task->extra)->field_8[3];
        obj->field_C  = &work->field_A28[3][0];
        obj->field_10 = 0;
        obj->field_12 = 0;
        obj->field_14 = 0;
        obj->field_18 = 0x3000B;
        obj->field_1C = 0x1C2;
        obj->flags    = 1;
        Gp_LinkObj(2, obj);
        obj->flags |= 0x8000;
        Gp_InitRec18Table(obj->field_C, 3, 0);
        obj->field_10 = 0;
        obj->field_12 = -0xC8;
        obj->field_14 = 0xC8;

        i      = 0;
        reach  = 0x12C;
        recOff = 0xA70;
        obj    = &work->field_9A8[1];
        do {
            idx = 8;
            if (i == 0) {
                idx = 0xC;
            }
            obj->field_8 = &((TmdObject*)task->extra)->field_8[idx];
            obj->field_C = (GpRec18*)((u8*)work + recOff);
            do {
                if (i == 0) {
                    obj->field_10 = -0x12C;
                } else {
                    obj->field_10 = reach;
                }
                obj->field_12 = 0;
                obj->field_14 = 0;
                obj->field_1C = reach;
                obj->field_18 = Gp_PackObjPair((GpObj50*)enemy, 1);
                obj->flags    = 1;
                Gp_LinkObj(3, obj);
                obj->flags &= 0x3FFF;
                Gp_InitRec18Table(obj->field_C, 3, 0);
                recOff += 0x48;
                i++;
                obj = &work->field_9A8[i + 1];
            } while (0);
        } while (i < 2);

        enemy->field_54                    = (s32)&work->field_A28[3][0];
        task->exitCallback                 = ActorsShared801384ac;
        ((TmdObject*)task->extra)->field_C = (u16)(((TmdObject*)task->extra)->field_C & 0xFF7F);
        task->field_24                     = &D_actor_104900_80147480;
        task->state++;
        enemy->field_4C = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80132D78);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_801339B0);

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

    pose = (GpCoordPose*)((TmdObject*)task->extra)->field_8;
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

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900", func_actor_104900_80137B1C);

/// Spawns the effect this actor's next state rides on and re-homes the actor.
///
/// The 0x58-byte work block goes in `Task::idMap` and the effect task comes back
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
/// 0x28 record, which takes 0x96s for `field_10` / `field_12` and points
/// `field_14` at the one-entry collision table `Gp_InitRec18Table` zeroes, and
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

    coord = ((TmdObject*)task->extra)->field_8;
    work  = Mem_Calloc(0x58, 0);
    if (work == NULL) {
        Task_CallExit(task);
        return;
    }
    task->idMap = (TaskIdMap*)work;
    eff         = Gp_SpawnEff(0x60081, coord, 0, 0);
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

    obj           = &work->obj;
    rec           = &work->rec;
    obj->field_8  = coord;
    obj->field_C  = (GpRec18*)rec;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->field_1C = 0;
    obj->field_18 = Gp_PackPair(&D_actor_104900_801392F0[0], 5);
    obj->flags    = 3;

    rec->field_14 = work->rec18;
    rec->field_8  = 0;
    rec->field_A  = 0;
    rec->field_C  = 0;
    rec->field_0  = 0;
    rec->field_2  = 0;
    rec->field_4  = 0;
    rec->field_10 = 0x96;
    rec->field_12 = 0x96;
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
