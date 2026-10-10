#ifndef INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
#define INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gShelterB1SleepingQuartersModel02DFC;

/// Actor layouts indexed by the sleeping quarters' placement variant, 0..21.
///
/// Slots 1, 2, 3, 11 and 17 contain layouts; all others are absent. Variant 1
/// has an Odd Stranger and four rats, 2 has seven amoebae, 3 has two Mad Chasers
/// and two Creeping Strangers, 11 has a GOLEM Knight and 17 has a GOLEM Bishop.
/// Each populated slot borrows sentinel-terminated placements and resources.
/// Spawning and loading require the room overlay to remain loaded; actors
/// retain placement pointers until teardown. This directory has no terminator.
extern AreaVariant gShelterB1SleepingQuartersAreaVariants[22];

/// The sleeping quarters' one-room directory of logical-view maps.
///
/// Room 1 borrows `gViewIdentityMap`, so mapped image and sprite indices equal
/// their logical view IDs. Reverse lookup searches 11 map entries; resource
/// arrays have independent bounds. The directory has no terminator and remains
/// valid only while this overlay is loaded; the byte map belongs to gameplay.
extern u8* gShelterB1SleepingQuartersRoomViewMaps[1];

/// Reverse-lookup search counts for the sleeping quarters' single room.
///
/// Room 1's signed 16-bit count searches 11 entries of `gViewIdentityMap`.
/// Indexed by room minus one, with no terminator. This count is independent
/// of the ten-camera array and eleven sprite descriptors. The room overlay
/// must remain loaded while the Shelter map directory borrows this array.
extern ViewCount gShelterB1SleepingQuartersRoomViewCounts[1];

/// Door endpoints for the main corridor and north maintenance walkway.
///
/// Slots 1 and 2 are indexed by warp ID minus one, without a terminator.
/// Both place player and companion at the same world position, facing yaw
/// 1024 or 2048 (4096 units per turn), and select room views 2 or 9.
/// Arrival/departure sounds use the room bank; blocked sounds and option flags
/// are absent. Slot 2 marks the walkway door on the map after arrival.
/// The Shelter map borrows these records while the room overlay is loaded;
/// transition code copies each complete entry before using it.
extern DirectionWarpEntry gShelterB1SleepingQuartersWarpEntries[2];

/// The sleeping quarters' room-space collision mesh and XZ cell index.
///
/// Contains 64 quads, 143 vertices and seven Q12 normals. The 4-by-3 cell
/// table has 4000-game-unit cells and additive X/Z biases of 1400/4400.
/// Setup binds the view coordinate; the descriptor and mesh pools remain
/// mutable and must stay loaded while active.
extern WorldCollisionGrid gShelterB1SleepingQuartersCollisionGrid;

/// Ten authored cameras for the sleeping quarters' mapped views 1..10.
///
/// Indexed by mapped view minus one, without a terminator. Transforms use
/// Q12 rotation and negated world origins; projection distances are pixels.
/// View 11 has image/sprite resources but no camera record in this array;
/// requests using this camera directory must stay within views 1..10.
/// Queued camera tasks borrow a record until application, so keep the room loaded.
extern ViewCamera gShelterB1SleepingQuartersViewCameras[10];

/// Sprite and background descriptors for the sleeping quarters' mapped views 1..11.
///
/// Indexed by mapped view minus one, without a descriptor terminator. Views
/// 3..8 and 10 select source sprites; 1, 2, 9 and 11 retain empty batch addresses
/// in the source slot. Every view requests decoded background strips. Views
/// 5..7 also supply clipping lists. Sources, batches and clips borrow mutable
/// room-overlay storage throughout packet allocation and drawing. The camera
/// directory has its own ten-record extent.
extern SpriteView gShelterB1SleepingQuartersSpriteViews[11];

/// The sleeping quarters' four directional and five point model-shading lights.
///
/// Every source admits all views; there are no cone lights. Coordinate updates
/// and lighting queries mutate the borrowed arrays, which remain valid while
/// this overlay is loaded.
extern WorldCoordRoomLights gShelterB1SleepingQuartersRoomLights;

/// Eighteen directed quads requesting transitions among room views 2..10.
///
/// Opposing pairs join 2/3 (twice), 3/4, 4/5, 5/6, 5/7, 7/8, 8/9 and 7/10.
/// Positions and radii use game units; normals use 4096 per unit. LAST marks
/// the final live record. Room setup binds the view coordinate, links and
/// enables these mutable records; keep their overlay loaded until unlinked.
extern WorldCollisionTrigger gShelterB1SleepingQuartersViewBoundaryTriggers[18];

/// Five room-space quads blocking line of sight in the sleeping quarters.
///
/// Origins and corners use game units; normals use 4096 per unit. The final
/// live quad carries LAST. Binding mutates links and enable flags; these records
/// must remain loaded until the room's occluder list is cleared.
extern WorldCollisionOccluder gShelterB1SleepingQuartersOccluders[5];

/// Two door-warp quads and fifteen facing-gated CAP interaction quads.
///
/// The doors request main-corridor arrival slot 5 from room warp slot 1, and
/// north-walkway arrival slot 1 from room warp slot 2. Interactions select CAP
/// commands 1..13; selectors 3 and 8 route through the room-message handler.
/// Positions/radii use game units and normals use 4096 per unit. LAST marks
/// the final live record. Setup mutates links, coordinate and enable flags;
/// collision updates hit latches. Keep the room overlay loaded until unlinked.
extern WorldCollisionTrigger gShelterB1SleepingQuartersActionTriggers[17];

/// Collision and footstep policies indexed by room surface class 0..7.
///
/// Classes 0 and 3..7 block probes, allow weapon impacts and have no footsteps.
/// Class 1 passes probes, ignores weapon impacts and has no footsteps; class 2
/// blocks probes and allows impacts with walk/run footstep pairs. All classes
/// apply pushback. Entries borrow scalar records owned by this loaded overlay;
/// the table has no terminator.
extern WorldCollisionSurfaceProperties* gShelterB1SleepingQuartersSurfacePropertiesByClass[8];

/// Hides the sleeping quarters' placed model when its packed object state is 2.
///
/// Requires a live TMD-body task and its area-spawned `Enemy` work in
/// `spawnArg2.pointer`. The low byte of `Enemy::placeKey` selects an object-state
/// index in 0..63 in the session's current stage; the room's placement uses 20.
/// Other state values show the model. Only the active-draw flag changes; the
/// task keeps its model and work until its owner tears them down.
void shelterB1SleepingQuartersAreaObjectTask(Task* task);

/// Runs the sleeping quarters' room-message receiver.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while the task and its borrowed
/// message table remain available. The receiver allocates no work.
void shelterB1SleepingQuartersRoomTask(Task* task);

/// Draws the Sleeping Quarters' view-specific light beams and red glow disc.
///
/// Bank-6 effect task 0x12D. On state 0, selects this room's glow-disc,
/// flying-spark and orange-burst effects and advances to state 1; drawing
/// starts in the same tick and continues in every nonzero state. Uses the
/// current mapped camera index; only views 2..10 emit lights.
/// Requires a live task, the room overlay loaded, composed view matrices,
/// and the current frame's scratch stack, primitive arena and ordering table.
/// Queued primitives remain in the frame arena until GPU completion.
void shelterB1SleepingQuartersDrawViewLightsTask(Task* task);

/// Runs an attached charge disc with player-joint sparks and a fading release ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1.value` selects tint 0 or 1.
/// Starts at state 0 to attach at the copied offset in its borrowed parent's
/// axes. The owner requests flicker, release or cancellation using
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Growth emits a flying spark from player
/// parts 3..18 every fourth active age, adopting each child for teardown.
/// Nonzero room effect control pauses; four or above cancels. Completion frees
/// work, children and task. Keep the parent, player model, installed flying-spark
/// effect, controller and sleeping-quarters overlay live through teardown.
void shelterB1SleepingQuartersRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the sleeping quarters' orange burst with a growing glow and fading ring.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body and initial state zero; `spawnArg1` is ignored. Running
/// updates expand the disc and layered glow, refresh a transient point light,
/// and fade the ring before the central disc. Room effect control 0 runs;
/// other values below 4 pause without drawing, and values 4 or above cancel.
/// Cancellation or completed fading releases the work and tears down the task.
/// The sleeping-quarters overlay must remain loaded while the task is live.
void shelterB1SleepingQuartersRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Runs the sleeping quarters' glowing-disc spark toward an initial target position.
///
/// Requires a counted effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state zero, and zero work age and animation index.
/// `spawnArg1.pointer` borrows a `GfxCoord` through the first running update;
/// both cached matrices must be composed into the same view space then.
/// That update narrows the initial displacement to signed 16-bit local
/// components and fixes a parent-space step scaled by 204/4096. Later updates
/// move by that step and draw on odd ages; the target is not sampled again.
/// At age 20 the work is released and the task torn down. Room effect control
/// 0 runs; other values below 4 pause without drawing, and values 4 or above
/// cancel and release the effect.
/// The sleeping-quarters overlay must remain loaded while the task is live.
void shelterB1SleepingQuartersRoomVisualEffectsFlyingSparkTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
