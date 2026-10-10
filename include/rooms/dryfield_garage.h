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

/// Saved-area changes applied after Gary's initial garage scene finishes.
///
/// Five updates and an `AREA_APPLY_END` record. Marks Main Street, Motel Room 1
/// and Parking Lot; selects Balcony variant 2 in normal/replay or Scavenger and
/// variant 7 in Bounty or Nightmare, marking it in either case. Applying an
/// update also clears saved enemy poses. Borrowed while this overlay is loaded.
extern const AreaApplyRec gDryfieldGarageGarySceneAreaUpdates[6];

/// Logical and mapped view domain for the one-room garage: 1..15.
enum { DRYFIELD_GARAGE_VIEW_COUNT = 15 };

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

/// Room 1's collision grid, fourteen view boundaries and eleven action triggers.
///
/// No sight occluders. The map borrows this one-element room directory; its
/// grid stays live while active and its writable triggers stay live until unlinked.
extern WorldCollisionRoomResources gDryfieldGarageRoomCollisionResources[1];

/// One-room directory mapping the garage's logical views to resource indices.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..15 select the same
/// camera, image and sprite indices. The shared map has 50 bytes; this room uses
/// only its first fifteen. Keep the garage and gameplay overlays loaded while used.
extern u8* gDryfieldGarageRoomViewMaps[1];

/// Room 1's fifteen-entry reverse-search limit in `gViewIdentityMap`.
///
/// Indexed by room minus one, without a terminator. Borrowed by the stage map
/// while this overlay is loaded; the camera and sprite extents are stated separately.
extern ViewCount gDryfieldGarageRoomViewCounts[1];

/// Room 1's point-light collection and minimum ambient colours for fifteen views.
///
/// Light transforms and attenuation remain writable; ambient entries are read-only.
/// The map overlay borrows this array while the garage overlay is loaded.
extern WorldCoordRoomLighting gDryfieldGarageRoomLighting[1];

/// The garage's two warp endpoints: slot 1 factory, slot 2 junk yard.
///
/// Arrival places player and companion and selects view 2 or 6. The factory
/// endpoint fades on departure; both carry room-bank sound requests and map
/// markers. Indexed by warp minus one, without a terminator; keep this overlay
/// loaded while the map borrows the table or an endpoint is copied.
extern DirectionWarpEntry gDryfieldGarageWarpEntries[2];

/// The fifteen mapped garage cameras, indexed by mapped view minus one.
///
/// Rotation uses 4096 per unit; translation is the negated world-space origin
/// and projection distance is in pixels. Keep this overlay loaded until a
/// queued camera application has copied its record. There is no terminator.
extern ViewCamera gDryfieldGarageViewCameras[DRYFIELD_GARAGE_VIEW_COUNT];

/// Sprite resources for the fifteen mapped garage views, retaining background strips.
///
/// Indexed by mapped view minus one, without a terminator. Views 1 and 8..14
/// have empty lists; views 2..7 and 15 select source rectangles. Every clipping
/// list is NULL. Keep this overlay loaded during packet allocation and drawing;
/// source depths and batch visibility remain writable.
extern SpriteView gDryfieldGarageSpriteViews[DRYFIELD_GARAGE_VIEW_COUNT];

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
