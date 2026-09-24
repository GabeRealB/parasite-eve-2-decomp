#ifndef ROOMS_SHELTER_B4_WATER_SUPPLY_H
#define ROOMS_SHELTER_B4_WATER_SUPPLY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// The twelve bytes `func_shelter_b4_water_supply_8017DB18` stages in
/// `D_shelter_b4_water_supply_80184E44` before spawning the task at
/// `D_shelter_b4_water_supply_801825E4`. Bytes 1..3 are run through the room's
/// message handler `func_shelter_b4_water_supply_8017DDFC` first, which can
/// rewrite `field_3`; `field_8` holds a sound-event id.
///
/// The layout matches `DnwhEventDesc` in another room, staged the same way;
/// whether the two are one type is open. `pad_6` is never touched here.
typedef struct ShelterB4WaterSupplyEventDesc {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  field_8;
} ShelterB4WaterSupplyEventDesc;
STATIC_ASSERT_SIZEOF(ShelterB4WaterSupplyEventDesc, 0xC);

/// Descriptor of the task spawned once the event block above is staged.
extern TaskDesc D_shelter_b4_water_supply_801825E4;

/// The staged event block, read by the task spawned from the descriptor above.
extern ShelterB4WaterSupplyEventDesc D_shelter_b4_water_supply_80184E44;

s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out);

#endif // ROOMS_SHELTER_B4_WATER_SUPPLY_H
