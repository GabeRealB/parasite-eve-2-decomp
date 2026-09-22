#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dadc.h"

s32 RoomsShared8017dadc(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->msgId == 0x19) {
        temp = gGameSession->at4.loc.stage;
        if (temp == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = temp;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x26 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->field_3 = 2;
            } else {
                out->field_3 = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->field_3 += 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->field_3 = 5;
            } else {
                out->field_3 = 6;
            }
        }
    }
    return 1;
}
