#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern s16 func_shelter_b3_dumping_hole_8017FB70(void);

s32 func_shelter_b3_dumping_hole_8017D760(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x28) {
        if (func_shelter_b3_dumping_hole_8017FB70() != 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x16);
            }
            return 0;
        }
        if (in->field_5 == 0) {
            out->field_3 = (u8)Game_Session->unknown_133[1] + 1;
        }
        return 1;
    }
    return 1;
}

s32 func_shelter_b3_dumping_hole_8017D82C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x12) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x11D) != 0 ? 0x12 : 0x17, 1);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole", D_shelter_b3_dumping_hole_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole", D_shelter_b3_dumping_hole_8017D5C4);
