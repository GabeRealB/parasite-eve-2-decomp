#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "psyq/libgpu.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "rooms/dryfield_water_tank.h"
#include "rooms/rooms_shared_80180b2c.h"

/// Spawn table for the task that takes over once the intro stream is done.
extern TaskDesc D_dryfield_water_tank_80180764;

/// Main-executable globals with no module header yet: `D_80114C12` is the
/// cutscene/among-us mode flag and `D_80071075` the live-cutscene gate the
/// cutscene task refuses to start behind. `D_80073BA9` is the base weapon id the
/// animation set ids are numbered from and `D_8007218A` selects the alternate
/// block.
extern s8 D_80114C12;
extern u8 D_80071075;
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// The two blocks `func_800E8634` is handed as raw addresses.
extern s32 D_dryfield_water_tank_8018050C;
extern s32 D_dryfield_water_tank_8018068C;

/// Fade the water tank to white and tear the task down.
///
/// State 0 allocates the ramp at `Task::work` and zeroes it; a failed
/// allocation kills the task outright. State 1 runs every frame: it links a
/// semi-transparent full-screen `TILE` (`-0xA0,-0x78`, `0x140x0xF0`) plus the
/// `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`, tinting the tile `r`/`g`/`r`,
/// then steps all three channels by `Task::spawnArg1`. Once `r` saturates past
/// 0xFF the screen is fully covered, so the task kills itself. The fade-up half
/// of the same pair is the shared `RoomsShared8017da58`.
void func_dryfield_water_tank_8017E3C4(Task* arg0)
{
    DwtFadeWork* fade;
    DwtFadeWork* alloc;
    u8           r;
    u8           g;
    TILE*        tile;
    DR_TPAGE*    dr;

    fade = (DwtFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (DwtFadeWork*)Mem_Malloc(8, 0);
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
            tile           = (TILE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
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

            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
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

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_5", func_dryfield_water_tank_8017E78C);

/// Body `func_dryfield_water_tank_8017E9F8` runs from its state 1 while the
/// session is up. Still `INCLUDE_ASM` in this unit.
void func_dryfield_water_tank_8017E78C(Task* task);

/// Cutscene task state machine. State 0 refuses to run when the cutscene flag
/// is already up or one is live, otherwise it parks the freshly zeroed
/// `DwtWork` block in `Task::work`, republishes this task as
/// `RoomsShared80180b2cTask` so the room's script helpers can reach that block,
/// and hands slot 3 the 0x3E8 message carrying the animation set of the
/// equipped weapon: `D_80073BA9 + 1` for the alternate block and
/// `D_80073BA9 + 0x22` for the base one. A failed `Mem_Malloc` kills the task
/// outright instead of returning, so the message and the state step still run
/// on that path. States 2, 3 and 4 only step; state 1 runs the per-frame
/// driver once the session is up, or steps when it has already torn down;
/// state 5 asks to be killed.
void func_dryfield_water_tank_8017E9F8(Task* task)
{
    DwtWork* work;
    GpRec14  script;
    s32      weaponId;
    s32      anim;

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
    if ((D_80114C12 != 1) && (D_80071075 == 0)) {
        work       = Mem_Malloc(0xC, false);
        task->work = (TaskIdMap*)work;
        if (work == NULL) {
            taskKill(task);
        } else {
            Mem_Set(work, 0, 0xC);
            work->owner             = gameGetPtrSlot(3);
            RoomsShared80180b2cTask = task;
        }
        weaponId        = D_80073BA9;
        anim            = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
        script.field_0  = anim;
        script.field_4  = 1;
        script.field_8  = 1;
        script.field_C  = 0xA;
        script.field_10 = 0;
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
