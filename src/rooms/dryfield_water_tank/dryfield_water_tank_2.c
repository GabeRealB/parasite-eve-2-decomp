#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_water_tank.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block for the water-tank cutscene task, allocated as 0xC zeroed bytes
/// by `func_dryfield_water_tank_8017E9F8` and hung off `Task::work` (0x1C): only
/// `owner` is written by that run, so the block's tail is outside its
/// allocation. The layout is the one the same cutscene-task body has in
/// `dryfield_gas_station` (`DgsWork`, which allocates the full 0x10):
/// `owner` is the slot-3 game pointer (`gameGetPtrSlot(3)`) the task dispatches
/// its messages to, and the two shorts at 0x4 are the script command and its
/// step counter, written together by `func_dryfield_water_tank_8017EB80`.
typedef struct DwtWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ s16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DwtWork;
STATIC_ASSERT_SIZEOF(DwtWork, 0x10);

/// Main-executable globals with no module header yet: the cutscene task
/// refuses to start while `Gp_StateC08.field_A` is 1 or `gDisplayState.pendingMode` is non-zero.
/// `Player_Status.weapon` is the equipped-weapon index the slot-3 msg 0x3E8 animation
/// record is keyed on, and `Mc_SaveData.characterId` picks which of the two weapon-id bases
/// that record uses.

/// Main-executable flag set to 1 before the view tasks are respawned.

/// Spawn table for the task that takes over once the intro stream is done.
extern TaskDesc D_dryfield_water_tank_80180764;

extern TaskDesc D_dryfield_water_tank_80184DF4;

/// Script record the cutscene owner is handed with msg 0x3F4.
extern s32 D_dryfield_water_tank_801804EC;

/// The placement the room sends the slot-3 task, the cutscene's owner, with
/// message 0x3E9.
extern GpXformArg D_dryfield_water_tank_801804F4;

/// The two blocks `func_800E8634` is handed as raw addresses.
extern s32 D_dryfield_water_tank_8018050C;
extern s32 D_dryfield_water_tank_8018068C;

/// The water tank's run: one `SVECTOR` position per frame, `y` fixed at -12000
/// and `z` stepping up the room, 52 entries of movement before the tail clamps.
extern SVECTOR D_dryfield_water_tank_80184530[];

/// The second leg of the tank's run, same `SVECTOR` shape and one entry per
/// frame: the seam repeats the first table's last entry (`x` 2532, `y` -12000,
/// `z` 972), after which `x` steps down while `z` holds. Exactly the 52 entries
/// its walk consumes, so unlike the first table it has no clamp tail.
extern SVECTOR D_dryfield_water_tank_801847C0[];

/// The tank's wobble spring, the four words at 0x801868BC: `801868BC` is the
/// accumulated yaw `Gfx_RotMatrixY` is handed (`>> 8`), `801868C0` its velocity,
/// `801868C4` the yaw it steps toward and `801868C8` the target that step
/// chases.
extern s32 D_dryfield_water_tank_801868BC;
extern s32 D_dryfield_water_tank_801868C0;
extern s32 D_dryfield_water_tank_801868C4;
extern s32 D_dryfield_water_tank_801868C8;

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C->roomEffectMode` variant index.
extern u16 D_dryfield_water_tank_801868CC[];

/// The room's cutscene task, parked by `func_dryfield_water_tank_8017E9F8` so
/// the script commands can reach its `DwtWork` block.
extern Task* D_dryfield_water_tank_80188D50;

/// Fade the water tank to white and tear the task down.
///
/// State 0 allocates the ramp at `Task::work` and zeroes it; a failed
/// allocation kills the task outright. State 1 runs every frame: it links a
/// semi-transparent full-screen `TILE` (`-0xA0,-0x78`, `0x140x0xF0`) plus the
/// `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`, tinting the tile `r`/`g`/`r`,
/// then steps all three channels by `Task::spawnArg1`. Once `r` saturates past
/// 0xFF the screen is fully covered, so the task kills itself. The fade-up half
/// of the same pair is `func_dryfield_water_tank_8017E220`.
void func_dryfield_water_tank_8017E3C4(Task* arg0)
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
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
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

            fade->r += (u16)arg0->spawnArg1;
            fade->g += (u16)arg0->spawnArg1;
            fade->b += (u16)arg0->spawnArg1;
            if (fade->r >= 0x100) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}

/// Water-tank intro cutscene driver: fades out, streams the room's movie via
/// CdCmd, and on completion clears the image buffers and hands off to the
/// follow-up task.
void func_dryfield_water_tank_8017E568(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            SndEvt_EnqueueType7(0x52150009, 0x3C);
            task->state = task->state + 1;
            return;
        case 1:
            key          = gGameSession->at4;
            key.loc.view = 0x64;
            slot         = Stream_FindSlot(key.raw.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x52150006, 0, 0);
            SndEvt_EnqueueType6(0x52150007, 0, 0);
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SndEvt_EnqueueType7(0x52150006, 0x1E);
            SndEvt_EnqueueType7(0x52150007, 0x1E);
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x52150009, 0, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Task_SpawnOnDefaultList(&D_dryfield_water_tank_80180764, 2, 8, 0);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Carries out the script command in `DwtWork::field_4`, then clears it; the
/// multi-frame commands step `field_6` and return early until they finish.
/// 2 hands the owner the `D_dryfield_water_tank_801804EC` script record as msg
/// 0x3F4 over two frames, the second also sending msg 0x3FD. 3 spawns entry 3 of
/// the room task table, 4 sends the owner the 0x800 warp (msg 0x3EE). 5 spawns
/// the view tasks, waits two frames, then hands the owner the room placement
/// (msg 0x3E9) and slot 3 the equipped weapon's 0x3E8 animation record, and
/// marks the view dirty.
void func_dryfield_water_tank_8017E78C(Task* task)
{
    DwtWork* work;
    DwtWork* cur;
    union {
        GpAnimArg  rec;
        GpXformArg warp;
    } msg;
    GpAnimArg  script;
    GpAnimArg* rec;
    u16        step;
    s32        weaponId;
    s32        idx;

    work = (DwtWork*)task->work;
    switch ((u16)work->field_4) {
        case 0:
        case 1:
            break;
        case 2:
            step = work->field_6;
            switch (step) {
                case 0:
                    cur = (DwtWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = &D_dryfield_water_tank_801804EC;
                        msg.rec.field_4       = 0;
                        msg.rec.field_8       = 0;
                        msg.rec.field_C       = 0;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    work->field_6++;
                    return;
                case 1:
                    cur = (DwtWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.animBlock.ptr = &D_dryfield_water_tank_801804EC;
                        msg.rec.field_4       = step;
                        msg.rec.field_8       = step;
                        msg.rec.field_C       = 0xF;
                        msg.rec.field_10      = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
                    break;
            }
            break;
        case 3:
            Task_SpawnFromTable(&D_dryfield_water_tank_80180764, 3, 8, 0);
            break;
        case 4:
            msg.warp.rot.vy = 0x800;
            Gp_DispatchMsg((Task*)work->owner, 0x3EE, (s32)&msg.warp, 0);
            break;
        case 5:
            idx = (u16)work->field_6;
            switch (idx) {
                case 0:
                    Display_SpawnWithOt(&D_dryfield_water_tank_80180764, 1, 0, 0);
                    gDisplayState.at100.flags.flipMode = 1;
                    Gp_SpawnViewTasks();
                    work->field_6++;
                    return;
                case 1:
                case 2:
                    work->field_6 = idx + 1;
                    return;
                case 3:
                    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_water_tank_801804F4, 0);
                    // Taken before the record is filled, the address sits in
                    // $a1 and `field_4` is stored through it.
                    rec                    = &script;
                    weaponId               = Player_Status.weapon;
                    script.animBlock.index = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                    rec->field_4           = 1;
                    script.field_8         = 0;
                    script.field_C         = 0;
                    script.field_10        = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&script, 0);
                    gGameSession->viewDirty = 1;
                    break;
                default:
                    return;
            }
            break;
    }
    work->field_4 = 0;
}

/// Cutscene task state machine. State 0 refuses to run when the cutscene flag
/// is already up or one is live, otherwise it parks the freshly zeroed
/// `DwtWork` block in `Task::work`, republishes this task as
/// `D_dryfield_water_tank_80188D50` so the room's script commands can reach
/// that block, and hands slot 3 the 0x3E8 message carrying the animation set of the
/// equipped weapon: `Player_Status.weapon + 1` for the alternate block and
/// `Player_Status.weapon + 0x22` for the base one. A failed `Mem_Malloc` kills the task
/// outright instead of returning, so the message and the state step still run
/// on that path. States 2, 3 and 4 only step; state 1 runs the per-frame
/// driver once the session is up, or steps when it has already torn down;
/// state 5 asks to be killed.
void func_dryfield_water_tank_8017E9F8(Task* task)
{
    DwtWork*  work;
    GpAnimArg script;
    s32       weaponId;
    s32       anim;

    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto advance;
        case 3:
            goto advance;
        case 4:
            goto advance;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
        work       = Mem_Malloc(0xC, false);
        task->work = (TaskIdMap*)work;
        if (work == NULL) {
            taskKill(task);
        } else {
            Mem_Set(work, 0, 0xC);
            work->owner                    = gameGetPtrSlot(3);
            D_dryfield_water_tank_80188D50 = task;
        }
        weaponId               = Player_Status.weapon;
        anim                   = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
        script.animBlock.index = anim;
        script.field_4         = 1;
        script.field_8         = 1;
        script.field_C         = 0xA;
        script.field_10        = 0;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&script, 0);
        func_800E8634((s32)&D_dryfield_water_tank_8018050C, 0,
                      (s32)&D_dryfield_water_tank_8018068C);
        goto advance;
    }
    return;

L_case1:
    if (gGameSession->eventState == 0) {
        goto advance;
    }
    func_dryfield_water_tank_8017E78C(task);
    return;

advance:
    task->state = task->state + 1;
    return;

L_case5:
    Task_RequestKill(task, 0);
}

/// Script command of the room's cutscene: stores `arg0` as the command the
/// cutscene task carries out next (`DwtWork::field_4`) and restarts its step
/// counter `field_6`. The block is reached through the cutscene task parked in
/// `D_dryfield_water_tank_80188D50`.
void func_dryfield_water_tank_8017EB80(s16 arg0)
{
    DwtWork* work = (DwtWork*)D_dryfield_water_tank_80188D50->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

void func_dryfield_water_tank_8017EBA0(void)
{
    GpAnimArg rec;
    s32       weaponId;
    s32       anim;

    Gp_DispatchMsg(((DwtWork*)D_dryfield_water_tank_80188D50->work)->owner, 0x3E9,
                   (s32)&D_dryfield_water_tank_801804F4, 0);
    weaponId            = Player_Status.weapon;
    anim                = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.animBlock.index = anim;
    rec.field_4         = 1;
    rec.field_8         = 0;
    rec.field_C         = 0;
    rec.field_10        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
    SetDispMask(1);
}

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

/// Walks the water tank one step along `D_dryfield_water_tank_80184530` per
/// frame: sends slot 3 that entry as a `GpXformArg` -- the spline position
/// with the tank's fixed half-turn about `y` -- and advances `killCountdown`.
/// At 0x34 the tank has finished its run, and the task kills itself.
void func_dryfield_water_tank_8017EC6C(Task* arg0)
{
    GpXformArg rec;

    if (arg0->killCountdown >= 0x34) {
        taskKill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_80184530[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_80184530[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_80184530[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = -0x7FF;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// The tank's second run leg, the continuation of `func_dryfield_water_tank_8017EC6C`:
/// walks it one step along `D_dryfield_water_tank_801847C0` per frame and sends
/// slot 3 that entry as a `GpXformArg`, this time with a quarter-turn about
/// `y` (0x400) instead of the first leg's half-turn. At 0x34 the tank has
/// finished its run a second time and the task kills itself.
void func_dryfield_water_tank_8017ED30(Task* arg0)
{
    GpXformArg rec;

    if (arg0->killCountdown >= 0x34) {
        taskKill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_801847C0[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_801847C0[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_801847C0[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = 0x400;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// Per-frame model update for the tank: the callback word at 0x801868A8 in the
/// room's task table `D_dryfield_water_tank_801868A4`. State 0 parents the
/// model's coordinate to `gGfxViewCoord` and places it against the room's north
/// wall, then advances to state 1. State 1 drives the tank's slow wobble about
/// `y`:
/// an occasional roll re-picks the target yaw, the step moves toward it 0x100
/// at a time, and the velocity follows 19/20 of the way to that step. Either
/// way the frame ends by publishing the model's `workm` translation as a
/// `VECTOR` to `func_800D7A9C`, rebuilding the coordinate's yaw matrix from the
/// accumulated angle, and clearing `flg` so the parent recomputes the world
/// matrix next frame.
///
/// The coordinate's load is written through the cast expression, *before* the
/// object pointer is assigned, because the pointer assignment has to stay a
/// separate register copy: assigned first, cse.c's `(set REG0 REG1)` swap folds
/// the load and the copy into one and the overlay comes up an `addu` short (see
/// DECOMPILATION_LEARNINGS.md, "A load the pointer variable must copy").
void func_dryfield_water_tank_8017EDF4(Task* arg0)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    coord = arg0->extra.tmd->coords;
    obj   = arg0->extra.tmd;
    switch (arg0->state) {
        case 0:
            obj->flags        = 0;
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = 0xBB8;
            coord->coord.t[1] = -0x34A8;
            coord->coord.t[2] = -0x4D8;
            arg0->state++;
            break;
        case 1:
            if (((s32)(rand() * 100) >> 15) <= 0) {
                if (((s32)(rand() * 100) >> 15) < 0x50) {
                    D_dryfield_water_tank_801868C8 = (s32)(rand() * 20) >> 7;
                } else {
                    D_dryfield_water_tank_801868C8 = 0;
                }
            }
            if (D_dryfield_water_tank_801868C4 < D_dryfield_water_tank_801868C8) {
                D_dryfield_water_tank_801868C4 += 0x100;
            } else if (D_dryfield_water_tank_801868C8 < D_dryfield_water_tank_801868C4) {
                D_dryfield_water_tank_801868C4 -= 0x100;
            }
            D_dryfield_water_tank_801868C0 =
                (D_dryfield_water_tank_801868C0 + D_dryfield_water_tank_801868C4) * 19 / 20;
            D_dryfield_water_tank_801868BC += D_dryfield_water_tank_801868C0;
            break;
    }
    if (gGameSession->at4.loc.view == 7) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gfx_RotMatrixY(&coord->coord, D_dryfield_water_tank_801868BC >> 8, 1);
    coord->flg = 0;
}

/// Toggle the room's cutscene-“watched” state over two of the area's sprite
/// commands, hiding one and showing the other through their
/// `GpSprtCmd::field_4`. Every use goes through one pointer variable: the compiler keeps it
/// in a global allocno, which is what pushes the two literals' constant into
/// `$v0` (see DECOMPILATION_LEARNINGS.md, "A one-constant toggle…").
void func_dryfield_water_tank_8017EFF4(s32 arg0)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* view;

    sess = &gGameSession->at4.loc;
    if (sess->stage == 2) {
        rec = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if (!(arg0 & 0xFF)) {
            view            = rec[2].field_4;
            view[3].field_4 = 0;
            view            = rec[7].field_4;
            view[1].field_4 = 1;
            return;
        }
        view            = rec[2].field_4;
        view[3].field_4 = 1;
        view            = rec[7].field_4;
        view[1].field_4 = 0;
    }
}

/// Publishes the variant index the current camera view maps to: reads the view
/// index back and stores `D_dryfield_water_tank_801868CC[view - 1]` into the
/// shared work block's `field_A`. Gameplay holds this address in its data
/// (0x80110614, pointing at the room overlay), and `dryfield_parking_lot` and
/// `dryfield_water_tower` carry the same body.
void func_dryfield_water_tank_8017F084(void)
{
    Gp_State1C->roomEffectMode = D_dryfield_water_tank_801868CC[(Gp_GetViewIndex() & 0xFF) - 1];
}
