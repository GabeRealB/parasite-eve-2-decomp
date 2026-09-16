#ifndef ACTOR_356100_H
#define ACTOR_356100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs off `Task::idMap`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_0` / `field_5A` / `field_68` are the
/// same state, clip-id and flag halfwords `Actor01900Work` keeps at those
/// offsets; `field_974` is the `field_5A & 0x3FF` snapshot
/// `func_actor_356100_8016A468` stores (same role as `Actor01900Work.field_8B4`).
/// The halfwords at 0x978..0x982 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes at 0x898..0x8A2; `field_984` is the halfword
/// `func_actor_356100_8016A5DC` snapshots into `field_982`, where
/// `Actor01900Work.field_8A4` sits, and `field_98E` / `field_990` the zero-pair
/// that function clears at +0x8AE / +0x8B0. `field_9BC` holds the 0x180 it
/// writes; `Actor01900_Fn0AA78` puts the same constant in the neighbouring
/// `GpObj` field it names `field_8C8.field_1C` (+0x8E4), not this one.
/// `field_B5C` / `field_B60` are the two helper tasks the exit callback
/// kills; same pair as `Actor01900Work` at +0xC38 / +0xC3C, without the three
/// `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /// Countdown `func_actor_356100_8016A668` decrements every frame and
    /// tests with `(s16)` — the 0x0F / 0x10 state it picks when the counter
    /// wraps is the transition into the state 0xB / 0xC clip it is running.
    /* 0x006 */ u16  field_6;
    /* 0x008 */ byte pad_8[0x52];
    /* 0x05A */ u16  field_5A;
    /* 0x05C */ byte pad_5C[0xC];
    /* 0x068 */ u16  field_68;
    /* 0x06A */ byte pad_6A[0x90A];
    /* 0x974 */ s32  field_974;
    /* 0x978 */ s16  field_978;
    /* 0x97A */ s16  field_97A;
    /* 0x97C */ byte pad_97C[2];
    /* 0x97E */ s16  field_97E;
    /* 0x980 */ byte pad_980[2];
    /* 0x982 */ s16  field_982;
    /* 0x984 */ s16  field_984;
    /* 0x986 */ byte pad_986[8];
    /* 0x98E */ s16  field_98E;
    /* 0x990 */ s16  field_990;
    /* 0x992 */ byte pad_992[0x2A];
    /* 0x9BC */ s16  field_9BC;
    /* 0x9BE */ byte pad_9BE[0x17C];
    /// Threshold `func_actor_356100_8016A834` tests once the enemy is still
    /// alive (`field_40 > 0`) to choose clip 4 or 0x11; the analogue of the
    /// `Actor00100Ctx.field_4C & 2` bit `Actor00100_Fn0BB2C` tests there.
    /* 0xB3A */ s16  field_B3A;
    /* 0xB3C */ byte pad_B3C[0x18];
    /// Random reload `func_actor_356100_8016A668` adds a 4-bit `Gp_LcgState`
    /// draw to when the work block's `field_4` flag is set.
    /* 0xB54 */ u16  field_B54;
    /* 0xB56 */ byte pad_B56[2];
    /// Copy of the first three bytes of the last event
    /// `func_actor_356100_8016A0B8` handled.
    /* 0xB58 */ u8    field_B58[3];
    /* 0xB5B */ byte  pad_B5B;
    /* 0xB5C */ Task* field_B5C;
    /* 0xB60 */ Task* field_B60;
} Actor356100Work;

/// Event record `func_actor_356100_8016A0B8` dispatches on: the first three
/// bytes are copied raw into `Actor356100Work::field_B58`, `w[0]` is the
/// event kind and `w[1]` its sub-code. Same shape as `Actor401300Event`.
typedef union Actor356100Event {
    u8  b[3];
    u16 w[2];
} Actor356100Event;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401000`. The sibling teardown
/// `func_actor_356100_8016A158` reaches those same slots as a `Task*`.
typedef struct Actor356100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor356100Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor356100;

/// Animation view of the work block above, as `func_actor_356100_801633DC`
/// reads it: the `Actor01900AnimWork` layout 0xE0 bytes later, so the two
/// `GpAnimCtx` blocks at 0x1C / 0x4C8 each sit 0x14 bytes before their
/// 0x28-byte slot array. `field_982` is the clip id the slot loop copies
/// minus 3 into `slots[i].field_9`, `field_98A` the clip written whole into
/// `blendSlots[i].field_9`, and `field_98C` the blend weight, the same three
/// roles `Actor01900AnimWork.field_8A2` / `field_8AA` / `field_8AC` have.
typedef struct Actor356100AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[24];
    /* 0x3F0 */ byte       pad_3F0[0xD8];
    /* 0x4C8 */ GpAnimCtx  blendAnim;
    /* 0x4DC */ GpAnimSlot blendSlots[24];
    /* 0x89C */ byte       pad_89C[0xE6];
    /* 0x982 */ s16        field_982;
    /* 0x984 */ byte       pad_984[6];
    /* 0x98A */ s16        field_98A;
    /* 0x98C */ s16        field_98C;
} Actor356100AnimWork;

/// Blends pose slots 1..0x14: the first eleven copy the two clip ids into
/// their slot records and are written from both animation contexts with
/// `0x1000 - field_98C` as the blend weight, the rest only tick. Same body as
/// `Actor01900_Fn01950` / `func_actor_403000_801336B4` with this overlay's
/// slot count.
void func_actor_356100_801633DC(Actor356100* arg0);

void func_actor_356100_80163508(Actor356100* arg0);

/// Event handler: copies the event's first three bytes into the work block's
/// `field_B58`, then dispatches on `w[0] == 0xB05` and `w[1]` — sub-code 1
/// puts the actor in state 0x1E, 0 and 2 in state 0. Anything else returns 0.
s32 func_actor_356100_8016A0B8(Actor356100* arg0, s32 arg1, Actor356100Event* arg2);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `field_54` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the 0x978..0x982 animation
/// slots; otherwise clears the model's root `flg`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `GpObj` flag masks.
void func_actor_356100_8016A21C(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 3`.
void func_actor_356100_8016A2AC(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A340(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A3D4(Actor356100* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern GpAnimArg D_actor_356100_80173244;

/// When the work block's `field_4` flag is set, writes the 0x978..0x982
/// animation slots, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `field_974`. Bit 1 of `field_68` forces `field_0`
/// to 0xE.
void func_actor_356100_8016A468(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the model's `field_C`,
/// clears the enemy's link node and writes the 0x978..0x982 animation slots
/// with `field_9BC` forced to 0x180. Bit 0 of `field_68` forces `field_0` to 7.
/// Same shape as `Actor01900_Fn0AA78` without its two `GpObj` flag masks.
void func_actor_356100_8016A5DC(Actor356100* arg0);

#endif
