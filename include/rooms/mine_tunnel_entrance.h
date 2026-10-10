#ifndef INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
#define INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_tunnel_entrance_8017F32C[12];

/// Room 1's authored lights and per-view minimum ambient colours.
///
/// The Shelter map borrows this one-entry room array while the tunnel overlay
/// is loaded. Light transforms and attenuation remain writable at runtime.
extern WorldCoordRoomLighting gMineTunnelEntranceRoomLighting[1];

extern WorldCollisionRoomResources D_mine_tunnel_entrance_8017DB60[];

/// Room 1's logical-view mapping, borrowing gameplay's identity map.
///
/// Logical views 1..6 select camera, image and sprite indices 1..6 unchanged.
/// This room directory lives in the tunnel overlay; the byte map lives in gameplay.
extern u8* gMineTunnelEntranceRoomViewMaps[1];

extern ViewCount D_mine_tunnel_entrance_8017DB74[];

extern DirectionWarpEntry D_mine_tunnel_entrance_8017DB78[];

/// Six cameras selected by mapped view index minus one.
///
/// Rotations use ONE (4096) for unity, translations are negated world-space
/// origins, and perspective distances are pixels. The Shelter map borrows
/// the array while this room overlay is loaded.
extern ViewCamera gMineTunnelEntranceViewCameras[6];

/// Six mapped views' depth-sorted sprites over decoded background strips.
///
/// Views 1 and 5 have no source sprites; views 2, 3, 4 and 6 select 13, 39,
/// 39 and 11 sources. Every batch list includes a zero-count first record
/// and an end marker. No view supplies clipping rectangles. The Shelter map
/// borrows this array and its mutable source/batch storage while the room is loaded.
extern SpriteView gMineTunnelEntranceSpriteViews[6];

/// Collision and footstep policies for surface classes 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback.
/// Class 2 supplies floor footsteps; the others are silent. Classes 0 and
/// 3..7 borrow one common policy, while class 1 retains a separate record.
/// The Shelter map borrows this directory and its records while the room is loaded.
extern WorldCollisionSurfaceProperties* gMineTunnelEntranceSurfacePropertiesByClass[8];

/// Draws the tunnel entrance's flickering light flares for the current view.
///
/// Bank-6 effect task 0x122 ignores its task argument and enables room view
/// effects on every frame. Mapped views 2..6 select world-space flare centres;
/// other views draw nothing. Each flare's pixel half-extent is its radius scale
/// (768, or 512 for view 6's second flare) times 39 divided by camera Z / 4.
/// Requires nonzero accepted projection depths, the loaded room overlay,
/// composed view matrices, scratch stack, ordering table and packet arena.
/// Queued packets live until the current frame has finished on the GPU.
void mineTunnelEntranceDrawFlaresTask(Task* unusedTask);

/// Runs the mine tunnel entrance's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 advances the arrival event,
/// 2 idles and 3 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void mineTunnelEntranceRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
