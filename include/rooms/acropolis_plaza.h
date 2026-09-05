#ifndef ROOMS_ACROPOLIS_PLAZA_H
#define ROOMS_ACROPOLIS_PLAZA_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgte.h>

/// Work block the plaza's scene task (`func_acropolis_plaza_8017DFE0`) keeps at
/// `Task::idMap`. `pos` is the world position `func_acropolis_plaza_8017DD90`
/// centres its vertex box on; the halfwords from 0x1E on are the per-emitter
/// "already playing" flags `func_acropolis_plaza_8017F770` tests and sets, one
/// per ambience voice started by `func_acropolis_plaza_8017F9EC`. The block
/// runs to at least 0x30 bytes — the scene task reads halfwords through 0x2E
/// — so this declaration is deliberately partial.
typedef struct AcropolisPlazaWork {
    /* 0x00 */ byte    pad_0[0xC];
    /* 0x0C */ VECTOR3 pos;
    /* 0x18 */ byte    pad_18[0x6];
    /* 0x1E */ u16     sfxState1E;
    /* 0x20 */ u16     sfxState20;
    /* 0x22 */ u16     sfxState22;
    /* 0x24 */ u16     sfxState24;
} AcropolisPlazaWork;

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

#endif // ROOMS_ACROPOLIS_PLAZA_H
