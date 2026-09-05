#include "common.h"

#include "rooms/shelter_b4_reservoir.h"

void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.field_0 = arg0;
    D_shelter_b4_reservoir_80187684.field_2 = arg1;
    D_shelter_b4_reservoir_80187684.field_4 = arg2;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_7", func_shelter_b4_reservoir_80182B1C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_7", func_shelter_b4_reservoir_80183074);
