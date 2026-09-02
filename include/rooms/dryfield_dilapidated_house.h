#ifndef ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
#define ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the task family whose state-0 init is
/// `func_dryfield_dilapidated_house_80180B84`, which allocates it with
/// `Mem_Malloc(0x6C, 0)` and parks it in the `Task::idMap` slot (0x1C) -- that
/// slot is *not* a `TaskIdMap` here. Reach it with
/// `(DdhCoordWork*)task->idMap`.
///
/// `func_dryfield_dilapidated_house_80180F5C` writes the same ramp value (from
/// `func_dryfield_dilapidated_house_80180FD8`, 0..0x1000) into all three of
/// `field_0` / `field_4` / `field_8`; `func_dryfield_dilapidated_house_80181028`
/// rebuilds `mtx` as the identity and then composes it against the parent's
/// `GsCOORDINATE2` chain, and `func_dryfield_dilapidated_house_80180B84` copies
/// `mtx` verbatim into a spawned child's `GsCOORDINATE2::coord`.
typedef struct DdhCoordWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ MATRIX mtx;
    /* 0x2C */ byte   pad_2C[0x40];
} DdhCoordWork;
STATIC_ASSERT_SIZEOF(DdhCoordWork, 0x6C);

// Cross-unit prototypes. Each function lives in the unit its address falls in;
// these are the ones a *different* unit calls.
void func_dryfield_dilapidated_house_8017EBB8(Task* task);
void func_dryfield_dilapidated_house_8017EE58(Task* task);
void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2);
void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3);
s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
void func_dryfield_dilapidated_house_80181028(Task* task);
void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src);

#endif // ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
