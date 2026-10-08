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

extern TmdSource gShelterB2LaboratoryAcropolisSanctuaryModel090F0;

extern AreaApplyRec D_shelter_b2_laboratory_80186488[5];

extern AreaApplyRec D_shelter_b2_laboratory_8018649C[2];

extern AreaVariant D_shelter_b2_laboratory_80186360[11];

// shelter_b2_laboratory
extern u8* D_shelter_b2_laboratory_80182C08[];

extern ViewCount D_shelter_b2_laboratory_80182C0C[];

extern DirectionWarpEntry D_shelter_b2_laboratory_80182C10[];

extern WorldCollisionGrid D_shelter_b2_laboratory_8018355C;

extern ViewCamera D_shelter_b2_laboratory_80183580[];

extern SpriteView D_shelter_b2_laboratory_801854D0[];

extern WorldCoordRoomLights D_shelter_b2_laboratory_80185944;

extern WorldCollisionTrigger D_shelter_b2_laboratory_8018595C[];

extern WorldCollisionTrigger D_shelter_b2_laboratory_80185D84[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_laboratory_801863B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_laboratory_80186468[];

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
