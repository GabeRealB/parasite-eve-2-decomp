#ifndef INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
#define INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Per-step surface classes for this area's two stair flights.
///
/// Rows 0/1 are ascent/descent for direction flight 2 (ten steps), and rows
/// 2/3 for flight 3 (eight steps). Step zero through the final landing selects
/// a byte for `GameActor.surfaceClass`; remaining row bytes have no proven use.
/// Gameplay borrows the rows while this room overlay is loaded.
extern u8 gShelterB4WaterSupplyStairSurfaceRows[4][16];

/// Placement/resource layouts indexed by this area's saved placement variant.
///
/// The twelve slots publish variants 1, 2, 3 and 11; slot 0 and slots 4..10
/// have no tables. Variant 1 is the default. These are placement variants,
/// independent of room/view selectors; keep the room loaded during use.
extern AreaVariant gShelterB4WaterSupplyPlacementVariants[12];

/// Logical-view map directory for the water-supply area's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..11 select the same
/// 1-based camera, image and sprite indices. Keep this overlay loaded during lookup.
extern u8* gShelterB4WaterSupplyViewMaps[1];

/// Eleven logical views for the water-supply area's single room.
///
/// Indexed by room minus one; this is a view count, not the identity map's
/// capacity. The logical views map to camera/image/sprite indices 1..11.
extern ViewCount gShelterB4WaterSupplyViewCounts[1];

/// Three 1-based arrival/departure endpoints for this area's warps.
///
/// Warp 1 connects the upper sewer (initial view 7), warp 2 the reservoir
/// (view 11), and warp 3 the valve's Water Hole route (view 2). Positions use
/// world units and yaw uses 4096 units per turn. The map overlay borrows these
/// records while this room is loaded; there is no terminator.
extern DirectionWarpEntry gShelterB4WaterSupplyWarpEntries[3];

/// Collision mesh and cell index for the water-supply area's single room.
///
/// The 4-by-4 X-major grid has 4000-unit cells and origin (400, -13900) in X/Z;
/// its 45 faces reference 59 vertices and 12 normals. The private pools remain
/// live with this mutable record; room setup binds the current view coordinate.
extern WorldCollisionGrid gShelterB4WaterSupplyCollisionGrid;

/// Cameras for mapped views 1..11, indexed by mapped view minus one.
///
/// Rotation uses 4096 per unit, translation is the negated world-space camera
/// origin, and projection distance is in pixels. Borrow only while this room is loaded.
extern ViewCamera gShelterB4WaterSupplyViewCameras[11];

/// Sprite resources for mapped views 1..11, indexed by mapped view minus one.
///
/// All views retain the decoded background; views 4, 7 and 8 add source sprites.
/// Batch lists end with `SPRITE_BATCH_END`; no view has a clipping-area list.
/// The records and their private source/batch arrays live with this room overlay.
extern SpriteView gShelterB4WaterSupplySpriteViews[11];

/// This room's ten point lights, with no directional or cone lights.
///
/// The private light array stays writable for transform composition and shading.
/// Keep this overlay loaded while the lighting directory borrows the collection.
extern WorldCoordRoomLights gShelterB4WaterSupplyRoomLights;

/// Eighteen directed view boundaries joining logical views 2..11.
///
/// Each adjacent boundary has both travel directions. Room setup binds and
/// links these writable records; the last live record carries LAST.
extern WorldCollisionTrigger gShelterB4WaterSupplyViewBoundaryTriggers[18];

/// Seven facing-gated actions: four stair ends, two exits and the valve route.
///
/// Stair selectors borrow flights 2/3 from `gShelterB4WaterSupplyStairSurfaceRows`.
/// Exits request the reservoir or upper sewer; the valve sends room action 10
/// with the nighttime Water Hole area. Setup mutates links, coordinates and
/// hit state; LAST marks the final live record. Keep the room overlay loaded.
extern WorldCollisionTrigger gShelterB4WaterSupplyActionTriggers[7];

/// Two room-space quads blocking line-of-sight segments.
///
/// Room setup links and enables these mutable records. LAST marks the second
/// included quad; low flag bit 0 has no established interpretation.
extern WorldCollisionOccluder gShelterB4WaterSupplyOccluders[2];

/// Minimum ambient colours for logical views 1..11, following a view-count header.
///
/// Entry zero stores 11; entries 1..11 store RGB minima in 16 units per colour
/// level and retained luma. The lighting directory borrows this room-overlay table.
extern WorldCoordRoomAmbientEntry gShelterB4WaterSupplyRoomAmbient[12];

/// Collision policies and footstep cue bases indexed by surface class 0..7.
///
/// Classes 0 and 4..7 share the silent blocking policy. Classes 1 and 2 block
/// probes and allow weapon impacts; class 3 passes probes and ignores impacts.
/// All apply pushback. Classes 1 and 3 share footstep cues; class 2 has its own.
/// This directory and its private records remain valid only while the room is loaded.
extern WorldCollisionSurfaceProperties* gShelterB4WaterSupplySurfacePropertiesByClass[8];

/// Runs the water-supply room receiver's initialize, idle or teardown state.
///
/// Requires a live bodyless task with state 0..2 and this package loaded for
/// its lifetime. State 0 registers the receiver and starts water rendering;
/// state 1 waits for messages; state 2 releases the task.
void shelterB4WaterSupplyRoomTask(Task* task);

/// Installs this room's effect IDs, emits player-water splashes and draws light beams.
///
/// Requires a live player model with coordinates through 17 and a writable
/// `EffectWork` in `spawnArg2.pointer`. State zero records parts 14 and 17's
/// composed XYZ without refreshing them; later ticks sample movement only while
/// room effect control is running and the water Y is less than the player's root Y.
/// Samples and odds narrow to signed halfwords. Two ordered rolls per part use
/// odds out of 512, with a 32-point ripple bias; no clamp occurs. Pauses retain
/// old samples. Spawns snapshot the temporary surface coordinate; the children
/// never read its retained source pointer. Beams draw for
/// mapped views 2..11 irrespective of effect control. The task lives until
/// external teardown; keep the player, room resources and render state live.
void shelterB4WaterSupplySplashAndLightBeamsTask(Task* task);

/// Runs the water-supply room's expanding, fading water-surface ripple.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero. `spawnArg1` bits 0..11 give the
/// initial local half-side in coordinate units; higher bits are ignored.
/// Running updates grow by 32 and dim by 2, lasting 32 updates. Non-running
/// control redraws the retained state; cancellation redraws once before
/// releasing the work and task. The coordinate's parent must remain live.
void shelterB4WaterSupplyWaterRippleTask(Task* task);

/// Runs the water-supply room's eight-cell water-spray particle with gravity.
///
/// Requires a counted effect task with owned zeroed `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state and frame index zero.
/// `spawnArg1` bits 0..11 give perspective size scale, bits 12..15 updates per
/// cell (0 selects 1), and bits 16..23 parent-space speed (0 selects 64).
/// Bits 24..27 select velocity: 0 stationary, 1 upward burst, 2 all-axis spray,
/// 3 narrow upward jet, 5 the copied offset in `pos`; other kinds retain a zero
/// direction before normalization. Any set bits 28..31 select upright drawing;
/// otherwise drawing is rotated. A preseeded nonzero `move` bypasses generation.
/// Initialization composes without drawing; later updates draw, move and add
/// 6 to signed-halfword Y velocity. Non-running control redraws without aging;
/// cancellation releases the work and task without drawing.
void shelterB4WaterSupplyWaterDriftTask(Task* task);

/// Runs this room's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a coordinate body and counted, owned, zeroed `EffectWork` in
/// `spawnArg2.pointer`. Its borrowed parent and copied offset establish the
/// attachment; `spawnArg1.value` selects tint 0 or 1. Start in state zero.
/// The owner requests flicker, release or cancellation through the shared
/// glow-disc states. Growing emits an adopted flying spark every fourth age
/// from player parts 3..18, requiring that live model and installed callback.
/// Nonzero room effect control pauses below four and cancels at four or above;
/// release fades the ring and frees work and child tasks. Keep this package and
/// coordinate ancestors live until teardown, then discard retained pointers.
void shelterB4WaterSupplyRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the water-supply glowing-disc spark toward an initial target coordinate.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and state zero. `spawnArg1.pointer` borrows a `GfxCoord`
/// through the first running update; both world matrices must be composed.
/// That update fixes a parent-space step at 204/4096 of the initial separation.
/// Later updates move and draw on odd ages, releasing the work and task at age
/// 20. Non-running control pauses without drawing; cancellation releases the
/// effect. The target is not sampled after initialization.
void shelterB4WaterSupplyRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the water-supply orange burst with a growing disc, glow and fading ring.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero; `spawnArg1` is ignored. Running
/// updates compose, grow the glow and fade the ring before the central disc.
/// Non-running control pauses without drawing. Cancellation or completed fading
/// releases the work and task; callers must not retain released pointers.
void shelterB4WaterSupplyRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B4_WATER_SUPPLY_H
