#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// Main-executable globals with no module header yet, which
/// `func_neo_ark_power_plant_1_8017D5EC` tests and sets.

/// Script blobs in the overlay's `.data`, handed to `func_800E8634` /
/// `func_800E8614` (which forward them to `Task_Spawn`) as raw addresses.
extern s32 D_neo_ark_power_plant_1_8017EB7C;
extern s32 D_neo_ark_power_plant_1_8017EDBC;
extern s32 D_neo_ark_power_plant_1_8017EEE4;

/// Countdown `func_neo_ark_power_plant_1_8017D5EC` arms to 4 and ticks down;
/// reaching 0 enqueues a sound event.
extern s32 D_neo_ark_power_plant_1_8017F01C;

/// Area-record list applied when the power-on script starts.
extern GpAreaApplyRec D_neo_ark_power_plant_1_80181C00[];

/// The room's own `GpMsgEntry[]` - the message table this task publishes.
extern GpMsgEntry D_neo_ark_power_plant_1_8017EB18[];

void func_neo_ark_power_plant_1_8017D5EC(Task* task);
void func_neo_ark_power_plant_1_8017D928(Task* task);

/// State table of the room task: `func_neo_ark_power_plant_1_8017D928`
/// installs the message table, `func_neo_ark_power_plant_1_8017D5EC` runs the
/// plant every frame, and the last state kills the task.
const TaskFuncTable3 D_neo_ark_power_plant_1_8017D5C4 = {
    { func_neo_ark_power_plant_1_8017D928, func_neo_ark_power_plant_1_8017D5EC, taskKill },
};

/// Second state of the room task, run every frame. While nibble 0xDE is clear
/// it sends message 0x7D6 to the slot-4 task, and when that returns 0 with
/// `Gp_StateC08.field_A` not 1 and `gDisplayState.pendingMode` clear, it sets nibbles 0xDE and 0xF6,
/// clears 0x1B2, applies `D_neo_ark_power_plant_1_80181C00`, sets
/// `Mc_SaveData.sceneEvent` to 0x16 and starts the event script at
/// `D_neo_ark_power_plant_1_8017EB7C`. When `Mc_SaveData.at4.loc.view` is 3 and nibble 0xFB
/// is clear, it sets 0xFB, clears `field_126` and `Gp_StateF0.field_0` and
/// starts the script at `D_neo_ark_power_plant_1_8017EEE4`. It re-arms the
/// countdown to 4 while `Mc_SaveData.at4.loc.view` differs from the current view with 0xDE
/// set and 0xDF clear; otherwise it ticks the countdown down and, on reaching
/// 0, enqueues sound event 0x5511000A (as type 6 in view 7, type 7 elsewhere).
void func_neo_ark_power_plant_1_8017D5EC(Task* task)
{
    Task* slot;

    if (GameFlag_GetNibble(0xDE) == 0) {
        slot = (Task*)Gp_LookupSlot4(0);
        if (slot != 0) {
            if (Gp_DispatchMsg(slot, 0x7D6, 0, 0) == 0) {
                if (Gp_StateC08.field_A != 1) {
                    if (gDisplayState.pendingMode == 0) {
                        GameFlag_SetNibble(0xDE, 1);
                        GameFlag_SetNibble(0xF6, 1);
                        GameFlag_SetNibble(0x1B2, 0);
                        Gp_ApplyAreaRecs(D_neo_ark_power_plant_1_80181C00);
                        Mc_SaveData.sceneEvent = 0x16;
                        func_800E8634((s32)&D_neo_ark_power_plant_1_8017EB7C, 0, (s32)&D_neo_ark_power_plant_1_8017EDBC);
                    }
                }
            }
        }
    }
    if ((Mc_SaveData.at4.loc.view == 3) && (GameFlag_GetNibble(0xFB) == 0)) {
        GameFlag_SetNibble(0xFB, 1);
        gGameSession->field_126 = 0;
        Gp_StateF0.field_0      = 0;
        func_800E8614((s32)&D_neo_ark_power_plant_1_8017EEE4, 0);
    }
    if ((Mc_SaveData.at4.loc.view != gGameSession->at4.loc.view) && (GameFlag_GetNibble(0xDE) != 0) && (GameFlag_GetNibble(0xDF) == 0)) {
        D_neo_ark_power_plant_1_8017F01C = 4;
        return;
    }
    if (D_neo_ark_power_plant_1_8017F01C != 0) {
        if (--D_neo_ark_power_plant_1_8017F01C == 0) {
            if (gGameSession->at4.loc.view == 7) {
                SndEvt_EnqueueType6(0x5511000A, 0, 0);
                return;
            }
            SndEvt_EnqueueType7(0x5511000A, 1);
        }
    }
}

/// Handler the room's message table gives message 0x13F1: accepts it and
/// does nothing.
s32 func_neo_ark_power_plant_1_8017D7AC(void)
{
    return 0;
}

/// Handler the room's message table gives message 0x13EE: copies the incoming
/// `GpSaveLoc` onto the outgoing one and passes both on to `func_80179B14`.
/// Always returns 1.
s32 func_neo_ark_power_plant_1_8017D7B4(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
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
            if (Gp_StateF0.field_0 == 2) {
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
s32 func_neo_ark_power_plant_1_8017D8C8(void)
{
    return 0;
}

/// Native call in the power-on event script: pulses `Gp_State1C` and sets bit
/// 0 of `Gp_StateC08.field_6`.
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
/// `Gp_StateF0.field_0` to 2. Then advances to the next state.
void func_neo_ark_power_plant_1_8017D928(Task* task)
{
    task->msgTable = D_neo_ark_power_plant_1_8017EB18;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 1) {
        gGameSession->flowFlags = 1;
    }
    if (GameFlag_GetNibble(0xFB) == 0) {
        gGameSession->field_126 = 1;
        Gp_StateF0.field_0      = 2;
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
