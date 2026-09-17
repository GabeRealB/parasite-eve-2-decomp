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

/// Spawn the actor's effect (id 0x60054) at its own coordinate slot, with the
/// offset vector the state picks: y 0x28A and z 0xC8 for the two states that
/// jump, which differ in x alone (-0x1F4 for 9, -0x3E8 for 7); y 0x258 with x
/// and z zero for the pair that shares a tail (14/17); and zero for the idle
/// pair (0/1). The switch has no `default`, so the other states reach the
/// spawn with the vector as the frame left it. The coordinate is
/// `TmdObject::field_8[arg1]`, the same `TmdObject` the re-init handler above
/// clears `field_C` of, and the vector is a local rather than the work-block
/// slot actor 00100 keeps at 0x898 - the same body as actor 421600's
/// `func_actor_421600_8013E700`, with different offsets.
void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2);

/// Handler for message 0x7DB in the same table: copies the payload's three
/// leading bytes into the work block and, when its `code` word is 0x202,
/// selects the animation state from `mode` -- 1 starts state 2, 0 and 2
/// restart state 0, and 3 keeps state `mode` as it stands. Every other code
/// only stores the bytes. Reached as `fns[code](task, arg1, msg, arg3)`.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, Actor323000Msg* msg, s32 arg3);

#endif
