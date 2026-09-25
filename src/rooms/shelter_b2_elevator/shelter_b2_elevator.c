#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Per-task state of an elevator car: its travel, kept within 0..500.
typedef struct {
    s32 travel;
} ShelterElevatorCar;

extern s8  D_8007218B;
extern s16 D_80071076;
extern s32 D_801378D0;
extern s32 D_801380F8;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_shelter_b2_elevator_8017DFA0[];

/// The room's spawnable tasks: two elevator cars, then the exit task.
extern TaskDesc D_shelter_b2_elevator_8017DF70[];

/// The two elevator-car tasks the room entry task spawns.
extern Task* D_shelter_b2_elevator_8017EA00[];

void func_shelter_b2_elevator_8017DB08(Task* task);

/// The room entry task's first state: installs the room's message table, takes
/// pointer slot 7 and spawns the two elevator cars. Unless the byte
/// `D_8007218B` is 9, it then either runs the first-visit sequence, setting
/// event nibble 0xCF, or on a later visit hides the HUD, spawns the exit task
/// and runs CAP command 3.
void func_shelter_b2_elevator_8017D5E8(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_8017DFA0;
    Game_SetPtrSlot(task, 7);
    D_shelter_b2_elevator_8017EA00[0] = Task_SpawnFromTable(D_shelter_b2_elevator_8017DF70, 0, 0, -1);
    D_shelter_b2_elevator_8017EA00[1] = Task_SpawnFromTable(D_shelter_b2_elevator_8017DF70, 1, 0, 1);
    if (D_8007218B != 9) {
        if (GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0xCF, 1);
            func_800E8634((s32)&D_801378D0, 0, (s32)&D_801380F8);
            func_800E3FAC(0xA2, 0x24);
        } else {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Task_SpawnFromTable(D_shelter_b2_elevator_8017DF70, 2, 0, 0);
            Gp_RunCapCmd(3, 0);
        }
    }
    task->state++;
}

/// An elevator car's task. The first frame allocates its state and places the
/// model; every later frame adds `spawnArg1` * 10 to the travel, clamps it to
/// 0..500, sets the model's z from the travel times `spawnArg2`, and submits
/// the model, with object flag 0x80 set except in camera view 2.
void func_shelter_b2_elevator_8017D70C(Task* task)
{
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    ShelterElevatorCar* car;
    VECTOR              vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    switch (task->state) {
        case 0:
            car = memCalloc(4, 0);
            if (car == NULL) {
                taskKill(task);
                return;
            }
            task->work        = (TaskIdMap*)car;
            car->travel       = 0;
            obj->otOffset     = 0x64;
            obj->flags        = 0;
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = 0x2A94;
            coord->coord.t[1] = 0;
            coord->coord.t[2] = -0x1F4;
            coord->flg        = 0;
            task->state++;
            break;
        case 1:
            car         = (ShelterElevatorCar*)task->work;
            car->travel = car->travel + task->spawnArg1 * 10;
            if (car->travel < 0) {
                car->travel = 0;
            }
            if (car->travel >= 0x1F5) {
                car->travel = 0x1F4;
            }
            coord->coord.t[2] = car->travel * (s32)task->spawnArg2 - 0x1F4;
            if (gGameSession->at4.loc.view == 2) {
                obj->flags = 0;
            } else {
                obj->flags = 0x80;
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            func_800D7A9C(obj, &vec, 0, 3);
            break;
    }
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_shelter_b2_elevator_8017D5C4 = {
    { func_shelter_b2_elevator_8017D5E8, func_shelter_b2_elevator_8017DB08, taskKill },
};

/// The exit task. After 21 frames and once the CAP script is idle, it sets the
/// destination area and warp from the event key the script chose (0xB, 0xC or
/// 0xD), then resolves the destination through `func_80179A04`, spawns task
/// 0x11 and ends.
void func_shelter_b2_elevator_8017D888(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            if (task->killCountdown >= 0x15) {
                task->state++;
            }
            task->killCountdown = task->killCountdown + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData.at4.loc.area = 9;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData.at4.loc.area = 0x1B;
                    Mc_SaveData.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData.at4.loc.area = 0x2A;
                    Mc_SaveData.at4.loc.warp = 3;
                    break;
            }
            task->state++;
            break;
        case 3:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_5 = 0;
            msg.msgId   = Mc_SaveData.at4.loc.area;
            msg.field_2 = Mc_SaveData.at4.loc.warp;
            msg.field_3 = Mc_SaveData.at4.loc.room;
            msg2        = msg;
            func_80179A04(&msg, &msg2);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.warp = msg2.field_2;
            Mc_SaveData.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message-table handler for message 0x13F1. Does nothing.
s32 func_shelter_b2_elevator_8017DA5C(void)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both to `func_80179A04`. Always returns 1.
s32 func_shelter_b2_elevator_8017DA64(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0. Does nothing.
s32 func_shelter_b2_elevator_8017DAA8(void)
{
    return 0;
}

/// Message-table handler for message 0x13EF. Does nothing.
s32 func_shelter_b2_elevator_8017DAB0(void)
{
    return 0;
}

/// Message-table handler for message 0x13EC: sets `spawnArg1` of both elevator
/// cars to 1.
s32 func_shelter_b2_elevator_8017DAB8(void)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1 = 1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1 = 1;
    return 0;
}

/// Message-table handler for message 0x13ED: sets `spawnArg1` of both elevator
/// cars to -1.
s32 func_shelter_b2_elevator_8017DAE0(void)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1 = -1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1 = -1;
    return 0;
}

/// The room entry task's idle state.
void func_shelter_b2_elevator_8017DB08(Task* task)
{
    char pad[0x10];
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_shelter_b2_elevator_8017DB18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_elevator_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b2_elevator_8017DB70(void)
{
}
