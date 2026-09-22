#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

void Room_Script01(Task* task)
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

/// Message 0x20 / 0x22: copies the incoming record onto the outgoing one and,
/// when not a report-only query (`field_5 == 0`), answers in `field_3` from
/// nibbles 0x51 / 0x53 / 0x52. Always returns 1 (not consumed).
s32 Room_Script06(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
