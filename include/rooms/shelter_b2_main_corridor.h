#ifndef INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four borrowed Bog Diver patrol rings selected by spawn-argument bits 4..5.
///
/// Each ring has eight points and wraps at index 7. Slots 1/3 share a ring;
/// slots 0 and 2 each have their own. X/Z use room units; swimming goal Y adds
/// unsigned waypoint Y to the sampled water height. There is no terminator.
/// Keep this overlay loaded while any diver holds a ring pointer.
extern SVECTOR* gShelterB2MainCorridorDiverPatrolWaypointSets[4];

/// Writable Bog Diver emergence spots with entry 0 reserved as the claim fallback.
///
/// Entries 1..10 supply room-space XZ; their Y is not read by spot searches.
/// SVECTOR.pad is a mutable claim mark (0 free, 1 claimed, -1 terminator).
/// Entry 11 ends the list and is never a spot. Divers release claims when
/// moving or despawning; keep this overlay loaded through every claim's lifetime.
extern SVECTOR gShelterB2MainCorridorDiverSurfaceSpots[12];

/// Undisplaced water-surface Y in signed world units, shared with the room's actors.
extern s16 gShelterB2MainCorridorWaterY;

/// Extents of the corridor's room-map directory and collision surface-class table.
enum {
    SHELTER_B2_MAIN_CORRIDOR_ROOM_COUNT          = 1,
    SHELTER_B2_MAIN_CORRIDOR_SURFACE_CLASS_COUNT = 8,
};

/// Extents of the corridor's exported view, placement and trigger tables.
enum {
    SHELTER_B2_MAIN_CORRIDOR_VIEW_COUNT                  = 13,
    SHELTER_B2_MAIN_CORRIDOR_WARP_ENTRY_COUNT            = 6,
    SHELTER_B2_MAIN_CORRIDOR_AREA_VARIANT_COUNT          = 22,
    SHELTER_B2_MAIN_CORRIDOR_VIEW_BOUNDARY_TRIGGER_COUNT = 18,
    SHELTER_B2_MAIN_CORRIDOR_ACTION_TRIGGER_COUNT        = 7,
};

/// Placement/resource pairs indexed by the saved corridor layout variant, 0..21.
///
/// Variants 1/2 place three Bog Divers, 3 a Bog Diver and large Mossback,
/// 4 an empty layout, and 11/21 two Watchers and one Rook GOLEM. All other
/// slots are absent (both pointers NULL); there is no table terminator.
/// The selected room lists end with `AREA_PLACEMENT_END` and remain borrowed
/// while loading or while spawned actors refer to their placements.
extern AreaVariant gShelterB2MainCorridorAreaVariants[SHELTER_B2_MAIN_CORRIDOR_AREA_VARIANT_COUNT];

/// Logical-view maps for the corridor's single room, indexed by room ID minus one.
///
/// Borrows gameplay's `gViewIdentityMap`: logical views 1..13 select the same
/// camera, background and sprite indices. The directory has no terminator and
/// stays live with this overlay; the borrowed map stays live with gameplay.
extern u8* gShelterB2MainCorridorViewMaps[SHELTER_B2_MAIN_CORRIDOR_ROOM_COUNT];

/// Logical-view reverse-search length for the corridor's single room.
///
/// Room ID minus one selects the signed element count 13. The borrowed
/// identity map has a separate 50-byte capacity; this count has no terminator
/// and stays valid while the corridor overlay is loaded.
extern ViewCount gShelterB2MainCorridorViewCounts[SHELTER_B2_MAIN_CORRIDOR_ROOM_COUNT];

/// Six arrival/departure endpoints selected by 1-based corridor warp IDs.
///
/// In order: elevator hall, EVE tunnel route, laboratory, observatory route,
/// breeding room and septic tank. Transforms use world units and 4096 yaw
/// units per turn; each initial view is 1..13. Sounds and optional map marks
/// belong to these endpoints. No terminator; keep this overlay loaded until
/// the entry has been copied or its transforms consumed by actor spawning.
extern DirectionWarpEntry gShelterB2MainCorridorWarpEntries[SHELTER_B2_MAIN_CORRIDOR_WARP_ENTRY_COUNT];

/// Collision mesh and XZ cell directory for the corridor's walls and floors.
///
/// Contains 104 faces in a 4-by-8 grid of 4000-world-unit cells. Activation
/// binds the mutable descriptor to the view; its mesh pools remain borrowed
/// from this overlay until the grid is deactivated. Surface classes select
/// `gShelterB2MainCorridorSurfacePropertiesByClass`.
extern WorldCollisionGrid gShelterB2MainCorridorCollisionGrid;

/// Cameras selected by the corridor's mapped view index minus one, 0..12.
///
/// Rotations use `ONE` per unit, translations are negated world-space camera
/// origins, and projection distances are pixels. No terminator; queued camera
/// application borrows these records until dispatch, while this overlay is loaded.
extern ViewCamera gShelterB2MainCorridorViewCameras[SHELTER_B2_MAIN_CORRIDOR_VIEW_COUNT];

/// Sprite sources and batch lists selected by mapped view index minus one, 0..12.
///
/// All views use decoded-image backgrounds and no draw-area list. Views 1,
/// 12 and 13 retain an empty batch-list address in `sources.empty`; the other
/// views select mutable source rectangles. This table has no terminator;
/// its borrowed lists remain valid while the corridor overlay is loaded.
extern SpriteView gShelterB2MainCorridorSpriteViews[SHELTER_B2_MAIN_CORRIDOR_VIEW_COUNT];

/// Eight point lights shared by every corridor view, with no directional or cone lights.
///
/// The room owns the mutable transforms and query attenuation. Keep this
/// overlay loaded while coordinate updates or lighting queries use them.
extern WorldCoordRoomLights gShelterB2MainCorridorRoomLights;

/// Eighteen directed boundaries changing the corridor's current logical view.
///
/// Source/destination bytes select views 2..11. Positions and radii use room
/// units and normals use `ONE` per unit. LAST marks the final included record.
/// Room setup binds the mutable nodes to the view; keep this overlay loaded
/// until their collision lists are unlinked.
extern WorldCollisionTrigger gShelterB2MainCorridorViewBoundaryTriggers[SHELTER_B2_MAIN_CORRIDOR_VIEW_BOUNDARY_TRIGGER_COUNT];

/// Seven interaction quads for four Shelter warps, two route actions and an access scene.
///
/// The six travel quads require facing and an interaction press. The final
/// access-scene quad activates automatically and carries LAST. Positions and
/// radii use room units; normals use `ONE` per unit. Room setup binds these
/// mutable nodes to the view; keep this overlay loaded until they are unlinked.
extern WorldCollisionTrigger gShelterB2MainCorridorActionTriggers[SHELTER_B2_MAIN_CORRIDOR_ACTION_TRIGGER_COUNT];

/// Collision responses for surface classes 0..7, indexed directly by class.
///
/// Class 1 passes probes and disables weapon impacts; class 2 blocks probes,
/// enables impacts and supplies floor footsteps. Classes 0 and 3..7 use the
/// same silent blocking response. Every class applies pushback. The table
/// and its borrowed records remain valid only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB2MainCorridorSurfacePropertiesByClass[SHELTER_B2_MAIN_CORRIDOR_SURFACE_CLASS_COUNT];

/// Runs the corridor's message receiver and starts its water-surface task.
///
/// Start a bodyless task in state 0. The first tick installs the borrowed room
/// message table, registers `GAME_TASK_SLOT_ROOM`, and spawns the water task.
/// State 1 idles; state 2 tears down the task. Other states are invalid table
/// indices. Keep the corridor overlay loaded while the task or its messages run.
void shelterB2MainCorridorRoomTask(Task* task);

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// Requires a counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state 0. Bits 0..11 of `spawnArg1` supply the
/// initial local half-side in world units (0..4095); higher bits are ignored.
/// Running ticks grow it by 32 and fade brightness from 64 by 2, lasting 32
/// running ticks. The first tick chooses a random yaw after composing the
/// coordinate. Non-running ticks redraw without aging; cancellation redraws
/// once before freeing the work, decrementing the effect count and destroying
/// the task and body. Keep this overlay and the effect controller live.
void shelterB2MainCorridorWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle in the corridor.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`,
/// one coordinate body, initial state 0 and cell index 0. Draws the coordinate's
/// existing `workm`; local motion dirties its cache without composing it.
/// The effect spawner initially composes the coordinate beneath the view.
///
/// `spawnArg1` bits 0..11 give size scale (0..4095), bits 12..15 the running
/// ticks per cell (0 selects 1), bits 16..23 speed in parent-coordinate units
/// per tick (0 selects 64), and bits 24..27 the velocity kind: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 direction from
/// the copied spawn offset. Other kinds retain the zero direction before
/// normalization. Nonzero bits 28..31 select an upright tile; otherwise the
/// quad retains a random angle in 4096 units per turn. A preloaded nonzero
/// `move` bypasses generation and scaling; `step` becomes 64 to enable motion.
///
/// The first running tick initializes without drawing or moving. Later ticks
/// draw the current cell, advance local translation by signed halfword velocity
/// and add 6 to its Y component, retaining the low 16 bits. Stationary particles
/// skip motion and gravity. Cells 0..7 last `period` ticks each; retirement
/// follows 8 * `period` running ticks after initialization (8..120 ticks).
///
/// Non-running control below cancellation redraws without initialization or
/// aging; cancellation retires without drawing. Retirement frees the work,
/// decrements the effect count and destroys the task and coordinate body.
/// Keep this overlay, controller, cached matrix and drawing resources live;
/// queued primitives must remain available until GPU consumption.
void shelterB2MainCorridorWaterDriftTask(Task* task);

/// Runs a pink charge flash, a screen tint at its peak, and a fading star.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`. Start in state 0 with a positive charge duration in
/// `spawnArg1.value`, in running ticks; it becomes a countdown. Nonzero room
/// effect control pauses updates, and values at least 4 cancel. Completion or
/// state 3 releases the work and task. Keep this overlay and controller live.
void shelterB2MainCorridorRoomVisualEffectsFlashTask(Task* task);

/// Records and draws two fading trails between parent-relative endpoints.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its borrowed parent coordinate must remain live.
/// State 0 allocates two eight-slot histories in `Task::work`, seeding them at
/// offsets (0, 190, -15) and (0, 1085, 180) in parent-space world units.
/// Allocation failure resets age and retries. State 1 stores world-space
/// snapshots and draws seven quads with RGB weights 1:2:3. `spawnArg1.value`
/// is an exact expiry age, compared only in state 1; zero never expires, and
/// age is signed 16-bit. Expiry releases both histories, the counted work and
/// the task. Control values below 2 allow updates, including pause value 1;
/// values at least 2 freeze it. Keep this overlay and effect controller live.
void shelterB2MainCorridorRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
///
/// Requires a coordinate body and owned, zero-aged `EffectWork` in
/// `spawnArg2.pointer`, supplied by `effectSpawn`. Nonzero `spawnArg1.value`
/// selects smoke; zero selects rings. Active age seven enters release and the
/// next active tick frees the work and task. Room effect control 1..3 pauses
/// the task and 4 or above cancels it. Children have independent lifetimes.
void shelterB2MainCorridorRoomVisualEffectsSparkBurstTask(Task* task);

/// Selects the corridor's effects and draws beams and flares for the mapped view.
///
/// Start in state 0 to bind the room's flash, twin-trail, spark-burst, ripple
/// and spray IDs; state 1 draws on that same tick and all later ticks. Mapped
/// view's low byte selects views 2..11; other views draw nothing. View 6 also
/// draws view 7's beams. Drawing ignores room effect pause/cancel control and
/// leaves the task live. Requires this loaded overlay, current view matrices,
/// scratch stack, packet arena and ordering table; queued packets must remain
/// live until GPU consumption. At most 24 beam quads, two flares and their
/// blend commands are queued per tick. The task's coordinate body is unused.
void shelterB2MainCorridorDrawViewGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_MAIN_CORRIDOR_H
