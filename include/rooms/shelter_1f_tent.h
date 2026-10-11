#ifndef INCLUDE_ROOMS_SHELTER_1F_TENT_H
#define INCLUDE_ROOMS_SHELTER_1F_TENT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_1F_TENT_AREA_VARIANT_COUNT = 13 };

/// Shelter 1F tent placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variant 1 publishes the three-soldier layout.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..12; there is no terminator or bounds check.
extern AreaVariant gShelter1fTentAreaVariants[SHELTER_1F_TENT_AREA_VARIANT_COUNT];

/// Extents of the tent's mapped sprite-view and collision surface directories.
enum {
    SHELTER_1F_TENT_VIEW_RESOURCE_COUNT = 10,
    SHELTER_1F_TENT_SURFACE_CLASS_COUNT = 8,
};

/// The tent's single-room directory of logical-view byte maps.
///
/// Room 1 borrows gameplay's `gViewIdentityMap`: logical views 1..10 select
/// the same 1-based camera, image and sprite indices. The directory lives in
/// this room overlay; the mapping remains owned by gameplay.
extern u8* gShelter1fTentViewMaps[1];

extern ViewCount D_shelter_1f_tent_80181D48[];

extern DirectionWarpEntry D_shelter_1f_tent_80181D4C[];

/// The tent's collision mesh and 2-by-3 XZ cell index.
///
/// References 17 normals, 66 vertices and 37 faces owned by this overlay.
/// Square cells span 4000 world units from X/Z origin (-4800, -200);
/// X-major cell lists end with `WORLD_COLLISION_GRID_CELL_END`. Activation
/// binds the writable grid to the view. Keep the overlay loaded while used.
extern WorldCollisionGrid gShelter1fTentCollisionGrid;

/// Authored cameras for mapped view indices 1..10, stored at index minus one.
///
/// Rotation coefficients use `ONE` (4096) per unit; translation is the negated
/// world-space camera origin in game units. Projection distance is in pixels.
/// The stage map borrows this array; keep the room overlay loaded until queued
/// camera tasks have applied their records. There is no terminator.
extern ViewCamera gShelter1fTentViewCameras[10];

/// The tent's ten sprite views, indexed by mapped view minus one.
///
/// Views 2, 3, 4, 8 and 9 select 78, 67, 47, 40 and 16 depth-sorted
/// rectangles; the other views have empty lists. All retain decoded background
/// strips and supply no clipping areas. Lists end with `SPRITE_BATCH_END`.
/// The writable sources and batches are borrowed while this overlay is loaded;
/// mapped indices must be in 1..10 when caching or drawing sprites.
extern SpriteView gShelter1fTentSpriteViews[SHELTER_1F_TENT_VIEW_RESOURCE_COUNT];

/// Three neutral point lights shared by every tent view, with no directional or cone lights.
///
/// Positions are in world units, intensity has 12 fractional bits, and falloff
/// spans 3500..5000 world units. The collection borrows this overlay's writable
/// light array: coordinate setup and lighting queries update it. Keep the
/// overlay loaded while the collection is in use.
extern WorldCoordRoomLights gShelter1fTentRoomLights;

extern WorldCollisionTrigger D_shelter_1f_tent_80183A94[];

extern WorldCollisionTrigger D_shelter_1f_tent_80183CF4[];

/// Collision policies and footstep cues indexed by surface class (0..7).
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Class 1 is the floor and supplies walk/run footstep pairs; scripted jumps
/// use its walking pair. Classes 0 and 2..7 share the record without footstep
/// cues. Records and sound bases are borrowed only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelter1fTentSurfacePropertiesByClass[SHELTER_1F_TENT_SURFACE_CLASS_COUNT];

/// Runs the tent's room controller for one tick.
///
/// Requires a live bodyless task with state 0..2 and loaded room/actor resources.
/// State 0 installs room messages and applies first-arrival or ambience setup;
/// state 1 stays idle for messages and state 2 requests teardown. The controller
/// owns no work; the first-arrival script and scene tasks have their own lifetimes.
void shelter1fTentRoomTask(Task* task);

/// Updates the tent's telephone save menu and optional statistics panels.
///
/// Called by the gameplay telephone dispatcher with a live UI task in state
/// 0..3 and its owned `UiObject` in `spawnArg2.pointer`. Start in state 0; keep
/// the tent's menu resources loaded until the UI tree closes. Normal play
/// opens the save dialog directly; a cleared game or demo scene 1 also offers
/// Play Data, Weapon Data and PE Data. Publishes dismissal through the UI object.
void shelter1fTentTelephoneMenuTask(Task* task);

/// Draws the tent's fixed world-space glows for the current mapped camera view.
///
/// Views 2..7 select capsules, flickering discs and pulsing cyan highlights;
/// other views emit no packets. `unusedTask` is unused, and no task state changes.
/// Requires the loaded room's view transform, initialized scratch stack and
/// current frame's ordering table and packet arena. Queued packets live until
/// that frame's GPU work completes.
void shelter1fTentDrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_TENT_H
