#ifndef SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// Bodyless movie launcher and display-controller recipes, in slots 0 and 1.
///
/// Both run at priority `TASK_PRIORITY_CONTROL` with zero spawn payloads. The launcher hands display
/// ownership to the controller; the controller plays the disc-selected movie
/// and restores game presentation. Keep this overlay loaded through restoration.
extern TaskDesc gDryfieldNightDilapidatedHouseMovieTaskDescriptors[2];

/// The house's eight point lights and one cone light, with no directional lights.
///
/// Shared by the single room selector's lighting descriptor. Counts are light
/// elements; coordinate setup and shading mutate the borrowed arrays. The
/// collection and arrays belong to this overlay and remain live while loaded.
extern WorldCoordRoomLights gDryfieldNightDilapidatedHouseRoomLights;

/// 12 interaction quads: 2 warps, 10 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Six CAP quads reject activation during battle; the other actions keep their authored gates.
/// Room binding enables all records through LAST; keep them live until unlinking.
extern WorldCollisionTrigger gDryfieldNightDilapidatedHouseActionTriggers[12];

/// Sight-blocking room quad.
///
/// Origin and corner offsets use room-local game units; normals use 4096 per unit.
/// Room binding links and enables every record through LAST; storage stays live
/// until unlinking or room/view rebinding. The authored low flag bit is unproven.
extern WorldCollisionOccluder gDryfieldNightDilapidatedHouseSightOccluder;

/// Minimum ambient colours for logical views 1..11.
///
/// Entry zero stores the count; each subsequent entry belongs to that 1-based
/// view. RGB uses 16 units per 8-bit colour level. Lighting reads the table
/// while this overlay is loaded and uses default minima above its count.
extern WorldCoordRoomAmbientEntry gDryfieldNightDilapidatedHouseAmbientByView[12];

/// Bodyless transition-task recipe shared by this overlay's gate and event task.
///
/// Runs at priority `TASK_PRIORITY_SCENE` with zero spawn payloads and unused metadata. The gate
/// reads the recipe synchronously after latching the request and destination;
/// keep those snapshots and the room callback code live through session reload.
extern TaskDesc gRoomEventTaskDesc;

/// Message routes borrowed by the registered nighttime house room task.
///
/// Gates the back-street departure, refuses key-item use, and ignores direction
/// actions and CAP room commands. The fifth entry terminates unsupported-ID
/// lookup with a zero reply. Keep this table and its callbacks loaded while the
/// room task can receive messages; never dispatch `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry gDryfieldNightDilapidatedHouseRoomMessages[5];

/// Normal first-visit movie and companion-dialogue script, paired with the skip script.
///
/// Borrows 88 commands through END and starts CAP command 11. Hides both actor
/// models for the movie, selects the scene/audio stream, installs the player
/// and companion animation extensions, and stages their dialogue with CAP cues
/// and frame waits. The authored weapon selector affects only the companion.
/// Frame waits count active interpreter updates; even zero yields an update.
/// Finishes at view 5 with scripted control released. Keep this overlay, both
/// actors and their animation resources loaded through playback and cleanup.
extern EvsCommand gDryfieldNightDilapidatedHouseFirstVisitScript[88];

/// First-visit skip path restoring final actor placements and view 5.
///
/// Borrows 16 commands through END. Fades across the placement and control
/// restoration, cancels scene streaming, and releases CAP/HUD scene control.
/// The interpreter's initial skip delay lets the normal path select its scene
/// before skipping is enabled. That key must match a loaded scene/audio slot
/// before cancellation restores saved random state. Keep this overlay and actor
/// resources loaded through cleanup.
extern EvsCommand gDryfieldNightDilapidatedHouseFirstVisitSkipScript[16];

/// Eight authored point lights contributing in every house view.
///
/// Positions and falloff radii use game units; RGB intensities use Q12.
/// Room setup parents these mutable transforms to the active view and lighting
/// updates their composition and attenuation. Storage belongs to this overlay.
extern WorldCoordPointLight gDryfieldNightDilapidatedHousePointLights[8];

/// The house's all-views cone light, authored with zero RGB intensity.
///
/// Positioned at (-5000, -2500, -4000) game units with 100/3000-unit falloff
/// radii and a full opening of 625 units per 4096-unit turn. Room setup and
/// lighting mutate its transform and attenuation while this overlay is loaded.
extern WorldCoordSpotLight gDryfieldNightDilapidatedHouseConeLights[1];

// Callbacks referenced by the overlay's shared data tables.

/// Refuses `ROOM_MESSAGE_USE_KEY_ITEM` without consuming the selected collected item.
///
/// Ignores both payload words and returns `ROOM_KEY_ITEM_USE_REFUSED`.
s32 dryfieldNightDilapidatedHouseRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArgument);

/// Gates the first back-street departure with the house's door event.
///
/// Borrows an eight-byte request and writable reply, which may be the same
/// record, and copies the whole request. Returns 1 for other destinations or
/// an unlocked door, and 2 while the door event is eligible. Queries preserve
/// progress; execution latches the request, unlocks the door and requests the
/// deferred CAP/sound/transition task, without rollback if allocation fails.
/// Keep the room and its event records loaded through that task's completion.
/// The receiver and message ID are unused.
s32 dryfieldNightDilapidatedHouseGateBackStreetExit(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Ignores `ROOM_MESSAGE_COMMAND` and returns zero without changing room state.
///
/// The integer command ID and its command-specific argument are unused.
s32 dryfieldNightDilapidatedHouseIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArgument);

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION` and returns zero without changing room state.
///
/// The borrowed trigger request is neither read nor retained; the second
/// payload word is unused.
s32 dryfieldNightDilapidatedHouseIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument);

/// Stages audio start for the scene selected by the first-visit event script.
///
/// The selected scene and its prepared buffers must survive CD consumption.
/// A later CD dispatch commits this deferred request; this callback does not
/// change the scene/audio mode.
void dryfieldNightDilapidatedHouseStageSceneAudioStart(void);

/// Enqueues playback of the first-visit script's selected scene/audio session.
///
/// Requires free CD request capacity and prepared playback buffers that remain
/// live through consumption. Without a selected slot, enters playing mode directly.
void dryfieldNightDilapidatedHouseEnqueueScenePlayback(void);

/// Ends first-visit scene streaming and restores the random values saved at selection.
///
/// Requires prior successful scene selection. Leaves CD cancellation, buffers
/// and task teardown to their owners.
void dryfieldNightDilapidatedHouseFinishScene(void);

/// Cancels the first-visit scene when its event script is skipped.
///
/// Discards the deferred CD replacement, requests asynchronous cancellation and
/// immediately ends streaming and restores saved random values. Requires prior
/// successful scene selection; buffer and task teardown remain with their owners.
void dryfieldNightDilapidatedHouseCancelScene(void);

/// Requests the house's movie-display launcher from the first-visit event script.
///
/// Borrows the room's two-entry descriptor table; keep the room loaded until
/// movie playback and game-display restoration finish. Allocation failure is
/// unchecked. This callback has no arguments or result.
void dryfieldNightDilapidatedHouseStartMovie(void);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_PRIVATE_H
