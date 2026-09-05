#include "common.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

#include "rooms/room_common.h"

extern s16      D_8007107A;
extern u8       D_8007216C;
extern TaskDesc D_acropolis_forked_road_80180F44;

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
/// stream file is open (`D_8007107A < 0 || D_8006AC30.sector == 0`), in which
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
        if ((D_8007107A < 0) || (D_8006AC30.sector == 0)) {
            if (in->field_5 != 0) {
                return 1;
            }
            out->field_2 = 2;
        } else if (GameFlag_GetNibble(1) == 2) {
            if (in->field_5 == 0) {
                D_8007216C = 7;
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

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road", D_acropolis_forked_road_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road", D_acropolis_forked_road_8017D5C4);
