#ifndef INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Saved placement variants for this room, indexed directly by variant (0..11).
///
/// Live slots are 1 (Watchers/Rats), 2 (Mossbacks/Amoebas), 3 (Amoebas/Caterpillars)
/// and 11 (Watchers/Pawn GOLEM); all other slots contain two NULL pointers.
/// Placements and resource recipes belong to this room overlay and must remain
/// loaded while actors borrow them. Gameplay associates these variants with
/// this area's saved spawn state.
extern AreaVariant gShelterB1SouthMaintenanceWalkwayAreaVariants[12];

/// Room 1's six point lights, with the default minimum ambient colours.
///
/// Indexed by room minus one. The map borrows this descriptor; lighting updates
/// its room-owned light transforms and attenuation while this overlay is loaded.
extern WorldCoordRoomLighting gShelterB1SouthMaintenanceWalkwayRoomLighting[1];

/// Collision resources for the B1 south walkway's sole room.
///
/// Room 1 borrows its grid, six view-boundary triggers, two door triggers and
/// one sight occluder. The last trigger/occluder in each list carries its LAST
/// flag and is also live. Index by room ID minus one; there is no table terminator.
/// Keep this room overlay loaded until its active collision lists are unlinked.
extern WorldCollisionRoomResources gShelterB1SouthMaintenanceWalkwayRoomCollisionResources[1];

/// Room 1's logical-view map, borrowing gameplay's identity mapping.
///
/// Indexed by room minus one, then logical view minus one. Views 1..5 map to
/// camera/image/sprite indices 1..5; the shared map's capacity is independent
/// of this room's five-view count. The directory stays live with this overlay.
extern u8* gShelterB1SouthMaintenanceWalkwayRoomViewMaps[1];

/// Logical-view reverse-search limit for this area's sole room.
///
/// Room 1 searches 5 entries in its view map. Index zero corresponds to room ID 1.
/// Counts are signed halfword element counts, independent of map capacity.
/// The stage map borrows this array while the room overlay is loaded; there
/// is no terminator.
extern ViewCount gShelterB1SouthMaintenanceWalkwayRoomViewCounts[1];

/// The south walkway's Elevator Hall and Storeroom endpoints.
///
/// Slots 1 and 2 arrive in views 5 and 2 respectively. Slot 2 carries
/// the optional Storeroom-side map-marker key.
/// Index by 1-based warp ID minus one; there is no terminator. Placements
/// use whole world units and yaw uses 4096 units per turn. The stage map
/// borrows this array while the room overlay and its sound bank stay loaded.
extern DirectionWarpEntry gShelterB1SouthMaintenanceWalkwayWarpEntries[2];

/// The five mapped-view cameras, indexed by mapped view minus one.
///
/// Rotations use ONE (4096) per unit, translations are negated world origins,
/// and each projection distance is 235 pixels. Borrowed while this room is loaded.
extern ViewCamera gShelterB1SouthMaintenanceWalkwayViewCameras[5];

/// Sprite lists for mapped views 1..5, indexed by mapped view minus one.
///
/// View 4 selects 66 source sprites; the other views select none. Every view
/// keeps decoded backdrop strips and has no clipping-area list. The records
/// and their mutable batches and sources stay live with this room overlay.
extern SpriteView gShelterB1SouthMaintenanceWalkwaySpriteViews[5];

/// Collision policies for surface classes 0..7, indexed directly by class.
///
/// All classes block probes, apply pushback and allow weapon impacts. Only
/// class 1, used by the two floor faces, supplies footstep cues. The map borrows
/// this directory and its room-owned records while the overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB1SouthMaintenanceWalkwaySurfacePropertiesByClass[8];

/// Runs this room's persistent message receiver.
///
/// The Shelter map spawns a bodyless task in state 0: setup installs its room
/// message table and publishes `GAME_TASK_SLOT_ROOM`, state 1 is idle, and
/// state 2 kills the task. State must stay in 0..2; no bound is checked.
/// Spawn arguments are unused. Keep the room overlay loaded while it runs.
void shelterB1SouthMaintenanceWalkwayRoomTask(Task* task);

/// Charges a pink flash, tints the screen at its peak, then fades it as a star.
///
/// Bank-6 slot 0x1C9 owns the `EffectWork` in `spawnArg2` and uses the task's
/// coordinate body. `spawnArg1.value` starts as a positive charge duration in
/// active ticks and is consumed as a countdown. Room effect control pauses at
/// nonzero and cancels at four or above; state 3 also releases the effect.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws a fading blue twin trail between their histories.
///
/// Bank-6 slot 0x1E5 owns the `EffectWork` in `spawnArg2` and allocates two
/// eight-coordinate histories in `Task::work`, released by task teardown.
/// The work's parent coordinate must stay live; the endpoint offsets are local
/// to it, while history snapshots retain world positions. `spawnArg1.value`
/// is compared with the signed 16-bit active age after initialization; zero
/// disables that lifetime check. Control values below two advance the trail;
/// two or above leave it untouched until it resumes or is killed externally.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an animated spark along a fixed step toward an initially sampled target.
///
/// Bank-6 slot 0x21B owns the `EffectWork` in `spawnArg2`. Its coordinate body
/// and the borrowed target `GfxCoord` in `spawnArg1.pointer` need matrices
/// composed into the same view space on the first active tick. It samples the
/// displacement once, stores a 204/4096 step with signed-16-bit narrowing,
/// and releases the effect at active age 20. Room effect control pauses at
/// nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs an attached charge disc with player-joint sparks and a fading release ring.
///
/// Bank-6 slot 0x21C owns a zeroed `EffectWork` in `spawnArg2.pointer` and a
/// coordinate body. `spawnArg1.value` selects tint 0 or 1. The work's parent
/// coordinate and ancestors must remain live; its copied local offset replaces
/// the initial placement. Growth emits adopted flying sparks from player parts
/// 3..18 every fourth active age, requiring the player model and this room's
/// installed flying-spark callback. The owner may request flicker, release or
/// cancel through `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Release fades the disc and
/// expands an orange ring; teardown releases work and adopted child tasks.
/// Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task);

/// Expands an orange disc and layered glow, fading its ring before its centre.
///
/// Bank-6 slot 0x21D owns the `EffectWork` in `spawnArg2` and uses the task's
/// coordinate body. Brightness starts at 224; the glow half-extent starts at
/// 128 world units and grows by 16 per active tick. The effect releases its
/// work after fading. Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Draws this room's view-dependent capsule glows and red disc.
///
/// Bank-6 slot 0x129 first binds this room's six shared effect IDs and enters
/// state 1. Saved views 2..5 select the glows; view 1 draws none. The radius
/// scale is 512, giving a screen radius of 512 * 64 / (camera Z / 4), with
/// the drawers' near-depth clipping and clamp. Requires current view matrices,
/// room effect state, ordering table and packet arena; owns no effect work.
void shelterB1SouthMaintenanceWalkwayDrawGlowsTask(Task* task);

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Bank-6 slot 0x201 owns a zero-aged `EffectWork` in `spawnArg2.pointer` and
/// a coordinate body. Nonzero `spawnArg1.value` emits smoke at active ages 1..7;
/// zero emits two independent bouncing sparks then draws fading orange rings.
/// Both variants enter release at age 7 and free work on the next active tick.
/// Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H
