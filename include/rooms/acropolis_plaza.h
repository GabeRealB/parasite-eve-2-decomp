#ifndef ROOMS_ACROPOLIS_PLAZA_H
#define ROOMS_ACROPOLIS_PLAZA_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// Spawn argument the plaza's scene task (`func_acropolis_plaza_8017DFE0`)
/// reads once in state 0: `view` seeds both `CdCmd_Queue.field_1EE` and
/// `field_1EA`, and a non-zero `noStream` skips the opening stream request
/// altogether.
typedef struct AcropolisPlazaSceneArg {
    /* 0x0 */ u16 view;
    /* 0x2 */ u16 noStream;
} AcropolisPlazaSceneArg;

/// Work block the plaza's scene task (`func_acropolis_plaza_8017DFE0`) keeps at
/// `Task::idMap`; it is a 0x34 allocation, distinct from the sequence task's
/// `AcropolisPlazaWork`. `pos` is the world position
/// `func_acropolis_plaza_8017DD90` centres its vertex box on; it is refreshed
/// every frame from the plaza's per-view position table, and `distX` is how far
/// that point sits ahead of the camera in X. `fwd` / `back` are the two "the
/// player has walked past the edge of this shot" flags the task recomputes each
/// frame from `distX`, `prev*` their values from the previous frame, and
/// `field_2E` selects the 10- or 40-frame stream step used when re-seeking.
typedef struct AcropolisPlazaSceneWork {
    /* 0x00 */ MATRIX*    mtx;
    /* 0x04 */ Task*      slot3;
    /* 0x08 */ GameActor* actor;
    /* 0x0C */ VECTOR3    pos;
    /* 0x18 */ s32        distX;
    /* 0x1C */ byte       pad_1C[0x2];
    /* 0x1E */ s16        prevSide;
    /* 0x20 */ s16        streamLen;
    /* 0x22 */ s16        fwd;
    /* 0x24 */ s16        back;
    /* 0x26 */ u16        baseView;
    /* 0x28 */ byte       pad_28[0x2];
    /* 0x2A */ s16        prevFwd;
    /* 0x2C */ s16        prevBack;
    /* 0x2E */ s16        field_2E;
    /* 0x30 */ byte       pad_30[0x4];
} AcropolisPlazaSceneWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaSceneWork, 0x34);

/// Work block the plaza's sequence task (`func_acropolis_plaza_80180054`)
/// allocates with `Mem_Malloc(0x28, 0)` and parks in `Task::idMap` -- that slot
/// is not a `TaskIdMap` here. State 0 caches the slot-3 task in `slot3` and the
/// task it spawns from entry 5 of the room's table in `field_C`; state 3 spawns
/// entry 1 into `field_8`, handing it `&field_10` as its spawn argument. The
/// halfwords from 0x1E on are the per-emitter "already playing" flags
/// `func_acropolis_plaza_8017F770` tests and sets, one per ambience voice
/// started by `func_acropolis_plaza_8017F9EC`.
///
/// 0x14..0x1D belong to `func_acropolis_plaza_8017FB50`, the scene stepper:
/// `step` is its own state machine, `evtId`/`evtKind`/`evtSub` latch the
/// pending `GpObj4C` event `Gp_TakePendingObj4C` hands it, `streamFrame`
/// snapshots `CdCmd_Queue.field_1EE` when the event arrives, and `variant`
/// counts how many times the entry-6 scene has run (capped at 2) so each pass
/// spawns it with the next `Task_SpawnFromTable` arg2.
typedef struct AcropolisPlazaWork {
    /* 0x00 */ byte  pad_0[0x4];
    /* 0x04 */ Task* slot3;
    /* 0x08 */ Task* field_8;
    /* 0x0C */ Task* field_C;
    /* 0x10 */ s16   field_10;
    /* 0x12 */ s16   field_12;
    /* 0x14 */ u16   step;
    /* 0x16 */ u16   evtId;
    /* 0x18 */ u8    evtKind;
    /* 0x19 */ u8    evtSub;
    /* 0x1A */ u16   streamFrame;
    /* 0x1C */ u16   variant;
    /* 0x1E */ u16   sfxState1E;
    /* 0x20 */ u16   sfxState20;
    /* 0x22 */ u16   sfxState22;
    /* 0x24 */ u16   sfxState24;
    /* 0x26 */ byte  pad_26[0x2];
} AcropolisPlazaWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaWork, 0x28);

/// Colour ramp the plaza's fade-out task (`func_acropolis_plaza_8017D8AC`)
/// allocates with `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- that slot is
/// not a `TaskIdMap` here. All three channels start at 0 and step by
/// `Task::spawnArg1` every frame, but the semi-transparent full-screen `TILE`
/// the task links into `Gpu_CurrentOt[-16]` takes its blue from `r`, so `b` is
/// only ever stepped and never read. The task kills itself (and blanks the
/// display) once `r` reaches 0x100. The fade-in counterpart at 0x8017DA58 is
/// the shared `RoomsShared8017da58` body under `src/rooms/lib/`, which repeats
/// this layout privately because the Dryfield water tank uses it too.
typedef struct AcropolisPlazaFadeWork {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ s16  r;
    /* 0x4 */ s16  g;
    /* 0x6 */ s16  b;
} AcropolisPlazaFadeWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaFadeWork, 0x8);

/// Work block the plaza's opening sequence (`func_acropolis_plaza_8017ECF8`)
/// allocates with `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- that slot is
/// not a `TaskIdMap` here. `slot3` caches the slot-3 task every message in the
/// sequence is addressed to; `timer` is the frame counter the waiting states
/// step (0x3D frames in state 7, 0xB in state 11, 2 in state 12).
typedef struct AcropolisPlazaOpeningWork {
    /* 0x0 */ Task* slot3;
    /* 0x4 */ s16   timer;
    /* 0x6 */ byte  pad_6[0x2];
} AcropolisPlazaOpeningWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaOpeningWork, 0x8);

/// The one scratch buffer the plaza's opening sequence shares between its area
/// lookup and its last stream request. `key` is the location key states 6 and 8
/// build from `Game_Session` before walking the nested area records for the
/// 0x6C room, and `slot` is the CD stream-slot triple state 13 hands to
/// `CdCmd_Enqueue(0x71, ...)`; the task only ever has one of them in flight.
typedef union AcropolisPlazaOpeningBuf {
    /* 0x0 */ GpAreaKey key;
    /* 0x0 */ u8        slot[4];
} AcropolisPlazaOpeningBuf;

/// Work block the plaza's warp task (`func_acropolis_plaza_8017E7E4`) allocates
/// with `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- that slot is not a
/// `TaskIdMap` here. It only caches the slot-3 task every message in the
/// sequence (0x3F2 place, 0x3EE warp, 0x3F0 poll) is addressed to; the
/// trailing four bytes are zeroed by `Mem_Set` and never read.
typedef struct AcropolisPlazaWarpWork {
    /* 0x0 */ Task* slot3;
    /* 0x4 */ byte  pad_4[0x4];
} AcropolisPlazaWarpWork;

/// Work block the plaza's cutscene tasks reach through `Task::spawnArg2`.
/// Every one of them (`func_acropolis_plaza_8017E7E4`, `..._8017E9A8`,
/// `..._8017F48C`, `..._8017F620`) runs the same handoff once `CdCmd_IsIdle`
/// reports the stream has finished: latch `CdCmd_Queue.field_1EE` into
/// `field_1A`, kill the task at `task`, and (in `..._8017F620`) run the
/// capture command named by `capCmd`. Only those three fields are identified,
/// so this declaration is deliberately partial.
typedef struct AcropolisPlazaCutWork {
    /* 0x00 */ byte  pad_0[0x8];
    /* 0x08 */ Task* task;
    /* 0x0C */ byte  pad_C[0xC];
    /* 0x18 */ s8    capCmd;
    /* 0x19 */ byte  pad_19[0x1];
    /* 0x1A */ u16   field_1A;
} AcropolisPlazaCutWork;

/// The 0x14-byte weapon record msg 0x3E8 takes, seen at its offset inside
/// `AcropolisPlazaTailMsg`: the plaza's opening tail builds it eight bytes into
/// the shared buffer, which is what makes that buffer 0x1C rather than 0x18
/// bytes long.
typedef struct AcropolisPlazaWeaponMsg {
    /* 0x00 */ byte    pad_0[0x8];
    /* 0x08 */ GpRec14 rec;
} AcropolisPlazaWeaponMsg;
STATIC_ASSERT_SIZEOF(AcropolisPlazaWeaponMsg, 0x1C);

/// The one scratch buffer `func_acropolis_plaza_8017E9A8` builds its late
/// payloads in. The task only ever has one of them in flight, so all three
/// views share a single frame slot, and the union is what makes that sharing
/// explicit: `slot` is the CD stream-slot triple handed to
/// `CdCmd_Enqueue(0x72, ...)` in state 3, `weapon.rec` the record msg 0x3E8
/// takes in state 5, and `place` the position + Euler rotation the 0x3E9
/// placement that follows it takes.
typedef union AcropolisPlazaTailMsg {
    /* 0x0 */ u8                      slot[4];
    /* 0x0 */ AcropolisPlazaWeaponMsg weapon;
    /* 0x0 */ RoomPlacement           place;
} AcropolisPlazaTailMsg;
STATIC_ASSERT_SIZEOF(AcropolisPlazaTailMsg, 0x1C);

#endif // ROOMS_ACROPOLIS_PLAZA_H
