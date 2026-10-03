/* The Neo Ark forest rooms' pool of roaming enemies. The room keeps up to five
 * reserve slots of banked HP, sized from game-flag nibbles per session slot:
 * each visit adds the slot's arming count (per location variant) to a running
 * total and caps it at five. The room's dormant slot-4 enemies (hp -999) are
 * revived from this pool. A 0x13EF request names a spawn point, and the room
 * gives the next dormant enemy a banked HP, raises a battle-state reference,
 * sends it the 0x7DB actor command and places it at that point with its yaw.
 * An enemy that retreats reports its HP through message 0x13F4. That HP goes
 * back into a free slot at 110%, capped at the enemy's maximum. When the
 * battle ends, the enemies still banked are folded back into the flag nibbles
 * so the count survives the room change. A frame cooldown spaces the arrivals.
 * Each room runs two such pools, one feeding area 0x1D (nibbles
 * 0x10C/0x10D/0x168) and one feeding area 0xB (nibbles 0x10A/0x10B/0x167).
 * They share the slots, cooldown and request state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ROAMING_ENEMIES_H
#define SRC_SHARED_ROAMING_ENEMIES_H

#include "types.h"

#include "gameplay/message.h"

/// One row of the first roaming-enemy pool's task-message table.
///
/// The pool's arming state installs the table in `Task::msgTable`, which
/// borrows it while the pool task can receive messages; the room task passes
/// on the messages meant for the pool to that task. A row is a message
/// id and the callback that handles it, and the table ends with
/// `TASK_MESSAGE_TABLE_END` and a null callback, so any other id answers zero.
///
/// The row is not a `TaskMessageEntry`: the pool's actor-event callback takes
/// one integer word and returns nothing, so `handler` stores one view for each
/// signature. Dispatch still passes both argument words in their registers and
/// forwards whatever the result register holds, so a sender must not read a
/// result from `ROOM_MESSAGE_ACTOR_EVENT` here. The second pool's handlers all
/// return a result, and its table is made of plain `TaskMessageEntry` rows.
typedef struct {
    s32 messageId;                                                           // Message the row answers, or TASK_MESSAGE_TABLE_END
    union {
        TaskMessageHandler message;                                          // Direction and actor-command rows; NULL on the end marker
        void               (*actorEvent)(Task* task, s32 messageId, s32 hp); // HP a retreating enemy leaves with
    } handler;                                                               // Callback for `messageId`, under the signature that message uses
} RoamerPoolAMessageEntry;
STATIC_ASSERT_SIZEOF(RoamerPoolAMessageEntry, 8);

void roamerBankRetreat(Task* task, s32 arg1, s32 arg2);
void roamerArmPoolA(Task* task);
void roamerTickPoolA(Task* task);
s32  roamerAmbushMsg(Task* task, s32 arg1, TaskMessageArg msg, TaskMessageArg arg3);
void roamerArmPoolB(Task* task);
s32  roamerLatchRequest(Task* arg0, s32 arg1, TaskMessageArg firstArg, TaskMessageArg arg3);

#endif /* SRC_SHARED_ROAMING_ENEMIES_H */
