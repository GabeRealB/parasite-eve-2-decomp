#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Night motel room 1's placement variants, indexed by GameLocationKey.variant.
///
/// Slots 1, 2 and 21 pair the night map's placements with this room's recipes;
/// slots 0 and 3..20 are absent (both pointers NULL). The unchecked index must
/// be 0..21. Gameplay's stage-3 area-11 record borrows this directory; keep the
/// room and map overlays loaded while their tables or placed actors are used.
extern AreaVariant gDryfieldNightMotelRoom1AreaVariants[22];

/// Two room-indexed lighting records sharing night motel room 1's authored lights.
///
/// Indexed by the 1-based room minus one (rooms 1..2). Both borrow the same
/// mutable collection and use the default minimum ambient levels. The Dryfield
/// night map borrows these records only while this room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom1RoomLighting[2];

/// Two room-indexed descriptors sharing night motel room 1's collision resources.
///
/// Rooms 1..2 select room - 1. Both borrow the same grid, eight view boundaries,
/// seven action triggers and one sight occluder. Setup mutates and links the
/// resources; keep the room overlay loaded until its active lists are cleared.
extern WorldCollisionRoomResources gDryfieldNightMotelRoom1RoomCollisionResources[2];

/// Night motel room 1's single-room directory of logical-view byte maps.
///
/// Room 1 selects entry zero; logical views 1..9 select map bytes at view - 1.
/// Each byte is a 1-based camera/image/sprite index (1..9), not a count or
/// terminator. The map overlay borrows this writable directory and its map;
/// neither pointer may outlive the loaded room overlay.
extern u8* gDryfieldNightMotelRoom1ViewMaps[1];

/// Room 1's logical-view search count: nine entries in its view map.
///
/// The night map borrows this signed-halfword array at room - 1 (room 1 only).
/// This count excludes the map's three trailing zeros and is independent of
/// mapped camera indices. Keep this room overlay loaded during lookups.
extern ViewCount gDryfieldNightMotelRoom1ViewCounts[1];

/// Motel-door warp endpoint, selected by 1-based warp ID 1.
///
/// Player and companion arrive at the same world position with yaw 3072
/// (4096 units per turn), logical view 2, door sounds and map marker 487.
/// The night map borrows this one-entry array; direction handling copies each
/// complete endpoint and requires the room overlay loaded during lookup.
extern DirectionWarpEntry gDryfieldNightMotelRoom1Warps[1];

/// Nine cameras for nighttime motel room 1.
///
/// Logical views 1..9 map to cameras {1, 9, 8, 4, 5, 6, 7, 3, 2}, stored
/// at mapped index minus one. Rotation uses ONE per unit, translation is the
/// negated world-space origin and perspective distance is pixels. The camera
/// directory borrows this mutable array while the room overlay is loaded.
extern ViewCamera gDryfieldNightMotelRoom1ViewCameras[9];

/// Dryfield night motel room 1's sprite resources, indexed by mapped view minus one.
///
/// Mapped views 1..3 and 7 have no sources. The logical-view map
/// reorders the slots; the other slots borrow sprite pools.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldNightMotelRoom1SpriteViews[9];

/// Collision and footstep policies indexed by night motel room 1's surface class.
///
/// Classes 0 and 4..7 share the default blocking, impact-enabled, silent policy;
/// classes 1 and 2 add distinct footstep cue pairs. Class 3 passes probes and
/// ignores weapon impacts. Every class applies pushback. The map and gameplay
/// borrow this table and its mutable records while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom1SurfacePropertiesByClass[8];

/// Draws night motel room 1's flickering light flares for the mapped camera.
///
/// Camera indices 2, 3, 8 and 9 select the flare with the larger radius scale;
/// 5 and 6 select the smaller one. Other indices draw nothing. Gameplay dispatches
/// this callback through effect-bank slot 0x107; `task` is unused.
/// The room overlay and view map must remain loaded, with the current view
/// matrices, scratch stack, packet arena and depth ordering table ready for
/// drawing. A selected flare reserves one packet even when depth-clipped.
void dryfieldNightMotelRoom1DrawGlowsTask(Task* task);

/// Runs night motel room 1's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightMotelRoom1RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H
