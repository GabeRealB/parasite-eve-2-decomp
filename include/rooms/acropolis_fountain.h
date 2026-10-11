#ifndef INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
#define INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Resource-directory extents exported by the fountain overlay.
enum {
    ACROPOLIS_FOUNTAIN_ROOM_COUNT          = 2,
    ACROPOLIS_FOUNTAIN_SURFACE_CLASS_COUNT = 8,
    ACROPOLIS_FOUNTAIN_WARP_COUNT          = 5,
    ACROPOLIS_FOUNTAIN_SPRITE_VIEW_COUNT   = 22,
    ACROPOLIS_FOUNTAIN_CAMERA_COUNT        = 21
};

/// Actor layout variants for the Acropolis fountain.
///
/// Indexed directly by `GameLocationKey.variant` (0..10); no terminator.
/// Slots 2 and 7 publish placement/resource pairs; all other slots are NULL pairs.
/// Variant 1 is empty.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAcropolisFountainAreaVariants[11];

/// Collision resources for fountain rooms 1 and 2, indexed by room - 1.
///
/// Both entries borrow the same mutable grid, twelve view boundaries and nine
/// action triggers; neither supplies sight occluders. Setup binds and enables
/// the triggers through each array's LAST record. Keep the fountain overlay
/// loaded while the grid is active or any of its triggers remain linked.
extern WorldCollisionRoomResources gAcropolisFountainRoomCollisionResources[ACROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Logical-view maps for the fountain's two room indices, selected at room - 1.
///
/// Both maps have 22 active entries, indexed at logical view - 1, returning
/// 1-based camera/image/sprite indices. Room 1 uses the identity map; room 2
/// maps logical views 2..6 to indices 10..14 and leaves the others unchanged.
/// These maps have no terminator. Keep the fountain and gameplay overlays
/// loaded while borrowing them; mapped indices require corresponding resources.
extern u8* gAcropolisFountainViewMaps[ACROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Signed logical-view search counts for rooms 1 and 2, indexed by room - 1.
///
/// Each reverse lookup searches 22 map bytes. These counts describe logical
/// slots, independently of the map capacities and the 21-camera resource extent.
/// Neither entry is a terminator; the fountain overlay owns both halfwords.
extern ViewCount gAcropolisFountainViewCounts[ACROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Lighting descriptors for both fountain room indices, selected at room - 1.
///
/// Both borrow the same mutable six-point-light collection and use the default
/// ambient minima. The descriptors and lights require the fountain overlay
/// to remain loaded while lighting binds or queries them.
extern WorldCoordRoomLighting gAcropolisFountainRoomLightingTable[ACROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Five fountain arrival/departure endpoints, indexed by warp ID 1..5 minus one.
///
/// Endpoints 1..3 face the patio, forked-road and hallway doors and select
/// logical views 2, 6 and 3. Endpoints 4 and 5 repeat the latter placements with
/// initial view 17. Player and companion placements agree; all endpoints have
/// door sounds, no blocked sound and no departure flags. Only endpoints 1..3
/// set map markers. There is no terminator; keep the overlay loaded while
/// resolving or copying an endpoint.
extern DirectionWarpEntry gAcropolisFountainWarpEntries[ACROPOLIS_FOUNTAIN_WARP_COUNT];

/// Sprite resources for mapped indices 1..22, indexed by mapped view - 1.
///
/// Every batch list keeps decoded background strips; no view supplies clipping.
/// Views 7 and 15 share sources and batches. Views 1, 2, 9, 10, 16, 18, 19, 21
/// and 22 have no source sprites and retain the batch address in `sources.empty`.
/// Other views select bounded source ranges ending with `SPRITE_BATCH_END`.
/// The descriptors and their mutable arrays are borrowed from this overlay and
/// must remain loaded during packet allocation and drawing.
extern SpriteView gAcropolisFountainSpriteViews[ACROPOLIS_FOUNTAIN_SPRITE_VIEW_COUNT];

/// Cameras for mapped indices 1..21, indexed by mapped camera - 1.
///
/// Rotations use 4096 per unit, translations are negated world origins in game
/// units, and projection distances are pixels. Index 1 is the plan camera;
/// indices 10..15 provide alternate records for indices 2..7. The table has no
/// terminator and supplies no camera for mapped index 22 despite the view maps'
/// retained slot. Camera callers must select 1..21 and keep the overlay loaded
/// until a queued camera request has copied its record.
extern ViewCamera gAcropolisFountainViewCameras[ACROPOLIS_FOUNTAIN_CAMERA_COUNT];

/// Collision policies and footstep cues indexed by fountain surface class 0..7.
///
/// Every class blocks probes and applies pushback. Class 2 suppresses weapon
/// impacts; all others allow them. Only classes 4 and 5 have footstep cues.
/// Both room indices share this directory and its borrowed records, which
/// remain valid only while the fountain overlay is loaded.
extern WorldCollisionSurfaceProperties* gAcropolisFountainSurfacePropertiesByClass[ACROPOLIS_FOUNTAIN_SURFACE_CLASS_COUNT];

/// Draws the fountain spray as a flickering, semitransparent textured quad.
///
/// Requires a live coordinate body, the room's texture and palette, and space
/// for one `POLY_FT4` in the frame arena. Draws in raw camera views 7, 8, 15 and
/// 21 while room effects are active. Reserves and releases scratch storage and
/// consumes a packet even when projected depth (SZ3 / 4) is below 17.
void acropolisFountainSprayTask(Task* task);

/// Queues the player's fountain climb from the direction-action dispatch.
///
/// Both callback arguments are ignored. Requires the loaded fountain overlay
/// and live player through climb completion. Task-allocation failure is ignored.
void acropolisFountainStartClimb(s32 unusedActionId, s32 unusedControl);

/// Spawns the fountain spray and replaces its movie task when the mapped view changes.
///
/// Effect-bank entry 0x026 starts in state 0 with a live coordinate body and
/// effect-owned `EffectWork` in `spawnArg2.pointer`. Its `scale` halfword latches
/// the 1-based mapped camera index, 1..22 for this room. Indices 2..8 and 17
/// start the movie; changing away from any of them queues its stop callback,
/// then starts the new view on the next update. Logical views sharing an index
/// do not restart it. The task draws nothing and has no task-work allocation.
/// Requires loaded room/view/movie resources. Only one controller may run,
/// and each queued stop requires the published movie-task handle still live.
/// Spawn failures are unchecked and the stop callback leaves that handle set.
void acropolisFountainViewEffectsTask(Task* task);

/// Runs the player's turn, one-step ascent and walk to the room's fixed destination.
///
/// Spawned from task bank 2, slot 14 with state zero. The player task must remain
/// live through the six phases (0 turn, 1 await turn, 2 ascend, 3 await ascent,
/// 4 walk, 5 await arrival). Completion restores player control and kills this
/// task. Its state is an unchecked index maintained only by these callbacks.
void acropolisFountainClimbTask(Task* task);

/// Runs the fountain's room-message receiver and restores its climb interaction.
///
/// State 0 registers the receiver and restores the saved climb trigger;
/// state 1 idles with messages available; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisFountainRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
