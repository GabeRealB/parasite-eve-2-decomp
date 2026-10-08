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
/// Plays the cafeteria movie with its area-music cue, then restores game presentation.
///
/// Requires a fresh bodyless display task and loaded stream ID 100, sub-ID zero,
/// matching the current room. Saved image-memory and VRAM regions must survive
/// restoration. Start cancels playback; both exits wait for CD idle before
/// restoring models and sprite images. `killCountdown` counts playing callbacks
/// with 16-bit wrap; tick 1091 starts area music. `spawnArg1.value` latches that
/// request (0 pending, 1 requested), so an earlier exit starts music after restore.
/// Frees the task and resumes the game loop when restoration completes.
void acropolisCafeteriaPlayMovieTask(Task* movieTask);

/// Holds the frame black during the cafeteria's movie transition.
///
/// Queues maximum subtractive intensity each tick. `killCountdown` advances
/// by four with 16-bit wraparound; its signed value reaching 256 ends the task.
/// A fresh bodyless task starts at zero and lasts 64 callback ticks.
void acropolisCafeteriaBlackoutTask(Task* task);

/// Hands frame presentation to cafeteria movie playback and releases the launcher.
///
/// Requires the movie descriptor and current camera resources to remain live.
/// Selects task-only flipping and queues the current camera/packets after the
/// spawn attempt, including when it fails; no retry is made.
void acropolisCafeteriaStartMovieTask(Task* task);

/// Room-effect message whose first integer payload controls cafeteria puff emission.
enum { ACROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED = 3000 };

/// Sets the cafeteria puff gate and creates an emitter for each nonzero request.
///
/// Handles `ACROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED`; `enabled` is stored
/// verbatim (zero disables, any other value enables). The receiver must have a
/// coordinate body; its coordinate supplies the new emitter's placement frame.
/// Repeated nonzero messages can create independent emitters. Clearing the gate
/// leaves existing emitters and puffs to release themselves on their next ticks.
/// `messageId` and `unused` are ignored. Returns zero even if spawning fails.
s32 acropolisCafeteriaSetPuffEnabled(Task* task, s32 messageId, s32 enabled, s32 unused);

#endif // SRC_ROOMS_ACROPOLIS_CAFETERIA_ACROPOLIS_CAFETERIA_PRIVATE_H
