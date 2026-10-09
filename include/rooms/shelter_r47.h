#ifndef INCLUDE_ROOMS_SHELTER_R47_H
#define INCLUDE_ROOMS_SHELTER_R47_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_r47_80187618;

/// Room 47's writable collision mesh and 5-by-3 XZ cell index.
///
/// Cells are 4000 game-coordinate units wide, with X/Z biases 1003/-1010.
/// The first two normals/faces and six vertices are reserved for Pierce's
/// collision replacement. Keep this overlay loaded while the grid is active.
extern WorldCollisionGrid gShelterR47CollisionGrid;

/// Room-space normals, including two entries replaced by Pierce's actor overlay.
extern SVECTOR gShelterR47CollisionGridNormals[13];

/// Room-space vertices, including six entries replaced by Pierce's actor overlay.
extern SVECTOR gShelterR47CollisionGridVerts[66];

/// Indexed collision faces, including two entries replaced by Pierce's actor overlay.
extern WorldCollisionGridFace gShelterR47CollisionGridFaces[30];

extern AreaApplyRec D_shelter_r47_8018A638[21];

extern AreaVariant D_shelter_r47_80187CB8[12];

/// The sole room's logical-view map, borrowing gameplay's identity mapping.
///
/// Room index 1 selects this pointer; logical views 1..46 use the same camera,
/// image and sprite indices. Both this overlay and gameplay must stay loaded.
extern u8* gShelterR47ViewMaps[1];

extern ViewCount D_shelter_r47_80187678[];

extern DirectionWarpEntry D_shelter_r47_8018767C[];

extern WorldCollisionTrigger D_shelter_r47_801876B4[];

extern ViewCamera D_shelter_r47_801882B0[];

extern SpriteView D_shelter_r47_80189C68[];

/// Room 47's nine point lights and two cone lights, with no directional lights.
///
/// All lights contribute in every view. Gameplay updates their transforms and
/// attenuation in place; their storage remains owned by the loaded room overlay.
extern WorldCoordRoomLights gShelterR47RoomLights;

/// Collision and footstep records indexed by room-local surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Classes
/// 1..3 have paired walk/run/scripted-jump cues; 0 and 4..7 are silent.
/// The pointed-to records are borrowed while this room overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterR47SurfaceProperties[8];

extern WorldCollisionTrigger D_shelter_r47_8018787C[13];

/// Dispatches this room's setup, per-frame event handler and teardown states.
///
/// Room-bank callback for Shelter area 47. `task->state` must be 0..2;
/// enter at 0 with this overlay loaded. Setup installs the room's message table
/// and starts ambience; state 1 advances the scripted terminal tour.
void shelterR47RoomTask(Task* task);

/// Updates this room's telephone save and statistics menu.
///
/// The shared telephone controller owns the UI object in `spawnArg2.pointer`;
/// the task must be initialized by the item menu's telephone flow. This room
/// overlay, its telephone panels and their loaded resources must remain live
/// until dismissal. The wrapper preserves the shared controller's task states
/// and result handling.
void shelterR47TelephoneMenuTask(Task* task);

/// Draws the room's additive light glows for the current mapped camera view.
///
/// Effect-bank callback 0x14B. Mapped views 5, 13, 14 and 44 select fixed
/// world-point subsets; other views emit no packets. The cyan glow at the
/// first point is a pulsing diamond except in view 44, where it is a layered
/// pulsing disc. The other points use flickering grey or cool-grey discs.
/// `unusedTask` is ignored; no task state, body or work is read or changed.
/// Requires this room overlay, its active view transform, initialized scratch
/// storage and the current frame's ordering table and packet arena with enough
/// space. Accepted points must project to nonzero depth. Queued packets remain
/// in that arena until the frame's GPU work finishes.
void shelterR47DrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_R47_H
