#ifndef INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

#include "rooms/room_visual_effects.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT = 22 };

/// Shelter B2 south maintenance walkway placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variants 1, 2, 3, 11 and 21 publish layouts.
/// All other slots, including 0, are absent (NULL, NULL).
/// Both lists belong to this room overlay. Keep it and any actors supplying
/// task descriptors loaded through loading, spawning and model-flag updates.
/// Selectors must be in 0..21; there is no terminator or bounds check.
extern AreaVariant gShelterB2SouthMaintenanceWalkwayAreaVariants[SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT];

/// The walkway's one-room directory of logical-view to camera/image indices.
///
/// Room 1 borrows `gViewIdentityMap`; its five views map to indices 1..5.
/// The directory remains valid while the walkway overlay is loaded.
extern u8* gShelterB2SouthMaintenanceWalkwayRoomViewMaps[1];

/// Logical-view reverse-search limit for this area's sole room.
///
/// Room 1 searches 5 entries in its view map. Index zero corresponds to room ID 1.
/// Counts are signed halfword element counts, independent of map capacity.
/// The stage map borrows this array while the room overlay is loaded; there
/// is no terminator.
extern ViewCount gShelterB2SouthMaintenanceWalkwayRoomViewCounts[1];

/// The B2 south walkway's Elevator Hall and Operating Room endpoints.
///
/// Slots 1 and 2 arrive in views 2 and 5 respectively, sharing door
/// sounds but carrying separate optional map-marker keys.
/// Index by 1-based warp ID minus one; there is no terminator. Placements
/// use whole world units and yaw uses 4096 units per turn. The stage map
/// borrows this array while the room overlay and its sound bank stay loaded.
extern DirectionWarpEntry gShelterB2SouthMaintenanceWalkwayWarpEntries[2];

/// The walkway's room-space collision mesh and two-by-three XZ cell index.
///
/// Contains 18 quad faces, 38 vertices and 14 normals; cells are 4000 game units
/// wide, with origin (-2872, -5300). Activation binds the view coordinate.
/// The descriptor and its mutable pools remain live while the room is loaded.
extern WorldCollisionGrid gShelterB2SouthMaintenanceWalkwayCollisionGrid;

/// Cameras for the walkway's mapped views 1..5, stored at index minus one.
///
/// Rotations use 4096 per unit, translations negate the world-space origin,
/// and projection distances are pixels. The room overlay owns their storage.
extern ViewCamera gShelterB2SouthMaintenanceWalkwayViewCameras[5];

/// Sprite lists for the walkway's mapped views 1..5, stored at index minus one.
///
/// Every view retains background-image strips; view 3 additionally draws 64
/// source rectangles. Other views retain empty batch addresses in their source
/// slots. All clipping lists are NULL. Referenced lists belong to the loaded room.
extern SpriteView gShelterB2SouthMaintenanceWalkwaySpriteViews[5];

/// The walkway's six point lights, with no directional or cone lights.
///
/// All lights contribute in every view and remain writable for coordinate
/// composition and attenuation queries while the room overlay is loaded.
extern WorldCoordRoomLights gShelterB2SouthMaintenanceWalkwayRoomLights;

/// Directed view boundaries for logical views 2<->3, 3<->4, 4<->5.
///
/// All 6 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB2SouthMaintenanceWalkwayViewBoundaryTriggers[6];

/// Interaction and transition quads for this room.
///
/// Two facing-gated doors lead to the Elevator Hall and Operating Room,
/// using departure slots 1 and 2 respectively.
///
/// All 2 records are live; the final one carries LAST, with no dummy
/// terminator. Room binding supplies the current view coordinate and enables
/// the quads. Origins, corner offsets and radii use room game units; normals
/// use 4096 per unit. Keep this mutable storage loaded until its list is cleared.
extern WorldCollisionTrigger gShelterB2SouthMaintenanceWalkwayActionTriggers[2];

/// Room-space sight-blocking quads borrowed by the active collision list.
///
/// The sole LAST record is linked and enabled during room binding.
/// Origins and relative corners use game units; normals use 4096 per unit.
/// Keep this mutable array loaded until the sight list is cleared. The stored
/// low flag bit is retained; its role is unproven.
extern WorldCollisionOccluder gShelterB2SouthMaintenanceWalkwaySightOccluders[1];

/// Collision and footstep policies indexed by room-local surface class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Only class 1 supplies footstep cues; other classes share the silent record.
/// The directory and its records belong to the loaded walkway overlay.
extern WorldCollisionSurfaceProperties* gShelterB2SouthMaintenanceWalkwaySurfacePropertiesByClass[8];

/// Receives room messages and dispatches the walkway's room-task state.
///
/// Requires state 0 (install handlers), 1 (idle) or 2 (release the task).
/// The room overlay must remain loaded while its registered task is live.
void shelterB2SouthMaintenanceWalkwayRoomTask(Task* task);

/// Draws the walkway's grey light glows and its view-4 red disc.
///
/// State 0 selects the room's effect task IDs, then advances to state 1.
/// Each tick draws only the anchors selected by mapped views 2..5; other
/// views draw nothing. Radius scale is 512 and angles use 4096 units per turn.
/// This task does not consult room-effect pause or cancellation control.
void shelterB2SouthMaintenanceWalkwayDrawGlowsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// `spawnArg1.value` starts as a positive charge duration in active ticks and
/// is consumed as a countdown. Nonzero room-effect control pauses the task;
/// control 4 or above cancels it. Completion or cancellation releases the work.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records and draws blue twin trails behind two parent-relative endpoints.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer` whose `parent` stays live. State 0
/// allocates two eight-frame world-space histories in `task->work`; allocation
/// failure retries on the next active tick. `spawnArg1.value` is a recording
/// end age in 2..32767 active ticks, or 0 for no age limit. Control below 2
/// advances the trails; higher control leaves them untouched. Completion
/// releases the effect work and both histories.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Emits an impact flash with smoke puffs or bouncing sparks and fading orange rings.
///
/// Requires a coordinate body, initial state 0 and owned, zero-aged, counted
/// `EffectWork` in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke;
/// zero selects sparks and rings. Enters release at active age seven and tears
/// down on the next active tick. Child effects are independent. Room control
/// pauses at nonzero values below four and cancels at four or above. The room
/// overlay and borrowed coordinate hierarchy must stay loaded and live.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs an attached charge disc, player-joint sparks and a fading release ring.
///
/// Requires a coordinate body and owned, zeroed, counted `EffectWork` in
/// `spawnArg2.pointer`, starting in state 0. `spawnArg1.value` selects tint 0 or 1.
/// The work's parent and ancestors stay live; attachment uses its copied local
/// position. The owner uses `ROOM_VISUAL_EFFECTS_GLOW_DISC_*` to request flicker,
/// release or cancel; GROW emits adopted flying sparks from live player
/// model coordinates 3..18. Room control pauses at nonzero values below four and
/// cancels at four or above. Completion releases work, body and adopted tasks;
/// the room overlay and installed flying-spark callback must remain loaded.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs an animated spark on a fixed flight step toward an initial target.
///
/// Requires a coordinate body, an owned, zero-initialized, counted `EffectWork`
/// in `spawnArg2.pointer`, and a borrowed target `GfxCoord` in `spawnArg1.pointer`.
/// Both coordinates need composed world matrices on the first active tick; the target is read
/// only then. The step is 204/4096 of the initial displacement in parent axes.
/// Age 20 releases the work. Nonzero control pauses; control 4 or above cancels.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the room's expanding orange disc, layered glow and fading ring.
///
/// Requires a coordinate body and an owned, zero-initialized, counted
/// `EffectWork` in `spawnArg2.pointer`, as supplied by `effectSpawn`.
/// Active ticks expand the glow and fade the ring, then fade the central disc
/// until the work is released. Nonzero room-effect control pauses the task;
/// control 4 or above cancels it and releases the work.
void shelterB2SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
