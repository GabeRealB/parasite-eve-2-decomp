#ifndef ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
#define ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H

#include "common.h"

/// Event parameters latched into the room's pending event when an event
/// starts. `field_8` is the game flag checked and set as the event starts
/// (0 for none); the other fields are only copied here, so their roles are
/// unproven from this room.
typedef struct Shelter1fVehicularAirlockEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} Shelter1fVehicularAirlockEvent;
STATIC_ASSERT_SIZEOF(Shelter1fVehicularAirlockEvent, 0xC);

#endif // ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
