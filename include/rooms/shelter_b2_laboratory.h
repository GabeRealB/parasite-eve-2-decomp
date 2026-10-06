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

void func_shelter_b2_laboratory_801804A4(Task* task);

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_b2_laboratory_801804FC(void);

/// Draws the laboratory's light glows for the current mapped camera view.
///
/// Bank-6 effect 0x13E resets its fast-pulse selection on the initial tick,
/// then draws the view's tinted capsules and pulsing cyan glow each frame.
/// Views 5, 10, 12 and 15 use a diamond; view 13 uses a layered disc.
/// Requires this room overlay, composed view matrices, a current depth table,
/// primitive arena and initialized scratch stack. Emits no packets for views
/// absent from the dispatch and releases temporary scratch storage each call.
void shelterB2LaboratoryGlowTask(Task* task);

void func_shelter_b2_laboratory_8017EAB4(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_LABORATORY_H
