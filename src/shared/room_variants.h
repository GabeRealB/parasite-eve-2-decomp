/* Per-stage resolvers that answer which variant of a room an area is showing,
 * from game-progress nibbles. Each takes an in/out RoomEventMsg pair and,
 * unless the request is a query, writes `room` for the areas whose room
 * changes with the story. The stage's map overlay exports the resolver to its
 * rooms; a room that settles a departure's destination itself carries its own
 * copy. Dryfield's ROOM_EVENT_MESSAGE_RESOLVE handlers also check departures
 * and start door events; their queries retain the copied room selector.
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

/// Departure choices returned by Dryfield's room-transition handlers.
///
/// A query chooses the caller's departure path: 0 refuses the transition,
/// 1 permits the ordinary departure, and 2 leaves handling to the receiver.
enum {
    ROOM_VARIANT_TRANSITION_REFUSED = 0,
    ROOM_VARIANT_TRANSITION_DIRECT  = 1,
    ROOM_VARIANT_TRANSITION_HANDLED = 2,
};

/// Main-street selection after the final Dryfield story chapter begins.
enum {
    ROOM_VARIANT_DRYFIELD_FINAL_CHAPTER = 4,
    ROOM_VARIANT_MAIN_STREET_FINAL_ROOM = 3,
};

static s32 _roomVariantMainStreetMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _roomVariantParkingLotMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _roomVariantSaloonMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _roomVariantUnderpassMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _roomVariantGasStationMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

// Balcony and loft carriers declare their selected message-handler instances
// in their source prologues or overlay-private headers before their tables.

#endif /* SRC_SHARED_ROOM_VARIANTS_H */
