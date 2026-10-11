#ifndef INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Encounter layouts for the tunnel, indexed by the saved area variant (0..21).
///
/// Slots 1, 2, 3 and 11 publish actors; slot 21 publishes empty terminated lists.
/// Other slots are absent. Placements, resources and their task descriptors are
/// borrowed from loaded overlays and must remain live through loading and spawning.
extern AreaVariant gShelterB1ControlRoomAccessTunnelAreaVariants[22];

/// Room 1's logical-view map, borrowing gameplay's shared identity mapping.
///
/// Room and logical-view indices are 1-based. Views 1..3 select the same camera,
/// image and sprite indices; keep both the room and gameplay overlays loaded.
extern u8* gShelterB1ControlRoomAccessTunnelRoomViewMaps[1];

/// Room 1's three logical views, limiting reverse searches in its identity map.
extern ViewCount gShelterB1ControlRoomAccessTunnelRoomViewCounts[1];

/// Two access-tunnel door endpoints.
///
/// Slots 1 and 2 face the Main Corridor and Control Room doors, arriving
/// in views 2 and 3 respectively.
/// Index by 1-based warp ID minus one; there is no terminator. Placements
/// use whole world units and yaw uses 4096 units per turn. The stage map
/// borrows this array while the room overlay and its sound bank stay loaded.
extern DirectionWarpEntry gShelterB1ControlRoomAccessTunnelWarpEntries[2];

/// Tunnel collision mesh with two X cells and one Z cell, each 4000 world units.
///
/// Six quads use surface class 0 for the enclosure and class 1 for the floor.
/// Activation binds the writable descriptor to the current view. Its mesh and
/// terminated cell lists belong to this overlay and remain live while active.
extern WorldCollisionGrid gShelterB1ControlRoomAccessTunnelCollisionGrid;

/// Cameras for mapped views 1..3, in world coordinates with pixel projection distances.
///
/// Indexed by mapped view minus one; borrowed until camera application finishes.
extern ViewCamera gShelterB1ControlRoomAccessTunnelViewCameras[3];

/// Decoded-background presentation for mapped views 1..3, without source sprites.
///
/// Indexed by mapped view minus one. Each view retains its own zero-count batch
/// and following terminator, with no clipping list. Descriptors and batch lists
/// borrow this overlay's lifetime while sprite allocation or drawing uses them.
extern SpriteView gShelterB1ControlRoomAccessTunnelSpriteViews[3];

/// Room 1's four point lights, contributing in every logical view.
///
/// No directional or cone lights. Coordinates and attenuation remain writable;
/// keep this overlay loaded while composing or sampling the lights.
extern WorldCoordRoomLights gShelterB1ControlRoomAccessTunnelRoomLights;

/// Directed view boundaries switching between tunnel views 2 and 3.
///
/// Both quads are live; LAST marks the second. Room activation binds and links
/// the writable records, which must stay loaded until their list is cleared.
extern WorldCollisionTrigger gShelterB1ControlRoomAccessTunnelViewBoundaryTriggers[2];

/// Facing-gated exits to the B1 Main Corridor and Control Room.
///
/// The corridor exit uses departure slot 1 and arrival slot 2; the control-room
/// exit uses departure slot 2 and arrival slot 1. LAST includes the second quad.
/// Records are writable and stay loaded while linked to the action-trigger list.
extern WorldCollisionTrigger gShelterB1ControlRoomAccessTunnelWarpTriggers[2];

/// Collision policies for room-local surface classes 0..7.
///
/// All block probes, allow weapon impacts and apply pushback. Only class 1
/// supplies footstep cues. Borrowed records remain valid while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB1ControlRoomAccessTunnelSurfacePropertiesByClass[8];

/// Runs the Control Room Access Tunnel controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while its task and borrowed
/// message table remain available.
void shelterB1ControlRoomAccessTunnelRoomTask(Task* task);

/// Binds the room's actor effects once and draws its glows for the mapped view.
///
/// A fresh task starts in state 0; later ticks keep the bindings. Views 2 and 3
/// each draw two dim grey capsules, and view 2 also draws a red disc. Radius
/// scales are 512 world units; capsule angles are +1024 in view 2 and -1024 in
/// view 3, in 4096 units per turn. Requires the current view transform, scratch
/// stack, ordering table and packet arena. Other views draw nothing.
void shelterB1ControlRoomAccessTunnelDrawGlowsTask(Task* task);

/// Runs the room's counted pink charge flash, peak screen tint and fading star.
///
/// Requires a coordinate body and an owned `EffectWork` in `spawnArg2.pointer`.
/// Start in state 0 with a positive charge duration in `spawnArg1.value`, in
/// ticks; the task consumes that countdown. Nonzero room effect control pauses
/// updates, and values at least 4 cancel. Completion or state 3 releases the
/// counted work and kills the task. The room overlay must remain loaded.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlashTask(Task* task);

/// Records and draws the room's counted pair of fading endpoint trails.
///
/// Requires a coordinate body and owned `EffectWork` in `spawnArg2.pointer`;
/// its borrowed parent coordinate must remain live. State 0 allocates two
/// eight-frame histories owned by `Task::work` and seeds them at the parent-
/// relative offsets (0, 190, -15) and (0, 1085, 180), in world units. State 1
/// records world-space snapshots and draws seven quads with R:G:B = 1:2:3.
/// Allocation failure resets the age and retries on the next eligible tick.
/// `spawnArg1.value` is an age in ticks: an exact nonzero match during state 1
/// releases both histories and counted work. Zero never expires; age is signed
/// 16-bit. Control values below 2 allow updates, including pause value 1;
/// values at least 2 freeze this task. The room overlay must remain loaded.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the tunnel's impact flash followed by smoke or fading rings and sparks.
///
/// Requires a coordinate body and zero-aged, counted `EffectWork` in
/// `spawnArg2.pointer`, initialized by `effectSpawn`, with task state 0.
/// Nonzero `spawnArg1.value` selects smoke puffs; zero selects two bouncing
/// sparks and orange rings. Enters release at active age 7 and frees the work
/// and task on the next active tick. Spawned effects are independent tasks.
/// Nonzero room effect control pauses updates; values at least 4 cancel.
/// Keep the effect controller and room overlay loaded while dispatched.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsSparkBurstTask(Task* task);

/// Runs the tunnel's attached charge disc, player-joint sparks and fading release ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`; `spawnArg1.value` selects tint 0 or 1.
/// Start in `ROOM_VISUAL_EFFECTS_GLOW_DISC_ATTACH`; its owner can request
/// FLICKER, RELEASE or CANCEL through the corresponding state constants.
/// Borrows the work's parent coordinate and saved local offset. Growth emits
/// flying sparks from live player model parts 3..18 and adopts them for teardown;
/// the flying-spark selector must be installed. Nonzero room effect control
/// pauses updates; four or above cancels. Release/cancel frees work and child
/// tasks. The anchor, player, effect controller and room overlay remain live.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the tunnel's animated spark along a fixed step toward its initial target position.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, initialized by `effectSpawn`, with task state zero.
/// `spawnArg1.pointer` borrows a target `GfxCoord`; both world matrices must be
/// composed on the first active tick. The target is sampled only on that tick.
/// Initialization transforms the displacement into parent axes, with signed
/// 16-bit intermediate components, and scales it by 204/4096. Later active ticks
/// move by this fixed step every tick and draw on odd ages;
/// age 20 releases the work and task. Nonzero room effect control pauses updates;
/// control 4 or above cancels and releases them. The room overlay and controller
/// must remain live while the task runs.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the tunnel's expanding orange disc, layered glow and fading ring.
///
/// Requires a coordinate body and a counted, owned `EffectWork` in
/// `spawnArg2.pointer`, initialized by `effectSpawn`, with task state zero.
/// `spawnArg1` is unused. Sizes are world units; the ring fades before the disc,
/// while the glow refreshes a flickering orange point light and a ground glow.
/// Nonzero room effect control pauses updates; control 4 or above cancels.
/// Completion and cancellation release the work and task. The room overlay and
/// controller must remain live while the task runs.
void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_H
