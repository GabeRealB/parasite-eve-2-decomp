#ifndef INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H
#define INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Extents of the laboratory's view resources and collision-surface directory.
enum {
    SHELTER_B2_LABORATORY_VIEW_COUNT          = 15,
    SHELTER_B2_LABORATORY_SURFACE_CLASS_COUNT = 8,
};

extern TmdSource gShelterB2LaboratoryItemBoxModel;

/// Saved-area changes committed when the laboratory terminal quiz succeeds.
///
/// Selects layout 3 and marks the mine secret passage, B1 north maintenance
/// walkway and B2 elevator hall; selects layout 12 and clears the night water
/// tank's mark. Applies in every save mode and removes saved enemy poses in
/// those areas. Four updates followed by `AREA_APPLY_END`; borrowed from the
/// loaded laboratory overlay by the terminal actor.
extern AreaApplyRec gShelterB2LaboratoryQuizCompletionAreaUpdates[5];

/// Conditional post-quiz saved-layout change for the pod-tunnel destination R47.
///
/// Selects layout 2, clears its map mark and removes saved enemy poses in every
/// save mode. The terminal applies it only while `GAME_FLAG_083` is clear.
/// One update followed by `AREA_APPLY_END`; keep the laboratory overlay loaded.
extern AreaApplyRec gShelterB2LaboratoryPodTunnelAreaUpdates[2];

/// Laboratory placement layouts indexed by the saved variant, 0..10.
///
/// Only variant 1 supplies placements and resources: a terminal package anchor
/// whose bodyless task immediately dies after loading the actor resources.
/// Slots 0 and 2..10 are empty. No terminator; gameplay borrows these records
/// and their tables while this room overlay is loaded.
extern AreaVariant gShelterB2LaboratoryAreaVariants[11];

/// Logical-view maps for the laboratory's single room.
///
/// Slot zero borrows `gViewIdentityMap`: logical views 1..15 select the same
/// 1-based camera, image and sprite indices. There is no terminator; keep the
/// room overlay and gameplay's shared map loaded while using this directory.
extern u8* gShelterB2LaboratoryRoomViewMaps[1];

/// Reverse-lookup view count for the laboratory's single room.
///
/// Room 1 searches fifteen logical-view entries. This count is independent of
/// the shared identity map's capacity; no terminator follows it. The stage map
/// borrows this array while the laboratory overlay is loaded.
extern ViewCount gShelterB2LaboratoryRoomViewCounts[1];

/// Laboratory arrival/departure endpoints for the main corridor and operating room.
///
/// Warp IDs 1 and 2 select entries 0 and 1, arriving in views 2 and 4.
/// The corridor endpoint sets map-marker nibble 451; the operating-room
/// endpoint has no map flag. Both place player and companion together and use
/// room-bank arrival/departure sounds. No terminator; borrowed while loaded.
extern DirectionWarpEntry gShelterB2LaboratoryWarpEntries[2];

/// Laboratory collision mesh indexed by a 4-by-3 XZ grid of 4000-unit cells.
///
/// Contains 51 faces using 123 vertices and 23 normals in room coordinates.
/// Activation binds the view coordinate; the descriptor and its writable
/// mesh and cell lists stay borrowed while this room overlay is loaded.
extern WorldCollisionGrid gShelterB2LaboratoryCollisionGrid;

/// Cameras for the laboratory's fifteen identity-mapped logical views.
///
/// Entry `view - 1` supplies Q12 world-to-camera rotation, negated origin in
/// world units and projection distance in pixels. No terminator; queued camera
/// tasks borrow the selected record until application, so keep this overlay loaded.
extern ViewCamera gShelterB2LaboratoryViewCameras[SHELTER_B2_LABORATORY_VIEW_COUNT];

/// Sprite and clipping descriptors for the laboratory's fifteen mapped views.
///
/// Entry `view - 1` borrows this overlay's mutable sources, sentinel-ended
/// batches and optional draw areas. Views 1, 6 and 10..14 have no source sprites;
/// every view retains decoded image strips. Keep this overlay loaded through
/// packet allocation, linking and any edits to the borrowed records.
extern SpriteView gShelterB2LaboratorySpriteViews[SHELTER_B2_LABORATORY_VIEW_COUNT];

/// Laboratory model-lighting collection, containing ten point lights for all views.
///
/// No directional or cone lights are authored. The room overlay owns the
/// writable transforms and attenuation values; consumers must release their
/// borrowed pointers before unloading it.
extern WorldCoordRoomLights gShelterB2LaboratoryRoomLights;

/// Fourteen directed boundaries switching between laboratory logical views.
///
/// Seven pairs connect views 2/4, 2/3, 5/6, 5/2, 7/8, 7/6 and 15/5.
/// The last included quad carries `WORLD_COLLISION_TRIGGER_LAST`. Room setup
/// binds and links these mutable records; keep their storage loaded until unlinked.
extern WorldCollisionTrigger gShelterB2LaboratoryViewBoundaryTriggers[14];

/// Laboratory interaction quads for exits, CAP commands and the console.
///
/// Nineteen included records end with `WORLD_COLLISION_TRIGGER_LAST` on the
/// final quad. The first two warp to the main corridor and operating room;
/// room-command 4 and directed-action 1 drive the console. Room setup borrows
/// and links the mutable records; keep this overlay loaded until unlinked.
extern WorldCollisionTrigger gShelterB2LaboratoryActionTriggers[19];

/// Minimum model ambient colours for the laboratory's fifteen logical views.
///
/// Entry zero is the view count; entries 1..15 hold RGB minima in units of
/// 1/16 of an 8-bit colour level and retained weighted brightness. View 3 has
/// elevated minima; the other views use one colour level. Borrowed from the
/// loaded room overlay, with no terminator beyond the counted entries.
extern WorldCoordRoomAmbientEntry gShelterB2LaboratoryViewAmbientTable[SHELTER_B2_LABORATORY_VIEW_COUNT + 1];

/// Laboratory collision and footstep policies indexed by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Classes
/// 1 and 2 select distinct footstep cue sets; classes 0 and 3..7 share a silent
/// record. The directory and its records belong to the loaded room overlay.
extern WorldCollisionSurfaceProperties* gShelterB2LaboratorySurfacePropertiesByClass[SHELTER_B2_LABORATORY_SURFACE_CLASS_COUNT];

/// Runs the laboratory's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void shelterB2LaboratoryTask(Task* task);

/// Starts laboratory ambience once and selects the glows' fast pulse.
///
/// Called by the console scene's normal and skip scripts with this room loaded.
/// Sets the ambience latch before spawning, so repeated calls are inert even
/// after allocation failure. Room command 4 clears the latch and permits a
/// later start. Keep this overlay loaded while its ambience task runs.
void shelterB2LaboratoryStartAmbience(void);

/// Draws the laboratory's light glows for the current mapped camera view.
///
/// Bank-6 effect 0x13E resets its fast-pulse selection on the initial tick,
/// then draws the view's tinted capsules and pulsing cyan glow each frame.
/// Views 5, 10, 12 and 15 use a diamond; view 13 uses a layered disc.
/// Requires this room overlay, composed view matrices, a current depth table,
/// primitive arena and initialized scratch stack. Emits no packets for views
/// absent from the dispatch and releases temporary scratch storage each call.
void shelterB2LaboratoryGlowTask(Task* task);

/// Updates the laboratory telephone's save menu and optional play statistics.
///
/// `spawnArg2.pointer` borrows the live UI object owned by this task. Save,
/// notice and statistics children must stay linked until their answers are
/// consumed. Requires this overlay and its telephone UI resources to stay loaded.
void shelterB2LaboratoryTelephoneMenuTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H
