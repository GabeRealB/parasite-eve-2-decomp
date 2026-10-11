#ifndef INCLUDE_ROOMS_MIST_R18_H
#define INCLUDE_ROOMS_MIST_R18_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Placement/resource layouts for Acropolis area 18's MIST briefing.
///
/// Indexed by saved placement variant in 0..12, without a bounds check or
/// terminator. Slots 1 and 3 borrow the same Rupert/Baldwin placements from the
/// Acropolis map and actor recipes from this room; every other slot is absent.
/// Keep the room, map and selected actor packages loaded while these resources
/// are used for loading, spawning or model updates.
extern AreaVariant gMistR18AreaVariants[13];

/// Collision resources for the single briefing room, indexed by room minus one.
///
/// Borrows the mutable floor grid, with no view boundaries, action triggers or
/// occluders. Acropolis area 18 selects this one-entry directory. Keep the room
/// overlay loaded while collision setup and runtime lists reference its data.
extern WorldCollisionRoomResources gMistR18CollisionRoomResources[1];

/// Logical-view map directory for the single briefing room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..10 map to resources
/// 1..10. The Acropolis map borrows this directory while the room is loaded.
extern u8* gMistR18ViewMaps[1];

/// Reverse-search limit for room 1's ten logical views, with a retained zero halfword.
///
/// Only room 1 has a view map here. The second stored halfword's role is
/// unproven; it is not established as another active room or a terminator.
extern ViewCount gMistR18ViewCounts[2];

/// Room-1 model lighting, selecting five point lights and default ambient minima.
///
/// Indexed by room minus one. The Acropolis map borrows this entry and its
/// writable light collection while this room overlay remains loaded.
extern WorldCoordRoomLighting gMistR18RoomLightingTable[1];

/// Sole briefing-area warp endpoint, indexed by warp minus one (warp 1 only).
///
/// Player and companion arrive at world (-6000, 0, 3218), yaw 2048 in a
/// 4096-unit turn, in view 2. No sounds, spawn/departure flags or map marker
/// are requested. Acropolis area 18 borrows this entry while the room is loaded;
/// consumers copy it by value. The two retained zero byte spans have unproven roles.
extern DirectionWarpEntry gMistR18WarpEntries[1];

/// Cameras for mapped briefing views 1..10, indexed by mapped view minus one.
///
/// Rotation uses 4096 for unity; translation is the negated camera origin in
/// world units and projection distance is in pixels. Keep the room loaded until
/// a queued camera task has applied its borrowed record. Views 7..10 share framing.
extern ViewCamera gMistR18ViewCameras[10];

/// Foreground sprite resources for mapped views 1..10, indexed by view minus one.
///
/// Views 4 and 6 select ten and seventeen source sprites; the other slots have
/// none. All retain decoded background strips and have no draw-area overrides.
/// Descriptors and referenced mutable lists stay borrowed while the room is loaded.
extern SpriteView gMistR18SpriteViews[10];

/// Collision policies and footstep requests indexed by room-local surface class 0..7.
///
/// All eight entries borrow one common policy. The floor uses class 1 and
/// collision setup reads every entry. Keep the room overlay loaded while used.
extern WorldCollisionSurfaceProperties* gMistR18SurfacePropertiesByClass[8];

/// Runs the MIST briefing and its key-item menu before departure to Dryfield.
///
/// Requires a live room task with state 0 setup, 1 advance briefing scripts or
/// 2 teardown; no index check is made. Setup registers the task in the resident
/// room slot. State 1 waits for event/menu completion and repeats the key-item
/// prompt until the Dryfield map is opened. A separate script cursor stops
/// advancing once departure begins. The callback table is copied by value before
/// dispatch. The room and Acropolis map overlays must remain loaded while active.
void mistR18BriefingTask(Task* task);

#endif // INCLUDE_ROOMS_MIST_R18_H
