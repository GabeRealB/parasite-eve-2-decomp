#ifndef INCLUDE_ROOMS_NEO_ARK_GARDEN_H
#define INCLUDE_ROOMS_NEO_ARK_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_garden_80181398;

extern AreaVariant D_neo_ark_garden_80182B54[13];

// neo_ark_garden
extern WorldCollisionRoomResources D_neo_ark_garden_8018140C[];

extern WorldCoordRoomLighting D_neo_ark_garden_8018141C[];

extern u8* D_neo_ark_garden_80181424[];

extern ViewCount D_neo_ark_garden_80181428[];

extern DirectionWarpEntry D_neo_ark_garden_8018142C[];

extern ViewCamera D_neo_ark_garden_801816E8[];

extern SpriteView D_neo_ark_garden_80182540[];

extern WorldCollisionSurfaceProperties* D_neo_ark_garden_80182BD8[];

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
