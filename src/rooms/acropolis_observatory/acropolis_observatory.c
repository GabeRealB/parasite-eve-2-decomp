#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern s16 D_8007107A;

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_acropolis_observatory_8017E7B8[];

/// Set once the room task has spawned the streamed scene for this visit.
extern s32 D_acropolis_observatory_8017E7D8;

/// The room's task table: the two streamed-scene rides, then the fade-out and
/// fade-in tasks they spawn.
extern TaskDesc D_acropolis_observatory_8017E7DC[];

/// The observatory scene task's descriptor.
extern TaskDesc D_acropolis_observatory_8017FE6C;

/// Message gate for the observatory's two hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 9 (the telescope) and message 0xB (the door) both answer with a
/// `field_2` refusal code — 5 and 1 respectively — while the disc has no stream
/// file open (`D_8007107A < 0 || D_8006AC30.sector == 0`) or the message's
/// nibble is not in the state that lets it run once. The first pass through
/// each also advances that nibble, so the refusal only shows on later visits.
/// `field_5` non-zero means "report only", which suppresses both the nibble
/// writes and the refusals.
s32 func_acropolis_observatory_8017D618(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 answer;

    *out = *in;
    if (in->msgId == 9 && in->field_5 == 0) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            out->field_2 = 5;
        }
        if (GameFlag_GetNibble(0x25) == 0) {
            GameFlag_SetNibble(0x25, 1);
        } else {
            out->field_2 = 5;
        }
        if (in->msgId == 9) {
            if (GameFlag_GetNibble(9) & 1) {
                out->field_3 = 2;
            }
        }
    }
    if (in->msgId == 0xB) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            if (in->field_5 == 0) {
                out->field_2 = 1;
            }
        }
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(1) == 3) {
                GameFlag_SetNibble(1, 4);
            } else {
                out->field_2 = 1;
            }
        }
        if (in->msgId == 0xB && in->field_5 == 0) {
            answer = GameFlag_GetNibble(2);
            if (answer == 0) {
                answer = 1;
            } else {
                answer = 2;
            }
            out->field_3 = answer;
        }
    }
    return 1;
}

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_acropolis_observatory_8017D7BC(void)
{
    return 0;
}

/// Message gate for the observatory hotspot: sub-id 1 arms the room's task the
/// first time it fires during session phase 2, latching nibble 0xCA so a later
/// visit does nothing. The outgoing record is never written - this handler only
/// consumes the message.
s32 func_acropolis_observatory_8017D7C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if ((in->field_2 == 1) && (gGameSession->at4.loc.room == 2) && (GameFlag_GetNibble(0xCA) == 0)) {
        GameFlag_SetNibble(0xCA, 1);
        Task_SpawnFromTable(&D_acropolis_observatory_8017FE6C, 0, 0, 0);
    }
    return 0;
}

/// Observatory task entry: parks the overlay's message table in the task and
/// registers it as the room's slot-7 pointer. On the phase-2 visit that has not
/// yet latched nibble 0xCA it also arms the shared field-actor byte, then steps
/// the task on to its next state.
void func_acropolis_observatory_8017D834(Task* task)
{
    task->msgTable = D_acropolis_observatory_8017E7B8;
    Game_SetPtrSlot(task, 7);
    if ((gGameSession->at4.loc.room == 2) && (GameFlag_GetNibble(0xCA) == 0)) {
        Gp_StateF0.field_1A = 1;
    }
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: on a visit that arrived by warp 3 or 4 it
/// spawns the matching streamed-scene ride from the room's task table (entry 1
/// or 0), once per visit.
void func_acropolis_observatory_8017D8AC(Task* task)
{
    if ((D_acropolis_observatory_8017E7D8 == 0) && (gGameSession->at4.loc.warp == 3)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(D_acropolis_observatory_8017E7DC, 1, 0, 0);
    }
    if ((D_acropolis_observatory_8017E7D8 == 0) && (gGameSession->at4.loc.warp == 4)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(D_acropolis_observatory_8017E7DC, 0, 0, 0);
    }
}

/// The room task's three states.
const TaskFuncTable3 D_acropolis_observatory_8017D5C4 = {
    { func_acropolis_observatory_8017D834, func_acropolis_observatory_8017D8AC, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_acropolis_observatory_8017D5C4`.
void func_acropolis_observatory_8017D950(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_observatory_8017D5C4;
    sp.funcs[task->state](task);
}
