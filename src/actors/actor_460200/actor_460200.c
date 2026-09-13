#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"

extern s8 D_8007272D;

extern s32 D_actor_460200_80135F14;
extern s32 D_actor_460200_8013607C;
extern s32 D_actor_460200_80136234;
extern s32 D_actor_460200_80137AA0;
extern s32 D_actor_460200_80137BA8;
extern s32 D_actor_460200_80137CB0;
extern s32 D_actor_460200_80137DA0;
extern s32 D_actor_460200_80137F98;
extern s32 D_actor_460200_80137FE0;
extern s32 D_actor_460200_80138028;
extern s32 D_actor_460200_80138070;

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131E2C);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80131FB0);

void func_actor_460200_80132090(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        Task_Kill(arg0);
        var_v0 = arg0->spawnArg1;
    }
    var_v0          = var_v0 - 1;
    arg0->spawnArg1 = var_v0;
}

void func_actor_460200_801320E0(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132124);

void func_actor_460200_80132204(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_460200_80132210(void)
{
    Task* slot;

    slot = (Task*)Gp_LookupSlot4(0);
    if (slot != NULL) {
        Gp_DispatchMsg(slot, 0x7D4, (s32)&D_actor_460200_80136234, 0);
        Gp_DispatchMsg(slot, 0x7D3, (s32)&D_actor_460200_8013607C, 0);
    }
    if (Gp_LookupSlot4(1) != 0) {
        Gp_MsgSlot4Chain(1, 2);
    }
    slot = (Task*)Gp_LookupSlot4(2);
    if (slot != NULL) {
        Gp_MsgSlot4Chain(2, 1);
        Gp_DispatchMsg(slot, 0x7D3, (s32)&D_actor_460200_80135F14, 0);
    }
}

void func_actor_460200_801322B8(void)
{
    switch (GameFlag_GetNibble(0x114)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137AA0, 0);
            GameFlag_SetNibble(0x114, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137BA8, 0);
            GameFlag_SetNibble(0x114, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80137CB0, 0);
            GameFlag_SetNibble(0x114, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80137DA0, 0);
            break;
    }
}

void func_actor_460200_80132390(void)
{
    switch (GameFlag_GetNibble(0x115)) {
        case 0:
            func_800E8614((s32)&D_actor_460200_80137F98, 0);
            GameFlag_SetNibble(0x115, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_460200_80137FE0, 0);
            GameFlag_SetNibble(0x115, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_460200_80138028, 0);
            GameFlag_SetNibble(0x115, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_460200_80138070, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132468);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_801325FC);

void func_actor_460200_80132808(void* enemy, Task* task);
void func_actor_460200_80132468(void* enemy, Task* task);

void func_actor_460200_801327B4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_460200_80132808, func_actor_460200_80132468 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132808);

void func_actor_460200_80132950(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132978);

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132A04);

void func_actor_460200_80132A50(Task* task)
{
    ActorsShared80132514Work* work;
    s32                       i;

    work = (ActorsShared80132514Work*)task->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132AC8);

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths `func_actor_460200_801325FC`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_460200_80132B2C(Task* task, s32 arg1, Actor460200AnimArgs* args)
{
    Actor460200Work* work;

    work = (Actor460200Work*)task->idMap;
    if (args->animId >= 0x10) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_460200_801325FC(task);
    return 0;
}

/// Script opcode: set the visibility flags of this actor's model and of the
/// model owned by the enemy task it was paired with. `flags` bit 0 hides both
/// models (`TmdObject::field_C` = 0) and its absence restores the default
/// 0x80; bit 1 additionally ORs in 0x4, the same bit `Tmd_Create` sets for its
/// own `flags & 1`. With no enemy paired (`Task::spawnArg1` == 0) the actor
/// drives its own model twice.
s32 func_actor_460200_80132B98(Task* task, s32 arg1, s32 flags)
{
    Actor460200PairedWork* work;
    TmdObject*             self;
    TmdObject*             other;

    self = (TmdObject*)task->extra;
    work = (Actor460200PairedWork*)task->idMap;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F0->extra;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->field_C  = 0;
        other->field_C = 0;
    } else {
        self->field_C  = 0x80;
        other->field_C = 0x80;
    }
    if (flags & 2) {
        self->field_C  |= 4;
        other->field_C |= 4;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_460200/actor_460200", func_actor_460200_80132C14);

INCLUDE_RODATA("actors/nonmatchings/actor_460200/actor_460200", D_actor_460200_80131E20);
