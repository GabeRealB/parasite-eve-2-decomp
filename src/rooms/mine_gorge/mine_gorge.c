#include "rooms/mine_gorge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

void func_mine_gorge_8017D828(Task* arg0);

/// The room's message table, installed by the room task's first state.
extern GpMsgEntry D_mine_gorge_8017E280[];

/// The cutscene task `func_mine_gorge_8017D5F8` spawns: one descriptor and a
/// terminator.
extern TaskDesc D_mine_gorge_8017E2B0[];

extern GpEvsCmd D_mine_gorge_8017E2F0[];
extern GpEvsCmd D_mine_gorge_8017E500[];
extern GpEvsCmd D_mine_gorge_8017E610[];

static void func_mine_gorge_8017D8D4(Task* arg0);
static void func_mine_gorge_8017D998(Task* task);

s32 func_mine_gorge_8017D5F8(Task*, s32, s32, GpMessageArg);
s32 func_mine_gorge_8017D6E8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_mine_gorge_8017D77C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_mine_gorge_8017D784(Task*, s32, GpMsg13EF*, GpMessageArg);
s32 func_mine_gorge_8017D7F4(Task*, s32, s32, GpMessageArg);

void func_mine_gorge_8017D8BC(u8);

extern GpAnimSet D_mine_gorge_8017E258;

extern GpAnimArg D_mine_gorge_8017E2DC;
extern GpAnimArg D_mine_gorge_8017E5E8;
extern GpCopyArg D_mine_gorge_8017E5E0;
void             func_mine_gorge_8017D8C8(s32);

AnimationPackedPose D_mine_gorge_8017DE10[10] = {
#include "assets/mine_gorge_animation_00C98_bank1.inc"
};

AnimationPackedRotation D_mine_gorge_8017DE88[95] = {
#include "assets/mine_gorge_animation_00C98_bank4.inc"
};

GpAnimRec D_mine_gorge_8017E004[139] = {
#include "assets/mine_gorge_animation_00C98_records.inc"
};

u16 D_mine_gorge_8017E230[20] = {
#include "assets/mine_gorge_animation_00C98_indices.inc"

};

GpAnimSet D_mine_gorge_8017E258 = {
    D_mine_gorge_8017E004,
    D_mine_gorge_8017E230,
    { NULL, D_mine_gorge_8017DE10, NULL, NULL, D_mine_gorge_8017DE88, NULL, NULL, NULL },
};

GpMsgEntry D_mine_gorge_8017E280[6] = {
    { 5102, func_mine_gorge_8017D6E8 },
    { 5105, func_mine_gorge_8017D5F8 },
    { 5103, func_mine_gorge_8017D784 },
    { 5104, func_mine_gorge_8017D77C },
    { 5106, func_mine_gorge_8017D7F4 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_mine_gorge_8017E2B0[2] = {
    { 0, 32, func_mine_gorge_8017D828, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpAnimArg D_mine_gorge_8017E2C8 = { { .index = 1 }, 24, 0, 0, 0 };

GpAnimArg D_mine_gorge_8017E2DC = { { .index = 1 }, 1, 1, 10, 0 };

GpEvsCmd D_mine_gorge_8017E2F0[22] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_mine_gorge_8017D8BC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54050009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2C8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_mine_gorge_8017E500[9] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpAnimSet* D_mine_gorge_8017E5D8[2] = {
    &D_mine_gorge_8017E258,
    NULL,
};

GpCopyArg D_mine_gorge_8017E5E0 = { { .sets = D_mine_gorge_8017E5D8 }, 2 };

GpAnimArg D_mine_gorge_8017E5E8 = { { .index = 1 }, 47, 1, 7, 1 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_mine_gorge_8017E5FC = { { .index = 1 }, 48, 1, 7, 1 };

GpEvsCmd D_mine_gorge_8017E610[14] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_gorge_8017E5E0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E2DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x54050007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_mine_gorge_8017D8C8 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_gorge_8017E5E8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Answers message `0x13F1` with argument `0x11F`: while flag nibble `0xA4` is
/// clear and a pending `GpObj4C` of kind 5 with `field_48 == 0xFF` and a
/// non-zero `field_4B` is queued, raises the nibble, spawns the cutscene task
/// `D_mine_gorge_8017E2B0`, moves the session to room 2 with the HUD hidden and
/// the room objects dirty, and starts the session event. Returns 1 when it
/// did so, 0 otherwise.
s32 func_mine_gorge_8017D5F8(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x11F) {
        if (GameFlag_GetNibble(0xA4) == 0) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                GameFlag_SetNibble(0xA4, 1);
                Task_SpawnOnDefaultList(D_mine_gorge_8017E2B0, 0, 0, 0);
                gGameSession->at4.loc.room = (Mc_SaveData[0].state.at4.loc.room = 2);
                gGameSession->hideHud      = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState   = 1;
                return 1;
            }
        }
    }
    return 0;
}

/// Answers message `0x13EE`: copies the event message to `out` and passes both
/// to `func_map_shelter_80179A04`. A message of id 2 arriving while flag nibble `0xB5` is
/// clear and `field_5` is zero sets nibble `field_6` to 2, runs cap command 3
/// and returns 0; every other case returns 1, except that a set `field_5`
/// returns 0 without acting.
s32 func_mine_gorge_8017D6E8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed != 2) {
        return 1;
    }
    if (GameFlag_GetNibble(0xB5) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(3);
    return 0;
}

/// Answers message `0x13F0` by doing nothing.
s32 func_mine_gorge_8017D77C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Cutscene gate on the `0x13EF` direction message: when the payload's
/// direction byte is 1, flag nibble `0xC5` is still clear and the session is in
/// place 1, raises the nibble and starts the script blob at
/// `D_mine_gorge_8017E610`.
s32 func_mine_gorge_8017D784(Task* task, s32 msgId, GpMsg13EF* arg2, GpMessageArg arg3)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1 && GameFlag_GetNibble(0xC5) == 0 && gGameSession->at4.loc.place == temp_s0) {
        GameFlag_SetNibble(0xC5, 1);
        func_800E8614(D_mine_gorge_8017E610, 0);
    }
    return 0;
}

/// Answers message `0x13F2`: argument `0xA` queues event sound `0x5405000A`.
s32 func_mine_gorge_8017D7F4(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 0xA) {
        SndEvt_EnqueueType6(0x54050000 | arg2, 0, 0);
    }
    return 0;
}

/// The cutscene task: the first pass raises `D_80115768` and the session's
/// `hideHud`, hides the display and starts the script blob pair
/// `D_mine_gorge_8017E2F0` / `D_mine_gorge_8017E500`; the next pass kills the
/// task and clears collection bit `0x11F`.
void func_mine_gorge_8017D828(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115768 = 1;
        SetDispMask(0);
        gGameSession->hideHud = 1;
        func_800E8634(D_mine_gorge_8017E2F0, 0, D_mine_gorge_8017E500);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x11F);
    }
    arg0->state = arg0->state + 1;
}

/// Script callback: stores its argument in `D_80115768`.
void func_mine_gorge_8017D8BC(u8 arg0)
{
    D_80115768 = arg0;
}

/// Script callback: stores its argument in `Gp_StateF0.field_1A`.
void func_mine_gorge_8017D8C8(s32 arg0)
{
    Gp_StateF0.field_1A = arg0;
}

/// Room task setup state: installs the message table and pointer slot 7, sets
/// `Gp_StateF0.field_1A` to `0x15` in place 1 once flag nibble `0xC5` is set, and on the
/// first pass with flag nibble `0xBE == 2` arms nibble `0x166`, clears nibble
/// `0xB5` and calls `Gp_SpawnIfCapIdle(8, 0)`. Then selects scene music entry 1 and
/// advances state.
static void func_mine_gorge_8017D8D4(Task* arg0)
{
    arg0->msgTable = D_mine_gorge_8017E280;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xC5) != 0)) {
        Gp_StateF0.field_1A = 0x15;
    }
    if ((GameFlag_GetNibble(0xBE) == 2) && (GameFlag_GetNibble(0x166) == 0)) {
        GameFlag_SetNibble(0x166, 1);
        GameFlag_SetNibble(0xB5, 0);
        Gp_SpawnIfCapIdle(8, 0);
    }
    arg0->state           = arg0->state + 1;
    gStageSceneMusicEntry = 1;
}

/// The room task's idle state.
static void func_mine_gorge_8017D998(Task* task)
{
}

/// State handlers of the room task `func_mine_gorge_8017D9A0` runs: the room's
/// setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_mine_gorge_8017D5C4 = {
    { func_mine_gorge_8017D8D4, func_mine_gorge_8017D998, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_mine_gorge_8017D5C4`, copying the table onto the stack and calling the
/// entry for the task's current state.
void func_mine_gorge_8017D9A0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_gorge_8017D5C4;
    sp.funcs[task->state](task);
}
