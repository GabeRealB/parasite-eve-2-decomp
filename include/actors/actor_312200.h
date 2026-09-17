#ifndef ACTOR_312200_H
#define ACTOR_312200_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Private work block of the actor 312200 task, hanging off `Task::idMap`,
/// `Mem_Calloc(sizeof(Actor312200Work), 0)` in the spawn handler.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x08 is
/// the heading `func_actor_312200_801635CC` reads back from the root
/// coordinate, two halfwords earlier than `ActorsShared80169f74Work::yaw`.
///
/// The named tail is the actor state block shared by this family, laid out like
/// `Actor110600Work`: `field_0` is the state word the 0x7DB handler raises,
/// `field_4` the live-actor flag every state handler tests on entry, `field_88C`
/// the work state, and `field_892` / `field_896` the two timers the state
/// handlers arm.
///
/// `field_8B4` / `field_8B6` / `field_8B8` are the record
/// `func_actor_312200_801636CC` leaves of the last 0x7DB command it saw: the
/// sender id's two bytes - stored as the bytes they are read as, not as the
/// halfword the handler tests - and then the action halfword.
///
/// `field_8BC` is the display node the spawn handler
/// `func_actor_312200_80163178` builds in place and hands to `Gp_LinkObj` - the
/// `GpObj` whose `field_C` it points at a three-entry `GpRec18` table at 0x8DC.
/// `func_actor_312200_80163778` clears bit 0x8000 of that node's `flags`.
typedef struct Actor312200Work {
    /* 0x000 */ s16 field_0;
    /// Second halfword of the state word above, set to -1 by the spawn handler.
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x2];
    /* 0x008 */ s16  yaw;
    /* 0x00A */ byte pad_A[0x6];
    /// Animation context the spawn body hands `func_800B3F84` first, with its
    /// 19 slots directly behind it: the pose buffer that function is handed
    /// fourth starts at 0x31C, exactly 0x10 + 0x14 + 19 * 0x28, the same pack
    /// `Actor210600Work` and `Actor110600Work` carry.
    /* 0x010 */ GpAnimCtx  anim;
    /* 0x024 */ GpAnimSlot slots[0x13];
    /// The flag halfword the per-tick callback tests: it falls inside the slot
    /// array, being the second slot's `field_10`, because the animation state
    /// runs from 0x24 to the pose buffer.
    /* 0x31C */ byte poses[0x570];
    /* 0x88C */ s16  field_88C;
    /* 0x88E */ byte pad_88E[0x4];
    /* 0x892 */ s16  field_892;
    /* 0x894 */ byte pad_894[0x2];
    /* 0x896 */ s16  field_896;
    /* 0x898 */ byte pad_898[0x14];
    /// The two bytes the spawn handler arms next to the display node; they sit
    /// immediately before the 0x7DB record, so they are the actor's own copy of
    /// that state rather than part of a message. `field_8AD` is read back with
    /// `lb` by the tick handler, so it is signed like the record halfwords.
    /* 0x8AC */ u8   field_8AC;
    /* 0x8AD */ s8   field_8AD;
    /* 0x8AE */ byte pad_8AE[0x6];
    /* 0x8B4 */ s16  field_8B4;
    /* 0x8B6 */ s16  field_8B6;
    /* 0x8B8 */ s16  field_8B8;
    /* 0x8BA */ byte pad_8BA[0x2];
    /// Display node: `GpObj` at 0x8BC, its `GpRec18` table at 0x8DC.
    /* 0x8BC */ GpObj   field_8BC;
    /* 0x8DC */ GpRec18 recs[3];
    /* 0x924 */ byte    pad_924[0x20];
    /// The light / colour matrices the spawn handler stores into
    /// `TmdObject::field_1C` / `field_20`, at the top of the block.
    /* 0x944 */ MATRIX light;
    /* 0x964 */ MATRIX color;
} Actor312200Work;
STATIC_ASSERT_SIZEOF(Actor312200Work, 0x984);

/// The four bytes of the id 0x7DB command, seen from the receiving end: the
/// handler records the payload a byte at a time but tests the sender id and the
/// action selector as the two halfwords they are, so both views are named. Same
/// shape as `Actor444000Msg7DB` / `Actor342000Cmd`.
typedef union Actor312200Msg7DB {
    u8 b[4];
    struct {
        /* 0x0 */ u16 id;
        /* 0x2 */ u16 action;
    } h;
} Actor312200Msg7DB;
STATIC_ASSERT_SIZEOF(Actor312200Msg7DB, 0x4);

/// Placement opcode: the three longs of `placement->pos` are copied onto the
/// actor's root coordinate, the Euler angles are applied X / Y / Z, and the
/// resulting heading is read back out of the matrix Z-axis with `ratan2` and
/// cached in the work block.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

/// Id 0x7DB command handler, listed in `D_actor_312200_80169F5C` next to the
/// 0x7D4 placement opcode: the payload is recorded in the work block, and a
/// command from sender 0x301 additionally selects the work state - action 1
/// takes state 2 and latches itself in the 0x892 timer, actions 2, 3 and 4 take
/// state 1 and latch themselves there. Either way the actor's `field_0` state
/// word is raised to 1 and the handler reports success.
s32 func_actor_312200_801636CC(Task* task, s32 msgId, Actor312200Msg7DB* msg);

/// Absolute; nonzero skips the per-frame state handler entirely.
extern u8 D_801153F4;

/// Per-tick handler, called with the task in the second argument: it builds the
/// actor's two-entry handler table on the stack - the show handler
/// `func_actor_312200_80163778` at index 0 and the tick handler
/// `func_actor_312200_801637CC` at index 1 - and, unless the global
/// `D_801153F4` holds the actor, dispatches the work block's `field_0` state.
/// `enemy` is the spawn argument the dispatcher hands in first, unused here.
///
/// The state move is recorded on the way in: `field_4` takes 1 when `field_0`
/// differs from the latched `field_2`, which is then re-latched from `field_0`.
/// The tail re-syncs the model with the actor: the display node's `GpRec18`
/// record is cleared while it is occupied (`recs[0].field_4`), the root
/// coordinate's translation is re-propagated over the three part coordinates
/// (`func_800D7A9C`, start 0, count 3) while `field_8AD` is set, and `field_8AD`
/// is then refreshed from that coordinate's `flg` - so the propagation runs on
/// the frame after the coordinate is dirtied. While the room is live
/// (`Game_Session->field_4D`) the coordinate's `flg` is dropped first and, from
/// view 0x10 with the 0x7DB action `field_8B8` at 1, sound 0x51030008 is queued
/// with the model's pan and depth as `s8`.
///
/// The `SOFT_BARRIER` in the body is a matching aid, not the original's: it
/// keeps the model lookup ahead of the handler table, which the patched
/// scheduler otherwise pulls that table's first `lui` in front of.
void func_actor_312200_80163370(GpEnemy* enemy, Task* task);

/// Per-tick state callback: on a live actor it re-enters state 2 with the
/// 0x896 timer armed at 0x10, then hands the tick to `func_actor_312200_801637CC`'s
/// anim/particle update. Once the 0x892 timer has run its 0x10 ticks with the
/// task's flag bit 0 set, it drops the state to 1 and shorts the timer to 4.
void func_actor_312200_801637CC(Task* task);

/// Show handler: on a live actor it sets the enemy's `node.field_4`, raises the
/// 0x80 draw bit of the model's `TmdObject::field_C`, clears
/// `GpEnemy::field_4D` and drops bit 0x8000 of the display node's `flags`.
void func_actor_312200_80163778(Task* task);

/// Spawn handler and the task's create callback: allocates the work block and
/// stores it in `Task::idMap`, then seeds the enemy object, the model's root
/// coordinate and the animation context from the `TmdObject` in `Task::extra` -
/// its `field_1C` / `field_20` are pointed at the block's light and colour
/// matrices, `GpEnemy::field_4` at the root coordinate's matrix, and
/// `GpEnemy::field_18` at the model's third part coordinate. `enemy` is the
/// `GpEnemy` the spawner left in the task's 0x20 spawn-argument slot.
void func_actor_312200_80163178(GpEnemy* enemy, Task* task);

void func_actor_312200_80162FB4(Task* task);

#endif // ACTOR_312200_H
