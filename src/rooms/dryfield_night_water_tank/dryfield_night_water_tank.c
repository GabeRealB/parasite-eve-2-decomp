#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern GpGridParams D_dryfield_night_water_tank_8017E08C;
extern GpGridParams D_dryfield_night_water_tank_8017F4B0;

/// 0xFF-terminated `GpAreaApplyRec` list the room applies when the scripted end
/// of the visit fires.
extern GpAreaApplyRec D_dryfield_night_water_tank_801808B0;

/// Main-executable halfword the second state waits on before it may advance.

/// Script blobs handed to `func_800E8634` (which forwards them to `Task_Spawn`)
/// as raw addresses.
extern s32 D_80137C28;
extern s32 D_80138570;

/// Message table of the night water-tank room, 0x13EE..0x13F1 with the
/// 0x7FFFFFFF terminator: `func_dryfield_night_water_tank_8017D714`,
/// `..._8017D70C`, `..._8017D76C` and `..._8017D73C`.
extern GpMsgEntry D_dryfield_night_water_tank_8017DFE8[];

/// Task descriptor tables spawned by the room entry task, each one entry and
/// the 0xFFFF terminator: `8017E010` runs the exit task
/// `func_dryfield_night_water_tank_8017D5D0`, `8017EE28` the tank model's
/// update `func_dryfield_night_water_tank_8017DB8C`.
extern TaskDesc D_dryfield_night_water_tank_8017E010[];
extern TaskDesc D_dryfield_night_water_tank_8017EE28[];

/// Absolute import: 0x8013224C has no name in main or gameplay, so the call is
/// emitted against bare address, the way the other rooms' `func_8013...` are.
extern void func_8013224C(void);

/// Absolute import: the shared room script descriptor 0x8013788C, spawned by
/// entry 0 in the handler below.
extern TaskDesc D_8013788C;

/// Model/lighting records the handler below toggles on message 3 and 4.
extern s32 D_dryfield_night_water_tank_8017DDD8;
extern s32 D_dryfield_night_water_tank_8017DEE0;

/// The tank's wobble spring: `8017EE40` is the accumulated yaw handed to
/// `Gfx_RotMatrixY` (`>> 8`), `8017EE44` its velocity, `8017EE48` the yaw it
/// steps toward and `8017EE4C` the target that step chases.
extern s32 D_dryfield_night_water_tank_8017EE40;
extern s32 D_dryfield_night_water_tank_8017EE44;
extern s32 D_dryfield_night_water_tank_8017EE48;
extern s32 D_dryfield_night_water_tank_8017EE4C;

void func_dryfield_night_water_tank_8017D9DC(s32 arg0);

/// Exit task of the night water-tank room, in the shape the other rooms' wait
/// tasks have: three states on `Task::state`. State 0 raises bit 0x80 of
/// `gGameSession::flowFlags` once `Gp_StateF0` has reached 1, then advances;
/// state 1 advances to 2 as soon as the halfword at `Gp_StateF0.field_6` clears; state
/// 2 runs the room's ending -- apply the area records, set flags 0x7B, 0x83,
/// 0x155 and 3, spawn the script `func_800E8634` is handed -- and kills the
/// task, or, while `gGameSession::field_126` is still clear, just ticks
/// `Task::killCountdown` down and waits for another frame.
void func_dryfield_night_water_tank_8017D5D0(Task* task)
{
    SVECTOR3 unused; // never referenced; only reserves the frame slot the ROM has

    switch (task->state) {
        case 0:
            if (Gp_StateF0.field_0 == 1) {
                gGameSession->flowFlags = gGameSession->flowFlags | 0x80;
                task->state             = task->state + 1;
                return;
            }
            return;
        case 1:
            if (Gp_StateF0.field_6 == 0) {
                task->state = 2;
                return;
            }
            break;
        case 2:
            if (gGameSession->field_126 != 0) {
                Gp_ApplyAreaRecs(&D_dryfield_night_water_tank_801808B0);
                GameFlag_SetNibble(0x7B, 2);
                GameFlag_SetNibble(0x83, 1);
                func_800E8634((s32)&D_80137C28, 0, (s32)&D_80138570);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0xE);
                taskKill(task);
                return;
            }
            task->killCountdown = task->killCountdown - 1;
            break;
    }
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_water_tank_8017D70C(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record it is handed onto the outgoing one and answers 1.
s32 func_dryfield_night_water_tank_8017D714(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_night_water_tank_8017D73C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Gp_StartCapSlot(0xE, 1, 1);
    }
    return 0;
}

s32 func_dryfield_night_water_tank_8017D76C(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    u8 temp_v1;

    if ((gGameSession->at4.loc.place != 0xA) || (GameFlag_GetNibble(0x7B) >= 2)) {
        if (in->field_2 == 3) {
            func_800E8614((s32)&D_dryfield_night_water_tank_8017DDD8, 0);
        }
        if (in->field_2 == 4) {
            func_800E8614((s32)&D_dryfield_night_water_tank_8017DEE0, 0);
        }
    }
    if (in->field_2 == 5) {
        temp_v1 = gGameSession->at4.loc.place;
        if ((u32)(temp_v1 - 0xA) < 2U) {
            if ((temp_v1 != 0xA) || (GameFlag_GetNibble(0x7B) >= 2)) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_8013788C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(0x17);
            }
        }
    }
    return 0;
}

/// Room entry task tick, the shape the other dryfield rooms' entry tasks have:
/// publish the message table the room's handlers hang off (0x13EE..0x13F1) in
/// `Task::msgTable`, claim game pointer slot 7, spawn the tank model's task
/// from `8017EE28`, then branch on the visit sub-id
/// (`gGameSession::at4.loc.place`).
///
/// Sub-ids 0xA and 0xB -- the two visits that reach this room -- both run the
/// prop updater `func_dryfield_night_water_tank_8017D9DC` on its zero argument;
/// 0xA additionally spawns the exit task from `8017E010`, and 0xB, the visit
/// the room is announced into, hands over to `func_8013224C` instead. The state
/// advances on every path.
void func_dryfield_night_water_tank_8017D870(Task* task)
{
    task->msgTable = D_dryfield_night_water_tank_8017DFE8;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_dryfield_night_water_tank_8017EE28, 0, 0, 0);
    if ((u32)(gGameSession->at4.loc.place - 0xA) < 2U) {
        func_dryfield_night_water_tank_8017D9DC(0);
    }
    if (gGameSession->at4.loc.place == 0xA) {
        Task_SpawnFromTable(D_dryfield_night_water_tank_8017E010, 0, 0, 0);
    }
    if (gGameSession->at4.loc.place == 0xB) {
        func_8013224C();
    }
    task->state = task->state + 1;
}

/// The room task's second state, run every frame after the entry tick: marks
/// the play time while the visit sub-id is 0xB.
void func_dryfield_night_water_tank_8017D94C(Task* task)
{
    if (gGameSession->at4.loc.place == 0xB) {
        Gp_MarkPlayTime();
    }
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_night_water_tank_8017D984`: the entry tick, the per-frame
/// state, then `taskKill`.
const TaskFuncTable3 D_dryfield_night_water_tank_8017D5C4 = {
    { func_dryfield_night_water_tank_8017D870, func_dryfield_night_water_tank_8017D94C, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_water_tank_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}

/// Restores the room's layout lists from their template, then offsets the six
/// `field_8` coordinates by (0, 0, -0xC8) when `arg0` is non-zero.
void func_dryfield_night_water_tank_8017D9DC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_water_tank_8017F4B0;
    src = &D_dryfield_night_water_tank_8017E08C;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
        d.vz = 0;
    } else {
        d.vx = 0;
        d.vy = 0;
        d.vz = -0xC8;
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Per-frame model update for the tank, the callback of the task entry 0 of
/// `D_dryfield_night_water_tank_8017EE28` describes (spawned by the room entry
/// task). State 0 parents the model's coordinate to `gGfxViewCoord` and places
/// it at (0xBB8, -0x34A8, -0x4D8), then advances to state 1. State 1 drives the
/// tank's slow wobble about `y`: an occasional roll re-picks the target yaw,
/// the step moves toward it 0x100 at a time, and the velocity follows 19/20 of
/// the way to that step. Every frame then sets the model's flags to 0x80 while
/// the view is 7 (0 otherwise), publishes the coordinate's `workm` translation
/// as a `VECTOR` to `func_800D7A9C`, rebuilds the coordinate's yaw matrix from
/// the accumulated angle, and clears `flg` so the world matrix is recomputed.
///
/// The coordinate's load is written through the cast expression, before the
/// object pointer is assigned: the pointer assignment has to stay a separate
/// register copy, or the overlay comes up an `addu` short.
void func_dryfield_night_water_tank_8017DB8C(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

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
                    D_dryfield_night_water_tank_8017EE4C = (s32)(rand() * 20) >> 7;
                } else {
                    D_dryfield_night_water_tank_8017EE4C = 0;
                }
            }
            if (D_dryfield_night_water_tank_8017EE48 < D_dryfield_night_water_tank_8017EE4C) {
                D_dryfield_night_water_tank_8017EE48 += 0x100;
            } else if (D_dryfield_night_water_tank_8017EE4C < D_dryfield_night_water_tank_8017EE48) {
                D_dryfield_night_water_tank_8017EE48 -= 0x100;
            }
            D_dryfield_night_water_tank_8017EE44 =
                (D_dryfield_night_water_tank_8017EE44 + D_dryfield_night_water_tank_8017EE48) * 19 / 20;
            D_dryfield_night_water_tank_8017EE40 += D_dryfield_night_water_tank_8017EE44;
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
    Gfx_RotMatrixY(&coord->coord, D_dryfield_night_water_tank_8017EE40 >> 8, 1);
    coord->flg = 0;
}

void func_dryfield_night_water_tank_8017DD8C(void)
{
}
