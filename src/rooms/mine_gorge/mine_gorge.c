#include "common.h"
#include "main/stage.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, installed by the room task's first state.
extern GpMsgEntry D_mine_gorge_8017E280[];

/// The cutscene task `func_mine_gorge_8017D5F8` spawns: one descriptor and a
/// terminator.
extern TaskDesc D_mine_gorge_8017E2B0[];

extern s32 D_mine_gorge_8017E2F0;
extern s32 D_mine_gorge_8017E500;
extern s32 D_mine_gorge_8017E610;

void func_mine_gorge_8017D8D4(Task* arg0);
void func_mine_gorge_8017D998(Task* task);

/// Answers message `0x13F1` with argument `0x11F`: while flag nibble `0xA4` is
/// clear and a pending `GpObj4C` of kind 5 with `field_48 == 0xFF` and a
/// non-zero `field_4B` is queued, raises the nibble, spawns the cutscene task
/// `D_mine_gorge_8017E2B0`, moves the session to room 2 with the HUD hidden and
/// the room objects dirty, and starts the session event. Returns 1 when it
/// did so, 0 otherwise.
s32 func_mine_gorge_8017D5F8(s32 arg0, s32 arg1, s32 arg2)
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
                gGameSession->at4.loc.room = (Mc_SaveData.at4.loc.room = 2);
                gGameSession->hideHud      = (gGameSession->roomObjsDirty = 1);
                gGameSession->eventState   = 1;
                return 1;
            }
        }
    }
    return 0;
}

/// Answers message `0x13EE`: copies the event message to `out` and passes both
/// to `func_80179A04`. A message of id 2 arriving while flag nibble `0xB5` is
/// clear and `field_5` is zero sets nibble `field_6` to 2, runs cap command 3
/// and returns 0; every other case returns 1, except that a set `field_5`
/// returns 0 without acting.
s32 func_mine_gorge_8017D6E8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 2) {
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
s32 func_mine_gorge_8017D77C(void)
{
    return 0;
}

/// Cutscene gate on the `0x13EF` direction message: when the payload's
/// direction byte is 1, flag nibble `0xC5` is still clear and the session is in
/// place 1, raises the nibble and starts the script blob at
/// `D_mine_gorge_8017E610`.
s32 func_mine_gorge_8017D784(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1 && GameFlag_GetNibble(0xC5) == 0 && gGameSession->at4.loc.place == temp_s0) {
        GameFlag_SetNibble(0xC5, 1);
        func_800E8614((s32)&D_mine_gorge_8017E610, 0);
    }
    return 0;
}

/// Answers message `0x13F2`: argument `0xA` queues event sound `0x5405000A`.
s32 func_mine_gorge_8017D7F4(s32 arg0, s32 arg1, s32 arg2)
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
        func_800E8634((s32)&D_mine_gorge_8017E2F0, 0, (s32)&D_mine_gorge_8017E500);
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
void func_mine_gorge_8017D8D4(Task* arg0)
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
void func_mine_gorge_8017D998(Task* task)
{
}

/// State handlers of the room task `func_mine_gorge_8017D9A0` runs: the room's
/// setup, an idle state, and `taskKill`.
const TaskFuncTable3 D_mine_gorge_8017D5C4 = {
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
