#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "rooms/room_common.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

/// The room's own task table, eight descriptors that attach no model,
/// spawned by index and closed by an entry whose `flags` is all ones.
extern TaskDesc D_acropolis_helicopter_landing_pad_80184DA0[];

extern s16 D_80071076;
extern s8  D_801153F1;
extern s32 D_801156A8;
extern s32 Gp_LcgState;

/// Main-executable byte with no module header yet; `+ 1` seeds the slot-3
/// msg 0x3E8 record's `field_0` in `func_acropolis_helicopter_landing_pad_8017DA9C`.
extern u8 D_80073BA9;

/// Turn-to-heading task state: current unwrapped yaw and signed step.
extern s32   D_acropolis_helicopter_landing_pad_80187F74;
extern s32   D_acropolis_helicopter_landing_pad_80187F78;
extern s16   D_acropolis_helicopter_landing_pad_80187F7C;
extern Task* D_acropolis_helicopter_landing_pad_80187F80;

/// Three `Gp_SpawnScript18` argument pairs used by the state timeline in
/// `func_acropolis_helicopter_landing_pad_8017DE78`, one pair per phase.
extern s32 D_acropolis_helicopter_landing_pad_80187D40;
extern s32 D_acropolis_helicopter_landing_pad_80187D48;
extern s32 D_acropolis_helicopter_landing_pad_80187D50;
extern s32 D_acropolis_helicopter_landing_pad_80187D60;
extern s32 D_acropolis_helicopter_landing_pad_80187D68;
extern s32 D_acropolis_helicopter_landing_pad_80187D78;

/// Script / cutscene blocks and message payloads used by the room's
/// state-machine task `func_acropolis_helicopter_landing_pad_8017DA9C`:
/// `..._801837B0` is the slot-3 msg 0x3E9 argument, `..._8018467C` /
/// `..._80184CF4` the `func_800E8634` script pair started at the end,
/// `..._80184E28` the 0x7D3 payload sent to the spawned enemy task and
/// `..._80184E3C` the 0x14-byte msg 0x3E8 record.
extern s32        D_acropolis_helicopter_landing_pad_801837B0;
extern s32        D_acropolis_helicopter_landing_pad_8018467C;
extern s32        D_acropolis_helicopter_landing_pad_80184CF4;
extern AhlpMsg7D3 D_acropolis_helicopter_landing_pad_80184E28;
extern GpRec14    D_acropolis_helicopter_landing_pad_80184E3C;

extern GpMsgEntry D_acropolis_helicopter_landing_pad_80183710[];
extern s32        D_acropolis_helicopter_landing_pad_801837E0[];
extern s32        D_acropolis_helicopter_landing_pad_80183A04;
extern s32        D_acropolis_helicopter_landing_pad_80183A34;
extern s32        D_acropolis_helicopter_landing_pad_80183FA4;
extern s32        D_acropolis_helicopter_landing_pad_80184E50;
extern GpObj4A    D_acropolis_helicopter_landing_pad_80185FAC[];

void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1);
void func_acropolis_helicopter_landing_pad_8017EA6C(Task* task);

/// A debug format string nothing in the room reads.
const char D_acropolis_helicopter_landing_pad_8017D5D0[] = "%s (%5d,%5d,%5d)";

/// State handlers of the room's script task
/// `func_acropolis_helicopter_landing_pad_8017EB00`, indexed by
/// `Task::state`: set-up, the per-frame phase tick and `taskKill`.
const TaskFuncTable3 D_acropolis_helicopter_landing_pad_8017D5E4 = {
    { func_acropolis_helicopter_landing_pad_8017EA6C, func_acropolis_helicopter_landing_pad_8017D9BC, taskKill },
};

/// Room state-machine task. State 0 resets the player weapon, posts 0x7D5 to
/// slot-4 entry 1 on a second-or-later visit (`Mc_SaveData.at4.loc.place`), stamps
/// the save location with 0x12 and sets the override vector. States 1-4 wait
/// for `gGameSession->viewReady`, post 0x7D9 to slot 4 on a first visit, then
/// call `func_800A99B4`. State 5 asks slot 4 to spawn the enemy task (0x7D8),
/// positions it (0x7D3), pushes the slot-3 weapon record with `field_4 = 9`
/// and hands the enemy's coordinate to slot 3 (0x3F5). State 6 queues CD
/// command 0x21 on a first visit and arms a 0x78 frame countdown; state 7
/// waits for the CD to go idle (or skips to 9 on a later visit); state 8
/// registers the area object and spawns the area; state 9 starts the exit
/// script pair and kills the task. Every frame, hitting countdown 0x5A queues
/// sound event 0x51100003.
void func_acropolis_helicopter_landing_pad_8017DA9C(Task* task)
{
    u8      param1[8];
    u8      param2[8];
    SVECTOR vec;
    Task*   spawned;
    void*   coord;

    switch (task->state) {
        case 0:
            task->spawnArg1 = 0;
            Gp_MsgPlayerWeapon(0);
            task->state += 1;
            if (Mc_SaveData.at4.loc.place >= 2) {
                Gp_DispatchMsg(Gp_LookupSlot4(1), 0x7D5, 0, 0);
            }
            Mc_SaveData.at4.loc.view = 0x12;
            vec.vx                   = 0x4B0;
            vec.vy                   = 0x4B0;
            vec.vz                   = 0x610;
            Gp_SetOverrideVec(&vec);
            break;
        case 1:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case 2:
            if (Mc_SaveData.at4.loc.place < 2) {
                Gp_DispatchMsg(gameGetPtrSlot(4), 0x7D9, 0, 0);
            }
            task->state += 1;
            break;
        case 3:
            task->state += 1;
            break;
        case 4:
            func_800A99B4();
            task->state += 1;
            break;
        case 5:
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7D8, 0x28, (s32)&spawned);
            Gp_DispatchMsg(spawned, 0x7D3, (s32)&D_acropolis_helicopter_landing_pad_80184E28, 0);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&D_acropolis_helicopter_landing_pad_801837B0, 0);
            D_acropolis_helicopter_landing_pad_80184E3C.field_4 = 9;
            D_acropolis_helicopter_landing_pad_80184E3C.field_0 = D_80073BA9 + 1;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&D_acropolis_helicopter_landing_pad_80184E3C, 0);
            coord = ((TmdObject*)spawned->extra)->coords;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F5, (s32)coord, 0);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&D_acropolis_helicopter_landing_pad_801837B0, 0);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            task->state += 1;
            break;
        case 6:
            if (Mc_SaveData.at4.loc.place < 2) {
                param1[2] = 0x33;
                param2[0] = 0xA;
                param2[2] = 3;
                param1[3] = 0;
                param1[0] = 0;
                param2[1] = 0;
                param2[3] = 5;
                CdCmd_Enqueue(0x21, param1, param2);
                func_800ABFF8();
                func_800AC000();
            }
            task->spawnArg1 = 0x78;
            task->state    += 1;
            break;
        case 7:
            task->spawnArg1 -= 1;
            if (Mc_SaveData.at4.loc.place >= 2) {
                task->state = 9;
            } else if (CdCmd_IsIdle()) {
                func_800A99B4();
                task->state += 1;
            }
            break;
        case 8:
            task->spawnArg1 -= 1;
            Gp_SetAreaObjId((GpAreaKey*)&Mc_SaveData.at4.loc.view, 2, 1);
            Gp_SyncAreaKeyIndex((GpAreaKey*)&Mc_SaveData.at4.loc.view);
            Gp_SpawnArea((GpAreaKey*)&Mc_SaveData.at4.loc.view);
            D_acropolis_helicopter_landing_pad_80184D9C = 4;
            task->state                                += 1;
            break;
        case 9:
            task->spawnArg1 -= 1;
            if (task->spawnArg1 <= 0) {
                func_800E8634((s32)&D_acropolis_helicopter_landing_pad_8018467C, 1,
                              (s32)&D_acropolis_helicopter_landing_pad_80184CF4);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1 == 0x5A) {
        SndEvt_EnqueueType6(0x51100003, 0, 0);
    }
}

/// Frame-counted timeline task: three phases, each spawning the enemy task
/// (`func_..._8017E618`) and, a few frames later, a script-18 pair; kills
/// itself at frame 0x280.
void func_acropolis_helicopter_landing_pad_8017DE78(Task* task)
{
    switch (task->state) {
        case 0xC8:
        case 0x0:
            func_acropolis_helicopter_landing_pad_8017E618(0xF, 2);
            break;
        case 0x7:
        case 0xCF:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D68, (s32)&D_acropolis_helicopter_landing_pad_80187D78);
            break;
        case 0x19A:
            func_acropolis_helicopter_landing_pad_8017E618(0x16, 3);
            break;
        case 0x1A5:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D50, (s32)&D_acropolis_helicopter_landing_pad_80187D60);
            break;
        case 0x208:
            func_acropolis_helicopter_landing_pad_8017E618(0x1E, 4);
            break;
        case 0x217:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D40, (s32)&D_acropolis_helicopter_landing_pad_80187D48);
            break;
        case 0x280:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void func_acropolis_helicopter_landing_pad_8017DFCC(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_FillPlayerHpMp();
            Gp_ClearCollectedBit(0x101);
            Gp_ClearCollectedBit(0x102);
            Gp_SetItemSeenBit(0x102, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184E68, 0, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.sceneEvent    = 1;
            Mc_SaveData.at4.loc.stage = 1;
            Mc_SaveData.at4.loc.area  = 0x12;
            Mc_SaveData.at4.loc.warp  = 1;
            Mc_SaveData.at4.loc.room  = 1;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Display_ReleaseRef();
            taskKill(arg0);
            return;
    }
}

/// Turn-to-heading task: rotates the player actor's yaw
/// (`GameActor.field_52`, masked to 12 bits) to `spawnArg1` in `0x100` steps
/// along the shorter direction. State 0 picks the unwrapped start angle
/// (`yaw`, or `yaw +/- 0x1000` when that is closer to the target) and the
/// step sign; state 1 steps, clamps onto the target and kills the task.
///
/// `tmp` carries three unrelated values (the first abs distance, the
/// "wrapped is closer" flag and state 1's new yaw), and `target` is re-read
/// in state 1: both are what puts the `slt` result and the state-1 sum in
/// the same registers as the original.
void func_acropolis_helicopter_landing_pad_8017E0F8(Task* arg0)
{
    GameActor* actor = (GameActor*)(gameGetPtrSlot(3))->work;
    s32        wrapped;
    s32        tmp;
    s32        dist;
    s32        target;
    s32        cur;

    switch (arg0->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F74 = actor->field_52 & 0xFFF;
            if (arg0->spawnArg1 < D_acropolis_helicopter_landing_pad_80187F74) {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 - 0x1000;
            } else {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 + 0x1000;
            }
            target = arg0->spawnArg1;
            cur    = D_acropolis_helicopter_landing_pad_80187F74;
            tmp    = wrapped - target;
            if (tmp < 0) {
                tmp = -tmp;
            }
            dist = cur - target;
            if (dist < 0) {
                dist = -dist;
            }
            tmp = tmp < dist;
            if (tmp) {
                D_acropolis_helicopter_landing_pad_80187F74 = wrapped;
            }
            if (arg0->spawnArg1 > D_acropolis_helicopter_landing_pad_80187F74) {
                D_acropolis_helicopter_landing_pad_80187F78 = 0x100;
            } else {
                D_acropolis_helicopter_landing_pad_80187F78 = -0x100;
            }
            arg0->state++;
            break;
        case 1:
            tmp                                         = D_acropolis_helicopter_landing_pad_80187F74 + D_acropolis_helicopter_landing_pad_80187F78;
            D_acropolis_helicopter_landing_pad_80187F74 = tmp;
            if (D_acropolis_helicopter_landing_pad_80187F78 > 0) {
                target = arg0->spawnArg1;
                if (target < tmp) {
                    D_acropolis_helicopter_landing_pad_80187F74 = target;
                    taskKill(arg0);
                }
            }
            if (D_acropolis_helicopter_landing_pad_80187F78 < 0) {
                if (D_acropolis_helicopter_landing_pad_80187F74 < arg0->spawnArg1) {
                    D_acropolis_helicopter_landing_pad_80187F74 = arg0->spawnArg1;
                    taskKill(arg0);
                }
            }
            actor->field_52 = D_acropolis_helicopter_landing_pad_80187F74;
            break;
    }
}

/// Helipad rotor / lift task: swings the player model's coord part 4 about X
/// by `-angle * 0x60 / 0x1000` and updates it. State 1 ramps
/// `D_acropolis_helicopter_landing_pad_80187F7C` up to 0x1000 (then state 2),
/// state 2 ramps it back to 0 (then state 0), state 0 resets it.
///
/// The extra locals are dead: the original wrote `dir` and reserved the
/// other aggregates (0x50 bytes of frame) for code that no longer runs, and
/// the constant stores survive in the binary.
void func_acropolis_helicopter_landing_pad_8017E270(Task* task)
{
    GsCOORDINATE2* coord;
    SVECTOR        unusedA;
    VECTOR         unusedB;
    VECTOR         dir;
    MATRIX         unusedM;
    SVECTOR        unusedC;

    coord  = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[4];
    dir.vx = -0x249;
    dir.vy = 0;
    dir.vz = 0xB8;

    switch (task->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F7C = 0;
            break;
        case 1:
            Gfx_RotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C += 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C > 0x1000) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0x1000;
                task->state                                 = 2;
            }
            break;
        case 2:
            Gfx_RotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C -= 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C < 0) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0;
                task->state                                 = 0;
            }
            break;
    }
}

/// Message 0x13EE handler: copies the requested `GpSaveLoc` to `dst`. For a
/// warp into stage 0xF it consults the room's phase
/// (`D_acropolis_helicopter_landing_pad_80184D9C`): phase 0 queues sound
/// event 0x1E and refuses the warp (returns 1); phase 2 starts cap slot 9
/// first. `field_5` set skips the side effect either way.
s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 0xF) {
        if (D_acropolis_helicopter_landing_pad_80184D9C == 0) {
            if (src->field_5 == 0) {
                SndEvt_EnqueueType7(-1, 0x1E);
            }
            return 1;
        }
        if (D_acropolis_helicopter_landing_pad_80184D9C == 2) {
            if (src->field_5 == 0) {
                Gp_StartCapSlot(9, 1, 0);
            }
            return 0;
        }
        return 0;
    }
    return 1;
}

/// Does nothing and returns 0.
s32 func_acropolis_helicopter_landing_pad_8017E49C(void)
{
    return 0;
}

/// Slot-3 msg `0x3EF` handler. On kind 0, once the room session flag
/// `D_acropolis_helicopter_landing_pad_80184E0C` is up and the phase is
/// still 0, starts the helicopter sequence: flags the session, loads the
/// bank pair, moves to phase 1 and swaps the visible `GpObj4A` from element
/// 4 to element 0. Kind 1 latches `D_acropolis_helicopter_landing_pad_80187F84`.
s32 func_acropolis_helicopter_landing_pad_8017E4A4(Task* task, s32 msgId, GpMsg13EF* msg)
{
    u8       kind;
    GpObj4A* obj;
    GpObj4A* obj2;

    if ((msg->field_2 == 0) && (D_acropolis_helicopter_landing_pad_80184D9C == 0) && (D_acropolis_helicopter_landing_pad_80184E0C != 0)) {
        gGameSession->flowFlags = 0x82;
        D_80062735              = 1;
        func_800E8634((s32)&D_acropolis_helicopter_landing_pad_80183A34, 0, (s32)&D_acropolis_helicopter_landing_pad_80183FA4);
        D_acropolis_helicopter_landing_pad_80184D9C = 1;
        obj                                         = D_acropolis_helicopter_landing_pad_80185FAC;
        obj2                                        = obj + 4;
        obj->field_4A                              |= 0x40;
        obj2->field_4A                             &= 0xBF;
    }
    kind = msg->field_2;
    if (kind == 1) {
        D_acropolis_helicopter_landing_pad_80187F84 = kind;
    }
    return 0;
}

s32 func_acropolis_helicopter_landing_pad_8017E570(s32 arg0, s32 arg1, s32 arg2)
{
    if ((arg2 == 4) && (D_acropolis_helicopter_landing_pad_80184D9C == 2)) {
        Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 4, 0, 0);
    }
    return 0;
}

/// Spawns entry 3 of the room's task table.
void func_acropolis_helicopter_landing_pad_8017E5B8(void)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 3, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E5E8(void)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 1, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 2, 0, arg0 | (arg1 << 8));
}

void func_acropolis_helicopter_landing_pad_8017E64C(void)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 5, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E67C(void)
{
    if (D_acropolis_helicopter_landing_pad_80187F84 != 0) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&D_acropolis_helicopter_landing_pad_80184E50, 0);
    }
}

void func_acropolis_helicopter_landing_pad_8017E6C0(s32 arg0)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 6, arg0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E6F0(void)
{
    Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 0x1B);
    D_801153F1 = 3;
}

/// Pulses the gameplay state with `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.field_6`.
void func_acropolis_helicopter_landing_pad_8017E724(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_acropolis_helicopter_landing_pad_8017E75C(s32 arg0)
{
    D_acropolis_helicopter_landing_pad_80187F80->state = arg0;
}

void func_acropolis_helicopter_landing_pad_8017E76C(Task* task)
{
    s32 args[2];

    switch (task->state) {
        case 0:
            args[0] = 0xC;
            args[1] = 9;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F2, (s32)&D_acropolis_helicopter_landing_pad_801837E0, (s32)args);
            task->state++;
            break;
        case 1:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F0, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017E81C(Task* arg0)
{
    register s32 tmp asm("v0");
    register s32 hi asm("v1");
    s32          packed;
    s32          lo;
    s32          scaled;
    s32          val;

    packed = (s32)arg0->spawnArg2;
    lo     = packed & 0xFF;

    switch (arg0->state) {
        case 0:
            arg0->spawnArg1 = -lo;
            arg0->state++;
            break;
        case 1:
            if (lo < arg0->spawnArg1) {
                Display_ClampField126(0);
                taskKill(arg0);
            } else {
                tmp         = ABS(arg0->spawnArg1);
                hi          = lo - tmp;
                tmp         = packed >> 8;
                scaled      = hi * tmp;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                hi          = (u32)Gp_LcgState >> 16;
                hi          = scaled * hi;
                hi          = hi / lo;
                val         = hi >> 16;
                if (arg0->spawnArg1 & 1) {
                    val = ABS(val);
                    Display_ClampField126(val);
                } else {
                    tmp = ABS(val);
                    val = -tmp;
                    Display_ClampField126(val);
                }
                arg0->spawnArg1++;
            }
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017E974(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(4, 1, 0);
        case 2:
        case 3:
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_801156A8 == 1) {
                    Gp_MsgPlayerWeapon(1);
                    taskKill(task);
                    break;
                }
                Display_AcquireRef();
                task->state++;
            }
            task->state++;
            break;
        case 4:
            D_acropolis_helicopter_landing_pad_80184D9C = 3;
            Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 0, 0, 0);
            taskKill(task);
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017EA6C(Task* task)
{
    task->msgTable = D_acropolis_helicopter_landing_pad_80183710;
    Game_SetPtrSlot(task, 7);
    D_acropolis_helicopter_landing_pad_80187F84 = 0;
    D_acropolis_helicopter_landing_pad_80184E0C = 0;
    task->state++;
    func_800E8614((s32)&D_acropolis_helicopter_landing_pad_80183A04, 1);
    D_acropolis_helicopter_landing_pad_80187F80              = Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 7, 0, 0);
    D_acropolis_helicopter_landing_pad_80185FAC[0].field_4A &= 0xBF;
}

/// The room's script task: runs the state handler
/// `D_acropolis_helicopter_landing_pad_8017D5E4` names for `Task::state`,
/// through a copy of the table taken onto the stack.
void func_acropolis_helicopter_landing_pad_8017EB00(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_helicopter_landing_pad_8017D5E4;
    sp.funcs[task->state](task);
}
