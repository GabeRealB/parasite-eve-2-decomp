#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern TaskDesc D_dryfield_junk_yard_8017DD48[];
extern s32      D_dryfield_junk_yard_8017E160;
extern s32      D_dryfield_junk_yard_8017E2B0;
extern s32      D_dryfield_junk_yard_8017E3D0;

/// Message gate for the yard's junk pile, the same shape as the other rooms'
/// gates: it copies the incoming record onto the outgoing one and then answers
/// two messages. Msg 0x18 reports the pile's search count -- the answer is 2
/// once nibble 0x7A has reached 4 and 1 before that. Msg 0x1B drives the
/// room's two-step progress latch: while nibble 0x38 is still 1 the answer is
/// 2 and the latch advances to 2, running the one-shot `func_800E8634`
/// cutscene the first time it is seen; otherwise the 0x28 / 0x4B pair is set
/// once to open the rest of the room and the answer is 1.
s32 func_dryfield_junk_yard_8017DA4C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0x18 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
    }
    if (in->msgId == 0x1B) {
        if (GameFlag_GetNibble(0x38) == 1) {
            if (in->field_5 == 0) {
                GameFlag_SetNibble(0x38, 2);
                func_800E8634((s32)&D_dryfield_junk_yard_8017E3D0, 0, (s32)&D_dryfield_junk_yard_8017E2B0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x28) == 0 && in->field_5 == 0) {
            out->field_2 = 2;
            GameFlag_SetNibble(0x28, 1);
            GameFlag_SetNibble(0x4B, 4);
        }
    }
    return 1;
}

s32 func_dryfield_junk_yard_8017DB78(Task* task, s32 msgId, GpMsg13EF* msg)
{
    Task* player;

    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x38) == 0) {
            GameFlag_SetNibble(0x38, 1);
            Task_SpawnFromTable(D_dryfield_junk_yard_8017DD48, 0, 0, 0);
        }
    }
    if (msg->field_2 == 2) {
        player = Game_GetPtrSlot(0xA);
        if ((player != NULL) && (((TmdObject*)(player->extra))->coords->coord.t[0] >= 0x5209) &&
            (GameFlag_GetNibble(0x38) == 1)) {
            GameFlag_SetNibble(0x38, 2);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E160, 0, (s32)&D_dryfield_junk_yard_8017E2B0);
        }
    }
    return 0;
}
