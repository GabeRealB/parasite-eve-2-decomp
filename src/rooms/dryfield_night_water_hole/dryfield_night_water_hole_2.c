#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern s32 D_dryfield_night_water_hole_8018067C;
extern s32 D_dryfield_night_water_hole_801807FC;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_2", func_dryfield_night_water_hole_8017DADC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_2", func_dryfield_night_water_hole_8017DC28);

s32 func_dryfield_night_water_hole_8017DD5C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp_s0;

    if ((in->field_2 == 2) && (GameFlag_GetNibble(0x95) == 0) && (Game_Session->field_9 == 1)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_8018067C, 0);
    }
    temp_s0 = in->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0x95) == 0) && (Game_Session->field_9 == temp_s0)) {
        GameFlag_SetNibble(0x95, 1);
        func_800E8614((s32)&D_dryfield_night_water_hole_801807FC, 0);
    }
    return 0;
}
