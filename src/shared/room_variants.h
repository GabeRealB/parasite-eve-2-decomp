/* Per-stage resolvers that answer which variant of a room an area is showing,
 * from game-progress nibbles. Each takes an in/out RoomEventMsg pair and,
 * unless the request is a query, writes `room` for the areas whose room
 * changes with the story. The stage's map overlay carries the resolver for its
 * markers, under the public name rooms call it by; a room that settles a
 * departure's destination itself carries its own copy.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. A map overlay defines its resolver under its public name by
 * defining the library name to it around the fragment's include.
 */

#ifndef SRC_SHARED_ROOM_VARIANTS_H
#define SRC_SHARED_ROOM_VARIANTS_H

#include "types.h"

#include "gameplay/message.h"

s32 roomVariantResolveShelter(RoomEventMsg* arg0, RoomEventMsg* arg1);
s32 roomVariantResolveNeoArk(RoomEventMsg* arg0, RoomEventMsg* arg1);

#endif /* SRC_SHARED_ROOM_VARIANTS_H */
