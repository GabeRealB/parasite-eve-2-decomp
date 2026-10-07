#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

extern WorldCoordRoomLights D_dryfield_night_motel_loft_8018004C[1];

extern WorldCollisionTrigger D_dryfield_night_motel_loft_80180064[12];

extern WorldCollisionTrigger D_dryfield_night_motel_loft_801803F4[14];

extern TaskMessageEntry D_dryfield_night_motel_loft_8017EB1C[6];

extern TaskDesc D_dryfield_night_motel_loft_8017EB4C[1];

extern EvsCommand D_dryfield_night_motel_loft_8017EB78[17];

extern SpriteBatch D_dryfield_night_motel_loft_8017F2D0[2];

extern SpriteBatch D_dryfield_night_motel_loft_8017F2E0[2];

extern SpriteSource D_dryfield_night_motel_loft_8017F2F0[8];

extern SpriteBatch D_dryfield_night_motel_loft_8017F390[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F3A8[13];

extern SpriteBatch D_dryfield_night_motel_loft_8017F4AC[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F4C4[20];

extern SpriteBatch D_dryfield_night_motel_loft_8017F654[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F66C[13];

extern SpriteBatch D_dryfield_night_motel_loft_8017F770[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F788[25];

extern SpriteBatch D_dryfield_night_motel_loft_8017F97C[6];

extern SpriteSource D_dryfield_night_motel_loft_8017F9AC[22];

extern SpriteBatch D_dryfield_night_motel_loft_8017FB64[4];

/// Resets the room's live grid from its template and, when `arg0` is set,
/// raises the grid's four corners by 0xBB8 in Y.
void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

// Callbacks referenced by the overlay's shared data tables.
/// Resolves the night balcony's room after its story scene.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution selects room 1 before the balcony scene and room 3 after
/// it. Queries retain the copied room. Returns 1 for every destination.
s32 roomVariantMotelBalconyMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

s32 func_dryfield_night_motel_loft_8017D5F8(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_loft_8017D67C(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_loft_8017D6BC(Task*, s32, s32, s32);

s32 func_dryfield_night_motel_loft_8017D6C4(Task*, s32, s32, s32);

void func_dryfield_night_motel_loft_8017D6F8(Task*);

void func_dryfield_night_motel_loft_8017D7EC(u8);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
