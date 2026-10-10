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

/// Opening modes passed in spawn argument 1 of the map-terminal descriptor.
enum {
    SHELTER_R47_MAP_MODE_USE   = 0, // Player use; cancel restores the entry view
    SHELTER_R47_MAP_MODE_TIMED = 1, // Unattended viewing; fades in, holds, then fades out
    SHELTER_R47_MAP_MODE_TOUR  = 2, // Guided event; only previous/next hotspots are installed
};

/// Bodyless, priority-192 spawn recipe for the room's map-terminal display.
///
/// Spawn descriptor index 0 in state 0, with a `SHELTER_R47_MAP_MODE_*` value
/// in argument 1 and zero in argument 2. Setup allocates terminal work and a
/// separate port-0 cursor task. Keep this overlay and its textures loaded until
/// teardown. Completion stops the terminal with result zero; the caller must
/// arrange exit dispatch to release its work. Timed mode leaves player-control
/// and event-state restoration to the event script.
extern TaskDesc gShelterR47MapTerminalTaskDesc;

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

/// Saved Shelter layouts and map marks applied after this room's entry scenes.
///
/// Twenty updates followed by `AREA_APPLY_END`; save-mode policies select
/// alternate layouts for normal/replay/Scavenger and Bounty/Nightmare saves.
/// Applying the list also discards affected saved enemy poses. Both scene actor
/// overlays borrow the list synchronously while this room overlay is loaded.
extern AreaApplyRec gShelterR47SavedAreaUpdates[21];

/// Placement layouts selected by the saved area-47 variant.
///
/// Slot 1 loads and places Pierce for the terminal tour; slot 2 loads the
/// companion-entry scene resource without spawning a placed actor. Slots 0 and
/// 3..11 are absent. Gameplay borrows these tables while this overlay is loaded.
extern AreaVariant gShelterR47AreaVariants[12];

/// The sole room's logical-view map, borrowing gameplay's identity mapping.
///
/// Room index 1 selects this pointer; logical views 1..46 use the same camera,
/// image and sprite indices. Both this overlay and gameplay must stay loaded.
extern u8* gShelterR47ViewMaps[1];

/// The sole room's 46-entry limit for reverse logical-view searches.
///
/// Room ID 1 selects element 0. This is a signed element count, independent
/// of the shared view-map capacity; the map overlay borrows it while loaded.
extern ViewCount gShelterR47ViewCounts[1];

/// The room's sole warp endpoint, selected by 1-based warp ID 1.
///
/// Arrivals place both actors in world coordinates and select view 2, with
/// optional arrival/departure sounds and the B2 laboratory map mark. No
/// terminator is stored. The map overlay borrows it while this room is loaded.
extern DirectionWarpEntry gShelterR47WarpEntries[1];

/// Six directed camera boundaries connecting room views 2, 3, 4 and 5.
///
/// The final record carries `WORLD_COLLISION_TRIGGER_LAST`. Gameplay binds and
/// links these mutable world-coordinate quads while this overlay is loaded.
extern WorldCollisionTrigger gShelterR47ViewBoundaryTriggers[6];

/// Cameras for mapped room-view indices 1..46, stored at index minus one.
///
/// Rotation uses 4096 for unity, translation is the negated world-space camera
/// origin, and screen distance is in pixels. There is no terminator; queued
/// camera application requires this overlay to stay loaded until the copy.
extern ViewCamera gShelterR47ViewCameras[46];

/// Sprite and clipping lists for mapped room-view indices 1..46.
///
/// Index at mapped view minus one. Source-free views retain the batch address
/// in `sources.empty`; those addresses supply no source elements. Each batch
/// list ends at `SPRITE_BATCH_END`, and NULL draw areas select no clip commands.
/// The descriptors and their lists remain borrowed while this overlay is loaded.
extern SpriteView gShelterR47SpriteViews[46];

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

/// Thirteen world-coordinate warp, stair, room-action and CAP trigger quads.
///
/// Gameplay binds and links them in place; the final record carries LAST.
/// Room entry disables either initial-entry slot 3 or repeat-entry slot 12,
/// and event progress switches from slot 3 to slot 12. Keep this overlay loaded
/// while the records are linked or their contacts are being consumed.
extern WorldCollisionTrigger gShelterR47ActionTriggers[13];

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
