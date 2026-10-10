#ifndef INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
#define INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H

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

/// Four Bog Diver patrol selections, indexed by spawn-argument bits 4..5.
///
/// Slots 0..2 borrow distinct eight-point rings; slot 3 repeats slot 1.
/// Each ring wraps at index 7 and has no terminator. X/Z use room-space
/// game units; ordinary swimming adds the sampled water Y to each depth.
/// The directory and its rings must remain loaded for the divers' lifetimes.
extern SVECTOR* gShelterB2SepticTankDiverPatrolRings[4];

/// Writable Bog Diver emergence spots, shared by the room's live divers.
///
/// Entry 0 is the reserved fallback, entries 1..10 are candidate XZ positions
/// in room-space game units, and entry 11 ends the list. `SVECTOR.pad` stores
/// the claim marker (0 free, 1 claimed, -1 end); it is not padding here.
/// Searches copy only X/Z, preserving the diver's own Y. Claims are released
/// when moved or on teardown; retain the room storage until every diver exits.
extern SVECTOR gShelterB2SepticTankDiverSurfaceSpots[12];

/// Undisplaced water-surface Y in signed world units, shared with the room's actors.
extern s16 gShelterB2SepticTankWaterY;

/// Septic-tank actor layouts indexed directly by placement variant 0..21.
///
/// Populated slots are 1..4, 11 and 21; every other slot has two NULL pointers.
/// Variant 1 starts the paired-diver introduction, 2 is swimming patrol, 3
/// combines a patrol diver and large Mossback, and 4 retains just that diver.
/// Variants 11 and 21 pair Watchers with wounded divers and a Pawn GOLEM,
/// respectively. Each placement/resource list ends at `AREA_PLACEMENT_END`.
/// Loaded room storage is borrowed through resource loading and actor teardown.
extern AreaVariant gShelterB2SepticTankAreaVariants[22];

/// Logical-view map directory for the septic tank's single room.
///
/// Room index zero borrows `gViewIdentityMap`: logical views 1..6 select
/// camera, image and sprite indices 1..6. There is no terminator. The
/// directory requires the room loaded; gameplay owns the mapped bytes.
extern u8* gShelterB2SepticTankViewMaps[1];

/// Six logical views in the septic tank's single room.
///
/// Indexed by room ID minus one (only room 1), with no terminator. The signed
/// halfword limits searches of the identity view map to logical views 1..6.
/// The stage map borrows the count while this room overlay is loaded.
extern ViewCount gShelterB2SepticTankRoomViewCounts[1];

/// Arrival and departure records for the corridor and pod-access-tunnel doors.
///
/// Warp IDs 1 and 2 select entries 0 and 1, starting in views 2 and 5.
/// Each stores player/companion placements in room units, yaw in 4096ths of a
/// turn and area-bank arrival/departure sounds; neither sets a map marker.
/// There is no terminator. The stage map borrows the loaded room's records;
/// transition consumers copy complete entries before using their placements.
extern DirectionWarpEntry gShelterB2SepticTankWarpEntries[2];

/// The septic tank's room-space collision mesh and 3-by-6 XZ cell index.
///
/// Cells are 4000 game units wide; the origin is (-5374, -18345) on X/Z.
/// The 31 faces borrow the room's vertex, normal and terminated cell pools.
/// Activation binds the descriptor to the current view; keep the room loaded
/// while collision queries use the grid.
extern WorldCollisionGrid gShelterB2SepticTankCollisionGrid;

/// Six authored cameras for the septic tank's mapped views 1..6.
///
/// Indexed by mapped view minus one, with no terminator. Transforms use
/// signed world units and 4096 for unit rotation; projection distances are
/// pixels. Queued camera requests borrow these records until application.
extern ViewCamera gShelterB2SepticTankViewCameras[6];

/// Foreground sprite resources for the septic tank's mapped views 1..6.
///
/// Indexed by mapped view minus one, with no descriptor terminator. Views
/// 2..5 select source rectangles and terminated batch lists. Views 1 and 6
/// retain empty batch addresses in the source slot. All six keep decoded
/// background strips and have no clipping list. Keep the descriptors and
/// their lists loaded throughout packet allocation and drawing.
extern SpriteView gShelterB2SepticTankSpriteViews[6];

/// The septic tank's eight point lights for model shading in every view.
///
/// Directional and cone arrays are absent. Point-light nodes remain writable
/// for parenting, composition and attenuation updates; keep the collection
/// and its array in the loaded room overlay while lighting uses them.
extern WorldCoordRoomLights gShelterB2SepticTankRoomLights;

/// Six directed view boundaries connecting mapped views 2, 3, 4 and 5.
///
/// Opposing quads connect 2/3, 3/4 and 4/5; the last record carries LAST.
/// Origins/corners use room units and normals use 4096 per unit. Runtime
/// collision binds, links and latches these writable records while the room
/// is active; keep their storage loaded until the trigger list is cleared.
extern WorldCollisionTrigger gShelterB2SepticTankViewBoundaryTriggers[6];

/// Door warps and two automatic encounter-action regions for the septic tank.
///
/// Entries 0/1 require facing and warp to the main corridor/pod access tunnel.
/// Entries 2/3 send room actions 1/2; the room handler ignores 1 and uses 2 to
/// start combat setup after the introduction. The final record carries LAST.
/// Geometry uses room units and normals use 4096 per unit. Runtime binding,
/// linking and hit latching require writable loaded storage until list clear.
extern WorldCollisionTrigger gShelterB2SepticTankActionTriggers[4];

/// Collision and footstep records indexed by septic-tank surface class 0..7.
///
/// All eight pointers are populated, with no terminator. Class 1 passes
/// probes and ignores weapon impacts; every other class blocks probes and
/// allows impacts. Only class 2 has footstep cues. All classes apply grid
/// pushback. Records and cue storage require the room overlay loaded.
extern WorldCollisionSurfaceProperties* gShelterB2SepticTankSurfacePropertiesByClass[8];

/// Runs room initialization, encounter monitoring or teardown for the septic tank.
///
/// Requires a live bodyless room task with state 0 (register messages and spawn
/// water), 1 (monitor the introductory encounter) or 2 (destroy the task).
/// Keep the room overlay loaded while dispatching; state 0 publishes the task
/// in the room slot for synchronous messages.
void shelterB2SepticTankRoomTask(Task* task);

/// Selects the room's effects and draws beams and discs for the mapped view.
///
/// State 0 installs the flash, twin-trail, spark-burst, ripple and spray IDs,
/// then draws immediately as state 1. Mapped views 2..6 select world-space
/// point lists; other views emit nothing. Requires the active room and view
/// transforms, ordering table and primitive arena. Spawn arguments are unused.
void shelterB2SepticTankGlowTask(Task* task);

/// Advances and draws an expanding, fading water-surface ripple.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`. The low 12 bits of `spawnArg1` give the initial local
/// half-side in world units. Running updates grow it by 32 and dim brightness
/// from 64 by 2, retiring after 32 draws. Other control values redraw without
/// advancing; cancellation draws once before releasing work and task. Keep
/// the room overlay and effect controller live through retirement.
void shelterB2SepticTankWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with frame index 0. `spawnArg1` packs size scale
/// (bits 0..11), updates per cell (12..15, zero means 1), speed (16..23, zero
/// means 64), velocity kind
/// (24..27: 0 stationary, 1 upward burst, 2 spray, 3 narrow jet, 5 from `pos`)
/// and upright drawing (any bits 28..31). A supplied nonzero velocity bypasses
/// launch generation. After initialization, each draw moves in parent-space
/// world units and adds 6 to a moving particle's Y velocity.
/// Paused particles redraw; cancellation or animation completion releases work
/// and task. Keep the room overlay, parent and effect controller live.
void shelterB2SepticTankWaterDriftTask(Task* task);

/// Runs a pink charge flash, a screen tint at its peak and a fading star.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; `spawnArg1.value` is a positive charge duration in
/// running ticks, consumed as a countdown. Nonzero room effect control pauses;
/// values at least 4 cancel. Completion or state 3 releases work and task.
/// Keep the room overlay and effect controller live.
void shelterB2SepticTankRoomVisualEffectsFlashTask(Task* task);

/// Records and draws two fading trails between parent-relative endpoints.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its borrowed parent must remain live. Allocates two
/// eight-slot histories in `Task::work`, retrying allocation failure, then
/// records world-space snapshots and draws seven quads with RGB weights 1:2:3.
/// `spawnArg1.value` is an exact expiry age tested in state 1; zero never
/// expires and age is signed 16-bit. Expiry releases histories, work and task.
/// Control below 2 allows updates, including pause value 1; higher values
/// freeze it. Keep the room overlay and effect controller live.
void shelterB2SepticTankRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
///
/// Start in state 0 with a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, with age zero. Nonzero `spawnArg1.value` selects smoke;
/// zero selects rings and sparks. Enters release at active age seven and frees
/// work and task on the next active tick. Nonzero room effect control below
/// four pauses; four or above cancels. Children run independently. Keep the
/// room overlay, effect controller and borrowed parent live through retirement.
void shelterB2SepticTankRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_SEPTIC_TANK_H
