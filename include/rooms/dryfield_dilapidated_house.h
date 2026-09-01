#ifndef ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
#define ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"

/// One entry of a room's message-handler table -- the object the room task
/// parks in `Task::field_24` right before `Game_SetPtrSlot(task, 7)` publishes
/// itself. `id` is a message id in the 0x13EE..0x13F2 range and `0x7FFFFFFF`
/// terminates the table (one zero word follows it). The same shape appears in
/// every room overlay checked (`acropolis_patio`, `neo_ark_bridge`, ...); hoist
/// it into a shared rooms header when one exists. A few tables hold `s32 (*)(void)`
/// stubs rather than real `TaskFunc`s.
typedef struct RoomMsgHandler {
    /* 0x0 */ s32      id;
    /* 0x4 */ TaskFunc handler;
} RoomMsgHandler;
STATIC_ASSERT_SIZEOF(RoomMsgHandler, 0x8);

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
s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
void func_dryfield_dilapidated_house_80181028(Task* task);
void func_dryfield_dilapidated_house_801810F8(GameActorExt* dst, GameActorExt* src);

#endif // ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
