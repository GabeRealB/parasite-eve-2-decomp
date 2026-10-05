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

/// Authored point lights contributing in every cafeteria view.
///
/// The room light collection borrows this complete array while the overlay is
/// loaded. Positions and falloff radii use integer world units; RGB intensities
/// use 12 fractional bits. Parent links, coordinate caches and per-query
/// attenuation remain writable.
extern WorldCoordPointLight gAcropolisCafeteriaPointLights[15];

extern WorldCoordRoomLights D_acropolis_cafeteria_8018AA18[1];

extern WorldCoordRoomAmbientEntry D_acropolis_cafeteria_8018C90C[25];

extern AreaApplyRec D_acropolis_cafeteria_8018C9D4[3];

extern s32 D_acropolis_cafeteria_8018D6A0;

extern s32 D_acropolis_cafeteria_8018D6A4;

extern s32 D_acropolis_cafeteria_8018D6A8;

/// Authored cone-light storage shared by all cafeteria lighting sets.
///
/// The room light collection borrows only the one-element `liveLights` array
/// while the overlay is loaded; `inactiveSlots` is outside its live count.
/// The light contributes in every view. Position and falloff radii use integer
/// world units, RGB intensities use 12 fractional bits, and the full cone opening
/// uses 0x1000 units per turn. Coordinate caches, parent links and query
/// attenuation remain writable; pointers into this block must not outlive the
/// overlay.
extern AcropolisCafeteriaSpotLightStorage gAcropolisCafeteriaSpotLightStorage;

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_cafeteria_8017E47C(Task*);

/// Holds the frame black during the cafeteria's movie transition.
///
/// Queues maximum subtractive intensity each tick. `killCountdown` advances
/// by four with 16-bit wraparound; its signed value reaching 256 ends the task.
/// A fresh bodyless task starts at zero and lasts 64 callback ticks.
void acropolisCafeteriaBlackoutTask(Task* task);

void func_acropolis_cafeteria_8017E6B8(Task*);

s32 func_acropolis_cafeteria_8017F908(Task*, s32, s32, s32);

#endif // SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
