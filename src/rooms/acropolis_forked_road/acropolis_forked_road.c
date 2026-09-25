#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room's message table, installed on its entry task.
extern GpMsgEntry D_acropolis_forked_road_80180F14[];

extern s32      D_acropolis_forked_road_80180F3C;
extern TaskDesc D_acropolis_forked_road_80180F44;
extern s32      D_acropolis_forked_road_801820B8;

void func_acropolis_forked_road_8017D92C(Task* task);
void func_acropolis_forked_road_8017D970(Task* task);

/// State handlers of the room's own task.
const TaskFuncTable3 D_acropolis_forked_road_8017D5C4 = {
    { func_acropolis_forked_road_8017D92C, func_acropolis_forked_road_8017D970, taskKill }
};

/// Message gate for the forked road's two hotspots: copies the incoming record
/// to the outgoing one, then answers according to the message id and the
/// game's progress nibbles. `field_5` non-zero means "report only", so every
/// side effect below is skipped while the answer stays the same.
///
/// Message 8 (the path back down) marks itself with `field_3 = 2` once nibble 9
/// has bit 1 set, then either plays capture slot 0 while nibble 0 is still
/// under 3 or, past that, plays slot 3 once and records it in nibble 0x13.
///
/// Message 0xA (the path on) runs capture command 2 while nibble 1 is under 2.
/// Once it is at 2 the forked-road cutscene spawns from
/// `D_acropolis_forked_road_80180F44` and nibble 1 advances to 3, unless no
/// stream file is open (`gDisplayState.field_112 < 0 || D_8006AC30.sector == 0`), in which
/// case the message is refused with `field_2 = 2`.
s32 func_acropolis_forked_road_8017D5EC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 8) {
        if ((GameFlag_GetNibble(9) & 2) && (in->field_5 == 0)) {
            out->field_3 = 2;
        }
        if (in->msgId == 8) {
            if (GameFlag_GetNibble(0) < 3) {
                if (in->field_5 == 0) {
                    Gp_SetNibbleIf(in->field_6, 2);
                    Gp_StartCapSlot(1, 1, 0);
                }
                return 0;
            }
            if (GameFlag_GetNibble(0) >= 3) {
                if (GameFlag_GetNibble(0x13) == 0) {
                    if (in->field_5 == 0) {
                        Gp_StartCapSlot(1, 1, 3);
                        GameFlag_SetNibble(0x13, 1);
                    }
                }
                return 1;
            }
        }
    }
    if (in->msgId == 0xA) {
        if (GameFlag_GetNibble(1) < 2) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        if ((gDisplayState.field_112 < 0) || (D_8006AC30.sector == 0)) {
            if (in->field_5 != 0) {
                return 1;
            }
            out->field_2 = 2;
        } else if (GameFlag_GetNibble(1) == 2) {
            if (in->field_5 == 0) {
                Mc_SaveData.at4.loc.view = 7;
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 0, 0, 0);
                GameFlag_SetNibble(1, 3);
            }
            return 0;
        } else {
            out->field_2 = 2;
        }
        if (in->field_5 != 0) {
            return 1;
        }
        if ((GameFlag_GetNibble(9) & 2) == 0) {
            return 1;
        }
        if (GameFlag_GetNibble(0xCA) != 0) {
            return 1;
        }
        out->field_3 = 2;
        return 1;
    }
    return 1;
}

/// Room script callback with nothing to do: always answers 0.
s32 func_acropolis_forked_road_8017D850(void)
{
    return 0;
}

s32 func_acropolis_forked_road_8017D858(s32 arg0, s32 arg1, s32 arg2)
{
    s32 cmd;

    if (arg2 == 3) {
        if ((Gp_GetCurBit2Flag(0x18) == 0) || (Gp_GetCurBit2Flag(0x18) == 1)) {
            cmd = 3;
        } else {
            cmd = 4;
        }
        Gp_RunCapCmd1(cmd);
    }
    return 0;
}

s32 func_acropolis_forked_road_8017D8A8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp;

    if (arg2->field_2 == 1 && (GameFlag_GetNibble(9) & 2)) {
        temp = gGameSession->at4.loc.place;
        if (((temp == 4) || (temp == 8)) && (GameFlag_GetNibble(0xCC) == 0)) {
            func_800E8614((s32)&D_acropolis_forked_road_801820B8, 1);
            GameFlag_SetNibble(0xCC, 1);
        }
    }
    return 1;
}

/// First state of the room's own task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
void func_acropolis_forked_road_8017D92C(Task* task)
{
    task->msgTable = D_acropolis_forked_road_80180F14;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room's own task: the first frame the session's warp
/// id is 2, spawns entry 2 of the room's task table, latching
/// `D_acropolis_forked_road_80180F3C` so that happens only once.
void func_acropolis_forked_road_8017D970(Task* task)
{
    if ((D_acropolis_forked_road_80180F3C == 0) && (gGameSession->at4.loc.warp == 2)) {
        D_acropolis_forked_road_80180F3C = 1;
        Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 2, 0, 0);
    }
}

/// Runs the room task's current state out of its three-entry handler table:
/// the setup state, the per-frame warp check, then `taskKill`. The table is
/// copied onto the stack before the call.
void func_acropolis_forked_road_8017D9CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_forked_road_8017D5C4;
    sp.funcs[task->state](task);
}
