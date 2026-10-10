#ifndef INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_dryfield_junk_yard_8017F558[13];

/// Models the Dryfield map UI overlay's enemy descriptors attach.
extern TmdSource gDryfieldJunkYardModel01378;

/// Collision resources for the junk yard's sole room, indexed by room ID minus one.
///
/// Borrows the room grid, ten view boundaries, nineteen action triggers and one
/// sight occluder. Setup binds their coordinates and mutable list links. Keep
/// this overlay loaded while the grid is active or any of the lists are linked.
/// The Dryfield map overlay imports this one-entry directory; it has no terminator.
extern WorldCollisionRoomResources gDryfieldJunkYardRoomCollisionResources[1];

/// Lighting for the junk yard's sole room, indexed by room ID minus one.
///
/// Borrows eight point lights, two cone lights and seven per-view ambient
/// minima. Keep this overlay loaded while its records or lights are in use.
extern WorldCoordRoomLighting gDryfieldJunkYardRoomLighting[1];

/// Logical-view map for the junk yard's sole room, indexed by room ID minus one.
///
/// Borrows gameplay's `gViewIdentityMap`: views 1..7 select the corresponding
/// camera, image and sprite slots. The map has no terminator and is not owned
/// by this overlay; the room directory remains valid only while it is loaded.
extern u8* gDryfieldJunkYardRoomViewMaps[1];

/// Logical-view search count for the sole room, indexed by room ID minus one.
///
/// The signed-halfword count limits searches to seven bytes in the borrowed
/// identity map, independently of that map's capacity.
/// The directory has no terminator and belongs to this loaded overlay.
extern ViewCount gDryfieldJunkYardRoomViewCounts[1];

/// Three arrival/departure endpoints indexed by 1-based warp ID minus one.
///
/// Slots 1/2 serve the garage and trailer coach doors, selecting views 2/5 and
/// map markers 472/471. Slot 3 selects view 3 without a marker; its caller is
/// unproven. Positions use whole world units and yaw uses 4096 units per turn.
/// All blocked sounds and flags are zero. Entries are copied in full during
/// transitions and borrowed synchronously for actor spawning; keep this overlay
/// loaded while the map directory refers to them. There is no terminator.
extern DirectionWarpEntry gDryfieldJunkYardWarpEntries[3];

/// Cameras for mapped junk-yard views 1..7, indexed by mapped view minus one.
///
/// Rotation uses 12 fractional bits, translation is the negated world-space
/// camera origin, and projection distance is in pixels. Queued camera tasks
/// borrow these overlay-owned records until application; there is no terminator.
extern ViewCamera gDryfieldJunkYardViewCameras[7];

/// Sprite resources for mapped junk-yard views 1..7, indexed by view minus one.
///
/// Views 1 and 6 retain empty batch-list addresses in the source slot. Every
/// first batch has zero sprites, preserving decoded background strips; the
/// other views add depth-sorted sprites. All clip-list pointers are NULL.
/// These mutable records and their lists belong to this loaded overlay and
/// must outlive cached-packet allocation and drawing. There is no terminator.
extern SpriteView gDryfieldJunkYardSpriteViews[7];

/// Collision policies for junk-yard surface classes 0..7, with no terminator.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Only class 1 supplies footstep cues; classes 0 and 2..7 share a silent record.
/// The pointer directory, policy records and cues belong to this loaded overlay.
extern WorldCollisionSurfaceProperties* gDryfieldJunkYardSurfacePropertiesByClass[8];

/// Enables the junk yard's view effects, including dust from actor footsteps.
///
/// Gameplay dispatches this room callback as effect task 0xD8. The room-effect
/// controller must be live; the task argument is ignored and no state advances.
void dryfieldJunkYardEnableViewEffectsTask(Task* unusedTask);

/// Updates the placed Wire Rope pickup's visibility and ground shadow.
///
/// Requires a live TMD-bodied task with a model root and its live `Enemy`
/// metadata in `spawnArg2.pointer`. Its place-key low byte selects a current-stage
/// object state (0..63); the stage must be 1..5. State 2 suppresses model drawing,
/// automatic buffer allocation and the shadow. Other states clear the model
/// flags and ordering bias, drawing the shadow when a primitive buffer exists.
/// Keeps the task alive without advancing its state or allocating work.
/// The enemy metadata and model remain owned by the task until external teardown.
/// Visible buffered models require an initialized scratch stack, current view
/// matrices, and a frame packet arena and ordering table. Keep the junk-yard
/// overlay and those resources live while scheduled.
void dryfieldJunkYardWireRopeTask(Task* task);

/// Runs the junk yard's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`; idle also runs the companion debug
/// hook when debug mode is enabled. Keep the junk-yard overlay loaded for
/// the task's lifetime.
void dryfieldJunkYardRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_JUNK_YARD_H
