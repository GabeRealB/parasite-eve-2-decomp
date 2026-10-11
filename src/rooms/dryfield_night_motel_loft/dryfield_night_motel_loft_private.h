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

/// Point-light collection shared by both room selectors.
///
/// Contains no directional or cone lights. The room owns the mutable light
/// array; coordinate setup and queries require this overlay to remain loaded.
extern WorldCoordRoomLights gDryfieldNightMotelLoftRoomLights;

/// Twelve directed view boundaries shared by both loft room selectors.
///
/// Parameter bytes pair source/destination logical views 2..7; the final
/// record carries LAST and participates in collision. Setup binds transforms,
/// links and enables all records. Room-owned mutable storage lives until unlink.
extern WorldCollisionTrigger gDryfieldNightMotelLoftViewBoundaryTriggers[12];

/// Balcony-door warp and thirteen CAP/room-command interaction quads.
///
/// Element 1 is the Jerry Can interaction, disabled after collection; element
/// 3 routes command 3 to the room instead of starting CAP directly. The final
/// quad carries LAST. Coordinates/radii use integer room units; normals use ONE.
/// Setup and hit processing mutate these room-owned records while linked.
extern WorldCollisionTrigger gDryfieldNightMotelLoftActionTriggers[14];

/// Room-controller dispatch for destination resolution, item use, actions, commands and sounds.
///
/// Five handlers precede TASK_MESSAGE_TABLE_END. The controller borrows this
/// table and its callback code for its lifetime; payload types follow each ID.
extern TaskMessageEntry gDryfieldNightMotelLoftMessageHandlers[6];

/// Bodyless caption-scene task recipe with execution priority 32.
///
/// Command 3 spawns descriptor zero after holding player control. Spawning
/// reads this room-owned recipe synchronously and retains only its callback.
extern TaskDesc gDryfieldNightMotelLoftCaptionSceneTaskDesc;

/// CAP-cued Stalker encounter started once after the Jerry Can is collected.
///
/// Runs CAP command 4, resets the player's equipped-bank idle, then waits for
/// three CAP cues around the room-2 switch, Stalker drop and area sound entries
/// 6/7. Zero-frame waits still yield one update. It engages battle, restores
/// the view, waits two countdown updates and releases scripted player control.
/// The interpreter retains the stream; its callbacks and borrowed animation
/// and command payloads must stay loaded through execution. END terminates it.
extern EvsCommand gDryfieldNightMotelLoftStalkerEncounterScript[17];

/// Mapped view 1's decoded-image background selector and empty sprite list.
///
/// The zero-count first record and SPRITE_BATCH_END select no sources. The
/// view retains this address in sources.empty; storage lives with the overlay.
extern SpriteBatch gDryfieldNightMotelLoftView1SpriteBatches[2];

/// Empty sprite list shared by mapped views 2 and 9, retaining decoded-image strips.
///
/// A zero-count background selector precedes SPRITE_BATCH_END. Both views
/// retain the list in sources.empty; mutable storage lives with the overlay.
extern SpriteBatch gDryfieldNightMotelLoftView2SpriteBatches[2];

/// Eight foreground texture rectangles and sorting depths shared by mapped views 3 and 10.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. One range selects all elements; mutable storage lives with
/// the overlay and cached packets retain their initialized geometry.
extern SpriteSource gDryfieldNightMotelLoftView3SpriteSources[8];

/// Background selector and complete eight-source range shared by mapped views 3 and 10.
///
/// The zero-count first record keeps decoded-image strips; SPRITE_BATCH_END
/// terminates the list. Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView3SpriteBatches[3];

/// Thirteen foreground texture rectangles and sorting depths for mapped view 4.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. One range selects all elements; mutable storage lives with
/// the overlay and cached packets retain their initialized geometry.
extern SpriteSource gDryfieldNightMotelLoftView4SpriteSources[13];

/// Mapped view 4's background selector, complete thirteen-source range and end sentinel.
///
/// The zero-count first record keeps decoded-image strips; SPRITE_BATCH_END
/// terminates the list. Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView4SpriteBatches[3];

/// Twenty foreground texture rectangles and sorting depths for mapped view 5.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. One range selects all elements; mutable storage lives with
/// the overlay and cached packets retain their initialized geometry.
extern SpriteSource gDryfieldNightMotelLoftView5SpriteSources[20];

/// Mapped view 5's background selector, complete twenty-source range and end sentinel.
///
/// The zero-count first record keeps decoded-image strips; SPRITE_BATCH_END
/// terminates the list. Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView5SpriteBatches[3];

/// Thirteen foreground texture rectangles and sorting depths for mapped view 6.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. One range selects all elements; mutable storage lives with
/// the overlay and cached packets retain their initialized geometry.
extern SpriteSource gDryfieldNightMotelLoftView6SpriteSources[13];

/// Mapped view 6's background selector, complete thirteen-source range and end sentinel.
///
/// The zero-count first record keeps decoded-image strips; SPRITE_BATCH_END
/// terminates the list. Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView6SpriteBatches[3];

/// Twenty-five foreground texture rectangles and depths shared by mapped views 7 and 11.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. Four ranges select all elements using palette rows 255..258;
/// mutable storage lives with the overlay. Cached packets snapshot geometry.
extern SpriteSource gDryfieldNightMotelLoftView7SpriteSources[25];

/// Background selector and four contiguous source ranges shared by mapped views 7 and 11.
///
/// Ranges cover all twenty-five sources as 18, 5, 1 and 1 elements, followed by
/// SPRITE_BATCH_END. The zero-count first record keeps decoded-image strips.
/// Serialized `SpriteBatch.field_6` bytes retain 3, 2 and 1 in three ranges; their role is
/// unproven. Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView7SpriteBatches[6];

/// Twenty-two foreground texture rectangles and sorting depths for mapped view 8.
///
/// Positions and sizes are view-centred pixels, UVs texels, and depths unscaled
/// sorting values. Two ranges select all elements; mutable storage lives with
/// the overlay and cached packets retain their initialized geometry.
extern SpriteSource gDryfieldNightMotelLoftView8SpriteSources[22];

/// Mapped view 8's background selector, two source ranges and end sentinel.
///
/// Ranges cover all twenty-two sources as 10 and 12 elements, followed by
/// SPRITE_BATCH_END. The zero-count first record keeps decoded-image strips.
/// The first range retains 1 in serialized `SpriteBatch.field_6`; its role is unproven.
/// Mutable range storage lives with the room overlay.
extern SpriteBatch gDryfieldNightMotelLoftView8SpriteBatches[4];

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
