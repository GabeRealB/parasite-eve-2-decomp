#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"
#include "rooms/rooms_shared_80181228.h"

/// Second descriptor of the trailer's spawn table (spawned by request 3).
extern TaskDesc D_dryfield_trailer_coach_80184FC0;

/// The cutscene record this room hands `RoomsShared80181228Desc`.
extern RoomsShared80181228Rec D_dryfield_trailer_coach_80189C9C;

/// Runs the trailer coach's day-2 hand-off. Request 3 spawns entry 1 of the
/// room's task table; request 0xE drops the save view back to 1 when it is on
/// 2, then either raises the `0x16C` flag and asks the cap system to run
/// command 0x1D, or fills in the room's cutscene record (view 0xA, slots 1,
/// files 3/4/5/6) and hands it to `RoomsShared80181228Desc`. Always returns 0.
s32 func_dryfield_trailer_coach_801825A8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Task_SpawnFromTable(&D_dryfield_trailer_coach_80184FC0, 1, 0, 0);
    }
    if (arg2 == 0xE) {
        if (Mc_SaveData.at4.loc.warp == 2) {
            Mc_SaveData.at4.loc.warp = 1U;
        }
        if (GameFlag_GetNibble(0x16C) == 0) {
            GameFlag_SetNibble(0x16C, 1);
            Gp_RunCapCmd1(0x1D);
            return 0;
        }
        D_dryfield_trailer_coach_80189C9C.field_0  = 0xA;
        D_dryfield_trailer_coach_80189C9C.field_1  = 1;
        D_dryfield_trailer_coach_80189C9C.field_3  = 1;
        D_dryfield_trailer_coach_80189C9C.field_2  = 0;
        D_dryfield_trailer_coach_80189C9C.field_4  = 0x521B0003;
        D_dryfield_trailer_coach_80189C9C.field_8  = 0x521B0005;
        D_dryfield_trailer_coach_80189C9C.field_10 = 0x521B0004;
        D_dryfield_trailer_coach_80189C9C.field_C  = 0x521B0006;
        Task_SpawnFromTable(&RoomsShared80181228Desc, 0, 3, (s32)&D_dryfield_trailer_coach_80189C9C);
        return 0;
    }
    return 0;
}
