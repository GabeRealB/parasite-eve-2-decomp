#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Player and held-model reflection recipes for daytime motel room 6.
///
/// Slots 0 and 1 are bodyless tasks at priority `TASK_PRIORITY_EFFECT`, without a terminator.
/// Player spawnArg1.value = 1 selects the room mirror plane; attachment
/// spawnArg2.pointer borrows the player reflection task. Spawning copies
/// the descriptor; its callbacks and borrowed geometry must remain loaded.
extern TaskDesc gDryfieldMotelRoom6ReflectionTaskDescs[2];

/// Dryfield motel room 6 placement/resource directory indexed by `GameLocationKey.variant` 0..11.
///
/// Slots 1, 2 publish paired tables; all other slots are absent (NULL, NULL).
/// This placement selector is independent of the room and logical-view selectors.
/// Placements belong to the stage map; resource recipes belong to this room.
/// Keep both overlays and the actors supplying task descriptors loaded during
/// loading, spawning and model-flag updates. Lookups require an index within
/// this array; there is no terminator or bounds check.
extern AreaVariant gDryfieldMotelRoom6AreaVariants[12];

/// Collision resources for daytime motel room 6's single room.
///
/// Room 1 borrows the grid, ten view boundaries and fifteen action triggers;
/// there are no sight occluders. Keep this overlay loaded while the grid is
/// active or any trigger is linked. The room directory has no terminator.
extern WorldCollisionRoomResources gDryfieldMotelRoom6RoomCollisionResources[1];

/// Logical-view maps for daytime motel room 6's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..12 select the same
/// camera, image and sprite indices. The directory has no terminator and
/// remains valid only while this room overlay is loaded.
extern u8* gDryfieldMotelRoom6RoomViewMaps[1];

/// Logical-view reverse-search count for daytime motel room 6's single room.
///
/// Room 1 searches twelve entries in its identity map. This signed element
/// count is independent of that map's capacity; the array has no terminator
/// and remains valid only while this overlay is loaded.
extern ViewCount gDryfieldMotelRoom6RoomViewCounts[1];

/// Authored lights and per-view ambient minima for daytime motel room 6.
///
/// Room 1 borrows mutable point/cone lights and ambient entries for views
/// 1..12. The collection, transforms and ambient table belong to this overlay
/// and must remain loaded while queried or composed.
extern WorldCoordRoomLighting gDryfieldMotelRoom6RoomLighting[1];

/// Balcony and water-tower warp endpoints for daytime motel room 6.
///
/// Slots 1 and 2 select room views 2 and 5, respectively. Both place the
/// player and companion at the same room-coordinate point. The water-tower
/// departure fades; balcony arrival marks its map nibble. No terminator is
/// stored. Consumers borrow or copy these entries while the overlay is loaded.
extern DirectionWarpEntry gDryfieldMotelRoom6WarpEntries[2];

/// Camera transforms and perspective distances for daytime motel room 6's twelve views.
///
/// The identity view map selects entries at mapped index minus one. Rotation
/// coefficients use `ONE` per unit, translation is the negated world-space
/// camera origin, and perspective distance is in pixels. There is no terminator;
/// the map and room overlays must remain loaded during camera lookup.
extern ViewCamera gDryfieldMotelRoom6ViewCameras[12];

/// Borrowed sprite lists for daytime motel room 6's twelve mapped views.
///
/// Indexed by mapped view minus one, without a terminator. Views 1, 6, 9, 11
/// and 12 have no source sprites; their retained source-slot addresses identify
/// empty batch lists. View 10 selects all 28 of its sources. Every batch list
/// starts with a zero-count background selector and ends at SPRITE_BATCH_END;
/// no view supplies draw-area clips. Keep the overlay loaded while accessing
/// these mutable records or allocating/linking their cached packets.
extern SpriteView gDryfieldMotelRoom6SpriteViews[12];

/// Collision policies and footstep cues for daytime motel room 6's surface classes.
///
/// Indexed directly by class 0..7, without a terminator. All classes block
/// probes, allow weapon impacts and apply pushback. Classes 1..3 select
/// distinct cue records; classes 0 and 4..7 share a record with no footsteps.
/// The directory and borrowed records stay valid while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom6SurfacePropertiesByClass[8];

/// Updates daytime motel room 6's telephone save and statistics menu.
///
/// Gameplay dispatches this wrapper while this room overlay is loaded.
/// Requires a live menu task in state 0..3, its writable UiObject in
/// `spawnArg2.pointer`, and loaded UI resources. The parent owns the menu;
/// the implementation opens child dialogs/panels and reports dismissal through
/// the object's result rather than killing this task.
void dryfieldMotelRoom6TelephoneMenuTask(Task* task);

/// Initializes and updates the player's reflection in daytime motel room 6.
///
/// State must be 0 (start bodyless) or 1 (initialized model and work).
/// `spawnArg1.value` selects the floor (0) or room mirror plane (1).
/// Requires a live player with a TMD body. The task owns its cloned model and
/// work, borrows the player's geometry, and is parented to the player.
/// Keep this overlay loaded while the reflection and its attachment tasks live.
void dryfieldMotelRoom6PlayerReflectionTask(Task* reflectionTask);

/// Draws daytime motel room 6's pulsing cyan glow at its fixed world point.
///
/// Mapped view indices 3 and 4 select a wide diamond; 12 selects layered discs
/// and blades. Other mapped views draw nothing. The task argument is unused.
/// Requires the current view matrix, initialized scratch stack and frame packet
/// arena and ordering table; accepted projections must have nonzero camera Z / 4.
/// Queued packets belong to the frame arena until GPU completion. Keep this
/// room overlay loaded while the effect task is live.
void dryfieldMotelRoom6DrawGlowTask(Task* unusedTask);

/// Runs daytime motel room 6's receiver through initialization, idle and teardown.
///
/// Requires a live bodyless task with state 0 (register), 1 (idle) or 2 (kill).
/// Initialization registers `GAME_TASK_SLOT_ROOM` and permits post-CAP sound
/// messages. Keep the room overlay loaded for the task's lifetime.
void dryfieldMotelRoom6Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_6_H
