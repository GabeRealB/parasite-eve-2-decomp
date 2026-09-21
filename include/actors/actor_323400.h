#ifndef ACTOR_323400_H
#define ACTOR_323400_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs behind `Task::work`; only the
/// slots its handlers touch are known. `field_0` is the animation state the
/// message handler below selects, the same slot `Actor323000Work` names
/// `field_0`; `field_4` is the live flag the dispatcher
/// `func_actor_323400_801644C4` raises when that state changes and the re-init
/// handler below clears. `field_6` is the frame counter
/// `func_actor_323400_801641C4` advances; zeroed by that handler's re-init
/// path. The `field_828` / `field_832` / `field_83E` / `field_840` group is
/// the animation-state set the `Actor00100Work` family keeps at the same
/// offsets, written in the same order by the sibling
/// `func_actor_323000_80164C58`. The block is the same 0x934 bytes the spawn
/// handler allocates, and the three bytes at 0x91C are the message payload's
/// three leading bytes, as in that sibling.
typedef struct Actor323400Work {
    /* 0x000 */ s16 field_0;
    /* 0x002 */ s16 field_2;
    /* 0x004 */ s16 field_4;
    /// Frame counter `func_actor_323400_801641C4` advances; zeroed by that
    /// handler's re-init path.
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
    /// Three bytes `func_actor_323400_80164974` takes from a message payload
    /// one at a time; nothing else in this overlay reads them.
    /* 0x91C */ u8   field_91C;
    /* 0x91D */ u8   field_91D;
    /* 0x91E */ u8   field_91E;
    /* 0x91F */ byte pad_91F[0x15];
} Actor323400Work;
STATIC_ASSERT_SIZEOF(Actor323400Work, 0x934);

/// Payload of the message the handler below answers. `code` is the sub-command
/// it selects on (0x1602 here) and `mode` its variation; the sender writes both
/// as halfwords, and the handler also reads their three leading bytes
/// individually, which is what `Actor323400MsgBytes` is for.
typedef struct Actor323400Msg {
    /* 0x0 */ u16 code;
    /* 0x2 */ u16 mode;
} Actor323400Msg;

/// Byte view of `Actor323400Msg`: `b0` and `b1` are the halves of `code` and
/// `b2` the low half of `mode`.
typedef struct Actor323400MsgBytes {
    /* 0x0 */ u8 b0;
    /* 0x1 */ u8 b1;
    /* 0x2 */ u8 b2;
} Actor323400MsgBytes;

/// Per-frame tick of the live actor, run once the work block's animation
/// slots are set up.
void func_actor_323400_80163B58(Task* task);

/// Re-init handler (table `D_actor_323400_80161E24`, index 1): when the work
/// block's `field_4` flag is set, clears the enemy's link-node flag, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots. Either way the tick above runs. Reached as
/// `fns[state](task->spawnArg2, task)`, like the other handler tables in this
/// family.
void func_actor_323400_80164BD0(GpEnemy* enemy, Task* task);

/// State handler (table `D_actor_323400_80161E24`, index 2): when the work
/// block's `field_4` flag is set, flags the enemy's link node, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots and clears `field_6` before the tick. Otherwise it
/// advances `field_6` and, on steps 9, 10, 12 and 13, spawns effect 0x60054
/// at the matching model-part coordinate while the room's effect set is live
/// (y 0x2BC at parts 9 and 7, y 0x258 at 14 and 17), plays a placed sound on
/// step 10, and always spawns at part 1 on step 13. The tick then runs and
/// the model root's `flg` is cleared. Reached as
/// `fns[state](task->spawnArg2, task)`.
void func_actor_323400_801641C4(GpEnemy* enemy, Task* task);

/// Message handler: copies the payload's three leading bytes into the work
/// block and, when its `code` word is 0x1602, sets the model root's coordinate
/// and selects the animation state from `mode` -- 1 writes the jump vector
/// (0x4330, `mode`, 0xA8C) into the root `GsCOORDINATE2` and clears its flag
/// (start state 2), 0 and 2 restart state 0, and every other mode only stores
/// the bytes. Reached as `fns[code](task, arg1, msg, arg3)`.
s32 func_actor_323400_80164974(Task* task, s32 arg1, Actor323400Msg* msg, s32 arg3);

/// Re-init handler (table `D_actor_323400_80161E24`, index 3): the same shape
/// as the handler above, but its animation-state slots hold a different state
/// (`field_82E` is 2 and `field_828` is 1, where the sibling writes none and
/// 2). The flag-set path clears `enemy->node.flags`, drops the model's root
/// `field_C`, rebuilds its buffers and writes those slots; the flag-clear path
/// instead clears the `flg` word of the model's trailing coord block, so the
/// coordinate matrix is rebuilt from the actor transform on the next draw.
/// Either way the tick runs last.
void func_actor_323400_80164C4C(GpEnemy* enemy, Task* task);

#endif
