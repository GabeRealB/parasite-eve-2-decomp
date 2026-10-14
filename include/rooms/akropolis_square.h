#ifndef INCLUDE_ROOMS_AKROPOLIS_SQUARE_H
#define INCLUDE_ROOMS_AKROPOLIS_SQUARE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Saved-area updates after the opening square cutscene advances Akropolis progress.
///
/// All save modes select variant 3 in the patio and cafeteria, variant 2 in
/// MIST parking, and mark those areas on the map. AREA_APPLY_END terminates the
/// list. Shared cutscene carriers borrow it only for stage 1, area 1; the square
/// overlay must be loaded when that branch applies the updates.
extern AreaApplyRec gAkropolisSquarePostCutsceneAreaUpdates[4];

/// Actor layout variants for the Akropolis square.
///
/// Indexed directly by `GameLocationKey.variant` (0..2); no terminator.
/// Slot 1 publishes a placement/resource pair; all other slots are NULL pairs.
/// The telephone scene selects empty variant 2, once the grinning stranger has gone, to clear the default layout.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAkropolisSquareAreaVariants[3];

/// Collision resources for the square's single room, borrowed by the Akropolis map.
///
/// Room 1 supplies a grid, sixteen view boundaries, twenty-six action triggers
/// and no sight occluders. Setup binds and links the mutable resources; keep the
/// room overlay loaded until the active lists are unlinked. No array terminator.
extern WorldCollisionRoomResources gAkropolisSquareRoomCollisionResources[1];

/// Logical-view mapping for the square's single room, borrowed by the Akropolis map.
///
/// Room 1 uses `gViewIdentityMap`: logical views 1..15 select the same 1-based
/// camera, image and sprite indices. The directory has no terminator; gameplay
/// owns the shared byte map. Keep this room overlay loaded while using the directory.
extern u8* gAkropolisSquareViewMaps[1];

/// Number of logical view slots in the square's single room.
///
/// Room 1 has fifteen slots. The signed halfword count bounds reverse mapping
/// through `gAkropolisSquareViewMaps`; it counts elements, with no terminator.
/// The Akropolis map borrows this storage while the room overlay is loaded.
extern ViewCount gAkropolisSquareViewCounts[1];

/// Model lighting for the square's single room, borrowed by the Akropolis map.
///
/// Room 1 supplies sixteen mutable point lights, no directional or cone lights,
/// and minimum ambient RGB of 16 for each of its fifteen logical views. RGB
/// minima use 16 units per 8-bit colour level. The room overlay owns the record
/// and referenced storage; keep it loaded while lighting uses them.
extern WorldCoordRoomLighting gAkropolisSquareRoomLightingTable[1];

/// Seven warp endpoints for the square, indexed by 1-based warp ID minus one.
///
/// Slots 1/7 share west-elevator placement but select views 13/15; slot 2 is
/// the east-elevator placement. Slots 3/5 and 4/6 pair ordinary and scripted
/// patio/forked-road placements. Positions use room game units and yaw uses
/// 4096 units per turn. No terminator; the Akropolis map borrows the array while
/// the overlay is loaded, and consumers copy complete entries by value.
extern DirectionWarpEntry gAkropolisSquareWarpEntries[7];

/// Foreground sprite resources for mapped square views 1..15, stored at index minus one.
///
/// Views 3..8 have source sprites. Views 1/2 and 9..15 retain their empty batch
/// lists in sources.empty and draw decoded background strips. Every drawAreas
/// pointer is NULL. Lists and sources are mutable room-owned storage borrowed
/// by the Akropolis map and sprite cache; keep the overlay loaded while used.
extern SpriteView gAkropolisSquareSpriteViews[15];

/// Fifteen square cameras in mapped 1-based view order, with no terminator.
///
/// Rotations use Q12 unit coefficients; translations are negated camera origins
/// in room game units, applied before rotation. Projection distances are pixels.
/// The Akropolis map and queued camera tasks borrow these room-owned records;
/// keep the overlay loaded until queued applications finish copying them.
extern ViewCamera gAkropolisSquareViewCameras[15];

/// Collision and footstep policies indexed by the square's surface classes 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 4 supplies walking/running cue pairs, with scripted jumps reusing walking;
/// classes 0..3 and 5..7 share a record with no footstep cues. The directory has
/// no terminator and borrows room-owned records; keep the square overlay loaded.
extern WorldCollisionSurfaceProperties* gAkropolisSquareSurfacePropertiesByClass[8];

/// Starts the square's one-time telephone scene: the grinning stranger by the phone
/// runs off and the phone rings until answered. Applies placement variant 2.
///
/// Returns 0 when newly armed, 1 if the saved scene flag was already set.
/// The argument is ignored. Commits the flag and session event state before
/// spawning; allocation failure does not undo them. Requires the square overlay
/// and its event resources to remain loaded through the asynchronous sequence.
s32 akropolisSquareStartTelephoneScene(s32 unused);

/// Registers the square's room effects and emits the telephone's light in the views that show it.
///
/// Requires a counted effect with a single-coordinate body and zeroed
/// `EffectWork` in `spawnArg2.pointer`; its exit callback owns that allocation.
/// State 0 publishes the effect receiver, resets the light's
/// mode and spawns floor/plane player reflections. State 1 emits glows in
/// logical views 4, 6, 7, 10, 14 and 9. Views must be valid before the mask
/// shift. Mode 1 speeds/enlarges the glows and adds streaks in the front views;
/// full signed-word modes are retained. Work and overlay resources must remain
/// live while the task and its spawned effects use them.
void akropolisSquareRoomEffectTask(Task* task);

/// Draws one frame of the square telephone's pulsing red or cyan light, then ends the effect.
///
/// Bank-6 slot 0x47 requires a live coordinate body and a counted room effect.
/// `spawnArg1.value` packs the pulse phase advance per animation frame in bits
/// 0..7, radius scale in bits 8..15 and color in bit 16 (0 red, 1 cyan).
/// Bit 31 selects a round fan with rays; otherwise the glow is a diamond,
/// with crossing streaks when bit 28 is set. Other bits are ignored.
/// The 256-step triangular pulse gives center intensity 0..254.
///
/// Composed world coordinates narrow to signed 16-bit before projection through
/// `GsWSMATRIX`. Only SZ3 / 4 depths at least 17 draw; GTE FLAG is not tested.
/// The word-aligned packet arena must hold up to twenty `POLY_G4` packets or
/// two `POLY_G4` plus two `LINE_G3`, and one blend-mode packet per primitive.
/// Packets remain live through GPU drawing; scratch storage is released here.
/// Every call frees `spawnArg2.pointer` and tears down the task, even when
/// nothing is drawn. That pointer must be NULL or owned primary-heap storage,
/// distinct from `Task::work`, with nested resources already released.
/// Keep this overlay loaded through the callback.
void akropolisSquareTelephoneLightTask(Task* task);

/// Updates the square telephone's save menu and optional play statistics.
///
/// `spawnArg2.pointer` borrows the live UI object owned by this task. Save,
/// notice and statistics children must stay linked until their answers are
/// consumed. Requires this overlay and its telephone UI resources to stay loaded.
void akropolisSquareTelephoneMenuTask(Task* task);

/// Initializes and updates the player's floor or mirror-plane reflection in the square.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and becomes a child of the player.
/// Keep this overlay and the player live while the reflection and its
/// attachment reflections run.
void akropolisSquarePlayerReflectionTask(Task* reflectionTask);

/// Registers the square's room task and checks its arrival cutscene each tick.
///
/// State 0 installs message handling and publishes `GAME_TASK_SLOT_ROOM`;
/// state 1 starts the warp-7 event once per overlay load; state 2 kills the task.
/// Only states 0..2 are valid. The task has no body or work requirements.
/// The Akropolis map UI spawns it for the square. Keep this overlay loaded
/// while the task, its message handlers and its event scripts can run.
void akropolisSquareRoomTask(Task* task);

#endif // INCLUDE_ROOMS_AKROPOLIS_SQUARE_H
