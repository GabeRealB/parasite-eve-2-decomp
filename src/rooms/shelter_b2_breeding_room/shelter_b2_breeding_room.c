#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Spawn argument of a model task whose visibility follows a 2-bit game flag;
/// `flagId` selects the flag.
typedef struct {
    u8 unk0[8];
    u8 flagId;
} _SpawnArg;

extern u8  D_80115598;
extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
s32        func_800E3FCC(s32 arg0);

/// Task table spawned by `func_shelter_b2_breeding_room_8017D6A4` once the
/// breeding-room script has run.
extern TaskDesc D_shelter_b2_breeding_room_80180444[];

/// Message table `func_shelter_b2_breeding_room_8017D7EC` installs on its task.
extern GpMsgEntry D_shelter_b2_breeding_room_80180414[];

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b2_breeding_room_8017D5F8(Task* task)
{
    TmdObject* obj = task->extra;

    if (Gp_GetCurBit2Flag(((_SpawnArg*)task->spawnArg2)->flagId) == 2) {
        obj->flags |= 0x80;
    } else {
        obj->flags &= ~0x80;
    }
}

s32 func_shelter_b2_breeding_room_8017D658(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both to `func_80179A04`, returning 1.
s32 func_shelter_b2_breeding_room_8017D660(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
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

s32 func_shelter_b2_breeding_room_8017D750(void)
{
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D758(s32 arg0, s32 arg1, s32 arg2)
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
void func_shelter_b2_breeding_room_8017D7EC(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_breeding_room_80180414;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_shelter_b2_breeding_room_8017D838(Task* task)
{
}

/// The room task's three states, dispatched by
/// `func_shelter_b2_breeding_room_8017D840`: install the message table, idle,
/// end.
const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4 = {
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
