#ifndef INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Enemy layouts indexed directly by the operating room's saved placement variant.
///
/// Slots 1, 2, 3 and 11 contain layouts; slot 0 and slots 4..10 are absent.
/// Variant 1 places Slouches and Stalkers, 2 Amoebas, 3 Maggots,
/// and 11 Amoebas with Slouches and a Sucklerceph. Select only a populated slot
/// for loading/spawning. The directory and both sentinel-terminated tables in
/// each layout remain borrowed while this room overlay is loaded.
extern AreaVariant gShelterB2OperatingRoomAreaVariants[12];

/// Logical-view maps for the operating room's single room slot.
///
/// Room ID 1 borrows `gViewIdentityMap`; logical views 1..7 select the same
/// camera/image/sprite IDs. The directory requires this room overlay to stay loaded.
extern u8* gShelterB2OperatingRoomRoomViewMaps[1];

/// Seven logical views for room ID 1, indexed by room ID minus one.
///
/// This signed element count bounds reverse lookup in the identity view map;
/// the array has no terminator and remains borrowed from the loaded room overlay.
extern ViewCount gShelterB2OperatingRoomRoomViewCounts[1];

/// Three operating-room door endpoints, indexed by one-based warp ID minus one.
///
/// Endpoints 1/2/3 lead to the south walkway, north walkway and laboratory,
/// respectively; arrivals select logical views 2/3/7. Each record places both
/// actors in world units and supplies departure facing, sounds and optional
/// door-map markers. There is no terminator; keep the room overlay loaded until
/// endpoint copies are consumed.
extern DirectionWarpEntry gShelterB2OperatingRoomWarpEntries[3];

/// Operating-room collision mesh and its 4-by-2 XZ cell directory.
///
/// Cells span 4000 game units from origin (-1000, 0). The 42 faces select
/// surface classes 0..2. Activation binds the view coordinate; the record and
/// all mesh/index storage remain borrowed while this room overlay is loaded.
extern WorldCollisionGrid gShelterB2OperatingRoomCollisionGrid;

/// Twelve authored operating-room cameras, indexed by camera ID minus one.
///
/// Normal logical views map to IDs 1..7; the remaining five records are
/// retained camera data. Rotation uses 12 fractional bits, origin uses game
/// units, and projection distance uses pixels. Keep the overlay loaded until
/// any queued camera application has consumed its borrowed record.
extern ViewCamera gShelterB2OperatingRoomViewCameras[12];

/// Sprite overlays for mapped operating-room views 1..7, stored at ID minus one.
///
/// View 1 draws only the decoded background; views 2..7 add depth-sorted
/// texture rectangles. All batch lists terminate with `SPRITE_BATCH_END`,
/// their first zero-count record retains background drawing, and clipping is
/// absent. The descriptors and their source/batch arrays live with this overlay.
extern SpriteView gShelterB2OperatingRoomSpriteViews[7];

/// Operating-room model lighting supplied by fourteen point lights in every view.
///
/// No directional or cone lights are present. Coordinate and attenuation
/// updates require writable records; all light storage lives with this overlay.
extern WorldCoordRoomLights gShelterB2OperatingRoomRoomLights;

/// Directed camera boundaries between logical view pairs 7/4, 5/2, 5/4, 7/6 and 3/2.
///
/// Each pair supplies both directions. Setup binds and enables all ten mutable
/// quads through the final LAST-marked record; positions use room units and
/// normals use 4096 per unit. Keep the room overlay loaded while they are linked.
extern WorldCollisionTrigger gShelterB2OperatingRoomViewBoundaryTriggers[10];

/// Three room-space quads blocking enemy line of sight in the operating room.
///
/// Setup links and enables all three mutable records, including the LAST-marked
/// final quad. Positions use game units and normals use 4096 per unit. Low flag
/// bit 0 is retained with no established meaning. Storage stays borrowed from
/// the room overlay until the list is cleared.
extern WorldCollisionOccluder gShelterB2OperatingRoomOccluders[3];

/// Three door warps and ten CAP interactions in room-space collision quads.
///
/// All require facing except the central command-5 quad, which also accepts
/// proximity. Commands 4/5 route through the room task for nursery-dependent
/// selection; the other interactions start CAP directly. Setup binds and enables
/// the thirteen mutable records through LAST; keep the room overlay loaded while
/// they are linked. Positions use game units and normals use 4096 per unit.
extern WorldCollisionTrigger gShelterB2OperatingRoomActionTriggers[13];

/// Minimum ambient colours for logical operating-room views 1..7.
///
/// Entry zero counts the seven following colours. RGB uses 16 units per
/// 8-bit colour level; the fourth component retains weighted brightness.
/// Lighting borrows this table only while the room overlay stays loaded.
extern WorldCoordRoomAmbientEntry gShelterB2OperatingRoomRoomAmbient[8];

/// Collision-policy directory indexed directly by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Classes
/// 1 and 2 select separate footstep cues; classes 0 and 3..7 share the silent
/// policy. Records and their sound tables remain borrowed from this overlay.
extern WorldCollisionSurfaceProperties* gShelterB2OperatingRoomSurfaceProperties[8];

/// Dispatches the operating room's persistent task and message service.
///
/// `task->state` must be 0 (install handlers and publish the room task),
/// 1 (idle, retaining the message service), or 2 (release the task).
/// The room overlay must remain loaded while this callback can be dispatched.
void shelterB2OperatingRoomTask(Task* task);

/// Runs this room's animated spark toward its initial target position.
///
/// Requires live room-effect state, a counted coordinate-body task starting at
/// state zero, and owned `EffectWork` in `spawnArg2.pointer` with zero age and
/// frame index. `spawnArg1.pointer` borrows a composed target `GfxCoord` until
/// the first running tick; the task's own composed coordinate uses the same
/// view space. That tick fixes a step at 204/4096 of the initial separation,
/// narrowing to signed 16-bit components before scaling. Later running ticks
/// move by that step and draw on odd ages; age 20 releases the work and task.
/// Room-effect control 1..3 pauses; 4 or above cancels and releases both.
/// Keep the room overlay loaded through dispatch and do not retain freed work.
void shelterB2OperatingRoomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Draws the operating room's fixed glows for the current mapped view.
///
/// State zero selects this room's glow-disc, flying-spark and orange-burst
/// effect IDs, then becomes state one. Every tick draws capsules and discs in
/// views 2..7; other views draw nothing. Requires the room overlay and view
/// mapping loaded, the current view matrix composed, and the frame's scratch
/// stack and GPU arena ready. Uses no task work or spawn payload; its owner
/// removes the task when leaving the room.
void shelterB2OperatingRoomDrawGlowsTask(Task* task);

/// Runs an attached two-tint charge disc with player-joint sparks and a release ring.
///
/// Start at state 0 with a coordinate body and counted, zeroed `EffectWork`
/// owned through `spawnArg2.pointer`; `spawnArg1.value` selects tint 0 or 1.
/// The work's parent coordinate stays borrowed until teardown; its stored
/// offset replaces the initial placement on attachment. Growing emits adopted
/// flying sparks from player parts 3..18 every fourth active age. The live
/// player rig and this room's installed flying-spark callback are required.
/// Its owner selects flicker, fading release or cancel through the
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*` states. Room-effect control 1..3 pauses;
/// 4 or above cancels. Release/cancel frees work and adopted children. Keep the
/// room, ancestors, effect resources, scratch and GPU arena live until teardown.
void shelterB2OperatingRoomRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs this room's orange burst with an expanding glow and fading ring.
///
/// Requires live room-effect state, a counted coordinate-body task starting at
/// state zero, and owned `EffectWork` in `spawnArg2.pointer`; `spawnArg1` is
/// ignored. Running ticks compose the coordinate, grow the disc and layered
/// glow, and fade the ring before fading the disc. Ground projection may add
/// a ground quad; the glow refreshes an orange point light even off screen.
/// Room-effect control 1..3 pauses; 4 or above cancels. Cancellation or finished
/// fading releases the work and task. Keep the room overlay loaded through
/// dispatch and do not retain freed work.
void shelterB2OperatingRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
