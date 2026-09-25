#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block for the `actor_303600` overlay's cutscene controller.
///
/// `func_actor_303600_8016216C` allocates it with `Mem_Malloc(0x10, 0)`, zeroes
/// it with `Mem_Set` and parks the pointer in the task's `Task::work` slot
/// (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor303600Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_303600_8016E4C0` and stores the `gameGetPtrSlot(3)` task
/// in `field_0`.
///
/// `command` is the request the overlay's state machine dispatches on:
/// `func_actor_303600_80161F40` switches on it through
/// `jtbl_actor_303600_80161E24` (values 0..8) and clears it again on the way
/// out.  `field_C` records the message the dispatcher last sent and `field_E`
/// is the "a message is outstanding" flag that
/// `func_actor_303600_801624B0` / `func_actor_303600_8016253C` test before
/// sending another.
typedef struct Actor303600Work {
    /* 0x0 */ Task* field_0; // gameGetPtrSlot(3) task
    /* 0x4 */ u16   command; // state-machine request, see jtbl_actor_303600_80161E24
    /* 0x6 */ s16   field_6; // cleared alongside command
    /* 0x8 */ byte  pad_8[0x4];
    /* 0xC */ s16   field_C; // message id last dispatched
    /* 0xE */ u16   field_E; // set to 1 while a dispatched message is outstanding
} Actor303600Work;
STATIC_ASSERT_SIZEOF(Actor303600Work, 0x10);

/// Light / colour matrix pair the overlay's actor hands to its model: the pair
/// `func_actor_303600_80162950` allocates with `memCalloc(0x44, 0)` and parks
/// in its own task's `Task::work` slot (0x1C, again not a `TaskIdMap`), so
/// reach it with `(Actor303600LightMats*)task->work`.  The four bytes after
/// the two matrices are part of the allocation and are never read here.
typedef struct Actor303600LightMats {
    /* 0x00 */ MATRIX lightMtx;
    /* 0x20 */ MATRIX colorMtx;
    /* 0x40 */ byte   pad_40[0x4];
} Actor303600LightMats;
STATIC_ASSERT_SIZEOF(Actor303600LightMats, 0x44);

/// Work block of the task `func_actor_303600_80162A7C` dispatches through
/// `D_actor_303600_80161E48`: `func_actor_303600_801626C0` allocates it with
/// `memCalloc(0x3C, 0)`, parks it in `Task::work` (0x1C, again not a
/// `TaskIdMap`), fills `children` with the five model tasks it spawns -- one
/// `Task_SpawnFromTable` of `D_actor_303600_8016E468` entry 1 each, spread
/// 8000 units apart in y and spliced under this task's own coordinate, so
/// `children[i]` owns the light matrices -- and installs the 0x7DB handler
/// table in `Task::msgTable`.  `func_actor_303600_801627B8` then moves the rig
/// each frame: `field_28` (a 16.16 speed) ramps toward `field_38` at `field_34`
/// a frame and stops once it passes it, and `field_18` accumulates `field_28`
/// and is folded back into +/-4000 before its integer half becomes the task
/// coordinate's `t[1]` (the `lh` from 0x1A).  The words this block does not yet
/// name are the same shape, so `field_28`/`field_34`/`field_38` are the three
/// the 0x7DB handler below arms.
typedef struct Actor303600RigWork {
    /* 0x00 */ Task*     children[5];
    /* 0x14 */ s32       field_14;
    /* 0x18 */ GpFixed16 field_18;
    /* 0x1C */ s32       field_1C;
    /* 0x20 */ s32       field_20;
    /* 0x24 */ s32       field_24;
    /* 0x28 */ s32       field_28;
    /* 0x2C */ s32       field_2C;
    /* 0x30 */ s32       field_30;
    /* 0x34 */ s32       field_34;
    /* 0x38 */ s32       field_38;
} Actor303600RigWork;
STATIC_ASSERT_SIZEOF(Actor303600RigWork, 0x3C);

extern Task*      D_actor_303600_8016E4C0;
extern Task*      D_actor_303600_8016E4C4;
extern TaskDesc   D_actor_303600_80162E98;
extern TaskDesc   D_actor_303600_8016E468[];
extern GpMsgEntry D_actor_303600_8016E480[];

/// The overlay's three flat lights, loaded into the model by
/// `func_actor_303600_80162A0C`; one `GsF_LIGHT` (0x10 bytes) each.
extern GsF_LIGHT D_actor_303600_8016E490[3];

/// The cutscene's two script blocks, handed to `func_800E8634` together when the
/// controller below arms the cutscene.
extern u8 D_actor_303600_80162AF0[];
extern u8 D_actor_303600_80162DD8[];

/// Main-executable globals with no module header yet: a `D_80114C12` of 1 or a
/// live `D_80071075` both mean a cutscene is already up, and `D_80071076` is the
/// latch state 2 below sets alongside `Mc_SaveData`.
extern s8 D_80114C12;

void func_actor_303600_80162850(Task* task);
void func_actor_303600_80162950(Task* task);
void func_actor_303600_80162A04(Task* task);
void func_actor_303600_80162A0C(Task* task);

/// Entry 3 of `D_actor_303600_80162E98`, spawned by the teardown and by
/// command 8: every frame it covers the screen with an opaque black tile,
/// linked 15 slots below `gGpuCurrentOt`, followed by a draw-mode packet with
/// dithering on. It never ends itself.
void func_actor_303600_80161E60(Task* task)
{
    TILE*     p;
    DR_TPAGE* dr;

    p              = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x60);
    p->r0 = 0;
    p->g0 = 0;
    p->b0 = 0;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0xF, p);

    dr             = (DR_TPAGE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0);
    addPrim(gGpuCurrentOt - 0xF, dr);
}

/// Command dispatcher the cutscene controller steps while the cutscene is up.
/// Commands 1-5 send the slot-4 task message 0x7DA carrying the session's two id
/// bytes and the command as selector, latching it in the published work block's
/// `field_C`; 4 then kills the fade in `D_actor_303600_8016E4C4` and spawns
/// `D_actor_303600_80162E98` entry 1. 6 and 7 spawn entry 2, and 8 kills the fade
/// and spawns entries 3 and 1. The command is cleared on the way out.
void func_actor_303600_80161F40(Task* arg0)
{
    Actor303600Work* work = (Actor303600Work*)arg0->work;
    Actor303600Work* w;
    GpCmdArg         msg;

    switch (work->command) {
        case 0:
            break;
        case 1:
            w                  = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 1;
            break;
        case 2:
            w                  = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 2;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 2;
            break;
        case 3:
            w                  = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 3;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 3;
            break;
        case 4:
            w                  = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 4;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 4;
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            Task_SpawnFromTable(&D_actor_303600_80162E98, 1, 4, 0);
            break;
        case 5:
            w                  = (Actor303600Work*)D_actor_303600_8016E4C0->work;
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            msg.from.loc.area  = gGameSession->at4.loc.area;
            msg.command        = 5;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            w->field_C = 5;
            break;
        case 6:
            Task_SpawnFromTable(&D_actor_303600_80162E98, 2, 8, 0);
            break;
        case 7:
            Task_SpawnFromTable(&D_actor_303600_80162E98, 2, 4, 0);
            break;
        case 8:
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            Task_SpawnFromTable(&D_actor_303600_80162E98, 3, 0, 0);
            Task_SpawnFromTable(&D_actor_303600_80162E98, 1, 4, 0);
            break;
    }
    work->command = 0;
}

/// Cutscene controller for the overlay. State 0 arms it once: a `D_80114C12` of
/// 1 or a live `D_80071075` both mean a cutscene is already up, so the state is
/// left where it is and the task returns; otherwise it allocates the
/// `Actor303600Work` block, zeroes it, parks the `gameGetPtrSlot(3)` task in
/// `field_0` and publishes itself in `D_actor_303600_8016E4C0` with
/// `D_actor_303600_8016E4C4` cleared, then falls into state 1, which hands the
/// overlay's two cutscene script blocks to `func_800E8634`. State 2 waits for
/// the session's `eventState` to clear -- the cutscene having finished -- and then
/// sets the saved location in `Mc_SaveData` to stage 5, area 0x1F, warp 1,
/// room 1, raises the `D_80071076` latch, starts the stage-0 type-0x11 task and
/// kills itself; while the cutscene is still up it steps the state machine
/// instead.
void func_actor_303600_8016216C(Task* arg0)
{
    Actor303600Work* work;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || gDisplayState.pendingMode != 0) {
                return;
            }
            work       = (Actor303600Work*)Mem_Malloc(0x10, 0);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x10);
                work->field_0           = gameGetPtrSlot(3);
                D_actor_303600_8016E4C0 = arg0;
                D_actor_303600_8016E4C4 = NULL;
            }
            arg0->state += 1;
            /* fallthrough */
        case 1:
            func_800E8634((s32)D_actor_303600_80162AF0, 0, (s32)D_actor_303600_80162DD8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Mc_SaveData.at4.loc.stage = 5;
                Mc_SaveData.at4.loc.area  = 0x1F;
                Mc_SaveData.at4.loc.warp  = 1;
                Mc_SaveData.at4.loc.room  = 1;
                gDisplayState.roomVariant = 1;
                Task_Spawn(0, 0x11, 0x10, 0);
                taskKill(arg0);
                break;
            }
            func_actor_303600_80161F40(arg0);
            break;
    }
}

/// Fade-out driver: the same eight-byte channel block `func_actor_303600_801623CC`
/// walks up, walked the other way.  State 0 allocates it and fills all three
/// channels with 0xFF; a failed allocation kills the task outright.  State 1
/// draws the overlay tinted `r`/`g`/`r` in mode 1, steps all three channels down
/// by `Task::spawnArg1` -- the fade rate, not a colour -- and once `r` has gone
/// below zero clears `D_actor_303600_8016E4C4` before killing the task.
void func_actor_303600_801622E8(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r -= (u16)arg0->spawnArg1;
            work->g -= (u16)arg0->spawnArg1;
            work->b -= (u16)arg0->spawnArg1;
            if (work->r < 0) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// Fade-in driver: state 0 allocates the eight-byte channel block and clears
/// all three channels; a failed allocation kills the task outright.  State 1
/// runs every frame: it draws the overlay tinted `r`/`g`/`r` in mode 1, steps
/// all three channels by `Task::spawnArg1` -- the fade rate, not a colour -- and
/// once `r` has passed 0x100 clears `D_actor_303600_8016E4C4` before killing the
/// task.  The fade-out counterpart that walks the same block the other way, from
/// 0xFF down past zero, is `func_actor_303600_801622E8`.
void func_actor_303600_801623CC(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 1);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if (work->r >= 0x100) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(arg0);
            }
            break;
    }
}

/// One-shot announcement of the cutscene: while the work block's "message
/// outstanding" flag is still clear, hand the slot-4 task the session's two id
/// bytes plus selector 9 as message 0x7DA, record 9 in the work block and raise
/// the flag so the message goes out only once.
void func_actor_303600_801624B0(void)
{
    Actor303600Work* work = (Actor303600Work*)D_actor_303600_8016E4C0->work;
    GpCmdArg         msg;

    if (work->field_E == 0) {
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 9;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }
}

/// Cutscene teardown: kill the task a previous cutscene left in
/// `D_actor_303600_8016E4C4`, then, while the work block's message flag is
/// still clear, send the same 0x7DA announcement
/// `func_actor_303600_801624B0` sends and latch selector 9.  Finishes by
/// spawning the overlay's own continuation task -- `D_actor_303600_80162E98`
/// entry 3 -- so this runs exactly once per cutscene.
void func_actor_303600_8016253C(void)
{
    Actor303600Work* work;
    GpCmdArg         msg;

    if (D_actor_303600_8016E4C4 != NULL) {
        taskKill(D_actor_303600_8016E4C4);
        D_actor_303600_8016E4C4 = NULL;
    }

    work = (Actor303600Work*)D_actor_303600_8016E4C0->work;
    if (work->field_E == 0) {
        msg.from.loc.stage = gGameSession->at4.loc.stage;
        msg.from.loc.area  = gGameSession->at4.loc.area;
        msg.command        = 9;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        work->field_C = 9;
        work->field_E = 1;
    }

    Task_SpawnFromTable(&D_actor_303600_80162E98, 3, 0, 0);
}

void func_actor_303600_80162600(s16 arg0)
{
    Actor303600Work* work = (Actor303600Work*)D_actor_303600_8016E4C0->work;

    work->command = arg0;
    work->field_6 = 0;
}

void func_actor_303600_80162620(void)
{
    Gp_PulseState1C80();
    Gp_StateC08.field_6 |= 1;
}

/// Opcode-0x0D callback in the actor's cutscene script: queues CD command 0x82
/// through `CdCmd_EnqueueReplaceOverlay82`.
void func_actor_303600_80162658(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Opcode-0x0D callback in the actor's cutscene script: queues CD command 0x81
/// through `CdCmd_EnqueueOverlay81`.
void func_actor_303600_80162678(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Opcode-0x0D callback in the actor's cutscene script: restores the stream
/// random-number state, then drops the pending replacement CD command through
/// `CdCmd_CancelReplaceAndActivate`.
void func_actor_303600_80162698(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// Spawn state of the overlay's rig controller: allocates the work block the
/// later states read through `Task::work` (`memCalloc(0x3C, 0)`, the struct's
/// own size), clears the task's own root coordinate, then spawns the five child
/// models -- one `Task_SpawnFromTable` of `D_actor_303600_8016E468` entry 1
/// each, parked in `children` and spread 8000 apart in Y.  The spread reaches
/// the coordinate through the strength-reduced `i * 8000 - 16000` loop.c folds
/// into an accumulator, so its initialiser is scheduled at the loop head beside
/// the hoisted `%hi` of the spawn table.  A failed spawn stops the loop early, a
/// failed allocation kills the task instead of leaving a half-built controller,
/// and the last three statements install the 0x7DB handler table at
/// `Task::msgTable`, the shared kill callback and the next state.
void func_actor_303600_801626C0(Task* task)
{
    Actor303600RigWork* work;
    GpCoord*            coord;
    GpCoord*            childCoord;
    Task*               child;
    s32                 i;

    work = memCalloc(0x3C, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    for (i = 0; i < 5; i++) {
        child = Task_SpawnFromTable(D_actor_303600_8016E468, 1, 0, (s32)task);
        if (child == NULL) {
            break;
        }
        work->children[i]      = child;
        childCoord             = child->extra.tmd->coords;
        childCoord->coord.t[1] = i * 0x1F40 - 0x3E80;
        childCoord->coord.t[0] = 0;
        childCoord->coord.t[2] = 0;
    }
    task->msgTable     = D_actor_303600_8016E480;
    task->exitCallback = func_actor_303600_80162850;
    task->state       += 1;
}

/// Per-frame rig motion, run on the work block `func_actor_303600_801626C0`
/// fills in: ramp the 16.16 speed `field_28` toward the limit `field_38` at
/// `field_34` a frame, drop the ramp once the speed passes the limit in the
/// ramp's own direction, integrate the speed into the angle accumulator
/// `field_18`, fold that back into +/-4000, and publish its integer half as the
/// model coordinate's Y.  The accel is read once for the sum and once for the
/// limit test -- the second read is the branch's own copy of it in the target.
void func_actor_303600_801627B8(Task* task)
{
    Actor303600RigWork* work  = (Actor303600RigWork*)task->work;
    GpCoord*            coord = task->extra.tmd->coords;
    s32                 speed;
    s32                 angle;
    s32                 var;

    speed          = work->field_28 + work->field_34;
    work->field_28 = speed;
    if (work->field_34 > 0) {
        var = speed > work->field_38;
    } else {
        var = speed < work->field_38;
    }
    if (var != 0) {
        work->field_34 = 0;
    }
    angle            = work->field_18.w + work->field_28;
    work->field_18.w = angle;
    if (angle > 0x0FA00000) {
        work->field_18.w = angle - 0x1F400000;
    } else if (angle < -0x0FA00000) {
        work->field_18.w = angle + 0x1F400000;
    }
    coord->coord.t[1] = work->field_18.h.hi;
    coord->flg        = 0;
}

/// Exit callback the rig controller installs at `Task::exitCallback`, and the
/// third entry of its state table: kills the task.
void func_actor_303600_80162850(Task* task)
{
    taskKill(task);
}

/// Message 0x7DB handler, listed in `D_actor_303600_8016E480` -- the table
/// `func_actor_303600_801626C0` installs at `Task::msgTable`.  The payload is
/// the 0x7DA record `Gp_SendMsgType9` forwards back to the slot-4 task's
/// type-9 children, so the halfword switched on here is the sender's selector:
/// 0 arms the rig's speed at 384.0 (16.16) with a positive ramp, 1 with a
/// negative one (-6.0 toward -48.0), and every other selector exits the task
/// through its own `Task::exitCallback`.  `func_actor_303600_801627B8` is what
/// consumes the ramped speed.
s32 func_actor_303600_80162870(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor303600RigWork* work;

    work = (Actor303600RigWork*)task->work;
    switch (msg->command) {
        case 0:
            work->field_28 = 0x01800000;
            work->field_34 = 0x00080000;
            work->field_38 = 0x03000000;
            break;
        case 1:
            work->field_34 = 0xFFFA0000;
            work->field_38 = 0xFD000000;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// State table of the rig controller: spawn, per-frame motion and the kill
/// callback. Dispatched by `func_actor_303600_80162A7C`.
const TaskFuncTable3 D_actor_303600_80161E48 = { {
    func_actor_303600_801626C0,
    func_actor_303600_801627B8,
    func_actor_303600_80162850,
} };

/// State table of the rig's model tasks: spawn, an empty per-frame tick and
/// `taskKill`. Dispatched by `func_actor_303600_801628E4`.
const TaskFuncTable3 D_actor_303600_80161E54 = { {
    func_actor_303600_80162950,
    func_actor_303600_80162A04,
    taskKill,
} };

/// Per-frame dispatcher of the rig's model tasks: runs their spawn, tick or
/// exit state from `D_actor_303600_80161E54`, skipping the frame while
/// `Gp_StateF0.field_4` is set.
void func_actor_303600_801628E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_303600_80161E54;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Builds the actor's light / colour matrix pair, hangs it off the task's
/// `work` slot, and splices this task's model root under its spawn parent's.
void func_actor_303600_80162950(Task* task)
{
    Task*                 parent      = task->spawnArg2;
    TmdObject*            obj         = task->extra.tmd;
    GpCoord*              coord       = obj->coords;
    TmdObject*            parentObj   = parent->extra.tmd;
    GpCoord*              parentCoord = parentObj->coords;
    Actor303600LightMats* mats;

    mats = memCalloc(0x44, 0);
    if (mats == NULL) {
        taskKill(task);
        return;
    }

    task->work = (TaskIdMap*)mats;
    coord->sub = parentCoord;
    coord->flg = 0;
    func_actor_303600_80162A0C(task);
    Task_Reparent(parent, task);
    obj->flags  &= 0xFF7F;
    task->state += 1;
}

void func_actor_303600_80162A04(Task* task)
{
}

/// Points the task's model at the light / colour matrix pair in its own work
/// block and loads the overlay's three flat lights into them.
void func_actor_303600_80162A0C(Task* task)
{
    Actor303600LightMats* mats = (Actor303600LightMats*)task->work;
    TmdObject*            obj  = task->extra.tmd;
    GsF_LIGHT*            light;
    s32                   i;

    obj->lightMtx = &mats->lightMtx;
    obj->colorMtx = &mats->colorMtx;
    for (i = 0, light = D_actor_303600_8016E490; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &mats->lightMtx, &mats->colorMtx);
    }
}

/// Per-frame dispatcher of the rig controller: runs its spawn, motion or exit
/// state from `D_actor_303600_80161E48`, skipping the frame while
/// `Gp_StateF0.field_4` is set.
void func_actor_303600_80162A7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_303600_80161E48;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}
