#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s8  D_80114C12;
extern u8  D_80115598;
extern u8  D_80115768;
extern s8  D_8007272D;
extern s32 D_dryfield_night_gas_station_80184034;
extern s32 D_dryfield_night_gas_station_80184098;
extern s32 D_dryfield_night_gas_station_801840AC;
extern s32 D_dryfield_night_gas_station_801841FC;
extern s32 D_dryfield_night_gas_station_80188B0C;
extern s32 D_dryfield_night_gas_station_80188B64;
extern s32 D_dryfield_night_gas_station_80188BF4;
extern s32 D_dryfield_night_gas_station_80189014;
extern s32 D_dryfield_night_gas_station_8018920C;
extern s32 D_dryfield_night_gas_station_801892E4;
extern s32 D_dryfield_night_gas_station_80189A7C;

extern GpAreaApplyRec D_dryfield_night_gas_station_801907A0;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", RoomsShared8017ea68Title);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D644);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D650);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D658);

void func_dryfield_night_gas_station_8017FBD4(s32 arg0);
void func_dryfield_night_gas_station_80180C20(void);

void func_dryfield_night_gas_station_8017F41C(Task* arg0)
{
    arg0->msgTable = &D_dryfield_night_gas_station_80184034;
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
