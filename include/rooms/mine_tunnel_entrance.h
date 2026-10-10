#ifndef INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H
#define INCLUDE_ROOMS_MINE_TUNNEL_ENTRANCE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Mine/Shelter area 3's encounter layouts, indexed by placement variant 0..11.
///
/// Variants 1, 2, 3, 4 and 11 contain paired placement/resource tables; slots
/// 0 and 5..10 are absent. Variant 1 is the default. The tables end at
/// `AREA_PLACEMENT_END`; their indices and entry IDs have different roles.
/// Keep this room overlay and each recipe's actor overlay loaded while loading,
/// spawning or using actors that borrow placements or task descriptors.
extern AreaVariant gMineTunnelEntranceAreaVariants[12];

/// Room 1's authored lights and per-view minimum ambient colours.
///
/// The Shelter map borrows this one-entry room array while the tunnel overlay
/// is loaded. Light transforms and attenuation remain writable at runtime.
extern WorldCoordRoomLighting gMineTunnelEntranceRoomLighting[1];

/// Room 1's collision grid, eight view boundaries and four action quads.
///
/// The Shelter map borrows this one-room directory at room ID minus one.
/// There are no sight occluders. Each trigger list includes its LAST-marked
/// final record; the grid and triggers stay writable and loaded while active,
/// and trigger storage must remain live until unlinked.
extern WorldCollisionRoomResources gMineTunnelEntranceRoomCollisionResources[1];

/// Room 1's logical-view mapping, borrowing gameplay's identity map.
///
/// Logical views 1..6 select camera, image and sprite indices 1..6 unchanged.
/// This room directory lives in the tunnel overlay; the byte map lives in gameplay.
extern u8* gMineTunnelEntranceRoomViewMaps[1];

/// Room 1's six-entry logical-view reverse-search limit.
///
/// The Shelter map borrows this signed-halfword count at room ID minus one
/// while the room overlay is loaded. It covers logical views 1..6 of the
/// borrowed identity map, independently of that map's 50-entry capacity.
extern ViewCount gMineTunnelEntranceRoomViewCounts[1];

/// Three player/companion warp endpoints, indexed by one-based warp ID minus one.
///
/// Endpoints 1, 2 and 3 serve the Mesa, Forked Tunnel and Tunnel connections
/// and select arrival views 2, 6 and 4. Positions use world units and yaw uses
/// 4096 units per turn. Each has arrival/departure sounds, no blocked sound,
/// no spawn/fade flags and no map marker. The Shelter map borrows the complete
/// records while this overlay is loaded; there is no terminator.
extern DirectionWarpEntry gMineTunnelEntranceWarpEntries[3];

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
