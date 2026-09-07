#ifndef AYA_20900_H
#define AYA_20900_H

#include "common.h"

/// 8-byte work block `func_aya_20900_80115CFC` allocates with `Mem_Malloc(8)`
/// and parks in `Task::idMap`. That slot is not a `TaskIdMap` here, so reach
/// it with `(Aya20900Work*)task->idMap`. `index` is the fade state machine,
/// `timer` counts the hold at full white, and `fade` is the TILE colour.
typedef struct Aya20900Work {
    /* 0x0 */ u16 index;
    /* 0x2 */ u16 timer;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 fade;
} Aya20900Work;
STATIC_ASSERT_SIZEOF(Aya20900Work, 0x8);

#endif
