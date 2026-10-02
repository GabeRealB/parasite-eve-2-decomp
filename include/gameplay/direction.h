#ifndef GAMEPLAY_DIRECTION_H
#define GAMEPLAY_DIRECTION_H

#include "common.h"

#include "gameplay/actor_spawn_types.h"
#include "gameplay/message.h"

/// Requests a room-specific action from the current trigger.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows this four-byte, two-byte-aligned
/// record until synchronous dispatch returns. Action IDs belong to the room's
/// handler, with zero a valid ID. The argument is a full byte whose meaning
/// depends on that action, such as a destination area or a task's spawn argument.
typedef struct DirectionActionRequest {
    u16 control;  // Trigger control: low byte 5 selects room dispatch; upper bits are preserved
    u8  actionId; // Room-specific action ID
    u8  argument; // Action-specific byte, interpreted by the selected room handler
} DirectionActionRequest;
STATIC_ASSERT_SIZEOF(DirectionActionRequest, 4);

/// Delivers a borrowed `DirectionActionRequest` with a zero second payload word.
enum {
    DIRECTION_MESSAGE_ROOM_ACTION = 0x13EF,
};

/// Optional sound and map-marker sentinels in a `DirectionWarpEntry`.
enum {
    DIRECTION_WARP_SOUND_NONE    = 0,
    DIRECTION_WARP_MAP_FLAG_NONE = 0,
};

/// Spawn and departure options stored in `DirectionWarpEntry.flags`.
enum {
    DIRECTION_WARP_FLAG_NONE            = 0,
    DIRECTION_WARP_FLAG_SCRIPTED_PLAYER = 0x01, // Start the arriving player in scripted actor mode
    DIRECTION_WARP_FLAG_FADE_DEPARTURE  = 0x02, // Fade out when the room query permits the transition
};

/// Placement and transition options for one warp endpoint in an area.
///
/// Stage directories borrow room-overlay entry arrays, indexed by 1-based
/// stage, area and warp IDs minus one. Extents vary by area; there is no
/// terminator. Lookups require a populated area, valid selectors and the owning
/// room overlay still loaded. Consumers copy the complete 56-byte, four-byte-
/// aligned entry by value; actor spawn calls borrow its transforms synchronously.
///
/// The current endpoint supplies departure facing and sounds; the destination
/// endpoint supplies actor placements, a room-local view and arrival effects.
/// Initial session setup suppresses the arrival sound and map-flag update.
/// Later arrivals write 1 to the optional map-marker nibble; room event handlers
/// may write 2 to show a marker. Both unexplained byte spans are retained by
/// whole-entry copies; their role is unproven.
typedef struct {
    ActorSpawnTransform player;         // Arrival placement; yaw also prepares departure facing
    byte                unknown_10[4];  // Stored bytes with no individual consumers; role unproven
    ActorSpawnTransform companion;      // Companion's arrival placement
    byte                unknown_24[4];  // Stored bytes with no individual consumers; role unproven
    s32                 arrivalSound;   // Arrival sound event (DIRECTION_WARP_SOUND_NONE skips playback)
    s32                 departureSound; // Departure sound event for room query result 1 (DIRECTION_WARP_SOUND_NONE skips playback)
    s32                 blockedSound;   // Sound event for room query result 0, which stays in the area (DIRECTION_WARP_SOUND_NONE skips playback)
    u8                  initialView;    // Default 1-based view slot in the destination room
    u8                  flags;          // DIRECTION_WARP_FLAG_* option bits
    u16                 mapFlagId;      // Optional map-marker nibble index (DIRECTION_WARP_MAP_FLAG_NONE skips updates, 1..503 valid)
} DirectionWarpEntry;
STATIC_ASSERT_SIZEOF(DirectionWarpEntry, 0x38);

#endif // GAMEPLAY_DIRECTION_H
