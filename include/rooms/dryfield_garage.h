#ifndef INCLUDE_ROOMS_DRYFIELD_GARAGE_H
#define INCLUDE_ROOMS_DRYFIELD_GARAGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaApplyRec D_dryfield_garage_80180204[6];

/// Garage grid normals in room axes, with 4096 representing unit length.
///
/// Gary's scene actor replaces the first three entries. Keep the garage overlay
/// loaded while editing these normals or querying the active grid.
extern SVECTOR gDryfieldGarageCollisionGridNormals[39];

/// Garage grid vertices in signed room-coordinate units.
///
/// Gary's scene actor replaces the first twelve entries. The active grid borrows
/// the entire pool for the lifetime of the loaded garage overlay.
extern SVECTOR gDryfieldGarageCollisionGridVerts[106];

/// Garage collision faces indexing its vertex and normal pools.
///
/// Gary's scene actor replaces the first three faces without changing the cell
/// lists. Indices must fit the pools; surface classes select this room's policies.
extern WorldCollisionGridFace gDryfieldGarageCollisionGridFaces[54];

extern AreaVariant D_dryfield_garage_801800E0[13];

// dryfield_garage
extern WorldCollisionRoomResources D_dryfield_garage_8017DCDC[];

/// One-room directory mapping the garage's logical views to resource indices.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..15 select the same
/// camera, image and sprite indices. The shared map has 50 bytes; this room uses
/// only its first fifteen. Keep the garage and gameplay overlays loaded while used.
extern u8* gDryfieldGarageRoomViewMaps[1];

extern ViewCount D_dryfield_garage_8017DCF0[];

/// Room 1's point-light collection and minimum ambient colours for fifteen views.
///
/// Light transforms and attenuation remain writable; ambient entries are read-only.
/// The map overlay borrows this array while the garage overlay is loaded.
extern WorldCoordRoomLighting gDryfieldGarageRoomLighting[1];

extern DirectionWarpEntry D_dryfield_garage_8017DCFC[];

extern ViewCamera D_dryfield_garage_8017E670[];

extern SpriteView D_dryfield_garage_8017F5E8[];

/// Garage collision policies indexed by surface class 0..7, with no terminator.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// supplies footstep cues; classes 0 and 2..7 share a policy without those cues.
/// Keep the garage overlay loaded while the directory and its records are borrowed.
extern WorldCollisionSurfaceProperties* gDryfieldGarageSurfacePropertiesByClass[8];

/// Inert room-effect callback for bank 6, slot 0xD6, selected for daytime Garage.
///
/// Ignores `unusedTask` without drawing, advancing state or releasing resources.
/// Keep the Dryfield Garage overlay loaded while this callback is scheduled.
void dryfieldGarageEffectNoopTaskD6(Task* unusedTask);

/// Runs the garage's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, applies companion placement for warp 2,
/// advances follow-up dialogue and restricts the first action trigger to variant 1.
/// Keep the garage overlay and active layout live for the task's lifetime.
void dryfieldGarageRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GARAGE_H
