#include "rooms/neo_ark_power_plant_1.h"

#include "types.h"

#include "neo_ark_power_plant_1_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// Main-executable globals with no module header yet, which
/// `func_neo_ark_power_plant_1_8017D5EC` tests and sets.

/// Area-record list applied when the power-on script starts.
extern GpAreaApplyRec D_neo_ark_power_plant_1_80181C00[];

static void func_neo_ark_power_plant_1_8017D5EC(Task* task);
static void func_neo_ark_power_plant_1_8017D928(Task* task);

/// State table of the room task: `func_neo_ark_power_plant_1_8017D928`
/// installs the message table, `func_neo_ark_power_plant_1_8017D5EC` runs the
/// plant every frame, and the last state kills the task.
static const TaskFuncTable3 D_neo_ark_power_plant_1_8017D5C4 = {
    { func_neo_ark_power_plant_1_8017D928, func_neo_ark_power_plant_1_8017D5EC, taskKill },
};

s32 D_neo_ark_power_plant_1_80181BB4[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

GpRoomParamRec D_neo_ark_power_plant_1_80181BC0[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_power_plant_1_80181BC8[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_neo_ark_power_plant_1_80181BD0[1] = {
    { 0, 0, 1, 0, D_neo_ark_power_plant_1_80181B9C },
};

GpRoomParamRec D_neo_ark_power_plant_1_80181BD8[1] = {
    { 0, 0, 1, 0, D_neo_ark_power_plant_1_80181BA8 },
};

GpRoomParamRec* D_neo_ark_power_plant_1_80181BE0[8] = {
    D_neo_ark_power_plant_1_80181BC0,
    D_neo_ark_power_plant_1_80181BC0,
    D_neo_ark_power_plant_1_80181BC0,
    D_neo_ark_power_plant_1_80181BC8,
    D_neo_ark_power_plant_1_80181BD0,
    D_neo_ark_power_plant_1_80181BD8,
    D_neo_ark_power_plant_1_80181BC0,
    D_neo_ark_power_plant_1_80181BC0,
};

GpAreaApplyRec D_neo_ark_power_plant_1_80181C00[2] = {
    { 5, 18, 2, 1 },
    { 255, 0, 0, 0 },
};

/// Second state of the room task, run every frame. While nibble 0xDE is clear
/// it sends message 0x7D6 to the slot-4 task, and when that returns 0 with
/// `Gp_StateC08.field_A` not 1 and `gDisplayState.pendingMode` clear, it sets nibbles 0xDE and 0xF6,
/// clears 0x1B2, applies `D_neo_ark_power_plant_1_80181C00`, sets
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` to 0x16 and starts the event script at
/// `D_neo_ark_power_plant_1_8017EB7C`. When `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` is 3 and nibble 0xFB
/// is clear, it sets 0xFB, clears `field_126` and `Gp_StateF0.prefix.bytes.field_0` and
/// starts the script at `D_neo_ark_power_plant_1_8017EEE4`. It re-arms the
/// countdown to 4 while `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` differs from the current view with 0xDE
/// set and 0xDF clear; otherwise it ticks the countdown down and, on reaching
/// 0, enqueues sound event 0x5511000A (as type 6 in view 7, type 7 elsewhere).
static void func_neo_ark_power_plant_1_8017D5EC(Task* task)
{
    Task* slot;

    if (GameFlag_GetNibble(0xDE) == 0) {
        slot = Gp_LookupSlot4(0);
        if (slot != 0) {
            if (Gp_DispatchMsg(slot, 0x7D6, 0, 0) == 0) {
                if (Gp_StateC08.field_A != 1) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        GameFlag_SetNibble(0xDE, 1);
                        GameFlag_SetNibble(0xF6, 1);
                        GameFlag_SetNibble(0x1B2, 0);
                        Gp_ApplyAreaRecs(D_neo_ark_power_plant_1_80181C00);
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x16;
                        func_800E8634(D_neo_ark_power_plant_1_8017EB7C, 0, D_neo_ark_power_plant_1_8017EDBC);
                    }
                }
            }
        }
    }
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 3) && (GameFlag_GetNibble(0xFB) == 0)) {
        GameFlag_SetNibble(0xFB, 1);
        gGameSession->battleResetPending = 0;
        Gp_StateF0.prefix.bytes.field_0  = 0;
        func_800E8614(D_neo_ark_power_plant_1_8017EEE4, 0);
    }
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) && (GameFlag_GetNibble(0xDE) != 0) && (GameFlag_GetNibble(0xDF) == 0)) {
        D_neo_ark_power_plant_1_8017F01C = 4;
        return;
    }
    if (D_neo_ark_power_plant_1_8017F01C != 0) {
        if (--D_neo_ark_power_plant_1_8017F01C == 0) {
            if (gGameSession->location.loc.view == 7) {
                SndEvt_EnqueueType6(0x5511000A, 0, 0);
                return;
            }
            SndEvt_EnqueueType7(0x5511000A, 1);
        }
    }
}

/// Handler the room's message table gives message 0x13F1: accepts it and
/// does nothing.
s32 func_neo_ark_power_plant_1_8017D7AC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `RoomEventMsg` onto the outgoing one and passes both on to `func_map_neo_ark_80179B14`.
/// Always returns 1.
s32 func_neo_ark_power_plant_1_8017D7B4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Handler the room's message table gives message 0x13F0: for `arg2` 2, 3, 9
/// or 12 runs `Gp_RunCapCmd1` with a command picked from that value and the
/// plant's flags; other values do nothing. Always returns 0.
s32 func_neo_ark_power_plant_1_8017D7F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 cmd;

    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0xDE) == 0) {
                cmd = 2;
            } else {
                cmd = 5;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 3:
            if (Gp_StateF0.prefix.bytes.field_0 == 2) {
                cmd = 7;
            } else if (GameFlag_GetNibble(0x148) != 0) {
                cmd = 6;
            } else {
                cmd = 3;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 9:
            if (GameFlag_GetNibble(0xDF) == 0) {
                cmd = 9;
            } else {
                cmd = 0xB;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 12:
            if (GameFlag_GetNibble(0xDF) != 0) {
                cmd = 0xA;
            } else {
                cmd = 0xC;
            }
            Gp_RunCapCmd1(cmd);
            break;
    }
    return 0;
}

/// Handler the room's message table gives message 0x13EF: accepts it and
/// does nothing.
s32 func_neo_ark_power_plant_1_8017D8C8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Native call in the power-on event script: requests all-effect cancellation on
/// `gRoomEffectState` and sets bit 0 of `Gp_StateC08.field_6`.
void func_neo_ark_power_plant_1_8017D8D0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

/// Native call in the power-on event script: halts the pad scripts.
void func_neo_ark_power_plant_1_8017D908(void)
{
    Gp_HaltPadScripts();
}

/// First state of the room task: installs the room's message table, registers
/// the task as pointer slot 7, sets `flowFlags` to 1 when the session's place
/// is 1 and, while nibble 0xFB is clear, sets `field_126` to 1 and
/// `Gp_StateF0.prefix.bytes.field_0` to 2. Then advances to the next state.
static void func_neo_ark_power_plant_1_8017D928(Task* task)
{
    task->msgTable = D_neo_ark_power_plant_1_8017EB18;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->location.loc.variant == 1) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
    }
    if (GameFlag_GetNibble(0xFB) == 0) {
        gGameSession->battleResetPending = 1;
        Gp_StateF0.prefix.bytes.field_0  = 2;
    }
    task->state = (s32)(task->state + 1);
}

/// Runs the room task's current state: the handler `Task::state` selects from
/// `D_neo_ark_power_plant_1_8017D5C4`, copied onto the stack before the call.
void func_neo_ark_power_plant_1_8017D9C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_power_plant_1_8017D5C4;
    sp.funcs[task->state](task);
}
