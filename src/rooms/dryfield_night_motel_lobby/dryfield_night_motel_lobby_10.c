#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_motel_lobby_801828E0[];
extern SVECTOR D_dryfield_night_motel_lobby_801828E8[];

void func_dryfield_night_motel_lobby_80182200(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_dryfield_night_motel_lobby_801812F8(void)
{
    switch (gGameSession->at4.loc.view) {
        case 2:
            Room_Draw18(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[2], 1, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[3], 1, 0x300);
            break;
        case 3:
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[0], 2, 0x300);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E8[3], 1, 0x300);
            break;
        case 4:
            Room_Draw18(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x60);
            func_dryfield_night_motel_lobby_80182200(&D_dryfield_night_motel_lobby_801828E0[1], 2, 0x300);
            break;
        case 5:
            Room_Draw05(&D_dryfield_night_motel_lobby_801828E0[0], 0x60, 0x30);
            break;
    }
}
