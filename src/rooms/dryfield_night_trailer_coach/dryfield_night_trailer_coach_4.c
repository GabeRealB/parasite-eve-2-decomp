#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_80181228.h"

/// Cutscene trigger for the trailer coach at night. Where the day version has
/// its own record and a save-view reset, this one only runs at the two ends of
/// the visit.
///
/// Request 0xE forces area 8 for the scene, fills the room's cutscene record
/// the same way the motel lobby fills its own -- save view 8, slot 1, and the
/// cap file picked by `GameFlag_GetNibble(0x7A)` (file 1 below four, file 2 at
/// four or more) -- then hands it to `RoomsShared80181228Desc`. Request 3
/// spawns entry 0 of the room's task table at `0x8018797C` and request 0x17
/// asks the cap system to run command 0x17. Always returns 0.
extern s8                     D_80072170;
extern TaskDesc               D_dryfield_night_trailer_coach_8018797C;
extern RoomsShared80181228Rec D_dryfield_night_trailer_coach_8018C21C;

s32 func_dryfield_night_trailer_coach_801826EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        D_80072170                                      = 1;
        D_dryfield_night_trailer_coach_8018C21C.field_0 = 8;
        D_dryfield_night_trailer_coach_8018C21C.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x380;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 1;
        } else {
            D_dryfield_night_trailer_coach_8018C21C.field_14 = 0x3C0;
            D_dryfield_night_trailer_coach_8018C21C.field_3  = 2;
        }
        D_dryfield_night_trailer_coach_8018C21C.field_2  = 0;
        D_dryfield_night_trailer_coach_8018C21C.field_4  = 0x531B0003;
        D_dryfield_night_trailer_coach_8018C21C.field_8  = 0x531B0005;
        D_dryfield_night_trailer_coach_8018C21C.field_10 = 0x531B0004;
        D_dryfield_night_trailer_coach_8018C21C.field_C  = 0x531B0006;
        Task_SpawnFromTable(&RoomsShared80181228Desc, 0, 3, (s32)&D_dryfield_night_trailer_coach_8018C21C);
    }
    if (arg2 == 3) {
        Task_SpawnFromTable(&D_dryfield_night_trailer_coach_8018797C, 0, 0, 0);
    }
    if (arg2 == 0x17) {
        Gp_RunCapCmd1(0x17);
    }
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182800(void)
{
    return 0;
}

s32 func_dryfield_night_trailer_coach_80182808(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x531B000D, 0, 0);
    }
    return 0;
}

void func_dryfield_night_trailer_coach_8018283C(void)
{
    Gp_StartCapSlot(9, 0, 1);
}

void func_dryfield_night_trailer_coach_80182864(void)
{
    func_800D4D2C((GameFlag_GetNibble(0xE0) == 0) ? 0x20 : 0x21);
}

extern s8 D_80071090;
extern u8 D_8007216C;

void func_dryfield_night_trailer_coach_80182898(void)
{
    char pad[0x10];

    if (D_8007216C == 5) {
        D_80071090 = 0;
    } else {
        D_80071090 = 3;
    }
}

extern SVECTOR D_dryfield_night_trailer_coach_801893F8[];
extern SVECTOR D_dryfield_night_trailer_coach_80189400[];
extern SVECTOR D_dryfield_night_trailer_coach_80189480[];

void func_dryfield_night_trailer_coach_801838B4(SVECTOR* v, s32 arg1);
void Room_Draw18(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw05(SVECTOR* v, s32 arg1, s32 arg2);
