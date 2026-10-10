#ifndef INCLUDE_ROOMS_NEO_ARK_GARDEN_H
#define INCLUDE_ROOMS_NEO_ARK_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of logical views and identity-mapped camera/sprite slots in the garden.
enum { NEO_ARK_GARDEN_VIEW_COUNT = 7 };

/// Bodyless, priority-192 water-refraction task started when the garden is entered.
///
/// Both spawn payloads are ignored. The callback draws only in logical view 2;
/// its room code must remain loaded for the task's lifetime.
extern TaskDesc gNeoArkGardenWaterRefractionTaskDesc;

extern AreaVariant D_neo_ark_garden_80182B54[13];

/// Collision resources for garden room 1, borrowed while its overlay is loaded.
///
/// Supplies one grid, directed view boundaries and interaction triggers;
/// no sight occluders are supplied. Linked resources remain writable and live
/// until gameplay unlinks them before unloading the room.
extern WorldCollisionRoomResources gNeoArkGardenCollisionRooms[1];

/// Lighting for garden room 1: four directional lights and default ambient minima.
///
/// The room-owned light coordinates remain writable while the overlay is loaded.
extern WorldCoordRoomLighting gNeoArkGardenRoomLighting[1];

/// Room-1 logical views 1..7 mapped to the same camera, image and sprite slots.
///
/// Borrows gameplay's `gViewIdentityMap`; the directory remains room-owned.
extern u8* gNeoArkGardenViewMaps[1];

/// Logical view count for garden room 1, limiting reverse view-map searches.
extern ViewCount gNeoArkGardenViewCounts[1];

/// Three 1-based garden warp endpoints with player/companion placement and cues.
///
/// Selected by warp ID minus one, with no terminator; the overlay must be loaded.
/// Arrival views are 2, 4 and 3. Endpoint 2 carries the altar map marker and a
/// blocked-departure cue; endpoint 3 has no sounds and carries map flag 0x1B5.
extern DirectionWarpEntry gNeoArkGardenWarpEntries[3];

/// Camera transforms and perspective distances for garden mapped views 1..7.
///
/// Indexed by mapped view minus one; queued camera requests borrow these records
/// until application and require the room overlay to remain loaded.
extern ViewCamera gNeoArkGardenViewCameras[NEO_ARK_GARDEN_VIEW_COUNT];

/// Garden mapped views 1..7, combining decoded backgrounds with depth-sorted sprites.
///
/// Views 2..4 have mutable source/batch arrays; views 1 and 5..7 have empty lists.
/// No view supplies clipping commands. Indexed by mapped view minus one;
/// all borrowed arrays require the garden overlay to remain loaded.
extern SpriteView gNeoArkGardenSpriteViews[NEO_ARK_GARDEN_VIEW_COUNT];

/// Neo Ark garden collision and footstep policies for surface classes 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Class 4 supplies walking/running/scripted-jump cue bases; all others share
/// silent properties. Setup reads all eight entries. The directory, records
/// and sound cues remain borrowed only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkGardenSurfacePropertiesByClass[8];

/// Updates the garden's view-dependent ambience and fixed-point visual effects.
///
/// Requires a coordinate-body effect task with cleared, counted `EffectWork` in
/// `spawnArg2.pointer` and initial state zero. Installs this room's glow-disc,
/// flying-spark and orange-burst IDs. `spawnArg1.value` stores the previous mapped
/// view byte; a change rearms a four-update sound delay in `EffectWork::scale`,
/// counted down only in views 2..7. After that delay, those views queue pan and
/// attenuation updates for `SOUND_NEO_ARK_GARDEN_AMBIENCE_1` and
/// `SOUND_NEO_ARK_GARDEN_AMBIENCE_2`; views 2, 4 and 5 also start the pair once.
/// Views 2 and 4 roll smoke only at `ROOM_EFFECT_CONTROL_RUNNING`; view 3 draws
/// the star and view 4 draws two rotating squares regardless of that control.
/// Runs until external teardown; the effect work and room resources must stay live.
void neoArkGardenAmbienceTask(Task* task);

/// Runs the garden's animated spark along a fixed step toward its initial target.
///
/// Requires a counted effect task with a coordinate body, owned `EffectWork`
/// in `spawnArg2.pointer`, state zero and age/frame index zero. `spawnArg1.pointer`
/// borrows a target `GfxCoord` through the first running update, when both cached
/// matrices must be current in the same view space. That update fixes a
/// parent-space step at 204/4096 of the initial displacement, with intermediate
/// signed 16-bit narrowing.
/// Later running updates move by that step and draw on odd ages; at age 20 the
/// work and task are released. Non-running room control pauses without drawing;
/// cancellation releases the effect. The target is not sampled again.
void neoArkGardenRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the garden's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer`; `spawnArg1.value` selects tint 0 or 1. The borrowed
/// parent coordinate and its ancestors must remain live. Growth emits and adopts
/// flying sparks from player model parts 3..18 every fourth active age, requiring
/// the live player model and installed flying-spark callback. The owner requests
/// flicker, release or cancellation with `ROOM_VISUAL_EFFECTS_GLOW_DISC_*` states.
/// Nonzero room control below 4 pauses; control 4 or above cancels. Completed
/// release or cancellation frees the work, disc task and adopted children.
void neoArkGardenRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the garden's orange burst with a growing disc, glow and fading ring.
///
/// Requires a counted effect task with a coordinate body, owned `EffectWork`
/// in `spawnArg2.pointer` and initial state zero; `spawnArg1` is ignored. Running
/// updates compose the coordinate and expand the glow, fading the ring before
/// the central disc. The glow also refreshes an orange transient point light.
/// Non-running room control pauses without drawing. Cancellation or completed
/// fading releases the work and task; callers must not retain released pointers.
void neoArkGardenRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs the garden's room-message task and arrival-scene setup.
///
/// Requires a live task in state 0..2: initialize the receiver and the scene
/// for warp 3 in variant 2, idle while messages handle requests, then teardown.
/// The room overlay must remain loaded through dispatch.
void neoArkGardenRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_GARDEN_H
