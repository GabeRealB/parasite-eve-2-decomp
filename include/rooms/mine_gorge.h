#ifndef INCLUDE_ROOMS_MINE_GORGE_H
#define INCLUDE_ROOMS_MINE_GORGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_gorge_80183544[12];

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

extern ViewCount D_mine_gorge_8017E7EC[];

extern DirectionWarpEntry D_mine_gorge_8017E7F0[];

extern ViewCamera D_mine_gorge_8017FA14[];

extern SpriteView D_mine_gorge_801827F8[];

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
