#ifndef ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "types.h"

#include "gameplay/area.h"

#include "gameplay/pad_script.h"

#include "gameplay/world_coords.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "rooms/room_common.h"

#include "main/task_types.h"

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

void func_acropolis_helicopter_landing_pad_80180A64(GpCoord* coord);

extern GpScriptCmd D_acropolis_helicopter_landing_pad_80187D34[2];

extern GpScriptRec D_acropolis_helicopter_landing_pad_80187D3C;

void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0);
void func_acropolis_helicopter_landing_pad_80181064(Task* arg0);
void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0);
extern GpAreaVariant D_acropolis_helicopter_landing_pad_801861E8[13];

void func_acropolis_helicopter_landing_pad_8017EF60(s32 unused0, s32 unused1);

void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0);

#endif
