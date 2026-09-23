#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"
#include "rooms/dryfield_water_tank.h"
#include "rooms/room_common.h"

extern s8             D_8007216C;
extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern GpMsgEntry     D_dryfield_water_tank_8017FD90[];
extern s32            D_dryfield_water_tank_8017FDC0;
extern s32            D_dryfield_water_tank_8017FEC8;
extern TaskDesc       D_dryfield_water_tank_8017FF88;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];
extern Task*          D_dryfield_water_tank_80188D4C;

/// Main-executable flag word with no module header yet: while its bit 2 is
/// raised the model task nudges the model 5 units off each position it snaps to.
extern s32 D_80070F6C[];

/// The model's placement run at 0x8017FD60. `[1]` (0x8017FD78) is the lowered
/// record: the model sinks until its Z passes `pos.vz` and is snapped to its
/// `pos.vy` and then `pos.vx`. The model task reaches that record both from
/// this head and by its own symbol, so the head is declared one element long;
/// the driver sends the head itself as the 0x7D4 placement.
extern RoomPlacement D_dryfield_water_tank_8017FD60[1];
extern RoomPlacement D_dryfield_water_tank_8017FD78;

/// X offsets the model task spawns effect 0x60054 with, indexed by the 0..10
/// `killCountdown` counter it wraps.
extern u16 D_dryfield_water_tank_8017FDA8[];

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
/// the lowered record's `pos.vy` (nudged by the `D_80070F6C` flag), and once the
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
    DwtColorMtx*   work  = (DwtColorMtx*)arg0->work;
    GsCOORDINATE2* coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2* effCoord;
    SVECTOR        pos;

    switch (work->field_4C) {
        case 0:
            coord->coord.t[2] += 0x14;
            coord->coord.t[1]  = D_dryfield_water_tank_8017FD60[1].pos.vy;
            if (D_80070F6C[0] & 4) {
                coord->coord.t[1] += 5;
            }
            if (coord->coord.t[2] > D_dryfield_water_tank_8017FD60[1].pos.vz) {
                work->field_4C++;
            }
            effCoord = ((TmdObject*)arg0->extra)->coords;
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
            if (D_80070F6C[0] & 4) {
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
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    DwtColorMtx*   mtx;
    TmdObject*     mdl;
    VECTOR         pos;

    switch (arg0->state) {
        case 0:
            extra      = (TmdObject*)arg0->extra;
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

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
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
    DwtMsg7DB      msg;

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
            msg.field_2 = 2;
            Gp_DispatchMsg(work->child, 0x7DB, (s32)&msg, 0);
            break;
        case 2:
            D_8007216C              = Gp_FindViewIndex(3);
            gGameSession->viewDirty = 1;
            /* The raw load is what makes this match: as `work->owner` it carries
             * MEM_IN_STRUCT_P, and sched1's true_dependence then disregards it
             * against D_8007216C's store, so the store sinks into the call's
             * delay slot and the two request tails stop cross-jumping. */
            Gp_DispatchMsg(*(Task**)((u8*)work + OFFSET_OF(DwtScriptWork, owner)), 0x3F3, 1, 0);
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

    obj = (TmdObject*)task->extra;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}
