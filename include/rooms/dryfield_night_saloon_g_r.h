#ifndef ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
#define ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// 4-byte payload this room sends as `Gp_DispatchMsg`'s `arg2` for message
/// 0x7DA, which the slot-4 task forwards to the 0x7DB handlers tagged with the
/// action taken. `field_0` / `field_1` are the session's two id bytes and
/// `field_2` the halfword the receiver switches on. Same four bytes as
/// `Dmr1Msg7DA`, `DbwMsg7DA` and `ActorsShared80132724Msg`.
typedef struct DnsgrMsg7DA {
    /* 0x0 */ u8  field_0; // gGameSession::at4.loc.stage
    /* 0x1 */ u8  field_1; // gGameSession::at4.loc.area
    /* 0x2 */ s16 field_2;
} DnsgrMsg7DA;
STATIC_ASSERT_SIZEOF(DnsgrMsg7DA, 0x4);

/// Builds the room's display mode object and reports success. Its argument is
/// unused; `func_dryfield_night_saloon_g_r_8017DB74` (state 2) still passes one.
s32 func_dryfield_night_saloon_g_r_8017E698(s32 arg0);

#endif // ROOMS_DRYFIELD_NIGHT_SALOON_G_R_H
