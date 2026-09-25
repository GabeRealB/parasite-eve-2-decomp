#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_water_tank.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block of the water-tank room's script-driver task, a
/// `Mem_Malloc(0x58, 0)` the driver `func_dryfield_water_tank_8017DEA4` hangs
/// off `Task::work` (0x1C). That task is also parked in
/// `D_dryfield_water_tank_80188D4C`, which is how the sibling entry points
/// `func_dryfield_water_tank_8017E194` and `..._8017E1B4` reach this block.
///
/// `owner` is `gameGetPtrSlot(3)`, the task every `Gp_DispatchMsg` in the
/// driver targets; `child` is the task spawned from
/// `D_dryfield_water_tank_8017FF88`, the one messages 0x7D4 / 0x7D5 / 0x7DB are
/// sent to. `field_50` is a request the driver's per-frame switch consumes and
/// clears, and `func_dryfield_water_tank_8017E194` is what sets it.
/// `field_52` has no identified role yet.
///
/// Distinct from `DwtWork`: that one belongs to the cutscene task parked in
/// `D_dryfield_water_tank_80188D50`.
typedef struct DwtScriptWork {
    /* 0x00 */ byte  pad_0[0x40];
    /* 0x40 */ Task* owner;
    /* 0x44 */ Task* child;
    /* 0x48 */ byte  pad_48[0x8];
    /* 0x50 */ u16   field_50;
    /* 0x52 */ s16   field_52;
    /* 0x54 */ byte  pad_54[0x4];
} DwtScriptWork;
STATIC_ASSERT_SIZEOF(DwtScriptWork, 0x58);

/// Light/colour matrix pair `func_dryfield_water_tank_8017DD20` allocates for
/// its `TmdObject` and republishes onto `TmdObject::lightMtx` / `field_20` —
/// the pair `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` /
/// `Gp_DefaultMtx2`. The task parks the block in `Task::work` (0x1C), which is
/// not a `TaskIdMap` here; `owner` is the slot-3 game task the same allocation
/// is registered with (`gameGetPtrSlot(3)`).
///
/// The same block carries the model task's script state: `field_4C` is the
/// state `func_dryfield_water_tank_8017DB98` switches on (0 lowers the model,
/// 1 lets it settle) and `field_4E` the settle counter state 1 advances. The
/// task's message 0x7DB handler, `func_dryfield_water_tank_8017E174`, restarts
/// that script by clearing `field_4C` and `field_54`.
///
/// A different block from `DwtScriptWork`, which the room's script driver
/// allocates at the same 0x58 size.
typedef struct DwtColorMtx {
    /* 0x00 */ MATRIX light; // TmdObject::lightMtx
    /* 0x20 */ MATRIX color; // TmdObject::colorMtx
    /* 0x40 */ Task*  owner; // gameGetPtrSlot(3)
    /* 0x44 */ byte   pad_44[0x8];
    /* 0x4C */ u16    field_4C;
    /* 0x4E */ s16    field_4E;
    /* 0x50 */ byte   pad_50[0x4];
    /* 0x54 */ s16    field_54;
    /* 0x56 */ byte   pad_56[0x2];
} DwtColorMtx;
STATIC_ASSERT_SIZEOF(DwtColorMtx, 0x58);

/// Main-executable flag word with no module header yet: while its bit 2 is
/// raised the model task nudges the model 5 units off each position it snaps to.

extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern GpMsgEntry     D_dryfield_water_tank_8017F324[];
extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern GpMsgEntry     D_dryfield_water_tank_8017FD90[];
extern s32            D_dryfield_water_tank_8017FDC0;
extern s32            D_dryfield_water_tank_8017FEC8;
extern TaskDesc       D_dryfield_water_tank_8017FF88;
extern TaskDesc       D_dryfield_water_tank_80180794;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern TaskDesc       D_dryfield_water_tank_801868A4[];
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];
extern Task*          D_dryfield_water_tank_80188D44;
extern s32            D_dryfield_water_tank_80188D48;
extern Task*          D_dryfield_water_tank_80188D4C;

/// The model's placement run at 0x8017FD60. `[1]` (0x8017FD78) is the lowered
/// record: the model sinks until its Z passes `pos.vz` and is snapped to its
/// `pos.vy` and then `pos.vx`. The model task reaches that record both from
/// this head and by its own symbol, so the head is declared one element long;
/// the driver sends the head itself as the 0x7D4 placement.
extern GpXformArg D_dryfield_water_tank_8017FD60[1];
extern GpXformArg D_dryfield_water_tank_8017FD78;

/// X offsets the model task spawns effect 0x60054 with, indexed by the 0..10
/// `killCountdown` counter it wraps.
extern u16 D_dryfield_water_tank_8017FDA8[];

void func_dryfield_water_tank_8017DB48(void);

void func_dryfield_water_tank_8017D618(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x55) == 3) {
                Gp_StartCapSlot(0xE, 1, 1);
                break;
            }
            gGameSession->eventState       = 1;
            D_dryfield_water_tank_80188D48 = Mc_SaveData.at4.loc.view;
            Gp_MsgPlayer3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(0xE, 0, 0);
            arg0->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 2;
                task->state        = task->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                Gp_StateF0.field_4 = 0;
                GameFlag_SetNibble(0x55, 3);
                SndEvt_EnqueueType6(0x52150004, 0, 0);
                Task_SpawnFromTable(&D_dryfield_water_tank_8017FF88, 0, 0, 0);
                func_dryfield_water_tank_8017DB48();
            } else {
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                Gp_StateF0.field_4       = 0;
                Mc_SaveData.at4.loc.view = (u8)D_dryfield_water_tank_80188D48;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Handler for message 0x13F1 in the room task's message table: accepts the
/// message and does nothing, answering 0.
s32 func_dryfield_water_tank_8017D7BC(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room task's message table: copies the
/// location record the sender passes onto the reply record and answers 1.
s32 func_dryfield_water_tank_8017D7C4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x36) == 0) {
            GameFlag_SetNibble(0x36, 1);
            Task_SpawnFromTable(&D_dryfield_water_tank_8017F34C, 0, 0, 0);
            GameFlag_SetNibble(0x56, 1);
        }
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0x33) == 0)) {
        GameFlag_SetNibble(0x33, 1);
        func_800E3FAC(0xA2, 0xE);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        Gp_ApplyAreaRecs(D_dryfield_water_tank_80188D1C);
        Gp_MsgPlayerWeapon(0);
        func_800E8634((s32)&D_dryfield_water_tank_80184E0C, 0, (s32)&D_dryfield_water_tank_801859DC);
    }
    if (arg2->field_2 == 3) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F114, 0);
    }
    if (arg2->field_2 == 4) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F21C, 0);
    }
    return 1;
}

/// Room message handler: on message `0xE` spawn the second entry of
/// `D_dryfield_water_tank_8017F34C`, the same table `func_dryfield_water_tank_8017D7EC`
/// takes entry 0 from.
s32 func_dryfield_water_tank_8017D910(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Task_SpawnFromTable(&D_dryfield_water_tank_8017F34C, 1, 0, 0);
    }
    return 0;
}

/// Room event task: state 0 spawns the child from `D_dryfield_water_tank_80180794`
/// and parks it in `D_dryfield_water_tank_80188D44`, state 1 kills this task once
/// that child has been killed.
void func_dryfield_water_tank_8017D948(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_water_tank_80188D44 = Task_SpawnFromTable(&D_dryfield_water_tank_80180794, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_water_tank_80188D44, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// State 0 of the room's event task: publish the message table the room's
/// handlers hang off (`0x13EE`–`0x13F1`), take pointer slot 7, spawn the
/// cutscene task from `D_dryfield_water_tank_801868A4`, queue sound event
/// `0x52150009`, run the game-flag `0x55` dispatch in
/// `func_dryfield_water_tank_8017DB48`, then advance.
void func_dryfield_water_tank_8017D9D4(Task* task)
{
    task->msgTable = D_dryfield_water_tank_8017F324;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_dryfield_water_tank_801868A4, 0, 0, 0);
    SndEvt_EnqueueType6(0x52150009, 0, 0);
    func_dryfield_water_tank_8017DB48();
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room task, run every frame: while `gGameSession->viewReady`
/// is set, queue the water-tank ambience sound events `0x52150011` and
/// `0x52150012`, each as a type-6 event in the view it belongs to (4 and 0xA
/// respectively) and as a type-7 event (argument 0x2D / 0x3C) from any other
/// view.
void func_dryfield_water_tank_8017DA4C(Task* task)
{
    if (gGameSession->viewReady != 0) {
        if (gGameSession->at4.loc.view == 4) {
            Gp_EnqueueStageSnd6(0x52150011, 0, 0);
        } else {
            Gp_EnqueueStageSnd7(0x52150011, 0x2D);
        }
        if (gGameSession->at4.loc.view == 0xA) {
            Gp_EnqueueStageSnd6(0x52150012, 0, 0);
            return;
        }
        Gp_EnqueueStageSnd7(0x52150012, 0x3C);
    }
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_water_tank_8017DAF0`: the entry tick, the per-frame
/// ambience, then `taskKill`.
const TaskFuncTable3 D_dryfield_water_tank_8017D5C4 = {
    { func_dryfield_water_tank_8017D9D4, func_dryfield_water_tank_8017DA4C, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_water_tank_8017DAF0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}

void func_dryfield_water_tank_8017DB48(void)
{
    switch (GameFlag_GetNibble(0x55)) {
        case 0:
        case 1:
        case 2:
            func_dryfield_water_tank_8017EFF4(1);
            break;
        case 3:
            func_dryfield_water_tank_8017EFF4(0);
            break;
    }
}

/// The model task's script, run each frame while the task is in state 2;
/// returning 1 tells the caller the model has arrived.
///
/// Script state 0 lowers the model: its Z grows by 0x14 a frame, its Y snaps to
/// the lowered record's `pos.vy` (nudged by the `gDisplayState.gameTick` flag), and once the
/// Z has passed that record's `pos.vz` the script steps to state 1. Every frame
/// of state 0 also spawns effect 0x60054 at the model, offset in X by the next
/// entry of the wrapping `killCountdown` table. State 1 advances the settle
/// counter; on its 0x3D-th tick it publishes the lowered record to the task
/// itself as the 0x7D4 placement and returns 1, and until then snaps the
/// model's X to that record's `pos.vx`. Every path that returns 0 clears
/// `coord->flg`, so the coordinate is recomputed on the next update.
///
/// The body is the water tower's `func_dryfield_water_tower_8017E428`; as there,
/// the Z test is written with the coordinate on the left, which is what loads it
/// before the record.
s32 func_dryfield_water_tank_8017DB98(Task* arg0)
{
    DwtColorMtx* work  = (DwtColorMtx*)arg0->work;
    GpCoord*     coord = arg0->extra.tmd->coords;
    GpCoord*     effCoord;
    SVECTOR      pos;

    switch (work->field_4C) {
        case 0:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = D_dryfield_water_tank_8017FD60[1].pos.vy;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > D_dryfield_water_tank_8017FD60[1].pos.vz) {
                work->field_4C++;
            }
            effCoord = arg0->extra.tmd->coords;
            if (arg0->killCountdown >= 0xA) {
                arg0->killCountdown = 0;
            } else {
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
            }
            pos.vy = 0;
            pos.vz = 0;
            pos.vx = D_dryfield_water_tank_8017FDA8[arg0->killCountdown];
            Gp_SpawnEff(0x60054, effCoord, 0x80002300, &pos);
            break;

        case 1:
            work->field_4E++;
            if ((s16)work->field_4E >= 0x3D) {
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_dryfield_water_tank_8017FD78, 0);
                return 1;
            }
            coord->coord.t[0] = D_dryfield_water_tank_8017FD78.pos.vx;
            if (gDisplayState.gameTick & 4) {
                coord->coord.t[0] += 5;
            }
            break;
    }
    coord->flg = 0;
    return 0;
}

/// Drives the model task the room's script spawns: state 0 allocates the
/// light/colour matrix pair for the task's `TmdObject` and reparents the task
/// to the script driver, state 1 idles, and state 2 waits for
/// `func_dryfield_water_tank_8017DB98` to report the model finished. Every
/// frame it hands the model part's translation to `func_800D7A9C`, which turns
/// it into the light/colour matrices.
void func_dryfield_water_tank_8017DD20(Task* arg0)
{
    TmdObject*   extra;
    GpCoord*     coord;
    DwtColorMtx* mtx;
    TmdObject*   mdl;
    VECTOR       pos;

    switch (arg0->state) {
        case 0:
            extra      = arg0->extra.tmd;
            coord      = extra->coords;
            mtx        = (DwtColorMtx*)Mem_Malloc(0x58, 0);
            arg0->work = (TaskIdMap*)mtx;
            if (mtx == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(mtx, 0, 0x58);
                mtx->owner   = gameGetPtrSlot(3);
                coord->sub   = &gGfxViewCoord;
                extra->flags = 0x80;
                Tmd_AllocBuffers(extra);
                extra->lightMtx = &mtx->light;
                extra->colorMtx = &mtx->color;
                arg0->msgTable  = D_dryfield_water_tank_8017FD90;
                Task_Reparent(D_dryfield_water_tank_80188D4C, arg0);
            }
            arg0->state += 1;
            break;
        case 1:
            break;
        case 2:
            if (func_dryfield_water_tank_8017DB98(arg0) & 0xFFFF) {
                arg0->state = 1;
            }
            break;
    }

    mdl    = arg0->extra.tmd;
    pos.vx = arg0->extra.tmd->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Per-frame script driver for the water-tank scene. It is the task parked in
/// `D_dryfield_water_tank_80188D4C`, which is how the room's two sibling entry
/// points reach the 0x58-byte `DwtScriptWork` it hangs off `Task::work`.
/// State 0 allocates that block, registers it with the slot-3 game task and
/// spawns the model task from `D_dryfield_water_tank_8017FF88` as its `child`;
/// state 1 sends the intro messages to both tasks; state 2 asks to be killed
/// once the session is gone. Every frame it then runs at most one request off
/// `field_50` and clears it: 1 rewinds the scene through owner 0x3F3 and child
/// 0x7D5 and hands 0x7DB the payload that moves the receiver to script state 2,
/// 2 publishes the view switch (the body `func_dryfield_water_tank_8017E1B4`
/// runs on its own) and 3 fires the scene's sound events.
void func_dryfield_water_tank_8017DEA4(Task* arg0)
{
    DwtScriptWork* work;
    GpCmdArg       msg;
    Task**         owner;

    work = (DwtScriptWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            work       = (DwtScriptWork*)Mem_Malloc(0x58, 0);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x58);
                work->owner                    = gameGetPtrSlot(3);
                D_dryfield_water_tank_80188D4C = arg0;
            }
            work        = (DwtScriptWork*)arg0->work;
            work->child = Task_SpawnFromTable(&D_dryfield_water_tank_8017FF88, 1, 0, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            Gp_DispatchMsg(work->child, 0x7D4, (s32)&D_dryfield_water_tank_8017FD60, 0);
            func_800E8634((s32)&D_dryfield_water_tank_8017FDC0, 0, (s32)&D_dryfield_water_tank_8017FEC8);
            arg0->state = arg0->state + 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
            }
            break;
    }

    work = (DwtScriptWork*)arg0->work;
    switch (work->field_50) {
        /* This arm does nothing, and the switch needs it as written: it is what
         * puts four values in the case list, so the decision tree roots at the
         * request-1 node the way the ROM's does. */
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->owner, 0x3F3, 0, 0);
            Gp_DispatchMsg(work->child, 0x7D5, 1, 0);
            msg.command = 2;
            Gp_DispatchMsg(work->child, 0x7DB, (s32)&msg, 0);
            break;
        case 2:
            Mc_SaveData.at4.loc.view = Gp_FindViewIndex(3);
            gGameSession->viewDirty  = 1;
            /* Through a pointer rather than as `work->owner`: a member load is
             * struct memory, which lets the store to the view index sink into
             * the call's delay slot; the two request tails then no longer
             * cross-jump as the original's do. */
            owner = &work->owner;
            Gp_DispatchMsg(*owner, 0x3F3, 1, 0);
            break;
        case 3:
            SndEvt_EnqueueType6(0x52150002, 0, 0);
            SndEvt_EnqueueType6(0x52150008, 0, 0);
            break;
    }
    work->field_50 = 0;
}

/// Hides the task's `TmdObject` (bit 0x80 of `field_C`) while `arg2` is zero,
/// and clears that bit otherwise. `arg1` is unused; the flag is the *third*
/// argument, so the second slot is only there to place it in `$a2`. Byte for
/// byte the actors library's `ActorsShared801346ec`, which toggles the same bit
/// of the same field for the model of the task it is handed.
void func_dryfield_water_tank_8017E0B4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}

/// Message 0x7D4 handler of the model task: copies `placement` onto the task's
/// `TmdObject` coordinate frame. The three longs become the translation, then
/// yaw / pitch / roll are applied with `Gfx_RotMatrixY` / `X` / `Z` and the
/// coordinate is marked dirty.
void func_dryfield_water_tank_8017E0E8(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    MATRIX*  mtx;

    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Message 0x7DB handler of the model task: restarts its script, clearing the
/// script state and `field_54` in its work block and moving the task to the
/// state the payload carries.
void func_dryfield_water_tank_8017E174(Task* task, s32 msgId, GpCmdArg* msg)
{
    DwtColorMtx* work;
    s32          state;

    work                = (DwtColorMtx*)task->work;
    work->field_4C      = 0;
    work->field_54      = 0;
    state               = msg->command;
    task->killCountdown = 0;
    task->state         = state;
}

/// Sibling entry point into the script driver: reaches the driver's work block
/// through the task parked in `D_dryfield_water_tank_80188D4C`, raises the
/// request halfword `field_50` — the value the driver's per-frame switch reads,
/// branches on and clears — and clears `field_52` beside it.
void func_dryfield_water_tank_8017E194(s16 request)
{
    DwtScriptWork* work;

    work           = (DwtScriptWork*)D_dryfield_water_tank_80188D4C->work;
    work->field_50 = request;
    work->field_52 = 0;
}

/// Second sibling entry point into the script driver, the one that ends the
/// water-tank scene: publishes view 3's area-record index, asks the view gate
/// for a switch through `GameSession.viewDirty`, dispatches message 0x3F3 with
/// argument 1 to the driver's `owner` task, and fires the scene's sound event.
void func_dryfield_water_tank_8017E1B4(void)
{
    DwtScriptWork* work;
    Task**         owner;

    work                     = (DwtScriptWork*)D_dryfield_water_tank_80188D4C->work;
    Mc_SaveData.at4.loc.view = Gp_FindViewIndex(3);
    /* Through a pointer rather than as `work->owner`: a member load is struct
     * memory, which lets the store to the view index sink into the call's
     * delay slot, and the original keeps it ahead of the load. */
    owner = &work->owner;
    Gp_DispatchMsg(*owner, 0x3F3, 1, 0);
    gGameSession->viewDirty = 1;
    SndEvt_EnqueueType7(0x52150002, 0xA);
}

/// Fade the water tank up from white, then tear the task down.
///
/// State 0 allocates the ramp at `Task::work` and saturates all three channels
/// at 0xFF; a failed allocation kills the task outright. State 1 runs every
/// frame: it links a semi-transparent full-screen `TILE` (`-0xA0,-0x78`,
/// `0x140x0xF0`) plus the `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`,
/// tinting the tile `r`/`g`/`r`, then steps all three channels down by
/// `Task::spawnArg1`. Once `r` drops below 0 the screen is clear, so the task
/// kills itself. The fade-out half is `func_dryfield_water_tank_8017E3C4`.
void func_dryfield_water_tank_8017E220(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;
    u8               r;
    u8               g;
    TILE*            tile;
    DR_TPAGE*        dr;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}
