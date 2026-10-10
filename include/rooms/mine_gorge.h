#ifndef INCLUDE_ROOMS_MINE_GORGE_H
#define INCLUDE_ROOMS_MINE_GORGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the gorge's room, mapped-view and encounter-variant directories.
enum {
    MINE_GORGE_ROOM_COUNT         = 2,
    MINE_GORGE_VIEW_COUNT         = 11,
    MINE_GORGE_AREA_VARIANT_COUNT = 12
};

/// Encounter layouts indexed directly by the saved area variant, 0..11.
///
/// Slots 1, 2 and 11 contain twelve bats each; slot 1 waits for the encounter
/// wave. Slot 3 contains six Mind Sucklers; slot 4 contains four Mind Sucklers
/// and eight moths. Slot 0 and slots 5..10 are absent, with both pointers NULL.
/// Each populated pair borrows terminated placement/resource tables from the
/// loaded gorge and must outlive loading and the actors that retain placements.
extern AreaVariant gMineGorgeAreaVariants[MINE_GORGE_AREA_VARIANT_COUNT];

/// Lighting for the gorge's two room states, indexed by room ID minus one.
///
/// Both states borrow the same six mutable point lights and ambient table.
/// Ambient entries use logical views 1..11, independently of camera remapping.
/// These descriptors and their resources remain valid while the gorge is loaded.
extern WorldCoordRoomLighting gMineGorgeRoomLighting[2];

extern WorldCollisionRoomResources D_mine_gorge_8017E7B8[];

/// Logical-view maps for the gorge before and after the oak board is laid.
///
/// Indexed by room ID minus one: room 1 borrows `gViewIdentityMap`; room 2
/// exchanges resource views 3..5 with 7..9. Logical views 1..11 select 1-based
/// camera, image and sprite indices. The directory and room-2 map belong to the
/// loaded gorge; the identity map belongs to gameplay. Neither has a terminator.
extern u8* gMineGorgeRoomViewMaps[2];

/// Logical reverse-search counts for the gorge's two room states.
///
/// Signed halfword element counts, indexed by room ID minus one; both search
/// eleven logical-view map bytes. There is no terminator. Counts and maps are
/// independent of the retained trailing map byte and remain live while loaded.
extern ViewCount gMineGorgeRoomViewCounts[MINE_GORGE_ROOM_COUNT];

extern DirectionWarpEntry D_mine_gorge_8017E7F0[];

/// Cameras indexed by the gorge's mapped resource view minus one, for views 1..11.
///
/// Rotation uses 4096 per unit; translation is the negated world camera origin,
/// and perspective distances are pixels. Pairs 3/7, 4/8 and 5/9 share camera
/// values; slots 10 and 11 also match. Queued view tasks borrow a record until application,
/// so the gorge must remain loaded. The array has no terminator.
extern ViewCamera gMineGorgeViewCameras[MINE_GORGE_VIEW_COUNT];

/// Sprite descriptors indexed by mapped gorge resource view minus one, for views 1..11.
///
/// Slots 1, 5, 10 and 11 have no source sprites and retain their batch-list
/// addresses in `sources.empty`; all other slots select source elements.
/// Every slot has a terminated batch list and no draw-area list. Descriptors
/// borrow gorge-owned mutable arrays that must remain loaded while drawing;
/// this directory itself has no terminator.
extern SpriteView gMineGorgeSpriteViews[MINE_GORGE_VIEW_COUNT];

/// Collision policies and footstep cues for the gorge's surface classes 0..7.
///
/// Classes 1 and 4 pass probes and ignore weapon impacts; the others block
/// probes and permit impacts. All apply pushback. Classes 2 and 4 share one
/// footstep set, class 3 uses another, and the remaining classes have no cues.
/// Classes 0 and 5..7 share one record. Pointers borrow gorge-owned records
/// for the loaded overlay's lifetime; neither level has a terminator.
extern WorldCollisionSurfaceProperties* gMineGorgeSurfacePropertiesByClass[8];

/// Updates the gorge's ambient-effect gate and draws the current view's fixed flares.
///
/// The loaded gorge must supply a mapped view index in 1..11. Views 2 and 6
/// enable ambient effects; view 1 draws no flares. Requires the current view
/// matrices, initialized scratch stack, ordering table and space for up to five
/// textured quads in the frame's primitive arena. The task argument is unused;
/// this per-frame callback neither advances nor releases the task.
void mineGorgeDrawViewFlaresTask(Task* unusedTask);

/// Runs the gorge's room-message receiver and one-time refuge-story setup.
///
/// Requires the loaded gorge and a live task in state 0 (initialize), 1 (idle)
/// or 2 (release). Borrows the room message table while registered; the body
/// and spawn arguments are unused. Teardown leaves the resident room slot set,
/// so message senders must stop using it before the task is killed.
void mineGorgeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_GORGE_H
