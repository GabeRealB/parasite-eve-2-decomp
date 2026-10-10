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

/// Extents of the laboratory's view colours and collision-surface directory.
enum {
    SHELTER_B2_LABORATORY_VIEW_COUNT          = 15,
    SHELTER_B2_LABORATORY_SURFACE_CLASS_COUNT = 8,
};

extern TmdSource gShelterB2LaboratoryAcropolisSanctuaryModel090F0;

extern AreaApplyRec D_shelter_b2_laboratory_80186488[5];

extern AreaApplyRec D_shelter_b2_laboratory_8018649C[2];

extern AreaVariant D_shelter_b2_laboratory_80186360[11];

/// Logical-view maps for the laboratory's single room.
///
/// Slot zero borrows `gViewIdentityMap`: logical views 1..15 select the same
/// 1-based camera, image and sprite indices. There is no terminator; keep the
/// room overlay and gameplay's shared map loaded while using this directory.
extern u8* gShelterB2LaboratoryRoomViewMaps[1];

extern ViewCount D_shelter_b2_laboratory_80182C0C[];

extern DirectionWarpEntry D_shelter_b2_laboratory_80182C10[];

/// Laboratory collision mesh indexed by a 4-by-3 XZ grid of 4000-unit cells.
///
/// Contains 51 faces using 123 vertices and 23 normals in room coordinates.
/// Activation binds the view coordinate; the descriptor and its writable
/// mesh and cell lists stay borrowed while this room overlay is loaded.
extern WorldCollisionGrid gShelterB2LaboratoryCollisionGrid;

extern ViewCamera D_shelter_b2_laboratory_80183580[];

extern SpriteView D_shelter_b2_laboratory_801854D0[];

/// Laboratory model-lighting collection, containing ten point lights for all views.
///
/// No directional or cone lights are authored. The room overlay owns the
/// writable transforms and attenuation values; consumers must release their
/// borrowed pointers before unloading it.
extern WorldCoordRoomLights gShelterB2LaboratoryRoomLights;

extern WorldCollisionTrigger D_shelter_b2_laboratory_8018595C[];

extern WorldCollisionTrigger D_shelter_b2_laboratory_80185D84[];

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
