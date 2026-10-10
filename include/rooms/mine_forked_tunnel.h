#ifndef INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
#define INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Encounter layouts indexed directly by the tunnel's saved placement variant, in 0..11.
///
/// Populated slots are 1 empty, 2 four scavengers, 3 two scorpions and eight
/// moths, 4 two scorpions and three Mind Sucklers, and 11 two bats. Other slots
/// contain two NULL pointers. Each populated slot borrows terminated placement
/// and resource lists from this overlay; keep them loaded for loading and spawning.
extern AreaVariant gMineForkedTunnelAreaVariants[12];

extern TmdSource gMineForkedTunnelModel01B48;

/// Lighting resources for the tunnel's single room, indexed by room ID minus one.
///
/// Borrows four mutable point lights and a counted ambient-colour table for
/// logical views 1..7. The map directory and gameplay queries may borrow the
/// descriptor and its resources only while this room overlay remains loaded.
extern WorldCoordRoomLighting gMineForkedTunnelRoomLighting[1];

/// Collision resources for the tunnel's single room, indexed by room ID minus one.
///
/// Borrows the mutable room grid, six view boundaries and six facing action
/// quads, with no sight occluders. Gameplay binds and links these resources;
/// keep this overlay loaded until the grid is retired and triggers unlinked.
extern WorldCollisionRoomResources gMineForkedTunnelRoomCollisionResources[1];

/// Logical-view map directory for the tunnel's single room, indexed by room ID minus one.
///
/// Borrows gameplay's identity map: logical views 1..7 select the same 1-based
/// camera, image and sprite indices. The separate room view count limits the
/// valid domain; the shared map has a larger capacity and no terminator.
extern u8* gMineForkedTunnelRoomViewMaps[1];

/// Logical-view count for the tunnel's single room, indexed by room ID minus one.
///
/// The seven entries in the room's identity map select cameras/images 1..7.
/// There is no count header or terminator; the stage map borrows this array
/// only while the room overlay is loaded.
extern ViewCount gMineForkedTunnelRoomViewCounts[1];

/// The tunnel's single departure/arrival endpoint, selected by 1-based warp slot 1.
///
/// Both arriving actors face a half-turn at (2350, 0, 10500) world units and
/// the initial logical view is 3. The endpoint supplies arrival/departure
/// sounds, no blocked sound, and no optional spawn, fade or map-marker flags.
/// The stage map borrows it while this overlay is loaded; there is no terminator.
extern DirectionWarpEntry gMineForkedTunnelWarpEntries[1];

/// Cameras for the tunnel's seven mapped views, indexed by mapped view minus one.
///
/// Each record holds a world-to-camera rotation at 4096 per unit, negated
/// world-space origin and perspective distance in pixels. Logical views 1..7
/// map directly to these records. The stage map borrows the array while this
/// overlay is loaded; there is no count header or terminator.
extern ViewCamera gMineForkedTunnelViewCameras[7];

/// Sprite resources for the tunnel's seven mapped views, indexed by mapped view minus one.
///
/// Views 2..5 have source sprites; views 1, 6 and 7 retain only empty batch
/// lists. Every view requests decoded background strips and has no draw-area
/// clips. View 4 batch 5 and view 5 batch 3 show the oak board until collection.
/// Gameplay borrows these records and their mutable batches while this overlay
/// remains loaded. Empty source slots retain batch-list addresses, not sprites.
extern SpriteView gMineForkedTunnelSpriteViews[7];

/// Collision policy directory indexed by the tunnel's surface class, in 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Only
/// class 1 supplies footstep cue pairs; classes 0 and 2..7 share a silent record.
/// The map directory borrows these pointers while the room overlay is loaded.
/// There is no count header or terminator.
extern WorldCollisionSurfaceProperties* gMineForkedTunnelSurfacePropertiesByClass[8];

/// Draws the tunnel's flickering light flares for the current mapped view.
///
/// Enables view room effects on every call. Views 2 and 3 draw one shared
/// anchor, view 4 draws two anchors, and view 5 draws one; other views draw
/// none. Requires this room's data, composed view matrix, scratch stack and
/// current-frame GPU packet arena and ordering table. Flare half-extent is
/// `768 * 39 / depth` pixels, with nonzero depth equal to camera Z / 4.
/// Gameplay dispatches this callback from effect slot 0x126; the task is ignored.
void mineForkedTunnelDrawViewFlaresTask(Task* unusedTask);

/// Runs the switch-controlled area object's setup, path update or teardown state.
///
/// Requires this room overlay loaded, a live TMD task and `state` in 0..2.
/// State 0 initializes work and a child; state 1 updates motion and drawing;
/// state 2 releases the area object's enemy allocation and starts task teardown.
/// No state runs while scene actor control is paused. Teardown may release the task.
void mineForkedTunnelAreaObjectTask(Task* task);

/// Runs the room controller's setup, idle message-receiving or teardown state.
///
/// Requires this room overlay loaded and `state` in 0..2. State 0 installs
/// room handlers, registers the task in `GAME_TASK_SLOT_ROOM` and initializes
/// room sprite visibility; state 1 remains idle; state 2 starts task teardown.
/// This controller continues dispatching while scene actor control is paused.
void mineForkedTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_FORKED_TUNNEL_H
