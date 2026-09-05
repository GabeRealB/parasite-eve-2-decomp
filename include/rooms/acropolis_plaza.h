#ifndef ROOMS_ACROPOLIS_PLAZA_H
#define ROOMS_ACROPOLIS_PLAZA_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgte.h>

/// Work block the plaza's scene task (`func_acropolis_plaza_8017DFE0`) keeps at
/// `Task::idMap`. Only `pos` is identified so far: it is the world position
/// `func_acropolis_plaza_8017DD90` centres its vertex box on. The block runs to
/// at least 0x30 bytes — the scene task also reads halfwords from 0x1E through
/// 0x2E — so this declaration is deliberately partial.
typedef struct AcropolisPlazaWork {
    /* 0x00 */ byte    pad_0[0xC];
    /* 0x0C */ VECTOR3 pos;
} AcropolisPlazaWork;

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
