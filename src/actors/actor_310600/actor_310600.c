#include "common.h"

#include "actors/actor_310600.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E24;

/// Per-animation cue lists: `D_actor_310600_80179660[field_475]` is a
/// zero-terminated list of the frames at which that animation fires its effect.
extern s16*    D_actor_310600_80179660[];
extern SVECTOR D_actor_310600_80179694;
extern s32     D_actor_310600_8017969C;
extern s32     D_actor_310600_801796A0;

/// Spawn table of the follow-up task queued once the cue has fired five times.
extern TaskDesc D_80182AD8[];

void func_actor_310600_80161E64(Task* task)
{
    Actor310600Work* work;
    GpObj*           obj;

    work = Mem_Calloc(0x538, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
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
    obj->field_8  = &((TmdObject*)task->extra)->coords[1];
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
    task->msgTable = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, 0x7D5, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
    task->state++;
}

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The actor's per-frame handler. Runs the entry of its second state table that
/// `field_47C` selects, then advances the root part by `step`: each axis'
/// accumulator carries a 16.16 offset whose whole part is added to the world
/// translation and whose fraction is kept, and clearing `flg` makes
/// `Gp_UpdateCoordTree` rebuild the world matrix from it.
///
/// Once the slots have been started (`field_474`) every animation slot is
/// ticked, and the frame counter `field_478` is walked against the cue list
/// `D_actor_310600_80179660[field_475]` -- a zero-terminated list of frames at
/// which the animation currently playing fires an effect. The effect is chosen
/// by the animation id: ids 1 and 2 spawn 0x6006A and ask slot 4 for the
/// follow-up message, but only for the first five of them, after which the
/// other payload is sent and `D_80182AD8` is spawned instead; id 3 spawns
/// 0x6006D. The remaining ids have no cue.
///
/// While the model is visible its ground shadow is drawn at the root part's
/// world position and the occupancy table is cleared, and while the session
/// flag at `field_4D` is set the second part is re-derived and re-lit.
/// `field_477` is the teardown countdown: it frees the model buffers on the
/// tick it reaches zero and then stops at -1.
void func_actor_310600_80161FA0(Task* task)
{
    TmdObject*       ext               = task->extra;
    Actor310600Work* work              = (Actor310600Work*)task->work;
    void             (*funcs[2])(void) = { func_actor_310600_80162A74, (void (*)(void))func_actor_310600_80162A7C };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s16*             cues;
    s16*             cue;
    s32              i;

    funcs[work->field_47C]();
    coord              = ((TmdObject*)task->extra)->coords;
    work->field_518   += work->step.vx;
    work->field_51C   += work->step.vy;
    work->field_520   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_518 >> 16);
    coord->coord.t[1] += (s16)(work->field_51C >> 16);
    coord->coord.t[2] += (s16)(work->field_520 >> 16);
    coord->flg         = 0;
    work->field_518    = (u16)work->field_518;
    work->field_51C    = (u16)work->field_51C;
    work->field_520    = (u16)work->field_520;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (work->field_475 > 0) {
        cues = D_actor_310600_80179660[work->field_475];
        if (cues != NULL) {
            if (*cues != 0) {
                cue = cues;
                do {
                    if (*cue == work->field_478) {
                        coord = &((TmdObject*)task->extra)->coords[8];
                        switch (work->field_475) {
                            case 1:
                            case 2:
                                if ((s16)work->field_47A++ < 5) {
                                    Gp_SpawnEff(0x6006A, coord, 9, NULL);
                                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_actor_310600_8017969C, 0);
                                } else {
                                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_actor_310600_801796A0, 0);
                                    Task_SpawnFromTable(D_80182AD8, 2, 0, 0);
                                }
                                break;
                            case 3:
                                Gp_SpawnEff(0x6006D, coord, 6, &D_actor_310600_80179694);
                                break;
                        }
                        break;
                    }
                    cue++;
                } while (*cue != 0);
            }
            work->field_478++;
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
        Gp_ClearRec18Occupied(&work->rec);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_477--;
    }
}

/// Arrival handler of the actor's second state table (`field_47E`), reached once
/// `func_actor_310600_80162B98` has laid down the per-frame `step` offset: takes
/// each horizontal axis' gap between the target point `field_4F8` / `field_500`
/// and the root part's world translation -- the low 16 bits of the signed
/// difference, as in `func_actor_310600_80162AD8` -- and compares it against the
/// axis' stop threshold in `limit`, which starts at 0x7FFF. Both gaps past their
/// threshold means the actor has stopped closing in: the arrival preset of
/// message 0x7D3 is queued (animation bank 0, id 0xD, path 1, param 0xA), but
/// only while `field_475` still holds 0xC, and then `step` and the two counters
/// are cleared and the handler returns without re-arming. Otherwise each
/// threshold is pulled down to the gap just measured, so the next tick that
/// fails to shrink it is the one that fires.
void func_actor_310600_8016231C(Task* arg0)
{
    Actor310600Work*  work;
    Actor310600Coord* coord;
    SVECTOR           d;
    s32               dx;
    s32               dz;
    Actor310600Cmd    cmd;

    work  = (Actor310600Work*)arg0->work;
    coord = (Actor310600Coord*)((TmdObject*)arg0->extra)->coords;
    if (work->field_4F8 - coord->coord.t[0] >= 0) {
        dx = (u16)work->field_4F8 - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->field_4F8;
    }
    d.vx = dx;
    if (work->field_500 - coord->coord.t[2] >= 0) {
        dz = (u16)work->field_500 - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->field_500;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        if (work->field_475 == 0xC) {
            cmd.animId = 0;
            cmd.state  = 0xD;
            cmd.path   = 1;
            cmd.param  = 0xA;
            cmd.unk10  = 0;
            func_actor_310600_8016246C(arg0, 0x7D3, &cmd, 0);
        }
        work->step.vx   = 0;
        work->step.vy   = 0;
        work->step.vz   = 0;
        work->field_47C = 0;
        work->field_47E = 0;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

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

    work = (Actor310600Work*)task->work;
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
/// `arg2`. Modes 0 and 2 hide the model: bit 0x80 of `TmdObject.flags` goes on,
/// the 0x8000 flag comes off the actor's own object, and 0x4 is cleared. Modes 1
/// and 3 show it: 0x80 comes off, 0x8000 goes on, the buffers are reinstated
/// through `Tmd_AllocBuffers`, and 0x4 is set. Mode 2 additionally latches
/// `field_477` to 2. Returns 1 for a mode outside 0..3.
///
/// `work` and `w` are the same block on purpose. cse turns the second load of
/// `task->work` into a copy of the first and keeps the copy's register for the
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

    work = (Actor310600Work*)task->work;
    ext  = task->extra;
    w    = (Actor310600Work*)task->work;
    obj  = &work->obj;
    ret  = 0;
    switch (arg2) {
        case 0:
            ext->flags |= 0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            ext->flags &= ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags |= 0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            w->field_477 = 2;
            ext->flags  |= 4;
            break;
        case 3:
            ext->flags &= ~0x80;
            p           = obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            ext->flags |= 4;
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
