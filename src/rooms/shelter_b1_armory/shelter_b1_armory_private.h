#ifndef SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H

#include "common.h"

#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "rooms/room_common.h"

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    u8 value;
    u8 retained[7];
} ShelterB1ArmoryStorage557C;
STATIC_ASSERT_SIZEOF(ShelterB1ArmoryStorage557C, 8);

extern s32 Shop_Data_80187628;

extern GpItemMap* Shop_Data_8018762C;

extern RoomEventMsg D_shelter_b1_armory_80185584;

extern u8 D_shelter_b1_armory_8018558C[4];

extern RoomEventReq D_shelter_b1_armory_80185590;

extern ShelterB1ArmoryStorage557C D_shelter_b1_armory_8018557C;

#endif // SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H
