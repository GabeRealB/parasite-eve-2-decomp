#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void func_dryfield_underpass_8017D5D0(Task* task)
{
    GpSaveLoc    src;
    GpSaveLoc    dst;
    GpSaveLoc*   s;
    GpSaveLoc*   d;
    GameSession* session;
    s32          flag;
    s32          state;
    s32          arg;
    u8           room;

    flag  = task->spawnArg1;
    state = task->state;
    arg   = (s32)task->spawnArg2;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
                if (flag == 0x51) {
                    d           = &dst;
                    s           = &src;
                    *(u16*)&src = 0x26;
                    src.field_5 = 0;
                    if (s->field_5 == 0) {
                        if (GameFlag_GetNibble(0xC9) != 0) {
                            if (GameFlag_GetNibble(0x53) != 0) {
                                d->field_3 = 2;
                            } else {
                                d->field_3 = 1;
                            }
                            if (GameFlag_GetNibble(0x51) == 0) {
                                dst.field_3 = dst.field_3 + 2;
                            }
                        } else {
                            if (GameFlag_GetNibble(0x51) != 0) {
                                d->field_3 = 5;
                            } else {
                                d->field_3 = 6;
                            }
                        }
                    }
                    session                  = gGameSession;
                    room                     = dst.field_3;
                    session->at4.loc.room    = room;
                    Mc_SaveData.at4.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->at4.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13EE: copies the incoming record onto the outgoing one
/// and, unless the query is report-only (`field_5` set), answers record id 0x20
/// with 1 or 2 from nibble 0x51, raised by 2 while nibble 0x53 is set, and
/// record id 0x22 with 1 or 2 from nibble 0x52. Always returns 1.
s32 func_dryfield_underpass_8017D788(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 val;

    *out = *in;
    USE_REG(out);
    USE_REG(out);
    if (in->msgId == 0x20 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x51);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    if (in->msgId == 0x22 && in->field_5 == 0) {
        val = GameFlag_GetNibble(0x52);
        if (val == 0) {
            val = 2;
        } else {
            val = 1;
        }
        out->field_3 = val;
    }
    return 1;
}
