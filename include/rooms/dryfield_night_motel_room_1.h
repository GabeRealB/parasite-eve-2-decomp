#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_1_8018075C[22];

/// Two room-indexed lighting records sharing night motel room 1's authored lights.
///
/// Indexed by the 1-based room minus one (rooms 1..2). Both borrow the same
/// mutable collection and use the default minimum ambient levels. The Dryfield
/// night map borrows these records only while this room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom1RoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_1_8017DA74[];

/// Night motel room 1's single-room directory of logical-view byte maps.
///
/// Room 1 selects entry zero; logical views 1..9 select map bytes at view - 1.
/// Each byte is a 1-based camera/image/sprite index (1..9), not a count or
/// terminator. The map overlay borrows this writable directory and its map;
/// neither pointer may outlive the loaded room overlay.
extern u8* gDryfieldNightMotelRoom1ViewMaps[1];

extern ViewCount D_dryfield_night_motel_room_1_8017DAA4[];

extern DirectionWarpEntry D_dryfield_night_motel_room_1_8017DAA8[];

extern ViewCamera D_dryfield_night_motel_room_1_8017E0BC[];

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
