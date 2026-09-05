#ifndef ROOMS_ACROPOLIS_PLAZA_H
#define ROOMS_ACROPOLIS_PLAZA_H

#include "common.h"

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

#endif // ROOMS_ACROPOLIS_PLAZA_H
