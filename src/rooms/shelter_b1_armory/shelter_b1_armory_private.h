#ifndef SHELTER_B1_ARMORY_PRIVATE_H
#define SHELTER_B1_ARMORY_PRIVATE_H

#include "common.h"

// Retain the zero tail after the accessed value. Whether it was spare
// fields or alignment storage remains unresolved.
typedef struct {
    u8 value;
    u8 retained[7];
} ShelterB1ArmoryStorage557C;
STATIC_ASSERT_SIZEOF(ShelterB1ArmoryStorage557C, 8);

extern ShelterB1ArmoryStorage557C D_shelter_b1_armory_8018557C;

#endif // SHELTER_B1_ARMORY_PRIVATE_H
