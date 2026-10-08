#ifndef SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

extern WorldCoordRoomLights D_dryfield_night_motel_loft_8018004C[1];

extern WorldCollisionTrigger D_dryfield_night_motel_loft_80180064[12];

extern WorldCollisionTrigger D_dryfield_night_motel_loft_801803F4[14];

extern TaskMessageEntry D_dryfield_night_motel_loft_8017EB1C[6];

extern TaskDesc D_dryfield_night_motel_loft_8017EB4C[1];

extern EvsCommand D_dryfield_night_motel_loft_8017EB78[17];

extern SpriteBatch D_dryfield_night_motel_loft_8017F2D0[2];

extern SpriteBatch D_dryfield_night_motel_loft_8017F2E0[2];

extern SpriteSource D_dryfield_night_motel_loft_8017F2F0[8];

extern SpriteBatch D_dryfield_night_motel_loft_8017F390[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F3A8[13];

extern SpriteBatch D_dryfield_night_motel_loft_8017F4AC[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F4C4[20];

extern SpriteBatch D_dryfield_night_motel_loft_8017F654[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F66C[13];

extern SpriteBatch D_dryfield_night_motel_loft_8017F770[3];

extern SpriteSource D_dryfield_night_motel_loft_8017F788[25];

extern SpriteBatch D_dryfield_night_motel_loft_8017F97C[6];

extern SpriteSource D_dryfield_night_motel_loft_8017F9AC[22];

extern SpriteBatch D_dryfield_night_motel_loft_8017FB64[4];

/// Rebuilds the movable collision wall beside the Jerry Can from its template.
///
/// Copies normal 0's XYZ, the complete first face and vertices 0..3's XYZ
/// into the live grid; the vectors' fourth halfwords and all other geometry
/// and cell lists remain intact. A nonzero `collected` shifts those vertices
/// 3000 integer coordinate units in positive Y, below the walkable floor.
/// Repeated calls restore before shifting, so the offset does not accumulate.
/// Requires both room-owned grids live; no pointer is retained.
void dryfieldNightMotelLoftRebuildJerryCanCollision(s32 collected);

// Callbacks referenced by the overlay's shared data tables.
/// Resolves the night balcony's room after its story scene.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; `task` and `messageId` are unused.
/// Borrows a complete request and writable reply, which may alias. Copies the
/// request; execution selects room 1 before the balcony scene and room 3 after
/// it. Queries retain the copied room. Returns 1 for every destination.
s32 roomVariantMotelBalconyMsg(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Refuses every key-item use in the nighttime loft.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM` and returns `ROOM_KEY_ITEM_USE_REFUSED`.
/// All four arguments are unused; no item is consumed or event started.
s32 dryfieldNightMotelLoftRefuseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);

/// Starts the loft's caption scene for room command 3.
///
/// Handles `ROOM_MESSAGE_COMMAND`; other commands are ignored. Holds scripted
/// player control before spawning the scene task. Returns zero even if spawning
/// fails. The receiver, message ID and second payload are unused.
s32 dryfieldNightMotelLoftCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg);

/// Ignores direction-trigger room-action requests in the nighttime loft.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero. All arguments
/// are unused; the borrowed request is neither read nor retained.
s32 dryfieldNightMotelLoftIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Starts loft-bank sound entry 5 for room sound cue 5.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys do nothing. Uses the current
/// stage with zero pan and attenuation offsets, requiring the room sound bank
/// to be loaded. `task`, `messageId` and `unusedSecondArg` are unused.
/// Returns zero whether or not the sound queue accepts the request.
s32 dryfieldNightMotelLoftPlaySoundCue(Task* task, s32 messageId, s32 cueKey, s32 unusedSecondArg);

/// Plays the loft's first or repeat caption scene and restores player control.
///
/// Starts in state zero after its caller holds scripted control. Pauses actors,
/// plays CAP command 3 before completion or 18 afterward, and uses choice-row
/// step 5. After CAP becomes idle, variant key 31 latches completion; other keys
/// leave progress unchanged. Resumes actors and player control, then kills the
/// task. Requires the loaded CAP resources throughout states 0..2.
void dryfieldNightMotelLoftCaptionSceneTask(Task* task);

/// Sets the current room selector in both the live session and live save.
///
/// Event-script byte callback: `roomId` is a valid room of the current area;
/// the loft encounter supplies 2. Requires both records live. This updates
/// their selectors without loading resources or moving actors.
void dryfieldNightMotelLoftSetCurrentRoom(u8 roomId);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_MOTEL_LOFT_DRYFIELD_NIGHT_MOTEL_LOFT_PRIVATE_H
