#ifndef SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

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

extern GpObj4C D_acropolis_helicopter_landing_pad_80185E7C[9];

extern GpObj3A D_acropolis_helicopter_landing_pad_80186128[2];

extern GpRoomCoordSet D_acropolis_helicopter_landing_pad_80186AE8[1];

extern SVECTOR D_acropolis_helicopter_landing_pad_80187F88;

extern GpSaveLoc D_acropolis_helicopter_landing_pad_80187F90;

extern GpMsgEntry D_acropolis_helicopter_landing_pad_80183710[5];

extern GpXformArg D_acropolis_helicopter_landing_pad_801837B0;

extern s32 D_acropolis_helicopter_landing_pad_801837E0[18];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_80183A04[2];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_80183A34[58];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_80183FA4[16];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_80184124[38];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_801844B4[19];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_8018467C[69];

extern GpEvsCmd D_acropolis_helicopter_landing_pad_80184CF4[7];

extern TaskDesc D_acropolis_helicopter_landing_pad_80184DA0[9];

extern AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E28;

extern AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E3C;

extern GpXformArg D_acropolis_helicopter_landing_pad_80184E50;

/// Per-frame phase tick of the room's script task.
void func_acropolis_helicopter_landing_pad_8017D9BC(Task* task);

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_helicopter_landing_pad_8017DA9C(Task*);

void func_acropolis_helicopter_landing_pad_8017DE78(Task*);

void func_acropolis_helicopter_landing_pad_8017DFCC(Task*);

void func_acropolis_helicopter_landing_pad_8017E0F8(Task*);

void func_acropolis_helicopter_landing_pad_8017E270(Task*);

s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task*, s32, GpSaveLoc*, GpSaveLoc*);

s32 func_acropolis_helicopter_landing_pad_8017E49C(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_acropolis_helicopter_landing_pad_8017E4A4(Task*, s32, GpMsg13EF*, GpMessageArg);

s32 func_acropolis_helicopter_landing_pad_8017E570(Task*, s32, s32, GpMessageArg);

void func_acropolis_helicopter_landing_pad_8017E5B8(void);

void func_acropolis_helicopter_landing_pad_8017E5E8(void);

void func_acropolis_helicopter_landing_pad_8017E64C(void);

void func_acropolis_helicopter_landing_pad_8017E67C(void);

void func_acropolis_helicopter_landing_pad_8017E6C0(s32);

void func_acropolis_helicopter_landing_pad_8017E6F0(void);

void func_acropolis_helicopter_landing_pad_8017E724(void);

void func_acropolis_helicopter_landing_pad_8017E75C(s32);

void func_acropolis_helicopter_landing_pad_8017E76C(Task*);

void func_acropolis_helicopter_landing_pad_8017E81C(Task*);

void func_acropolis_helicopter_landing_pad_8017E974(Task*);

#endif // SRC_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_ACROPOLIS_HELICOPTER_LANDING_PAD_PRIVATE_H
