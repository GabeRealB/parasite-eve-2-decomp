#ifndef INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT = 22 };

/// Shelter B2 north maintenance walkway placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variants 1, 3, 4, 11 and 21 publish layouts.
/// All other slots, including 0, are absent (NULL, NULL).
/// Both lists belong to this room overlay. Keep it and any actors supplying
/// task descriptors loaded through loading, spawning and model-flag updates.
/// Selectors must be in 0..21; there is no terminator or bounds check.
extern AreaVariant gShelterB2NorthMaintenanceWalkwayAreaVariants[SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT];

// shelter_b2_north_maintenance_walkway
/// Room-1 logical-view map selecting the same camera, image and sprite indices.
///
/// The sole pointer borrows `gViewIdentityMap`; only logical views 1..8 are
/// used here, despite that shared map's larger capacity. There is no room
/// sentinel or room-2 map. Keep the room directory and gameplay loaded.
extern u8* gShelterB2NorthMaintenanceWalkwayRoomViewMaps[1];

/// Logical-view reverse-search limit for this area's sole room.
///
/// Room 1 searches 8 entries in its view map. The second stored zero is unread by
/// the one-room directory; its role is unproven.
/// Counts are signed halfword element counts, independent of map capacity.
/// The stage map borrows this array while the room overlay is loaded; there
/// is no terminator.
extern ViewCount gShelterB2NorthMaintenanceWalkwayRoomViewCounts[2];

/// The B2 north walkway's Operating Room and Breeding Room endpoints.
///
/// Slots 1 and 2 arrive in views 2 and 5 respectively. The Operating
/// Room endpoint supplies a blocked-door sound and an optional map-marker key.
/// Index by 1-based warp ID minus one; there is no terminator. Placements
/// use whole world units and yaw uses 4096 units per turn. The stage map
/// borrows this array while the room overlay and its sound bank stay loaded.
extern DirectionWarpEntry gShelterB2NorthMaintenanceWalkwayWarpEntries[2];

/// Walkway collision mesh and its two-by-three XZ cell directory.
///
/// Cell coordinates are (X + 2872) / 4000 and (Z + 4937) / 4000 in room
/// game units. The mesh has 18 quads, 38 vertices and 14 normals; class 1
/// marks the floor. Activation binds the view transform. The descriptor
/// and its writable pools remain borrowed while this room is loaded.
extern WorldCollisionGrid gShelterB2NorthMaintenanceWalkwayCollisionGrid;

/// Eight camera records selected by mapped view index minus one.
///
/// Rotations have 12 fractional bits; translation is the negated camera
/// origin in world game units and projection distance is in pixels.
/// Keep the room loaded until any queued camera application finishes.
extern ViewCamera gShelterB2NorthMaintenanceWalkwayViewCameras[8];

/// Sprite descriptors for mapped views 1..8, indexed by view minus one.
///
/// Views 1, 2, 5 and 6 supply only decoded-image backgrounds. Views 3, 4,
/// 7 and 8 add texture rectangles; every batch list preserves the image
/// background and ends with `SPRITE_BATCH_END`. No view supplies draw-area
/// clipping. Descriptors and their writable arrays require the loaded room.
extern SpriteView gShelterB2NorthMaintenanceWalkwaySpriteViews[8];

/// Room-1 model lighting from five point lights, with no directional or cone lights.
///
/// All five contribute in every view. The collection and mutable light
/// records are borrowed only while the walkway overlay remains loaded.
extern WorldCoordRoomLights gShelterB2NorthMaintenanceWalkwayLights;

/// Directed view boundaries for logical views 2<->3, 3<->4, 4<->5.
///
/// All 6 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB2NorthMaintenanceWalkwayViewBoundaryTriggers[6];

/// Interaction and transition quads for this room.
///
/// Two facing-gated doors lead to the Operating Room and Breeding Room.
/// The final quad automatically requests room action 1, which starts the
/// entry scene only in variant 1 while its seen flag is clear.
///
/// All 3 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB2NorthMaintenanceWalkwayActionTriggers[3];

/// Room-space sight-blocking quads borrowed by the active collision list.
///
/// The sole LAST record is linked and enabled during room binding.
/// Origins and relative corners use game units; normals use 4096 per unit.
/// Keep this mutable array loaded until the sight list is cleared. The stored
/// low flag bit is retained; its role is unproven.
extern WorldCollisionOccluder gShelterB2NorthMaintenanceWalkwaySightOccluders[1];

/// Collision and footstep policies indexed by surface class 0..7.
///
/// Class 1 is the floor with footstep cues; all other slots share a policy
/// without surface cues. Every slot blocks probes, permits weapon impacts
/// and applies pushback. Keep this room and the selected sound bank loaded
/// while collision or footstep playback uses these borrowed records.
extern WorldCollisionSurfaceProperties* gShelterB2NorthMaintenanceWalkwaySurfacePropertiesByClass[8];

/// Runs the north maintenance walkway receiver's setup, idle or teardown state.
///
/// Requires a bodyless task in state 0..2 and this package loaded. Setup
/// registers the room receiver and clears the variant-1 actor's script handle;
/// idle keeps it available for messages. Dispatch copies the state table.
void shelterB2NorthMaintenanceWalkwayRoomTask(Task* task);

/// Runs this room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` is a positive charge duration in
/// active ticks, consumed as a countdown. Nonzero room effect control pauses
/// it; control 4 or above, state 3, or completion releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records two parent-relative endpoints and draws their fading twin trail.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with a parent that stays live until teardown. `work`
/// starts NULL and owns two eight-frame world-space coordinate histories after
/// initialization. `spawnArg1.value` is the nonzero signed-16-bit stopping age,
/// tested only on later recording ticks; zero leaves lifetime to external
/// teardown. Age advances with room effect control below 2; control 2 or above
/// pauses without cancelling. Failed history allocation resets age for retry.
/// Teardown releases both work allocations and the coordinate body.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs this room's impact flash followed by smoke or fading orange spark rings.
///
/// Start at state 0 with a coordinate body and counted, zero-aged `EffectWork`
/// owned through `spawnArg2.pointer`. Nonzero `spawnArg1.value` emits seven
/// smoke puffs; zero emits two bouncing sparks and draws fixed/expanding rings.
/// Active age 7 enters release; age 8 releases work and task. Children are
/// independent. Room-effect control 1..3 pauses and 4 or above cancels. Keep
/// this room, coordinate ancestors, effect resources, scratch and GPU arena live
/// until teardown; caller work pointers become invalid when the effect ends.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs this room's vertically drifting animated mote until its brightness fades.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with age and animation index initially zero.
/// `spawnArg1` packs a game-unit half-extent in bits 0..11, a palette selector
/// in bits 12..15 (0 default), unsigned speed in bits 16..23 in parent-space
/// units per active tick, and a signed lifetime in bits 24..31 in active ticks.
/// Bits 0..1 overlap the extent: either selects steady motion, with bit 1
/// selecting upward motion; otherwise it rises with added random speed 0..63.
/// The first active tick only initializes; later ticks draw on odd ages and
/// fade by 16 once age exceeds lifetime minus eight. Nonzero room effect
/// control pauses it; control 4 or above or completion releases work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task);

/// Runs this room's expanding tinted halo, shrinking ring and fading star.
///
/// Requires a coordinate body and a counted, owned, zero-initialized
/// `EffectWork` in `spawnArg2.pointer`; its parent must stay live. Initialization
/// attaches at the work's parent-relative offset in game units.
/// `spawnArg1.halves.low` is a positive unsigned expansion duration in active
/// ticks; `halves.high` selects tint row 0..2. Initialization replaces the word
/// with the remaining ticks. Nonzero room effect control pauses every phase,
/// including state 3's release request; control 4 or above or completion
/// releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task);

/// Runs this room's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1` is unused. Nonzero room effect control
/// pauses it; control 4 or above or completion releases the work and task.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits twenty independent motes at rotating, successively higher local offsets.
///
/// Requires a coordinate body and counted, owned, zero-aged `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1` is ignored. The installed mote callback and
/// coordinate ancestors must stay live. Active ages 1..20 emit at radius about
/// 768 and Y = -128 * age in parent units, advancing a halfword heading by
/// 512..1023 in 4096-unit angles. Each mote descends eight units per active tick.
/// Age 21 releases the emitter; its motes remain independent. Nonzero room
/// effect control pauses below four and cancels at four or above.
void shelterB2NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask(Task* task);

/// Installs this room's enemy-effect IDs and draws its view-specific light glows.
///
/// State 0 installs the seven effect IDs and becomes state 1; every tick draws
/// the glows for mapped camera indices 2..8. Other views draw nothing. The
/// operating-room north-door flag selects an orange locked-door glow or a blue
/// unlocked-door glow in views 2 and 7. Requires the room overlay and current
/// view/rendering state to stay live; retains the task until external teardown.
void shelterB2NorthMaintenanceWalkwayGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
