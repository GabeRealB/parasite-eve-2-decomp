#ifndef SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_EVE_ACCESS_TUNNEL_NEO_ARK_EVE_ACCESS_TUNNEL_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

extern WorldCoordRoomLights D_neo_ark_eve_access_tunnel_801802D4[1];

extern WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801802EC[6];

extern WorldCollisionTrigger D_neo_ark_eve_access_tunnel_801804B4[6];

extern WorldCollisionOccluder D_neo_ark_eve_access_tunnel_80180720[1];

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EA88;

extern TaskMessageEntry D_neo_ark_eve_access_tunnel_8017EA94[6];

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[3];

extern SpriteBatch D_neo_ark_eve_access_tunnel_8017F17C[2];

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
