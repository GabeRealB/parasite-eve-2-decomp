#ifndef ACTOR_323000_H
#define ACTOR_323000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs behind `Task::idMap`. Only the
/// fields the handlers touch are known: `field_4` is the live-actor flag
/// every state handler tests, and `field_6` / `field_828` / `field_82E` /
/// `field_832` / `field_83E` / `field_840` are the animation-state slots the
/// `Actor00100Work` family keeps at the same offsets, where
/// `Actor00100_Fn0B52C` writes them in the same order.
typedef struct Actor323000Work {
    /// Animation state, the same slot `ActorShared80164af0Work` names
    /// `field_0`; `func_actor_323000_80164A54` picks it from a message, and
    /// the animation handler `ActorsShared80164af0` restarts it.
    /* 0x000 */ s16  field_0;
    /* 0x002 */ byte pad_2[2];
    /* 0x004 */ s16  field_4;
    /// Frame counter `func_actor_323000_80163A30` advances; zeroed by the
    /// re-init handler below.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x820];
    /* 0x828 */ s16  field_828;
    /* 0x82A */ byte pad_82A[4];
    /* 0x82E */ s16  field_82E;
    /* 0x830 */ byte pad_830[2];
    /* 0x832 */ s16  field_832;
    /* 0x834 */ byte pad_834[0xA];
    /* 0x83E */ s16  field_83E;
    /* 0x840 */ s16  field_840;
    /* 0x842 */ byte pad_842[0xDA];
    /// Three bytes `func_actor_323000_80164A54` takes from a message payload
    /// one at a time; nothing else in this overlay reads them.
    /* 0x91C */ u8 field_91C;
    /* 0x91D */ u8 field_91D;
    /* 0x91E */ u8 field_91E;
} Actor323000Work;

/// Animation view of the same work block, as `func_actor_323000_8016331C`
/// reads it: the pose context at 0x1C and its blend twin at 0x420, each
/// followed by 0x28-byte `GpAnimSlot`s, plus the weight and the two clip-id
/// bytes the loop copies into them. The pads stand in for the rest of the
/// block -- a slot array cannot span the fields `Actor323000Work` names at
/// 0x828..0x830, and 0x420 is not a whole number of slots past 0x30. The clip
/// ids are bytes here where `Actor323000Work.field_832` is the halfword
/// `func_actor_323000_80164C58` writes.
typedef struct Actor323000AnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[25];
    /* 0x418 */ byte       pad_418[8];
    /* 0x420 */ GpAnimCtx  blendAnim;
    /* 0x434 */ GpAnimSlot blendSlots[25];
    /* 0x81C */ byte       pad_81C[0x16];
    /// Clip id the primary slots are seeded from, three behind the one they
    /// play.
    /* 0x832 */ u8   field_832;
    /* 0x833 */ byte pad_833[7];
    /// Clip id the blend slots are seeded from as it stands.
    /* 0x83A */ u8   field_83A;
    /* 0x83B */ byte pad_83B;
    /// Blend weight written into the two pose-context copies.
    /* 0x83C */ s16 field_83C;
} Actor323000AnimWork;

/// Payload of message 0x7DB, the handler table `D_actor_323000_801739D0`
/// carries for this overlay. `code` is the sub-command the handler selects on
/// (0x202 here) and `mode` its variation; the sender writes both as words, and
/// `func_actor_323000_80164A54` also reads their three leading bytes
/// individually, which is what `Actor323000MsgBytes` is for.
typedef struct Actor323000Msg {
    /* 0x0 */ u16 code;
    /* 0x2 */ u16 mode;
} Actor323000Msg;

/// Byte view of `Actor323000Msg`: `b0` and `b1` are the halves of `code` and
/// `b2` the low half of `mode`.
typedef struct Actor323000MsgBytes {
    /* 0x0 */ u8 b0;
    /* 0x1 */ u8 b1;
    /* 0x2 */ u8 b2;
} Actor323000MsgBytes;

/// Per-frame tick of the live actor, run once the work block's animation
/// slots are set up.
void func_actor_323000_80163A30(Task* task);

/// Re-init handler (table `D_actor_323000_80161E24`, index 2): when the work
/// block's `field_4` flag is set, flags the enemy's link node, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots and clears the frame counter. Either way the tick
/// above runs. Reached as `fns[state](task->spawnArg2, task)`, like the other
/// handler tables in this family.
void func_actor_323000_80164C58(GpEnemy* enemy, Task* task);

/// Handler for message 0x7DB in the same table: copies the payload's three
/// leading bytes into the work block and, when its `code` word is 0x202,
/// selects the animation state from `mode` -- 1 starts state 2, 0 and 2
/// restart state 0, and 3 keeps state `mode` as it stands. Every other code
/// only stores the bytes. Reached as `fns[code](task, arg1, msg, arg3)`.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3);

/// Blends pose slots 1..0x11: the first eleven copy the two clip-id bytes into
/// their slot records and are written from both animation contexts with
/// `0x1000 - field_83C` as the blend weight, the rest only tick. Same body as
/// `func_actor_356100_801633DC` / `func_actor_421600_80133B30` with this
/// overlay's slot count and a blend weight read from the work block.
void func_actor_323000_8016331C(Task* task);

#endif
