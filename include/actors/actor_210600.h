#ifndef ACTOR_210600_H
#define ACTOR_210600_H

#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// Dual-width view of the animation rate at 0x886. The message handler
/// `func_actor_210600_8014B770` arms the pair as one halfword, while the
/// seeding body copies the low byte into every slot's `GpAnimSlot::field_9`,
/// the rate `Gp_AnimResetSlot` otherwise writes 0x10 into; the same
/// `word` / `half` overlap `ActorsShared80168d3cFlags` describes.
typedef union Actor210600Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor210600Rate;
STATIC_ASSERT_SIZEOF(Actor210600Rate, 0x2);

/// Per-actor state block for the `actor_210600` overlay. `func_actor_210600_8014B8C8`
/// is the overlay's allocator: it calls `Mem_Calloc(0x8D8, 0)` and stores the
/// result in `Task::idMap` (0x1C), which an enemy actor reuses for its own work
/// block, so it is *not* a `TaskIdMap` here. The same function hands
/// `work + 0x898` and `work + 0x8B8` to the task's own 0x1C / 0x20 slots. The
/// size below is the allocation, not a guess; only the fields this overlay's
/// matched bodies touch are named.
typedef struct Actor210600Work {
    /// Animation context the spawn body hands `func_800B3F84` as `anim`, with
    /// its 19 slots (`0x28` each) directly behind it: the pose buffer that
    /// function is handed as `poses` starts at 0x30C, exactly `0x14 + 19 *
    /// 0x28`, the same pack `Actor206100Work` and `Actor151000Work` carry.
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x570];
    /// Animation request state. `field_87C` is the step the seeding body
    /// `func_actor_210600_8014B2C0` dispatches on -- 1 seeks every slot to
    /// `field_882`, 2 resets them, and both settle on 3 and clear the frame
    /// counter at `field_884`, which the running step then counts in.
    /// `field_882` is the requested clip, `field_880` the clip the previous
    /// request latched (the row `D_actor_210600_8015A498` is indexed with);
    /// `field_88A` steps 2 to 3 on the first update that sees it at 2.
    /* 0x87C */ s16             field_87C;
    /* 0x87E */ byte            pad_87E[0x2];
    /* 0x880 */ s16             field_880;
    /* 0x882 */ u16             field_882;
    /* 0x884 */ u16             field_884;
    /* 0x886 */ Actor210600Rate field_886;
    /* 0x888 */ byte            pad_888[0x2];
    /* 0x88A */ s16             field_88A;
    /* 0x88C */ byte            pad_88C[0x4];
    /* 0x890 */ s16             field_890;
    /* 0x892 */ byte            pad_892[0x4];
    /// Clip id of slot 0 the update body last saw, masked to 12 bits and kept
    /// so the once-per-clip effect is not respawned while it is held; the same
    /// slot `Actor110600AnimWork::field_8AC` remembers its cue in.
    /* 0x896 */ s16 field_896;
    /// The light / colour matrices the spawn body stores into
    /// `TmdObject::field_1C` / `field_20`. They sit at the top of the block
    /// rather than at its head, so the overlay's actor keeps its animation
    /// state in the first half of the allocation.
    /* 0x898 */ MATRIX light;
    /* 0x8B8 */ MATRIX color;
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

/// Step table the seeding body `func_actor_210600_8014B2C0` walks: one 5-byte
/// row per clip the previous request latched in `Actor210600Work::field_880`,
/// addressed by the requested clip in `field_882`. The byte it reads is handed
/// to `func_800B4114` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

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

/// Spawn body: allocates the actor's 0x8D8-byte `Actor210600Work`, hands it to
/// the task, and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. `enemy` is the
/// `GpEnemy` the spawner left in the task's 0x20 spawn-argument slot.
void func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task);

s32 func_actor_210600_8014B770(Task* task, s32 msgId, Actor210600Msg* msg);

/// Display-object mode handler: 0 hides the object, 1 shows it, 2 and any
/// other value set bit 0x4, with modes 0 and 1 reinstating the object's
/// buffers and modes 0 and 2 arming `Actor210600Work::field_890`. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2);

#endif // ACTOR_210600_H
