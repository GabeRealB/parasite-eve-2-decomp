#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern s32 D_dryfield_night_junk_yard_801805A4;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard", func_dryfield_night_junk_yard_8017D6AC);

s32 func_dryfield_night_junk_yard_8017D82C(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    if ((in->field_2 == 3) && (Game_Session->field_9 == 1) && (GameFlag_GetNibble(0x9F) == 0)) {
        GameFlag_SetNibble(0x9F, 1);
        func_800E8614((s32)&D_dryfield_night_junk_yard_801805A4, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard", D_dryfield_night_junk_yard_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_junk_yard/dryfield_night_junk_yard", RoomsShared8017d878Table);
