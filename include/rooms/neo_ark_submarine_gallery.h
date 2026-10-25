#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Four eight-point Bog Diver patrol rings selected by spawn-argument bits 4..5.
///
/// Slots 0 and 3 share a ring; slots 1 and 2 change its waypoint order. XZ uses
/// room-coordinate units, and Y is depth below the sampled Diver water surface.
/// Each ring wraps at index 7 without a terminator. Keep this overlay loaded
/// throughout the borrowing actors' lifetimes.
extern SVECTOR* gNeoArkSubmarineGalleryDiverPatrolRings[4];

/// Writable emergence spots shared by the gallery's Bog Divers.
///
/// Entry 0 is the unsearched fallback; entries 1..4 supply candidate XZ in room
/// units, and entry 5 terminates with `pad == -1`. `pad` is the shared claim
/// mark (0 free, 1 claimed); the search preserves the actor's own Y rather than
/// copying spot Y. Keep this overlay loaded until every claim is released.
extern SVECTOR gNeoArkSubmarineGalleryDiverSurfaceSpots[6];

/// Water-surface Y sampled at spawn by the gallery's Bog and Sea Divers.
///
/// Signed room-coordinate units, positive downward; the initial surface is
/// 5000. Bog Diver patrol depth is added to this value. This room publishes no
/// live controller for the height.
extern s16 gNeoArkSubmarineGalleryDiverWaterY;

/// Values of the Sea Diver's room-owned pre-arrival indication.
enum {
    NEO_ARK_SUBMARINE_GALLERY_SEA_DIVER_ARRIVAL_NOT_PENDING = 0,
    NEO_ARK_SUBMARINE_GALLERY_SEA_DIVER_AWAITING_ARRIVAL    = 1,
};

/// Whether the Sea Diver has spawned but has not reached its entrance arrival.
///
/// Starts at 0, becomes 1 after successful actor-work allocation, and returns to
/// 0 at the first arrival-phase tick. The fight continues after that clear.
/// Room command 2 selects CAP entry 2 while set and entry 4 otherwise. Storage
/// belongs to this loaded room; actor destruction does not separately reset it.
extern s16 gNeoArkSubmarineGallerySeaDiverAwaitingArrival;

/// Coordinate-bodied red-disc spawn recipe, with execution priority 96.
///
/// Spawned during Sea Diver death and immediately in layout variant 4. Uses
/// zero payload words; its callback and this room must remain loaded while live.
extern TaskDesc gNeoArkSubmarineGalleryRedDiscTaskDesc;

/// Saved layouts applied when the Sea Diver is defeated, followed by `AREA_APPLY_END`.
///
/// Submarine Tunnel, Island and Submarine Gallery switch to layout 4 in every
/// save mode, discard saved enemy poses and clear their map marks. The actor
/// borrows this read-only list during death; keep the gallery overlay loaded.
extern const AreaApplyRec gNeoArkSubmarineGallerySeaDiverDefeatAreaUpdates[4];

/// Bodyless water-refraction and band-distortion recipes, in slots 0 and 1.
///
/// Both have execution priority `TASK_PRIORITY_CONTROL` and receive zero payload words. Gameplay
/// starts both for this room; their callbacks borrow the loaded room's drawing
/// resources. This is an indexed two-entry table with no terminator.
extern TaskDesc gNeoArkSubmarineGalleryWaterTaskDescriptors[2];

/// Placement/resource directory indexed by layout variant 0..12.
///
/// Slots 1, 2, 3 and 11 pair map-owned placements with room-owned resource
/// recipes; all other slots are empty. Slot 1 loads Sea and Bog Divers, and the
/// other populated slots load Bog Divers only. Keep both overlays loaded while
/// loading, spawning or querying their borrowed tables.
extern AreaVariant gNeoArkSubmarineGalleryPlacementVariants[13];

/// Number of mapped camera and sprite views in the gallery.
enum { NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT = 7 };

/// Number of room-local collision surface-class slots.
enum { NEO_ARK_SUBMARINE_GALLERY_SURFACE_CLASS_COUNT = 8 };

/// The gallery's single logical room uses the shared identity view map.
///
/// Logical views 1..7 select the corresponding camera, image and sprite view.
/// The directory is borrowed while this overlay and gameplay remain loaded.
extern u8* gNeoArkSubmarineGalleryViewMaps[1];

/// Seven logical camera views for the gallery's single room.
///
/// Indexed by 1-based room minus one; bounds reverse searches of the room's
/// identity view map. The Neo Ark map borrows this loaded room's count array.
extern ViewCount gNeoArkSubmarineGalleryViewCounts[1];

/// The gallery's single warp endpoint, indexed by 1-based warp minus one.
///
/// Arrivals place player and companion at (1300,5000,3100), yaw 2304 in
/// 4096-unit turns, and view 2. Arrival and departure use the island-travel
/// sound; departure requests a fade. The island trigger uses this endpoint
/// for departure and island endpoint 2 for arrival. Borrowed while loaded.
extern DirectionWarpEntry gNeoArkSubmarineGalleryWarpEntries[1];

/// Collision mesh and 3-by-3 XZ cell index for the gallery.
///
/// The 4000-unit cells cover X/Z from -5000 through 6999. Its 50 faces select
/// room-local surface classes 0, 3 and 4. Activation binds the mutable grid to
/// the current view; its mesh pools remain borrowed from this loaded overlay.
extern WorldCollisionGrid gNeoArkSubmarineGalleryCollisionGrid;

/// Camera transforms and pixel projection distances for mapped views 1..7.
///
/// Indexed by mapped view minus one, with Q12 rotations and signed world-unit
/// translations. Queued camera application requires this overlay to stay loaded.
extern ViewCamera gNeoArkSubmarineGalleryViewCameras[NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT];

/// Sprite sources and terminated batch lists for mapped views 1..7.
///
/// Indexed by mapped view minus one. View 1 has no source sprites; every view
/// requests decoded-image strips and has no clipping list. Source and batch
/// storage remains mutable and borrowed while this room overlay is loaded.
extern SpriteView gNeoArkSubmarineGallerySpriteViews[NEO_ARK_SUBMARINE_GALLERY_VIEW_COUNT];

/// Four directional and ten point lights shading the gallery in every view.
///
/// No cone lights are present. Coordinate composition and light queries mutate
/// the borrowed light arrays; keep this overlay loaded throughout their use.
extern WorldCoordRoomLights gNeoArkSubmarineGalleryRoomLights;

/// Eight directed camera boundaries connecting gallery views 2, 3, 4 and 5.
///
/// Room-space quads use game-coordinate units and Q12 normals. The last record
/// carries LAST; setup binds coordinates, links and enables the writable records.
/// Keep this overlay loaded until the view-boundary list has been unlinked.
extern WorldCollisionTrigger gNeoArkSubmarineGalleryViewBoundaryTriggers[8];

/// Ten interaction quads for island departure and gallery CAP commands.
///
/// One requests island warp 2 from local endpoint 1; the others select room
/// commands 2/3 or direct CAP entries 1/7. Positions use room units and normals
/// use Q12. The last record carries LAST. Setup links and mutates these borrowed
/// records; keep this overlay loaded until the action list has been unlinked.
extern WorldCollisionTrigger gNeoArkSubmarineGalleryActionTriggers[10];

/// Collision and footstep policies indexed by room-local surface class 0..7.
///
/// Class 3 passes probes and ignores weapon impacts; class 4 is the sounding
/// floor. Other classes block probes and permit impacts without footstep cues.
/// All classes apply pushback. Records borrow this overlay's loaded lifetime.
extern WorldCollisionSurfaceProperties* gNeoArkSubmarineGallerySurfacePropertiesByClass[NEO_ARK_SUBMARINE_GALLERY_SURFACE_CLASS_COUNT];

/// Draws the gallery's fixed glows for the current mapped camera view.
///
/// State zero selects this room's water-ripple and water-spray effects and
/// changes to state one. Every invocation, including the first, draws the
/// capsules and discs visible in mapped views 2..6; view 2 also draws a pulsing
/// light prism. Other mapped views emit no primitives. The task remains live.
///
/// `task` must have a live coordinate body whose coordinate is already composed
/// for the prism. Requires the loaded gallery overlay, current view matrices,
/// initialized scratch stack and frame packet arena and ordering table with
/// room for up to 39 Gouraud quads and their additive blend commands. Points and
/// the coordinate are borrowed for this call; queued packets live until GPU
/// completion. Drawing also runs while effects are suspended or cancelled.
void neoArkSubmarineGalleryDrawViewGlowsTask(Task* task);

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// `task` must be a counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body and initial state zero.
/// `spawnArg1` bits 0..11 give the local half-side in game units (0..4095).
/// Running updates grow it by 32 and fade brightness from 64 by 2 per update;
/// a fresh ripple lasts 32 running updates. The first update selects a random
/// surface yaw, applied after that update's coordinate composition.
/// Non-running updates redraw without aging; cancellation redraws once before
/// retirement. Retirement frees the work and destroys the task and body,
/// decrementing the room's effect count. Pointers to retired objects expire.
void neoArkSubmarineGalleryWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle in this room.
///
/// `task` must be a counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state zero and cell index zero.
/// `spawnArg1` bits 0..11 give perspective size (0..4095), bits 12..15 updates
/// per cell (0 selects 1), and bits 16..23 launch speed in parent-coordinate
/// units per update (0 selects 64). Bits 24..27 select velocity: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 copied spawn-offset
/// direction; other values normalize the zero direction. Nonzero bits 28..31
/// select upright drawing; otherwise the sprite keeps a random rotation.
/// A supplied nonzero `EffectWork::move` bypasses velocity generation.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to Y velocity, narrowing it to s16; stationary
/// particles skip movement and gravity. Cells 0..7 each last the decoded period.
/// Suspended updates redraw without aging; cancellation retires without drawing.
/// Retirement frees the work and destroys the task and body, decrementing the
/// room's effect count. Pointers to retired objects expire.
void neoArkSubmarineGalleryWaterDriftTaskU16(Task* task);

/// Runs the gallery's room-message task through initialization, idle and release.
///
/// Requires a live task with state 0, 1 or 2, respectively. Initialization
/// installs the room's message table, publishes `GAME_TASK_SLOT_ROOM` and starts
/// the red-disc task in variant 4. Keep the gallery overlay loaded throughout
/// this task's lifetime; state 2 destroys the task.
void neoArkSubmarineGalleryMessageTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
