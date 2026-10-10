#ifndef INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
#define INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_saloon_g_r_80181B1C[13];

// dryfield_saloon_g_r
extern WorldCollisionRoomResources D_dryfield_saloon_g_r_8017EDA0[];

/// Logical-view maps for daytime saloon room selectors 1 and 2.
///
/// Indexed by room minus one; both entries borrow the same unsigned byte map.
/// Logical views 1..13 select camera, image and sprite indices 1..9 or 13;
/// views 9..12 all select resource index 9. No map terminator is used.
/// The saloon overlay must remain loaded while its directory or map is used.
extern u8* gDryfieldSaloonGRRoomViewMaps[2];

extern ViewCount D_dryfield_saloon_g_r_8017EDD8[];

/// Lighting for daytime saloon room selectors 1 and 2.
///
/// Indexed by room minus one; both entries share seven mutable point lights.
/// No directional or cone lights are supplied; NULL ambient tables select the
/// default view minima. Descriptors and light storage borrow the loaded overlay.
extern WorldCoordRoomLighting gDryfieldSaloonGRRoomLighting[2];

extern DirectionWarpEntry D_dryfield_saloon_g_r_8017EDEC[];

extern ViewCamera D_dryfield_saloon_g_r_8017F7A4[];

/// Thirteen mapped-view sprite descriptors for the daytime saloon.
///
/// Indexed by mapped resource view minus one, independently of room selector.
/// Slots 2..7 have source sprites; the other slots retain empty batch lists.
/// Every list keeps decoded background strips, and no slot has clipping areas.
/// Resource slots 10..12 are unselected by the current logical-view map.
/// Descriptors and mutable source/batch storage stay live with the room overlay.
extern SpriteView gDryfieldSaloonGRSpriteViews[13];

/// Collision response and footsteps for the saloon's eight surface classes.
///
/// Indexed directly by class 0..7 from grid faces or contacts, with no terminator.
/// Classes 0 and 4..7 share silent blocking properties; class 1 has its own
/// footsteps, and classes 2/3 share another cue set. Only class 3 passes probes
/// and suppresses weapon impacts; every class applies pushback.
/// The directory and its borrowed records stay live with the saloon overlay.
extern WorldCollisionSurfaceProperties* gDryfieldSaloonGRSurfacePropertiesByClass[8];

/// Queues the saloon's view-gated flares, twin light shafts and tapered beam each frame.
///
/// Requires a live `TASK_BODY_COORD` task with an already composed local-to-world
/// coordinate, the saloon overlay loaded, and its current 1-based view in 1..13.
/// Point entries 0..5 select texture column 0 and 6..10 select column 2; the
/// two shafts share roots 14/17 with guide pairs 15/18 and 16/19. The beam
/// runs from point 13 to point 12. Flare half-extents are `512 * 39 / depth`
/// pixels and beam radii are `256 * 64 / depth`, with depth in camera Z / 4.
/// Flares, each shaft's last tip and the beam's second end require depth >= 17;
/// the beam's first depth is clamped to 16.
///
/// Requires initialized view matrices, scratch stack, ordering table and a
/// writable frame packet arena. Packets remain live through GPU completion;
/// no input pointers are retained and the task's state is unchanged.
void dryfieldSaloonGRDrawLightEffectsTask(Task* task);

/// Runs the daytime saloon's room initialization, message wait and teardown.
///
/// State 0 installs the room handlers; state 1 waits for messages;
/// state 2 kills the task.
/// Requires a live task with state in 0..2 and the room overlay loaded.
/// The initialized task must stay live while its published room slot is used.
void dryfieldSaloonGRRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_SALOON_G_R_H
