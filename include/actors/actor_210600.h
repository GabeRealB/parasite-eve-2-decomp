#ifndef ACTOR_210600_H
#define ACTOR_210600_H

#include "common.h"

#include "main/task.h"

/// Per-actor state block for the `actor_210600` overlay. `func_actor_210600_8014B8C8`
/// is the overlay's allocator: it calls `Mem_Calloc(0x8D8, 0)` and stores the
/// result in `Task::idMap` (0x1C), which an enemy actor reuses for its own work
/// block, so it is *not* a `TaskIdMap` here. The same function hands
/// `work + 0x898` and `work + 0x8B8` to the task's own 0x1C / 0x20 slots. The
/// size below is the allocation, not a guess; only the fields this overlay's
/// matched bodies touch are named.
typedef struct Actor210600Work {
    /* 0x000 */ byte pad_0[0x87C];
    /* 0x87C */ s16  field_87C;
    /* 0x87E */ byte pad_87E[0x4];
    /* 0x882 */ u16  field_882;
    /* 0x884 */ byte pad_884[0x2];
    /* 0x886 */ s16  field_886;
    /* 0x888 */ byte pad_888[0x8];
    /* 0x890 */ s16  field_890;
    /* 0x892 */ byte pad_892[0x46];
} Actor210600Work;
STATIC_ASSERT_SIZEOF(Actor210600Work, 0x8D8);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor143900Msg` and `Actor560800Msg`.
typedef struct Actor210600Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor210600Msg;
STATIC_ASSERT_SIZEOF(Actor210600Msg, 0x4);

/// State table the overlay dispatches through, indexed by `Task::state`. The
/// rodata object `D_actor_210600_80149E24` is exactly its 3 words: create
/// (0x8014B8C8), update (0x8014B434) and `Gp_DestroyEnemy`. Both handlers take
/// the task's 0x20 spawn argument first, like `Actor100300StateFuncTable3`.
typedef struct Actor210600StateFuncTable3 {
    void (*funcs[3])(void* spawnArg2, Task* task);
} Actor210600StateFuncTable3;
STATIC_ASSERT_SIZEOF(Actor210600StateFuncTable3, 0xC);

extern Actor210600StateFuncTable3 D_actor_210600_80149E24;

/// Stack copy `func_actor_210600_8014BA3C` makes before the indirect call.
/// The copy itself moves only the 3 words of `D_actor_210600_80149E24`, but
/// the dispatcher's frame is 0x30 with `$ra` at 0x28, which needs 17-24 bytes
/// of locals - so the copy target is this larger record and not the table
/// type. Only `table` is written here; the trailing fields are unread, and
/// `actor_521100`'s dispatcher builds the same 20-byte table-plus-context
/// shape.
typedef struct Actor210600DispatchCtx {
    /* 0x00 */ Actor210600StateFuncTable3 table;
    /* 0x0C */ s32                        field_C;
    /* 0x10 */ s32                        field_10;
} Actor210600DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor210600DispatchCtx, 0x14);

s32 func_actor_210600_8014B770(Task* task, s32 msgId, Actor210600Msg* msg);

/// Display-object mode handler: 0 hides the object, 1 shows it, 2 and any
/// other value set bit 0x4, with modes 0 and 1 reinstating the object's
/// buffers and modes 0 and 2 arming `Actor210600Work::field_890`. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2);

#endif // ACTOR_210600_H
