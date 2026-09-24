#include "common.h"
#include "main/fs.h"

extern s32 D_8007107C;
extern s8  D_8007217B;
extern u8* D_shelter_b4_water_supply_80184E50;
void       func_shelter_b4_water_supply_8017DE74(s32);
void       func_shelter_b4_water_supply_8017E5D8(s32);

void func_shelter_b4_water_supply_8017EDD0(s32 arg0)
{
    if (D_8007217B == 0) {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b4_water_supply_8017DE74(arg0);
    func_shelter_b4_water_supply_8017E5D8(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply_3", func_shelter_b4_water_supply_8017EE54);
