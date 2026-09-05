#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x36) == 0) {
            GameFlag_SetNibble(0x36, 1);
            Task_SpawnFromTable(&D_dryfield_water_tank_8017F34C, 0, 0, 0);
            GameFlag_SetNibble(0x56, 1);
        }
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0x33) == 0)) {
        GameFlag_SetNibble(0x33, 1);
        func_800E3FAC(0xA2, 0xE);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        Gp_ApplyAreaRecs(D_dryfield_water_tank_80188D1C);
        Gp_MsgPlayerWeapon(0);
        func_800E8634((s32)&D_dryfield_water_tank_80184E0C, 0, (s32)&D_dryfield_water_tank_801859DC);
    }
    if (arg2->field_2 == 3) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F114, 0);
    }
    if (arg2->field_2 == 4) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F21C, 0);
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017D910);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017D948);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017D9D4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017DA4C);
