#ifndef INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
#define INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Bodyless daytime trailer-coach shop session, executed at priority 192.
///
/// Gameplay's shop dispatcher supplies the packed stock/category selector in
/// spawnArg1 and requests room reload on exit. Keep this overlay loaded while
/// the session and its UI tasks run; the UI tasks own their objects.
extern TaskDesc gDryfieldTrailerCoachShopSessionTaskDesc;

/// Complete extent of the trailer coach's saved-placement variant directory.
enum { DRYFIELD_TRAILER_COACH_AREA_VARIANT_COUNT = 13 };

/// Daytime trailer-coach placement/resource variants, indexed by location.variant.
///
/// Indices 0..12 are present: variant 1 pairs the stage map's Gary placements
/// with this room's Gary resources; slot 0 and slots 2..12 are NULL pairs.
/// The loader and scene borrow the directory and resource table from this room,
/// and the placements from the Dryfield map. Keep both overlays loaded while
/// their tables are used. There is no terminator or index check.
extern AreaVariant gDryfieldTrailerCoachAreaVariants[DRYFIELD_TRAILER_COACH_AREA_VARIANT_COUNT];

extern TmdSource gDryfieldTrailerCoachItemBoxModel;

/// Collision resources for room 1 of daytime Dryfield area 27.
///
/// Indexed at room minus one by the stage map. Borrows one mesh, four view
/// boundaries and twelve action quads; there are no occluders. Room setup
/// mutates the grid/trigger records. Keep this overlay loaded while active.
extern WorldCollisionRoomResources gDryfieldTrailerCoachRoomCollisionResources[1];

/// Number of logical views and mapped camera/sprite records in the trailer coach.
enum { DRYFIELD_TRAILER_COACH_VIEW_COUNT = 11 };

/// Number of collision surface classes in the trailer coach's surface directory.
enum { DRYFIELD_TRAILER_COACH_SURFACE_CLASS_COUNT = 8 };

/// Light collection and per-view ambient minima for room 1 of daytime Dryfield area 27.
///
/// The stage map borrows this array at room minus one. Its collection contains
/// twelve mutable point lights; ambient entries cover logical views 1..11.
/// Keep the trailer-coach overlay loaded while any of these pointers are used.
extern WorldCoordRoomLighting gDryfieldTrailerCoachRoomLighting[1];

/// Logical-view map for room 1 of daytime Dryfield area 27.
///
/// Borrows `gViewIdentityMap`: logical views 1..11 select the same camera/image
/// and sprite indices. Index this directory at room minus one, then the map at
/// view minus one. The directory belongs to the loaded trailer-coach overlay;
/// the mapped byte array belongs to gameplay and has no terminator.
extern u8* gDryfieldTrailerCoachViewMaps[1];

/// Eleven logical views for room 1 of daytime Dryfield area 27.
///
/// Indexed at room minus one, this signed-halfword count bounds reverse view
/// lookup in `gDryfieldTrailerCoachViewMaps`. Keep the owning overlay loaded.
extern ViewCount gDryfieldTrailerCoachViewCounts[1];

/// Two trailer-coach arrival endpoints, selected by warp id minus one.
///
/// Both place player and companion at (1579, 0, -800), yaw half a turn, and
/// update map flag 471; endpoint 1 selects view 2, endpoint 2 selects view 5.
/// Sound cues belong to daytime Dryfield area 27. The stage map borrows this
/// array while the room overlay is loaded; there is no terminator.
extern DirectionWarpEntry gDryfieldTrailerCoachWarpEntries[2];

/// Authored cameras for mapped trailer-coach views 1..11.
///
/// Indexed at mapped view minus one. Rotations use 4096 units for 1.0;
/// translations are negated world-coordinate origins and projection distances
/// are pixels. The stage map borrows this array while this overlay is loaded.
extern ViewCamera gDryfieldTrailerCoachViewCameras[DRYFIELD_TRAILER_COACH_VIEW_COUNT];

/// Sprite resources for mapped trailer-coach views 1..11.
///
/// Indexed at mapped view minus one. Views 2, 3, 4 and 8 supply source sprites;
/// the other views retain empty batch-list pointers in their source slots.
/// Every view keeps its image background and has no clipping-area list.
/// Sources and batch lists remain borrowed from this overlay while it is loaded.
extern SpriteView gDryfieldTrailerCoachSpriteViews[DRYFIELD_TRAILER_COACH_VIEW_COUNT];

/// Collision policies indexed by trailer-coach surface class 0..7.
///
/// Every class blocks probes, enables weapon impacts and applies pushback.
/// Classes 0..4 and 7 share the default record; class 5 has a separate silent
/// record, and class 6 supplies paired footstep cues. Records and their sound
/// data belong to this overlay and must not be retained after it unloads.
extern WorldCollisionSurfaceProperties* gDryfieldTrailerCoachSurfaceClasses[DRYFIELD_TRAILER_COACH_SURFACE_CLASS_COUNT];

/// Draws the trailer coach's pulsing glows for the current camera view.
///
/// Gameplay's effect slot 0xD9 supplies a live `TASK_BODY_COORD` body in
/// `task->extra.coordBody`. Views 2 and 8 draw a cyan diamond with diagonal
/// rays; view 10 draws a cyan disc with four rays. Other views draw nothing.
/// The active view must be a valid trailer-coach view (1..11). Both drawers
/// transform the same local point by the body's coordinate and reject depths
/// below camera Z / 4 = 17. They advance the pulse by 96 angle units per
/// animation frame, with 4096 units per turn.
///
/// Requires the current view matrices, scratch stack, ordering table and packet
/// arena. Queued primitives belong to that frame until GPU drawing completes;
/// the task keeps no additional work allocation or state between frames.
void dryfieldTrailerCoachDrawGlowsTask(Task* task);

/// Updates the daytime trailer coach's telephone save and statistics menu.
///
/// Gameplay dispatches this wrapper while this room overlay is loaded.
/// Requires a live menu task in state 0..3, its writable UiObject in
/// `spawnArg2.pointer`, and loaded UI resources. The parent owns the menu;
/// the implementation opens child dialogs/panels and reports dismissal through
/// the object's result rather than killing this task.
void dryfieldTrailerCoachTelephoneMenuTask(Task* task);

/// Runs the trailer-coach room's entry, depth-scale update or teardown state.
///
/// The stage map spawns this receiver for area 27. `task->state` must be 0..2:
/// entry installs the room table and starts the arrival presentation; state 1
/// keeps the receiver alive and selects the current view's ordering-table
/// depth scale; state 2 releases the task. Keep this overlay loaded while the
/// task is active. No additional work or spawn payload is consumed here.
void dryfieldTrailerCoachRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TRAILER_COACH_H
