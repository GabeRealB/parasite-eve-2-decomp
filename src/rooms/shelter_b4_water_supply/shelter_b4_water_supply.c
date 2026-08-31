#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc D_shelter_b4_water_supply_80182620[];

void func_shelter_b4_water_supply_8017DB18(void);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", D_shelter_b4_water_supply_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", D_shelter_b4_water_supply_8017D5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D7C0);

s32 func_shelter_b4_water_supply_8017D970(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D978);

s32 func_shelter_b4_water_supply_8017DA28(void)
{
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b4_water_supply_801825F0`:
/// the directed action on the water-supply valve (`field_2` 0xA / `field_3`
/// 0x20).
s32 func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 0x20) {
            if (GameFlag_GetNibble(0xB8) != 0) {
                if (GameFlag_GetNibble(0x139) != 0) {
                    func_shelter_b4_water_supply_8017DB18();
                } else {
                    GameFlag_SetNibble(0x139, 1);
                    Gp_MsgPlayerWeapon(0);
                    Gp_RunCapCmd1(3);
                    Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(1);
                GameFlag_SetNibble(0x1BD, 2);
            }
        }
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DAE4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DB18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DC28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DD40);

void func_shelter_b4_water_supply_8017DD9C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DDA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DDFC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017DE74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017E5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017ED28);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017ED90);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017EDD0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017EE54);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017F24C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", jtbl_shelter_b4_water_supply_8017D624);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", jtbl_shelter_b4_water_supply_8017D63C);
