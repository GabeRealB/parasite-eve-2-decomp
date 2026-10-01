#ifndef SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_flags.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// The cafeteria's live cone light followed by fourteen inactive record slots.
///
/// `liveLights` is the complete array borrowed by the room's light collection.
/// Coordinates and attenuation remain writable while this overlay is loaded.
/// `inactiveSlots` preserves unused representations at the spotlight stride;
/// their remaining payload's role is unproven.
typedef struct {
    WorldCoordSpotLight liveLights[1];                                  // Cone light used by the room's lighting descriptor.
    u8                  inactiveSlots[14][sizeof(WorldCoordSpotLight)]; // Opaque inactive records, excluded from the live count.
} AcropolisCafeteriaSpotLightStorage;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaSpotLightStorage, 1620);

extern TaskDesc D_acropolis_cafeteria_80184178[];

extern TaskMessageEntry D_acropolis_cafeteria_80184CEC[2];

extern s32 D_acropolis_cafeteria_80184CFC;

extern WorldCoordPointLight D_acropolis_cafeteria_80189E24[15];

extern WorldCoordRoomLights D_acropolis_cafeteria_8018AA18[1];

extern WorldCoordRoomAmbientEntry D_acropolis_cafeteria_8018C90C[25];

extern GpAreaApplyRec D_acropolis_cafeteria_8018C9D4[3];

extern s32 D_acropolis_cafeteria_8018D6A0;

extern s32 D_acropolis_cafeteria_8018D6A4;

extern s32 D_acropolis_cafeteria_8018D6A8;

extern AcropolisCafeteriaSpotLightStorage D_acropolis_cafeteria_8018A3C4;

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_cafeteria_8017E47C(Task*);

void func_acropolis_cafeteria_8017E658(Task*);

void func_acropolis_cafeteria_8017E6B8(Task*);

s32 func_acropolis_cafeteria_8017F908(Task*, s32, s32, s32);

#endif // SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
