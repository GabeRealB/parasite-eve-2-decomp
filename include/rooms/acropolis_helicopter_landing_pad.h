#ifndef ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// Payload of room msg `0x7D3`, handled by
/// `func_acropolis_helicopter_landing_pad_8017D824`. `phase` selects which
/// of the two `RoomPlacement`s the model is moved to (0 / 2 first, 1 second).
typedef struct AhlpMsg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 phase;
} AhlpMsg7D3;
STATIC_ASSERT_SIZEOF(AhlpMsg7D3, 0x8);

/// Two descriptors that attach no model: entry 0 runs
/// `func_acropolis_helicopter_landing_pad_8017ED00`, entry 1
/// `func_acropolis_helicopter_landing_pad_8017EB58`.
extern TaskDesc D_acropolis_helicopter_landing_pad_80184E68[];

/// Progress of the helicopter sequence. The msg 0x3EF handler moves it from 0
/// to 1, the phase tick from 1 to 2, the cap-slot task to 3 and the room
/// state-machine task to 4. Phase 0 refuses the warp into stage 0xF; phase 2
/// makes the warp start cap slot 9 and lets
/// `func_acropolis_helicopter_landing_pad_8017E570` spawn task entry 4.
extern s32 D_acropolis_helicopter_landing_pad_80184D9C;

/// Raised by the phase tick once the session reaches camera view 5 and
/// cleared when the script task starts; the msg 0x3EF handler only starts
/// phase 1 while it is set.
extern s32 D_acropolis_helicopter_landing_pad_80184E0C;

/// Latched by kind 1 of msg 0x3EF and cleared when the script task starts or
/// `gGameSession->eventState` is 0; while set, a handler forwards msg 0x3E9
/// to slot 3.
extern s32 D_acropolis_helicopter_landing_pad_80187F84;

/// Per-frame phase tick of the room's script task.
void func_acropolis_helicopter_landing_pad_8017D9BC(Task* task);

#endif
