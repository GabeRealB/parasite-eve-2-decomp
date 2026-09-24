#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b4_water_supply.h"

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc   D_shelter_b4_water_supply_80182620[];
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];
extern TaskDesc   D_shelter_b4_water_supply_8018263C[];
void              func_shelter_b4_water_supply_8017DB18(void);

/// Message 0x20: unless a report-only query, answers in `field_3` from
/// nibbles 0x51 (1 when set, 2 when clear) and 0x53 (adds 2 when set).
/// Always returns 1 (not consumed).
s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->msgId == 0x20 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017DE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017E5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_2", func_shelter_b4_water_supply_8017ED28);
