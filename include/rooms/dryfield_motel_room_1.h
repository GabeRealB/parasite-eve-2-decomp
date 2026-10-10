#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_room_1_801814DC[13];

// dryfield_motel_room_1
extern WorldCollisionRoomResources D_dryfield_motel_room_1_8017E484[];

/// Motel room 1's two room entries sharing one logical-to-resource view map.
///
/// Index by room minus one (rooms 1 and 2). Logical views 1..9 select
/// camera, image and sprite indices 1, 9, 8, 4, 5, 6, 7, 3 and 2.
/// The directory and writable byte map are borrowed from this loaded overlay.
extern u8* gDryfieldMotelRoom1RoomViewMaps[2];

extern ViewCount D_dryfield_motel_room_1_8017E4B8[];

/// Motel room 1's two room lighting entries with the same five point lights.
///
/// Index by room minus one (rooms 1 and 2). Both entries use default ambient
/// minima and have no directional or cone lights. The point lights contribute
/// in every logical view; their transforms and attenuation remain writable.
/// Keep this overlay loaded while borrowing the records and their light array.
extern WorldCoordRoomLighting gDryfieldMotelRoom1RoomLighting[2];

extern DirectionWarpEntry D_dryfield_motel_room_1_8017E4CC[];

extern ViewCamera D_dryfield_motel_room_1_8017EAE0[];

extern SpriteView D_dryfield_motel_room_1_80180C90[];

/// Motel room 1's collision and footstep policies for surface classes 0..7.
///
/// Classes 0 and 4..7 share silent, blocking, impact-enabled properties.
/// Classes 1 and 2 also supply distinct footstep cue pairs; class 3 passes
/// probes and ignores weapon impacts without cues. Every class applies pushback.
/// The mesh uses classes 0..3; setup reads all eight entries. Borrow the
/// directory, properties and cues only while this overlay remains loaded.
extern WorldCollisionSurfaceProperties* gDryfieldMotelRoom1SurfacePropertiesByClass[8];

/// Inert callback for motel room 1's room-effect slot 0xC9 in task bank 6.
///
/// Ignores `unusedTask`, draws nothing and leaves the task live without
/// advancing its state. The task's resources remain owned until external teardown.
void dryfieldMotelRoom1NoOpEffectTask(Task* unusedTask);

/// Runs motel room 1's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`; variant 3 broadcasts command 0 to
/// placed actors until the room event has been seen. The scene manager must
/// be live on that path. Keep the room overlay loaded for the task's lifetime.
void dryfieldMotelRoom1Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_1_H
