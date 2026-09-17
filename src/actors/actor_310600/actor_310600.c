#include "common.h"

#include "actors/actor_310600.h"
#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E24;

void func_actor_310600_80161E64(Task* task)
{
    Actor310600Work* work;
    GpObj*           obj;

    work = Mem_Calloc(0x538, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_477 = -1;
    work->field_47C = 0;
    work->field_47E = 0;
    work->field_518 = 0;
    work->field_51C = 0;
    work->field_520 = 0;
    Task_SpawnFromTable(D_actor_310600_801796A4, 1, 8, (s32)task);
    func_actor_310600_80162A58(task);
    obj           = &work->obj;
    obj->field_8  = &((TmdObject*)task->extra)->field_8[1];
    obj->field_C  = &work->rec;
    obj->field_18 = 0x30000;
    obj->field_1C = 0x100;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->field_C, 1, 0);
    task->field_24 = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, 0x7D5, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_80161FA0);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_8016231C);

void         func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
extern void* D_actor_310600_80179640[]; // animation bank table `work->field_476` indexes
extern s8    D_actor_310600_80179644[]; // extra ticks owed to the animation id in `work->field_475`

/// Animation preset handler of message 0x7D3, the twenty-slot twin of
/// `func_actor_335800_801632A4`: re-seeds the slot array off bank table
/// `D_actor_310600_80179640` when the preset's bank index changes -- clearing
/// the latched id to -1 so the state below is re-applied -- then restarts or
/// resets every slot and ticks them, repeating the tick pass `1 +
/// D_actor_310600_80179644[state]` times.
///
/// The two byte stores must stay in this order. The second one is a QImode
/// store to a varying address, so cse treats it as aliasing everything and
/// drops the equivalence the first one recorded; that is what keeps
/// `work->field_476` a reload instead of the register `cmd->animId` arrived in.
s32 func_actor_310600_8016246C(Task* task, s32 arg1, Actor310600Cmd* cmd, s32 arg3)
{
    Actor310600Work* work;
    TmdObject*       ext;
    s32              i;
    s32              j;

    work = (Actor310600Work*)task->idMap;
    ext  = task->extra;
    if (cmd->animId != work->field_476) {
        work->field_476 = cmd->animId;
        work->field_475 = -1;
        func_800B3F84(&work->anim, D_actor_310600_80179640[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    if (cmd->state != work->field_475) {
        work->field_475 = cmd->state;
        if (cmd->path != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->anim, i, work->field_475, 0, cmd->param);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_475);
            }
        }
        for (j = 0; j <= D_actor_310600_80179644[work->field_475]; j++) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->anim, i);
            }
        }
        work->field_474 = 1;
        work->field_478 = 0;
        work->field_47A = 0;
    }
    return 0;
}

/// Mode handler for the actor's display object, called by the setup path
/// (`func_actor_310600_80161E64` with message 0x7D5 and mode 0) with the mode in
/// `arg2`. Modes 0 and 2 hide the model: bit 0x80 of `TmdObject.field_C` goes on,
/// the 0x8000 flag comes off the actor's own object, and 0x4 is cleared. Modes 1
/// and 3 show it: 0x80 comes off, 0x8000 goes on, the buffers are reinstated
/// through `Tmd_AllocBuffers`, and 0x4 is set. Mode 2 additionally latches
/// `field_477` to 2. Returns 1 for a mode outside 0..3.
///
/// `work` and `w` are the same block on purpose. cse turns the second load of
/// `task->idMap` into a copy of the first and keeps the copy's register for the
/// mode 0..2 walks, because the only later use of the first load's register is
/// the `obj` assignment in the entry block -- so mode 3's walk reads the first
/// load's register and the other three read the copy's, the split the target
/// has. Writing `&work->obj` inside case 3 instead leaves cse canonicalizing the
/// walks the other way, and the overlay comes out three instructions short.
s32 func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    Actor310600Work* work;
    Actor310600Work* w;
    TmdObject*       ext;
    GpObj*           p;
    GpObj*           obj;
    s32              i;
    s32              ret;

    work = (Actor310600Work*)task->idMap;
    ext  = task->extra;
    w    = (Actor310600Work*)task->idMap;
    obj  = &work->obj;
    ret  = 0;
    switch (arg2) {
        case 0:
            ext->field_C |= 0x80;
            p             = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            ext->field_C &= ~4;
            break;
        case 1:
            ext->field_C &= ~0x80;
            p             = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            Tmd_AllocBuffers(ext);
            ext->field_C &= ~4;
            break;
        case 2:
            ext->field_C |= 0x80;
            p             = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            w->field_477  = 2;
            ext->field_C |= 4;
            break;
        case 3:
            ext->field_C &= ~0x80;
            p             = obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            ext->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E24);

void func_actor_310600_8016274C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E24;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600", func_actor_310600_801627A4);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E48);

INCLUDE_RODATA("actors/nonmatchings/actor_310600/actor_310600", D_actor_310600_80161E54);
