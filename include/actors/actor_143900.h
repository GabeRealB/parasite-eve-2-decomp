#ifndef ACTOR_143900_H
#define ACTOR_143900_H

#include "common.h"
#include "actors/actors_shared_80132ecc.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/task.h"

/// Per-actor work block for the `actor_143900` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131E70) allocates it with `Mem_Calloc(0x4F0, 0)` and
/// stores the pointer both in `ActorsShared80131f9cWork` and in the task's
/// 0x1C slot, so the size below is the allocation and not a guess. Every
/// other function in the overlay reaches the block through the global.
///
/// `light` and `color` are the two matrices the block itself supplies to the
/// model: `ActorsShared80131f9cSub0` publishes `&work->light` / `&work->color`
/// into `TmdObject::field_1C` / `field_20`, which is what `Tmd_SetupDraw` loads
/// in place of `GsLIGHTWSMATRIX` and `D_80074080` - the same pair
/// `Actor161500Work` and `Actor160600Work` hand over. `anim` is the animation
/// context `Gp_AnimTickIndex` and friends walk, and `slots` and `pad_374` are
/// what `func_800B3F84` fills in beside it.
typedef struct Actor143900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode `func_actor_143900_80132624` selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_143900_80132624` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[2];
    /// Cleared by `ActorsShared80131f9cSub0` beside `field_4EC`; no matched
    /// code reads it back yet, so its role is still unknown.
    /* 0x4EA */ s16  field_4EA;
    /* 0x4EC */ s16  field_4EC; // animation reset argument, latched by the 0x7DB handler
    /* 0x4EE */ byte pad_4EE[2];
} Actor143900Work;
STATIC_ASSERT_SIZEOF(Actor143900Work, 0x4F0);

extern Actor143900Work* ActorsShared80131f9cWork;

/// The overlay's own variant publishes the task's work block here, the same
/// pair of places `func_actor_143900_801328D4` fills in. It is not the block
/// above: that one is the shared body's `Mem_Calloc(0x4F0, 0)`, while this
/// variant allocates `Mem_Calloc(0x4F8, 0)` and keeps two helper tasks at
/// 0x4F0 / 0x4F4 - the `ActorsShared80132eccWork` shape, which is what the
/// dispatcher here parks in the task's 0x1C slot.
extern ActorsShared80132eccWork* D_actor_143900_801496C4;

/// The task that variant publishes next to the block above - `func_actor_143900_801328D4`
/// stores it on the way through, the same pair of places the shared body's
/// `ActorsShared801326b4Task` is filled in. `func_actor_143900_80133254` reads
/// its `extra` to reach the overlay's models; `func_actor_143900_801331C4`
/// hands the task itself to the per-frame update.
extern Task* D_actor_143900_801496C8;

/// Reset argument the overlay's own variant forwards to the reseed, the field
/// the shared body's carriers keep in `D_actor_143900_801413B8`: the
/// play-animation handler latches the preset's `field_C` here and
/// `func_actor_143900_80133144` reads it back.
extern s16 D_actor_143900_80149630;

/// The overlay's message handler table (entry 0x7DB is
/// `func_actor_143900_80133360`); the spawn routine parks its address in the
/// task's `field_24` so the dispatcher can find it, the same slot the shared
/// body's carriers fill with their own table.
extern u8 D_actor_143900_80149634[];

/// Spawn table the spawn routine starts its two helper tasks from, indices 1
/// and 2, whose descriptors are the ones parked in `field_4F0` / `field_4F4`.
extern TaskDesc D_actor_143900_80149664[];

/// Model stream the spawn routine binds into the work block's animation
/// context with `func_800B3F84` (its `arg1`), the overlay's own standing
/// stream; the shared body's carriers hand over their own.
extern u8 D_actor_143900_80149688[];

/// Animation stream `ActorsShared80131f9cSub0` seeds the shared body's slots
/// from - the second `func_800B3F84` source in the overlay, 0x20 below
/// `D_actor_143900_80149688`'s. The overlay's own spawn routine binds the
/// stream above instead.
extern u8 D_actor_143900_801413F8[];

/// Message table `ActorsShared80131f9cSub0` publishes as `Task::field_24`,
/// the same role `D_actor_143900_80149634` plays in the overlay's own spawn
/// routine. Three `GpMsgEntry` records, the same 8-byte shape the
/// 110300/110800 carriers park there.
extern GpMsgEntry D_actor_143900_801413BC[];

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg` and `Actor342400Msg`. This
/// overlay's 0x7DB handler, `func_actor_143900_80132778`, reads the halfword
/// at 0x2.
typedef struct Actor143900Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor143900Msg;
STATIC_ASSERT_SIZEOF(Actor143900Msg, 0x4);

/// Animation preset the overlay's "play animation" message handler applies to
/// the work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- 1 for the blended `func_800B4114` reseed, 2 for a plain one -- and
/// `field_C` becomes the reset argument the reseed forwards. The id range is
/// the handler's own: `func_actor_143900_80132624` takes the first 0x14.
typedef struct Actor143900AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor143900AnimPreset;
STATIC_ASSERT_SIZEOF(Actor143900AnimPreset, 0x10);

void func_actor_143900_80131FD4(Task* task);
void func_actor_143900_8013242C(Task* task);
void func_actor_143900_801324C8(void);
void func_actor_143900_80132A9C(Task* task);
s32  func_actor_143900_80132624(Task* task, s32 arg1, Actor143900AnimPreset* preset);
s32  func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement);
s32  func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg);
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task);
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task);
s32  func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg);

#endif
