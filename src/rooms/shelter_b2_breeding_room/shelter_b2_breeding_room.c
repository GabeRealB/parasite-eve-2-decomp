#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

/// Task table spawned by `func_shelter_b2_breeding_room_8017D6A4` once the
/// breeding-room script has run.
extern TaskDesc D_shelter_b2_breeding_room_80180444[];

/// Message table `func_shelter_b2_breeding_room_8017D7EC` installs on its task.
extern GpMsgEntry D_shelter_b2_breeding_room_80180414[];

s32  func_shelter_b2_breeding_room_8017D658(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_breeding_room_8017D660(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_breeding_room_8017D6A4(Task*, s32, s32, s32);
s32  func_shelter_b2_breeding_room_8017D750(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_breeding_room_8017D758(Task*, s32, s32, GpMessageArg);
void func_shelter_b2_breeding_room_8017D7A8(Task*);

extern SVECTOR D_shelter_b2_breeding_room_801803A4[4];
extern TmdBone D_shelter_b2_breeding_room_8018037C[1];
extern u32     D_shelter_b2_breeding_room_801803A0[1];
extern u32     D_shelter_b2_breeding_room_801803C4[11];

TmdBone D_shelter_b2_breeding_room_8018037C[1] = {
#include "assets/shelter_b2_breeding_room_model_02E30_skeleton.inc"
};

u32 D_shelter_b2_breeding_room_801803A0[1] = {
#include "assets/shelter_b2_breeding_room_model_02E30_partVerts.inc"
};

SVECTOR D_shelter_b2_breeding_room_801803A4[4] = {
#include "assets/shelter_b2_breeding_room_model_02E30_verts.inc"
};

u32 D_shelter_b2_breeding_room_801803C4[11] = {
#include "assets/shelter_b2_breeding_room_model_02E30_stream.inc"
};

TmdSource D_shelter_b2_breeding_room_801803F0 = { 0, 40, 0, 1, D_shelter_b2_breeding_room_801803A0, D_shelter_b2_breeding_room_801803A4, &D_shelter_b2_breeding_room_801803A4[4], D_shelter_b2_breeding_room_8018037C, D_shelter_b2_breeding_room_801803C4 };

GpMsgEntry D_shelter_b2_breeding_room_80180414[6] = {
    { 5102, func_shelter_b2_breeding_room_8017D660 },
    { 5105, func_shelter_b2_breeding_room_8017D658 },
    { 5103, func_shelter_b2_breeding_room_8017D750 },
    { 5104, func_shelter_b2_breeding_room_8017D6A4 },
    { 5106, func_shelter_b2_breeding_room_8017D758 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b2_breeding_room_80180444[1] = {
    { 0, 32, func_shelter_b2_breeding_room_8017D7A8, { .model = NULL } },
};

static void func_shelter_b2_breeding_room_8017D7EC(Task* arg0);
static void func_shelter_b2_breeding_room_8017D838(Task* task);

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b2_breeding_room_8017D5F8(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (Gp_GetCurBit2Flag(((RoomFlagModelArg*)task->spawnArg2.pointer)->flagId) == 2) {
        obj->flags |= 0x80;
    } else {
        obj->flags &= ~0x80;
    }
}

s32 func_shelter_b2_breeding_room_8017D658(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both to `func_map_shelter_80179A04`, returning 1.
s32 func_shelter_b2_breeding_room_8017D660(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// Handler for msg `0x16`: the first entry into the breeding room. Runs the
/// scripted scene once, then replays cap script `0x16` on later visits.
s32 func_shelter_b2_breeding_room_8017D6A4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 0x16) {
        if (GameFlag_GetNibble(0x120) != 0) {
            Gp_RunCapCmd1(0x16);
        } else {
            GameFlag_SetNibble(0x120, 1);
            if (func_800E3FCC(0xA2) == 0x1E) {
                func_800E3FAC(0xA2, 0x1F);
            }
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            Task_SpawnFromTable(D_shelter_b2_breeding_room_80180444, 0, 0, 0);
        }
    }
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D750(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D758(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 7:
            SndEvt_EnqueueType6(0x54200007, 0, 0);
            break;
        case 0x68:
            SndEvt_EnqueueType6(0x54200008, 0, 0);
            break;
    }
    return 0;
}

void func_shelter_b2_breeding_room_8017D7A8(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        Gp_ResetCap();
        Gp_MsgPlayerWeapon(1);
        taskKill(arg0);
    }
}

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7, sets `D_80115598` and advances to the next state.
static void func_shelter_b2_breeding_room_8017D7EC(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_breeding_room_80180414;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

static void func_shelter_b2_breeding_room_8017D838(Task* task)
{
}

/// The room task's three states, dispatched by
/// `func_shelter_b2_breeding_room_8017D840`: install the message table, idle,
/// end.
static const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4 = {
    { func_shelter_b2_breeding_room_8017D7EC, func_shelter_b2_breeding_room_8017D838, taskKill }
};

/// Runs the handler for the task's state from the room's three-entry state
/// table, copied onto the stack first.
void func_shelter_b2_breeding_room_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_breeding_room_8017D5C4;
    sp.funcs[task->state](task);
}
