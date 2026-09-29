#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

extern GpRoomCoordSet D_dryfield_night_motel_loft_8018004C[1];

extern GpObj4C D_dryfield_night_motel_loft_80180064[12];

extern GpMsgEntry D_dryfield_night_motel_loft_8017EB1C[6];

extern TaskDesc D_dryfield_night_motel_loft_8017EB4C[1];

extern GpEvsCmd D_dryfield_night_motel_loft_8017EB78[17];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F2D0[2];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F2E0[2];

extern GpSprtElem D_dryfield_night_motel_loft_8017F2F0[8];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F390[3];

extern GpSprtElem D_dryfield_night_motel_loft_8017F3A8[13];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F4AC[3];

extern GpSprtElem D_dryfield_night_motel_loft_8017F4C4[20];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F654[3];

extern GpSprtElem D_dryfield_night_motel_loft_8017F66C[13];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F770[3];

extern GpSprtElem D_dryfield_night_motel_loft_8017F788[25];

extern GpSprtCmd D_dryfield_night_motel_loft_8017F97C[6];

extern GpSprtElem D_dryfield_night_motel_loft_8017F9AC[22];

extern GpSprtCmd D_dryfield_night_motel_loft_8017FB64[4];

/// Resets the room's live grid from its template and, when `arg0` is set,
/// raises the grid's four corners by 0xBB8 in Y.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
s32 func_dryfield_night_motel_loft_8017D5F8(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_dryfield_night_motel_loft_8017D600(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_night_motel_loft_8017D67C(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_loft_8017D6BC(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_dryfield_night_motel_loft_8017D6C4(Task*, s32, s32, s32);

void func_dryfield_night_motel_loft_8017D6F8(Task*);

void func_dryfield_night_motel_loft_8017D7EC(u8);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
