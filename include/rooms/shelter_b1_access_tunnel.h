#ifndef INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
#define INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Access-tunnel layouts indexed by the saved placement variant (0..21).
///
/// Variant 1 places five Watchers, 11 one Knight GOLEM and 17 one Bishop GOLEM.
/// Other slots are absent. Placement/resource lists end with
/// `AREA_PLACEMENT_END`; the descriptor table itself has no terminator.
/// Gameplay borrows these records and actors retain placements while this
/// room overlay is loaded.
extern AreaVariant gShelterB1AccessTunnelAreaVariants[22];

/// Number of mapped camera and sprite views in the access tunnel.
enum { SHELTER_B1_ACCESS_TUNNEL_VIEW_COUNT = 5 };

/// Logical-view map for the access tunnel's single room slot.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..5 select the same
/// 1-based camera, image and sprite indices. The shared mapping has its own
/// larger capacity; this directory remains valid while the room is loaded.
extern u8* gShelterB1AccessTunnelRoomViewMaps[1];

/// Reverse-search length for room 1's logical-view map, in byte entries.
///
/// The signed count covers logical views 1..5, independently of the identity
/// map's capacity. It has no terminator and borrows this overlay's lifetime.
extern ViewCount gShelterB1AccessTunnelRoomViewCounts[1];

/// Four warp endpoints joining the control room, freezer, parking and transfer tunnel.
///
/// Indexed by 1-based endpoint minus one, with no terminator. Each supplies
/// actor placements, arrival/departure sounds and the default logical view.
/// Positions are whole room-coordinate units and yaw uses 4096 units per turn.
/// Warp consumers borrow the array from this loaded room and copy entries.
extern DirectionWarpEntry gShelterB1AccessTunnelWarpEntries[4];

/// Access-tunnel collision mesh and its 3-by-3 XZ spatial index.
///
/// Starts at room coordinates (10, 670), with 4000-unit cells. Fifteen quads
/// index 24 vertices and six normals; each nonempty cell list ends with -1.
/// Activation binds the mutable view coordinate. The descriptor and private
/// pools stay borrowed and writable while this room overlay is loaded.
extern WorldCollisionGrid gShelterB1AccessTunnelCollisionGrid;

/// Access-tunnel cameras, indexed by the mapped 1-based view minus one.
///
/// Rotation uses `ONE` per unit, translation is the negated world-space camera
/// origin and perspective distance is pixels. Keep the room overlay loaded
/// until queued camera application has copied its selected record.
extern ViewCamera gShelterB1AccessTunnelViewCameras[SHELTER_B1_ACCESS_TUNNEL_VIEW_COUNT];

/// Background and depth-sorted foreground sprites for the tunnel's five views.
///
/// Indexed by mapped view minus one. Views 2/3/4 select 22/74/31 source
/// rectangles; views 1/5 have empty batch lists. All keep decoded background
/// strips and have no clipping list. The mutable lists and descriptors remain
/// borrowed from this overlay while allocating or drawing their packets.
extern SpriteView gShelterB1AccessTunnelSpriteViews[SHELTER_B1_ACCESS_TUNNEL_VIEW_COUNT];

/// Access-tunnel model lighting: four directional and five point lights.
///
/// Every source contributes in all views; there are no cone lights. Lighting
/// parents and composes the mutable coordinates and updates attenuation.
/// The descriptor and arrays must remain loaded while lighting uses them.
extern WorldCoordRoomLights gShelterB1AccessTunnelRoomLights;

/// Six directed view boundaries connecting logical views 5/2, 2/3 and 3/4.
///
/// The final record carries WORLD_COLLISION_TRIGGER_LAST. Activation binds
/// the view coordinate, links and enables the writable records; contacts
/// change the saved logical view only from the indicated source view.
/// Keep this room overlay loaded while the records are linked.
extern WorldCollisionTrigger gShelterB1AccessTunnelViewBoundaryTriggers[6];

/// Four facing-tested departure quads for the tunnel's warp endpoints.
///
/// Select control-room arrival 2, freezer arrival 1, parking arrival 1 and
/// transfer-tunnel arrival 2, respectively. The final record carries
/// WORLD_COLLISION_TRIGGER_LAST. Activation binds the view coordinate, links
/// and enables the mutable records; keep this room loaded while linked.
extern WorldCollisionTrigger gShelterB1AccessTunnelWarpTriggers[4];

/// Room-space quad blocking actor line of sight across the access tunnel.
///
/// The sole record carries WORLD_COLLISION_OCCLUDER_LAST. Activation links
/// and enables it; positions are game units and the normal uses 4096 per unit.
/// This mutable room-owned record stays loaded while linked into the sight list.
extern WorldCollisionOccluder gShelterB1AccessTunnelSightOccluders[1];

/// Collision policies and footstep cues indexed by tunnel surface class 0..7.
///
/// All classes apply pushback. Class 2 passes probes and suppresses weapon
/// impacts; other classes block probes and allow impacts. Classes 1/2 share
/// footstep cues; classes 0 and 3..7 have none. The directory, property records
/// and cues remain borrowed from the loaded access-tunnel overlay.
extern WorldCollisionSurfaceProperties* gShelterB1AccessTunnelSurfacePropertiesByClass[8];

/// Runs the access tunnel's room-message receiver.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Shelter map overlays loaded while its task and borrowed
/// message table remain available. The task allocates no work.
void shelterB1AccessTunnelRoomTask(Task* task);

/// Draws the access tunnel's light glows for mapped views 2 through 5 each frame.
///
/// Other views emit no packets. The task argument is unused; this callback does
/// not advance a state or end its task. Requires the room overlay, current view
/// matrices, scratch stack and frame packet arena to remain available during
/// drawing; queued primitives live in that arena until GPU completion.
void shelterB1AccessTunnelDrawGlowsTask(Task* unused);

#endif // INCLUDE_ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
