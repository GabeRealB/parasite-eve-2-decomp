/* Per-stage resolvers that answer which variant of a room an area is showing,
 * from game-progress nibbles. Each takes an in/out RoomEventMsg pair and,
 * unless the request is a query, writes `room` for the areas whose room
 * changes with the story. The stage's map overlay carries the resolver for its
 * markers, under the public name rooms call it by; a room that settles a
 * departure's destination itself carries its own copy. The Dryfield rooms
 * answer message 0x13EE for single neighbouring areas with small handlers of
 * the same kind.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A map overlay defines its resolver under its public name by
 * defining the library name to it around the fragment's include.
 */

#ifndef SRC_SHARED_ROOM_VARIANTS_H
#define SRC_SHARED_ROOM_VARIANTS_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

/// A stage's room-variant resolver, as a room holds one to call it indirectly.
///
/// The resolver reads the destination area from `request` and, unless the
/// request is a query, writes the room that area currently shows to `reply`.
/// Both may be the same record. It always returns 1.
typedef s32 (*RoomVariantResolver)(RoomEventMsg* request, RoomEventMsg* reply);

s32 roomVariantResolveShelter(RoomEventMsg* arg0, RoomEventMsg* arg1);
s32 roomVariantResolveNeoArk(RoomEventMsg* arg0, RoomEventMsg* arg1);
s32 roomVariantMainStreetMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantParkingLotMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantMotelBalconyMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

s32 roomVariantMotelBalconyDoorsMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
s32 roomVariantMotelBalconySoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3);

s32 roomVariantSaloonMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantUnderpassMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantGasStationMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif /* SRC_SHARED_ROOM_VARIANTS_H */
