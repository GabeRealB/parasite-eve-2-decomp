/* Per-stage resolvers that answer which variant of a room an area is showing,
 * from game-progress nibbles. Each takes an in/out RoomEventMsg pair and,
 * unless the request is a query, writes `room` for the areas whose room
 * changes with the story. The stage's map overlay exports the resolver to its
 * rooms; a room that settles a departure's destination itself carries its own
 * copy. The Dryfield rooms
 * answer message 0x13EE for single neighbouring areas with small handlers of
 * the same kind.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A map overlay binds the corresponding resolver identifier before
 * including this header and keeps it defined through the fragment. Shelter
 * carriers declare their instance in the prologue: static in each room, or
 * externally linked in the map overlay's public header.
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

/// Selects the Mine/Shelter resolver's function identifier in the shared body.
///
/// A carrier declares an s32 (RoomEventMsg*, RoomEventMsg*) function before the
/// fragment; that declaration determines its linkage. map_shelter binds the
/// public `mapShelterRoomVariantResolve` before this header. Room carriers use
/// the default `_roomVariantResolveShelter` and declare it static in their
/// prologues. Retain the binding through room_variants_shelter.inc.c, then
/// undefine it. The replacement is one identifier, with no runtime evaluation,
/// arguments, captured values, stringification or token pasting.
#ifndef ROOM_VARIANT_RESOLVE_SHELTER
#define ROOM_VARIANT_RESOLVE_SHELTER _roomVariantResolveShelter
#endif

/// Resolves Neo Ark destination rooms from game progress for a room-local departure.
///
/// Reads `request->areaId` and `request->queryOnly`; on `ROOM_EVENT_EXECUTE` only,
/// writes `reply->room` for the observatory, pavilion, altar, shrine and pyramid.
/// Other areas and queries preserve the initialized reply. The arguments may
/// alias; neither pointer is retained. Always returns 1.
s32 roomVariantResolveNeoArk(RoomEventMsg* request, RoomEventMsg* reply);
s32 roomVariantMainStreetMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantParkingLotMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantMotelBalconyMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

s32 roomVariantMotelBalconyDoorsMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
s32 roomVariantMotelBalconySoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3);

s32 roomVariantSaloonMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantUnderpassMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
s32 roomVariantGasStationMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif /* SRC_SHARED_ROOM_VARIANTS_H */
