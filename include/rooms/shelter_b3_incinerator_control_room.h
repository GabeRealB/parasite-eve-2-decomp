#ifndef INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the control room's resource directories and terminated collision arrays.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT          = 2,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT          = 8,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SURFACE_CLASS_COUNT = 8,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_AREA_VARIANT_COUNT  = 11,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_WARP_COUNT          = 4,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_BOUNDARY_COUNT = 4,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_1_ACTION_COUNT = 5,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_2_ACTION_COUNT = 6,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_OCCLUDER_COUNT      = 1,
};

/// Placement layouts indexed by the saved area's variant byte, in 0..10.
///
/// Only variant 1 publishes tables: an empty placement list and the base
/// actor-142600 scene resource. Slots 0 and 2..10 have neither table; there is
/// no terminator. This directory and its borrowed tables require the room loaded.
extern AreaVariant gShelterB3IncineratorControlRoomAreaVariants[SHELTER_B3_INCINERATOR_CONTROL_ROOM_AREA_VARIANT_COUNT];

/// Logical-view maps for the control room's two logical rooms.
///
/// Indexed by room - 1; both entries borrow `gViewIdentityMap` from gameplay.
/// Logical views 1..8 select the same 1-based camera/image/sprite indices.
/// The directory requires the control-room overlay to remain loaded.
extern u8* gShelterB3IncineratorControlRoomViewMaps[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT];

/// Reverse-lookup limits for logical views in the two control-room logical rooms.
///
/// Indexed by room - 1; each signed halfword counts eight logical map entries.
/// These limits require live room-overlay storage and do not size the shared map.
extern ViewCount gShelterB3IncineratorControlRoomViewCounts[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT];

/// Four arrival/departure endpoints indexed by the 1-based warp ID minus one.
///
/// Slots 1 and 4 share the incinerator-side placement, with initial views 5 and
/// 6; warp 4 also starts the arrival scene. Slot 2 serves the lower-sewer stair
/// passage in view 3, and slot 3 the elevator-hall door in view 4. Positions use
/// world units and yaw uses 4096 per turn. Entries are copied whole and have no
/// terminator; keep the overlay loaded. Arrivals set a nonzero map marker to 1 (visited).
extern DirectionWarpEntry gShelterB3IncineratorControlRoomWarpEntries[SHELTER_B3_INCINERATOR_CONTROL_ROOM_WARP_COUNT];

/// Collision mesh and XZ cell index shared by both logical control-room rooms.
///
/// The 4-by-2 grid has 4000-world-unit cells and origin X/Z (-6742, -3500).
/// Its private pools contain 27 vertices, ten normals and 19 quadrilateral
/// faces with surface classes 0, 1 and 3. The X-major directory borrows signed
/// face-index lists ending at -1; NULL cells are empty. Activation binds the
/// view coordinate. Keep the record and all writable pools loaded while active.
extern WorldCollisionGrid gShelterB3IncineratorControlRoomCollisionGrid;

/// World-to-camera transforms and pixel projection distances for mapped views 1..8.
///
/// Indexed by mapped view - 1 for either logical room. Rotations use ONE (4096)
/// for 1.0; translations are negated world-coordinate camera origins. A queued
/// camera application borrows the record until it runs, so keep the overlay loaded.
extern ViewCamera gShelterB3IncineratorControlRoomViewCameras[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT];

/// Sprite sources and batch lists for mapped control-room views 1..8.
///
/// Indexed by mapped view - 1 for either logical room. Views 3, 4 and 5 contain
/// source sprites; the other views retain empty batch-list addresses. Every view
/// selects decoded-image background strips and has no draw-area clipping list.
/// Descriptors and mutable backing arrays live in the loaded room overlay.
extern SpriteView gShelterB3IncineratorControlRoomSpriteViews[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT];

/// Eight point lights shared by both control-room logical rooms in every view.
///
/// No directional or cone lights are present. The loaded room owns the mutable
/// transforms and attenuation state; positions and falloff radii use world units,
/// and RGB intensities use 12 fractional bits. Room lighting uses default ambient
/// minima because the map supplies no ambient table.
extern WorldCoordRoomLights gShelterB3IncineratorControlRoomLights;

/// Directed view boundaries shared by both logical rooms: 4 to 5, 5 to 4, 3 to 4 and 4 to 3.
///
/// The fourth record carries LAST and is included in setup. Room binding supplies
/// the view coordinate, links and enables the writable records. Positions and
/// radii use world units; normals use 4096 per unit. Keep storage live until unlinked.
extern WorldCollisionTrigger gShelterB3IncineratorControlRoomViewBoundaryTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_BOUNDARY_COUNT];

/// Logical-room-1 interactions: stair descent, sewer and hall exits, incinerator dialogue and telephone use.
///
/// Six stair steps use surface class 2 and yaw 1024. The automatic lower-sewer
/// exit uses current endpoint 2 and destination endpoint 1; the hall exit uses
/// endpoints 3 and 1. Telephone use sends room command 1. The fifth record carries
/// LAST and is included. Room binding mutates links, flags and hit latches;
/// keep the room loaded until unlinked. Positions/radii use world units,
/// normals use 4096 per unit. The room selector is independent of layout variant.
extern WorldCollisionTrigger gShelterB3IncineratorControlRoomRoom1ActionTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_1_ACTION_COUNT];

/// Logical-room-2 interactions, replacing stair descent with CAP command 4 and adding a telephone region.
///
/// The sewer and hall exits retain endpoints 2/1 and 3/1. Two regions send
/// telephone command 1; the last accepts nearby overlap or facing overlap and
/// carries LAST. Incinerator-side interaction runs CAP command 5. All six
/// records are bound, linked and enabled in place; keep storage live until
/// unlinked. Positions/radii use world units, normals use 4096 per unit.
extern WorldCollisionTrigger gShelterB3IncineratorControlRoomRoom2ActionTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_2_ACTION_COUNT];

/// One line-of-sight blocking quad shared by both logical rooms.
///
/// LAST marks this included record. Setup links and enables it in place;
/// keep the overlay loaded until unlinked. Origin/corners use world units
/// and the plane normal uses 4096 per unit. The sight test does not use its radius.
extern WorldCollisionOccluder gShelterB3IncineratorControlRoomOccluders[SHELTER_B3_INCINERATOR_CONTROL_ROOM_OCCLUDER_COUNT];

/// Collision policies and footstep cues indexed by surface class 0..7.
///
/// Every class blocks probes and applies pushback; class 3 suppresses weapon
/// impacts. Classes 1 and 3 share footstep cues, class 2 uses its own cues, and
/// classes 0 and 4..7 have none. All records require the room overlay to stay loaded.
extern WorldCollisionSurfaceProperties* gShelterB3IncineratorControlRoomSurfacePropertiesByClass[SHELTER_B3_INCINERATOR_CONTROL_ROOM_SURFACE_CLASS_COUNT];

/// Runs control-room entry setup, message-service idle and teardown.
///
/// The map's room descriptor starts in state 0; valid states are 0 setup,
/// 1 idle and 2 release the task. Dispatch copies the three-entry table by
/// value and does not check the state. Setup installs the room message slot;
/// arrival warp 4 also starts the incinerator follow-up scene. The room overlay
/// and scene resources must remain loaded while their tasks use them.
void shelterB3IncineratorControlRoomTask(Task* task);

/// Runs the incinerator control room's telephone menu for `telephoneDispatchMenuTask`.
///
/// `spawnArg2.pointer` borrows a live UiObject; start its task in state 0.
/// Updates save/statistics child panels and reports completion through that
/// object for the inventory menu to close. States 0..3 and the room's menu
/// resources must remain available until the enclosing menu finishes.
void shelterB3IncineratorControlRoomTelephoneMenuTask(Task* task);

/// Draws the incinerator control room's fixed light glows for the mapped camera view.
///
/// Mapped views 2 through 6 and 8 select capsule glows; views 4 and 8
/// also draw a pulsing cyan diamond or disc. Other views emit no packets.
/// `task` is unused. Requires a composed view matrix, an initialized scratch
/// stack and the current frame's ordering table and packet arena. Queued
/// additive packets remain in that arena until GPU completion.
void shelterB3IncineratorControlRoomDrawViewGlowsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
