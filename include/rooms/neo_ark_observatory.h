#ifndef INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
#define INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Spawn descriptors for the room's player reflection and its attachment reflections.
///
/// Slot 0 starts the player reflection; slot 1 starts attachment or equipment
/// reflections. Both start bodyless at priority 112. There is no terminator.
/// The callbacks and their room resources require this overlay to stay loaded.
extern TaskDesc gNeoArkObservatoryReflectionTaskDescs[2];

/// Placement variants indexed directly by the saved `GameLocationKey.variant` (0..12).
///
/// Only variant 1 is present: its map-owned placements select the room's
/// scene-stop recipe. Slot 0 and slots 2..12 are absent, with both pointers
/// NULL. Keep the Neo Ark map, room and scene-actor overlays loaded while
/// loading, spawning or retaining references from the populated variant.
extern AreaVariant gNeoArkObservatoryAreaVariants[13];

/// Collision resources for room configurations 1 and 2, indexed by room ID minus one.
///
/// Both borrow the same complete grid, 18 view boundaries, 14 action triggers
/// and four sight occluders. Setup links and enables the trigger/occluder arrays;
/// the grid's leading obstacle slots remain mutable. Keep this room overlay
/// loaded until its resources are inactive and its linked records are removed.
extern WorldCollisionRoomResources gNeoArkObservatoryRoomCollisionResources[2];

/// Lighting for room configurations 1 and 2, indexed by the room ID minus one.
///
/// Both use 17 point lights and the default ambient minima. The light records
/// stay writable for coordinate composition and attenuation queries; all
/// borrowed pointers remain valid only while this room overlay is loaded.
extern WorldCoordRoomLighting gNeoArkObservatoryRoomLighting[2];

/// Logical-view maps for room configurations 1 and 2, indexed by the room ID minus one.
///
/// Each has 21 logical views, addressed at view minus one. Configuration 1
/// maps views 4..6 to camera/image/sprite indices 16..18; configuration 2 uses
/// the gameplay identity map. Room-owned pointers require this overlay loaded.
extern u8* gNeoArkObservatoryViewMaps[2];

/// Logical-view counts for room configurations 1 and 2, indexed by room ID minus one.
///
/// Each exposes 21 logical slots through `gNeoArkObservatoryViewMaps`.
/// The map overlay borrows this array while the room overlay is loaded.
extern ViewCount gNeoArkObservatoryViewCounts[2];

/// Three arrival/departure recipes, indexed by the area's 1-based warp slot minus one.
///
/// Slots 1..3 arrive in logical views 2, 11 and 10. Slot 1 supplies room-bank
/// arrival/departure sounds and a map-marker flag; slots 2 and 3 are silent.
/// Positions use room units and yaw uses 4096 units per turn. There is no
/// terminator; lookups borrow the entries while this room overlay is loaded.
extern DirectionWarpEntry gNeoArkObservatoryWarpEntries[3];

/// Cameras for mapped views 1..21, indexed by mapped view minus one.
///
/// Logical views select them through `gNeoArkObservatoryViewMaps`. Matrices
/// rotate world axes into camera axes with 4096 per unit; translations are
/// negated world origins, and perspective distances are pixels. There is no
/// terminator. The room overlay must remain loaded until a queued view applies
/// its borrowed camera.
extern ViewCamera gNeoArkObservatoryViewCameras[21];

/// Sprite resources for mapped camera views 1..21, indexed by mapped view minus one.
///
/// All views retain decoded background strips and use no draw-area clipping.
/// Views 1, 2, 13, 15, 16, 20 and 21 select no source sprites; their source slot
/// retains the zero-count batch-list address. Lists and mutable source records
/// are borrowed while the observatory overlay is loaded. Logical views reach
/// this table through `gNeoArkObservatoryViewMaps`.
extern SpriteView gNeoArkObservatorySpriteViews[21];

/// Collision and footstep properties indexed by the room-local surface class (0..7).
///
/// Every class blocks probes, enables weapon impacts and applies pushback.
/// Classes 1..3 have their own walking, running and scripted-jump cue bases;
/// classes 0 and 4..7 share a record without footstep cues. Borrowed records
/// and sound tables remain valid only while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gNeoArkObservatorySurfaceProperties[8];

/// Sets the base grey intensity of the observatory's light beams.
///
/// Stores the signed low halfword without clamping; room entry and scenes use
/// 0..160. The drawer adds a four-frame pulse of -4..4 and skips negative
/// results. This changes beams only; the view's glow discs retain their colours.
/// The room overlay must be loaded. The glow task resets the value on its first tick.
void neoArkObservatorySetLightBeamIntensity(s32 intensity);

/// Updates the companion obstacle reserved at the start of the room collision grid.
///
/// Uses the companion's model root, falling back to the player's. At least one
/// must be a live model task with a root mapping local geometry into room space.
/// Applies the room-axis offset (0, 0, -200) while a companion exists and
/// `GAME_FLAG_0D7` is nonzero; otherwise raises it by 10000 room units. The grid's
/// cell lists and remaining geometry stay intact. Requires the observatory
/// overlay; changes GTE state. The event-script argument is unused.
void neoArkObservatoryUpdateCompanionObstacle(s32 unusedEventArg);

/// Draws the observatory's glow discs and light beams for the mapped camera view.
///
/// Effect-table slot 0x14E resets beam intensity once in state 0, then draws
/// each tick in state 1. Requires the room overlay, composed view matrices,
/// initialized scratch stack, and the current frame's packet arena and ordering
/// table. It retains no pointers into those frame resources.
void neoArkObservatoryGlowTask(Task* effectTask);

/// Runs the observatory's room-message task and companion presentation.
///
/// Requires a live task in state 0..2: initialize the receiver, companion
/// obstacle and light beam; update companion visibility each tick; then
/// teardown. The room overlay must remain loaded through dispatch.
void neoArkObservatoryRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
