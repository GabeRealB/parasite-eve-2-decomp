#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern u8  D_80115598;
extern s32 D_dryfield_night_gas_station_80184034;
extern s32 D_dryfield_night_gas_station_80184098;
extern s32 D_dryfield_night_gas_station_80188B0C;
extern s32 D_dryfield_night_gas_station_801892E4;
extern s32 D_dryfield_night_gas_station_80189A7C;

void func_dryfield_night_gas_station_8017FBD4(s32 arg0);
void func_dryfield_night_gas_station_80180C20(void);

void func_dryfield_night_gas_station_8017F41C(Task* arg0)
{
    arg0->field_24 = &D_dryfield_night_gas_station_80184034;
    Game_SetPtrSlot(arg0, 7);
    if ((GameFlag_GetNibble(0x63) >= 2) && (Game_GetPtrSlot(0xA) != 0)) {
        Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_night_gas_station_80188B0C, 0);
        Gp_AllyAnimId(&D_dryfield_night_gas_station_80184098);
        Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_night_gas_station_80184098, 0);
        func_dryfield_night_gas_station_8017FBD4(0);
    }
    if (GameFlag_GetNibble(0xA0) == 0) {
        GameFlag_SetNibble(0xA0, 1);
        func_800E3FAC(0xA2, 0x12);
        GameFlag_SetNibble(0x4C, 2);
        func_dryfield_night_gas_station_80180C20();
        if (Game_GetPtrSlot(0xA) != 0) {
            func_800E8634((s32)&D_dryfield_night_gas_station_801892E4, 0, (s32)&D_dryfield_night_gas_station_80189A7C);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F544);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F6B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F7E0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F89C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F990);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017F9E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017FA6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_4", func_dryfield_night_gas_station_8017FAEC);
