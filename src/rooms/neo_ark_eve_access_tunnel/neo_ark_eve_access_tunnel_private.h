#ifndef SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

/// The tunnel's light collection: five mutable point lights, no directional or cone lights.
///
/// Borrowed by room lighting while this overlay is loaded.
extern WorldCoordRoomLights gNeoArkEveAccessTunnelRoomLights;

/// Six directed view-boundary quads connecting views 5/4, 4/2 and 3/2 in both directions.
///
/// The last included record has LAST set; room setup binds and links this mutable pool.
extern WorldCollisionTrigger gNeoArkEveAccessTunnelViewBoundaryTriggers[6];

/// Six facing-sensitive interactions for the elevator, Shelter departure and tunnel parts.
///
/// Includes direct CAP 4 and room commands 6/7; the last included record has LAST set.
/// Keep the mutable pool loaded until room teardown unlinks it.
extern WorldCollisionTrigger gNeoArkEveAccessTunnelActionTriggers[6];

/// One room-space sight-blocking quad, also the LAST record of its mutable pool.
///
/// Keep it loaded until room teardown unlinks it.
extern WorldCollisionOccluder gNeoArkEveAccessTunnelSightOccluders[1];

/// Bodyless priority-32 task that consumes the staged departure to Mine/Shelter.
///
/// Spawn entry 0 with zero payload words after staging `gRoomDeparture`. The
/// descriptor is read synchronously; its callback and staged record require
/// the tunnel overlay to stay loaded until departure has committed the reload.
extern TaskDesc gNeoArkEveAccessTunnelDepartureTaskDesc;

/// Room-message dispatch for transitions, key items, actions, commands and sound.
///
/// Five handlers followed by `TASK_MESSAGE_TABLE_END`; unsupported IDs return
/// zero. The room task borrows the table while it receives messages, so the
/// tunnel overlay must remain loaded through dispatch.
extern TaskMessageEntry gNeoArkEveAccessTunnelRoomMessages[6];

/// Bodyless priority-32 tasks for Shelter departure, elevator departure and CAP completion.
///
/// Slots 0, 1 and 2 respectively; no terminator. Slot 0 takes a destination
/// area byte in `spawnArg1.value`, slot 1 uses the staged elevator selectors,
/// and slot 2 takes a game-flag nibble ID in `spawnArg1.value`. All ignore
/// `spawnArg2`. Descriptors are read at spawn; callbacks require the overlay
/// loaded until their tasks finish.
extern TaskDesc gNeoArkEveAccessTunnelEventTasks[3];

/// Decoded-backdrop marker and batch terminator for mapped view 1, with no sprites.
///
/// The first zero-count record requests image strips; the second ends the list
/// at `SPRITE_BATCH_END`. The view retains this address in its empty source
/// slot as well as its batch slot. Both borrow this overlay's mutable storage.
extern SpriteBatch gNeoArkEveAccessTunnelView1SpriteBatches[2];

// Callbacks referenced by the overlay's shared data tables.

/// Runs the tunnel's confirmation and staged departure to Mine/Shelter.
///
/// Requires state 0..4 and a valid destination area in the low byte of
/// `spawnArg1.value`. CAP reply 12 releases the task and resumes player control.
/// Otherwise waits through CAP completion and one extra tick, holds the player,
/// resolves default room 1/warp 2 at half-turn facing, and hands the copied
/// `RoomDeparture` to the outgoing controller. The tunnel must stay loaded until
/// that controller has consumed the staging record; concurrent departures share it.
void neoArkEveAccessTunnelShelterDepartureTask(Task* task);

/// Runs the tunnel's confirmed elevator transition through a live-save reload.
///
/// Requires state 0..4 and the destination staged by
/// `neoArkEveAccessTunnelResolveTransition`. CAP reply 12 cancels and resumes
/// player control. Otherwise holds the player, waits for
/// `SOUND_NEO_ARK_EVE_TUNNEL_TO_ELEVATOR` to finish, then commits only the staged
/// area, warp and room bytes to the live save and requests a frame-capturing reload.
/// The overlay and staging storage must remain live until that commit.
void neoArkEveAccessTunnelElevatorDepartureTask(Task* task);

/// Refuses key-item use in the EVE access tunnel without changing room state.
///
/// Installed for `ROOM_MESSAGE_USE_KEY_ITEM`. All arguments are ignored;
/// returns `ROOM_KEY_ITEM_USE_REFUSED` so the item menu shows its refusal.
s32 neoArkEveAccessTunnelRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Resolves room selection and handles the tunnel's departure to the EVE elevator.
///
/// The complete borrowed request is copied to the writable reply before resolution;
/// they may alias. Returns 1 for other destinations and 0 for the elevator,
/// including queries. Execute requests play the locked-elevator CAP command or,
/// when unlocked, stage the resolved selectors, arm scene event 24, hold the player
/// and start `neoArkEveAccessTunnelElevatorDepartureTask`. Queries commit no effects.
/// The staging record supports one pending departure while the tunnel is loaded.
s32 neoArkEveAccessTunnelResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Selects CAP responses for the two destructible tunnel parts in encounter variant 11.
///
/// Commands 6 and 7 refer to parts 0 and 1. An intact part responds only during
/// an engaged battle; a destroyed part responds with CAP 8 or 9. Other commands
/// and variants do nothing. Always returns zero; other arguments are ignored.
s32 neoArkEveAccessTunnelHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

/// Handles the tunnel's action-10 request to depart for Mine/Shelter.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request; its argument byte
/// is the destination area. An allowed request holds the player and starts
/// `neoArkEveAccessTunnelShelterDepartureTask`. A blocked request plays CAP 5 and
/// starts a completion task for nibble 431. Other actions do nothing. Returns zero
/// and retains no request pointer; the second payload word and receiver are unused.
s32 neoArkEveAccessTunnelHandleAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Plays the EVE access tunnel's room-bank script for sound cue 1.
///
/// Installed for `ROOM_MESSAGE_SOUND`; `cueKey` is the CAP sound selector.
/// Other keys do nothing. Always returns zero; the remaining arguments are
/// ignored. Queues the sound with zero pan and depth while the room is loaded.
s32 neoArkEveAccessTunnelSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Records the tunnel's CAP completion in the supplied game-flag nibble.
///
/// While CAP is busy the live task waits. Once idle, variant key 12 preserves
/// the flag; every other key writes 2 to the flag ID in `spawnArg1.value`.
/// The task then releases itself regardless of the key. `spawnArg2` and state
/// are unused. The flag ID must be valid and the room overlay remain loaded.
void neoArkEveAccessTunnelRecordCapCompletionTask(Task* task);

#endif // SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
