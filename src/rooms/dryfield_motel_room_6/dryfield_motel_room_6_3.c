#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_motel_room_6.h"
#include "rooms/rooms_shared_80181228.h"

/// Task table of the room's cutscene: entry 0 is the cutscene task, entry 1
/// the sound task it runs alongside the scene.
extern TaskDesc D_dryfield_motel_room_6_80182D24;

/// Script record the room's event handler fills in and hands to the cutscene
/// task as its `spawnArg2`.
extern RoomsShared80181228Rec D_dryfield_motel_room_6_80186830;

/// Handler of message 0x13F0 in the room's message table. For event 0x16 it
/// fills in the cutscene script record - the cap file and fade chosen from
/// flag nibble 0x7A and the stage, and the scene's sound events - and spawns
/// the cutscene task on it. Any other event goes to
/// `func_dryfield_motel_room_6_80181910`.
s32 func_dryfield_motel_room_6_80181740(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 count;

    count = 0;
    if (arg2 == 0x16) {
        D_dryfield_motel_room_6_80186830.field_0 = 0xC;
        D_dryfield_motel_room_6_80186830.field_1 = 1;
        switch (GameFlag_GetNibble(0x7A)) {
            case 0 ... 3:
                if (gGameSession->at4.loc.stage == 2) {
                    count                                     = 4;
                    D_dryfield_motel_room_6_80186830.field_14 = 0x3C0;
                    D_dryfield_motel_room_6_80186830.field_3  = 1;
                } else {
                    count                                     = 2;
                    D_dryfield_motel_room_6_80186830.field_14 = 0x380;
                    D_dryfield_motel_room_6_80186830.field_3  = 1;
                }
                break;
            case 4 ... 6:
                count                                     = 2;
                D_dryfield_motel_room_6_80186830.field_14 = 0x3C0;
                D_dryfield_motel_room_6_80186830.field_3  = count;
                break;
        }
        D_dryfield_motel_room_6_80186830.field_2  = 0;
        D_dryfield_motel_room_6_80186830.field_4  = Gp_PackStageSndId(0x521E0008);
        D_dryfield_motel_room_6_80186830.field_8  = Gp_PackStageSndId(0x521E000B);
        D_dryfield_motel_room_6_80186830.field_10 = Gp_PackStageSndId(0x521E0009);
        D_dryfield_motel_room_6_80186830.field_C  = Gp_PackStageSndId(0x521E000A);
        Task_SpawnFromTable(&D_dryfield_motel_room_6_80182D24, 0, count, (s32)&D_dryfield_motel_room_6_80186830);
    } else {
        func_dryfield_motel_room_6_80181910(arg0, arg1, arg2, arg3);
    }
    return 0;
}
