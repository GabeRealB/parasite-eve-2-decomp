#ifndef INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b6_corridor_80180304[13];

/// Number of mapped camera, image and sprite views in the corridor.
enum { SHELTER_B6_CORRIDOR_VIEW_COUNT = 5 };

/// The corridor's single room borrows the gameplay identity map for logical views 1..5.
///
/// The room directory is indexed by room minus one. Keep this overlay and
/// gameplay loaded while using it; the map's capacity exceeds the room's count.
extern u8* gShelterB6CorridorViewMaps[1];

/// Logical-view search count for the corridor's single room, indexed by room minus one.
extern ViewCount gShelterB6CorridorViewCounts[1];

/// Arrival placements and sound cues for the elevator and training-room endpoints.
///
/// Warp IDs 1 and 2 select entries 0 and 1. The elevator endpoint arrives in
/// view 4 with an arrival sound; the training endpoint arrives silently in
/// view 3 and supplies a departure sound. Borrowed while this overlay is loaded.
extern DirectionWarpEntry gShelterB6CorridorWarpEntries[2];

/// The corridor's floor, ceiling and walls, indexed by three X cells and one Z cell.
///
/// Cells are 4000 game units wide, with room origin (X=0, Z=-1350). The mesh
/// has six normals, twelve vertices and twelve faces. Surface class 1 is the
/// central floor strip, class 2 its side strips, and class 0 the other faces.
/// Room setup binds the mutable grid to the current view; keep this overlay
/// loaded until the grid is no longer active.
extern WorldCollisionGrid gShelterB6CorridorCollisionGrid;

/// Five corridor camera transforms and projection distances, indexed by mapped view minus one.
///
/// Rotations use 4096 per unit, translations negate world-space camera origins,
/// and projection distances are pixels. Queued camera tasks borrow the entries
/// until application; this overlay must remain loaded through that dispatch.
extern ViewCamera gShelterB6CorridorViewCameras[SHELTER_B6_CORRIDOR_VIEW_COUNT];

/// Destroyed-part sprite overlays for each corridor view, indexed by mapped view minus one.
///
/// View 2 shows part slots 0 and 1, view 3 slots 2 and 1; views 1, 4 and 5
/// have no source sprites. Every first batch has zero count, retaining decoded
/// background strips. The part controller changes batch visibility at setup
/// and destruction. Records and their mutable lists live in this overlay.
extern SpriteView gShelterB6CorridorSpriteViews[SHELTER_B6_CORRIDOR_VIEW_COUNT];

/// The corridor's single white point light, available to every view.
///
/// No directional or cone lights are present. Coordinate composition and
/// attenuation update the borrowed point-light storage while the room is active.
extern WorldCoordRoomLights gShelterB6CorridorRoomLights;

/// Six directional boundaries switching corridor views 2/3 and 2/4.
///
/// The final boundary carries LAST and is also linked. Room setup mutates the
/// list links and view coordinates; keep these records loaded until unlinked.
extern WorldCollisionTrigger gShelterB6CorridorViewBoundaryTriggers[6];

/// Two exit quads and three Eve-part interaction quads for the corridor.
///
/// Exits request the elevator and training room; interactions send room command
/// IDs 2..4 for part slots 0..2. The final interaction carries LAST and is linked.
/// Coordinates and list links remain mutable and live until room teardown.
extern WorldCollisionTrigger gShelterB6CorridorActionTriggers[5];

/// Minimum ambient light per corridor view, after a header containing the view count.
///
/// Entry 0 holds five; entries 1..5 use 16 units per 8-bit RGB level. The
/// fourth colour component retains authored luma, unused by lighting.
extern WorldCoordRoomAmbientEntry gShelterB6CorridorRoomAmbientByView[SHELTER_B6_CORRIDOR_VIEW_COUNT + 1];

/// Collision policies and footstep cues for the corridor's eight surface classes.
///
/// Indexed directly by class 0..7, without a terminator. All classes block
/// probes, allow weapon impacts and apply pushback. Classes 1 and 2 supply
/// separate floor footstep cues; classes 0 and 3..7 share a record with no cues.
/// Borrowed only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gShelterB6CorridorSurfaceProperties[8];

/// Runs the corridor's initialization, background-mask upkeep or teardown state.
///
/// The map overlay selects this callback for stage 5, area 24. `task->state`
/// must be 0 (initialize), 1 (keep future image decodes masked), or 2 (kill).
/// The room overlay, session and loaded image workspace must remain available.
void shelterB6CorridorRoomTask(Task* task);

/// Draws Eve's two yellow body glows and occasionally spawns a joint flash.
///
/// Called by the live corridor body tick. Requires its 19-coordinate TMD with
/// coordinate 1 and joints 3..18 available and already composed, current view
/// transforms and a ready frame arena/ordering table. Runs only while room
/// effects are running. Disc radius numerators 512/1024 scale by 64/(SZ3/4+1);
/// alternating frames use RGB (64,64,32) and (80,80,40). One LCG draw tests a
/// one-in-four flash; a second selects joint 3..18, size 128 and palette 1.
/// The task and coordinates are borrowed; GPU packets live until drawing ends.
void shelterB6CorridorDrawBodyGlow(Task* task);

/// Scenery states accepted by `shelterB6CorridorSetPartDestroyedSprites`.
enum {
    SHELTER_B6_CORRIDOR_PART_INTACT    = 0,
    SHELTER_B6_CORRIDOR_PART_DESTROYED = 1,
};

/// Shows or hides the destroyed scenery for one of the corridor's three Eve parts.
///
/// `partSlot` is 0..2 and `destroyed` is 0 (intact, hidden) or 1 (destroyed,
/// visible); other byte values do nothing. Slot 0 uses sprite-view element 1,
/// slot 1 elements 1 and 2, and slot 2 element 2 (zero-based). Requires the
/// current stage and area's corridor sprite tables to remain loaded.
void shelterB6CorridorSetPartDestroyedSprites(u8 partSlot, u8 destroyed);

/// Draws the corridor's yellow wall-light glows for the current mapped view.
///
/// Bank-6 slot 0x15E resets the player-hit-glow generation on its first tick.
/// Mapped views 2, 3 and 4 draw four, six and one capsule glows respectively;
/// other views draw none. Drawing continues regardless of effect-control state.
/// Requires the loaded corridor overlay, current view transform, initialized
/// scratch stack and a ready frame arena and ordering table.
void shelterB6CorridorDrawViewGlowsTask(Task* task);

/// Draws the newest expanding yellow player-hit disc and retires older instances.
///
/// Bank-6 slot 0x299 owns a zeroed `EffectWork` in `spawnArg2.pointer` and a
/// composed coordinate body. `scale` holds brightness; `angle` is the base
/// radius numerator (the disc uses four times it), scaled to pixels by
/// 64 / (OTZ + 1). `spawnArg1.value` records this instance's generation on its
/// first running tick.
/// Pauses while effects are stopped; cancellation, supersession and fade-out
/// release the counted work and task. Fading follows drawing from running tick
/// 9, ending at tick 16. Requires the room overlay and frame drawing resources.
void shelterB6CorridorPlayerHitGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
