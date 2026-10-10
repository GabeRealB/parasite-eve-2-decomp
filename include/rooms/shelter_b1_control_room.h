#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b1_control_room_80181BBC[2];

extern AreaApplyRec D_shelter_b1_control_room_80183BE0[2];

extern TaskDesc D_shelter_b1_control_room_80181B88;

extern AreaVariant D_shelter_b1_control_room_80183A98[22];

/// The control room's one-room directory of logical-view maps.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..8 select the same
/// 1-based camera, image and sprite indices. The directory has no terminator;
/// keep the room and gameplay overlays loaded while borrowing it.
extern u8* gShelterB1ControlRoomRoomViewMaps[1];

extern ViewCount D_shelter_b1_control_room_80181C74[];

extern DirectionWarpEntry D_shelter_b1_control_room_80181C78[];

/// The control room's walls, floors and raised walkway collision mesh.
///
/// Contains 29 faces and a 3-by-1 XZ cell index with 4000-unit square cells.
/// Vertices are room coordinates; normals use 4096 for unit length. Activation
/// binds the mutable grid to the view coordinate. Its private mesh pools and
/// face-index lists stay live only while this room overlay is loaded.
extern WorldCollisionGrid gShelterB1ControlRoomRoomCollision;

/// Eight control-room cameras, indexed by the mapped view minus one.
///
/// Matrices contain world-to-camera Q12 rotations and negated world origins;
/// projection distances are pixels. There is no terminator. Queued camera
/// application borrows a record until dispatch, requiring the room to stay loaded.
extern ViewCamera gShelterB1ControlRoomViewCameras[8];

/// The control room's eight mapped-view sprite descriptors.
///
/// Indexed by mapped view minus one. Views 2..6 select 99, 43, 29, 21 and 22
/// source rectangles; views 1, 7 and 8 retain empty batch-list addresses in
/// their source slots. Every view keeps decoded image strips and uses no clip
/// list. Descriptors and private mutable sources/batches borrow the room's lifetime.
extern SpriteView gShelterB1ControlRoomSpriteViews[8];

/// The control room's two authored point lights, with no directional or cone lights.
///
/// Both contribute in every logical view. Coordinate updates and lighting queries
/// mutate their private transforms and attenuation; the collection and light
/// storage must remain loaded while used. Position and radii use world units,
/// and RGB intensities use 4096 for full strength.
extern WorldCoordRoomLights gShelterB1ControlRoomRoomLights;

extern WorldCollisionTrigger D_shelter_b1_control_room_801834F4[];

extern WorldCollisionTrigger D_shelter_b1_control_room_80183624[];

/// The control room's ambient-light minima: a count header and eight view colours.
///
/// Entry zero holds the count; logical views 1..8 index the colour entries
/// directly. RGB minima use 16 units per 8-bit colour level. Lighting reads
/// these records while the room overlay is loaded; views above the count use
/// gameplay's default ambient minima.
extern WorldCoordRoomAmbientEntry gShelterB1ControlRoomAmbientByView[9];

/// Collision policies and footstep cues for the control room's surface classes 0..7.
///
/// All classes block probes, permit weapon impacts and apply pushback. Class 1
/// selects the main-floor cues; class 2 selects raised-floor and stair cues.
/// Classes 0 and 3..7 share the silent policy. There is no terminator; the
/// directory, private records and cue bases stay live with this room overlay.
extern WorldCollisionSurfaceProperties* gShelterB1ControlRoomSurfacePropertiesByClass[8];

/// Runs the control room's message receiver and entry-scene setup.
///
/// Requires a live task with state 0 (register handlers and optional layout-11
/// scene), 1 (idle), or 2 (kill). Keep the room and Shelter map overlays loaded
/// while the receiver and its borrowed message table remain available.
/// The receiver allocates no work; scene tasks own their own resources.
void shelterB1ControlRoomRoomTask(Task* task);

/// Draws the control room's fixed glows for the current mapped camera view.
///
/// On state 0, installs this room's glow-disc, flying-spark and orange-burst
/// effect IDs, sets state 1 and draws in the same tick. Mapped views 2 and 3
/// draw six capsules and an orange disc; view 3 adds four blue discs. Views 4
/// and 6 draw three and four blue discs respectively; other views draw none.
/// Requires the room overlay and view mapping to remain loaded, the current
/// view matrix composed, and the frame's scratch stack and GPU arena ready.
/// Uses only `task->state`; keeps running until its owner removes the task.
void shelterB1ControlRoomDrawGlowsTask(Task* task);

/// Runs the control room's attached charge disc, player-joint sparks and release ring.
///
/// Requires a coordinate body and counted `EffectWork` initialized by
/// `effectSpawn` in `spawnArg2.pointer`, with age, brightness, radius and ring
/// growth initially zero. `spawnArg1.value` selects tint 0 or 1.
/// Start in `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH`; the owner can request
/// FLICKER, RELEASE or CANCEL through the corresponding state constants.
/// Borrows the work's parent coordinate and saved local offset. Growth emits
/// flying sparks from live player model parts 3..18 and adopts them for teardown;
/// the flying-spark selector must be installed. Nonzero room effect control
/// pauses updates; values at least 4 cancel. Release/cancel frees work and child
/// tasks. Keep the anchor, player, effect controller and room overlay live.
void shelterB1ControlRoomRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the control room's animated spark toward its initial target position.
///
/// Requires live `gRoomEffectState`, a counted effect task with a coordinate
/// body, initial state zero, and owned `EffectWork` in `spawnArg2.pointer` with
/// age and frame index zero.
/// `spawnArg1.pointer` borrows a target `GfxCoord` through the first running
/// update; both cached matrices must be current in the same view space.
/// That update fixes a parent-space step at 204/4096 of the initial separation,
/// with signed 16-bit narrowing before scaling. Later running updates move by
/// that step and draw on odd ages; age 20 releases the work and tears down the
/// task. Room effect control 1..3 pauses; 4 or above cancels. The target is not
/// sampled again. The control room overlay must remain loaded while dispatched.
void shelterB1ControlRoomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the control room's orange burst with an expanding glow and fading ring.
///
/// Requires live `gRoomEffectState`, a counted effect task with a coordinate
/// body, initial state zero, and owned `EffectWork` in `spawnArg2.pointer`;
/// `spawnArg1` is ignored.
/// Running updates compose the coordinate, grow the disc and layered glow,
/// and fade the ring before fading the disc. A successful ground projection
/// adds a ground quad; the glow refreshes a flickering orange point light even
/// when off screen. Room effect control 1..3
/// pauses; 4 or above cancels. Cancellation or completed fading releases the
/// work and tears down the task; callers must not retain released pointers.
/// The control room overlay must remain loaded while dispatched.
void shelterB1ControlRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_H
