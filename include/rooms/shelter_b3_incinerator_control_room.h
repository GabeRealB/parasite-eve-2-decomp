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

extern AreaVariant D_shelter_b3_incinerator_control_room_80182610[11];

/// Extents of the control room's logical-room, mapped-view and surface directories.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT          = 2,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT          = 8,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SURFACE_CLASS_COUNT = 8,
};

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

extern DirectionWarpEntry D_shelter_b3_incinerator_control_room_8018192C[];

extern WorldCollisionGrid D_shelter_b3_incinerator_control_room_80181CC0;

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

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801824B8[];

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_80182668[];

extern WorldCollisionTrigger D_shelter_b3_incinerator_control_room_801827E4[];

extern WorldCollisionOccluder D_shelter_b3_incinerator_control_room_801829AC[];

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
