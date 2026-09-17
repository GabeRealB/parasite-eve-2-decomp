#include "common.h"

#include "rooms/dryfield_night_motel_lobby.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_4", func_dryfield_night_motel_lobby_801802A8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_4", func_dryfield_night_motel_lobby_80180440);

s32 func_dryfield_night_motel_lobby_80180734(void)
{
    u8* p = D_dryfield_night_motel_lobby_801844D8;

    if (p[6] != 0xA) {
        return 0;
    }
    if (p[5] != p[6]) {
        return 0;
    }
    if (p[4] != p[5]) {
        return 0;
    }
    if (p[3] != 3) {
        return 0;
    }
    if (p[2] != 0) {
        return 0;
    }
    /* Compares the third digit with the first rather than against a repeated
       literal: the earlier test leaves that load live, and re-testing it is
       what keeps it in one register instead of a fresh `addiu`. */
    if (p[1] != p[3]) {
        return 0;
    }
    return p[0] == 3;
}
