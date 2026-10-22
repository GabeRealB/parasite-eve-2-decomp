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

/// Descriptor indices for the cafeteria movie transition; there is no end marker.
enum {
    ACROPOLIS_CAFETERIA_MOVIE_TASK_LAUNCH   = 0,
    ACROPOLIS_CAFETERIA_MOVIE_TASK_BLACKOUT = 1,
    ACROPOLIS_CAFETERIA_MOVIE_TASK_PLAYBACK = 2,
    ACROPOLIS_CAFETERIA_MOVIE_TASK_COUNT    = 3,
};

/// Bodyless priority-192 movie launcher, blackout and playback spawn recipes.
///
/// Select one descriptor by the indices above; the table has no terminator.
/// Launcher and playback are selected from separate translation units, while
/// the blackout recipe has no current spawn site. Descriptors are consumed
/// synchronously, but callback code and movie resources must remain loaded.
extern TaskDesc gAcropolisCafeteriaMovieTasks[ACROPOLIS_CAFETERIA_MOVIE_TASK_COUNT];

/// Cafeteria room-effect dispatch for puff control, followed by its end marker.
///
/// The effect task borrows both read-only entries while the overlay is loaded.
/// The first integer payload stores the puff gate verbatim (0 disables,
/// nonzero enables); unsupported message IDs return zero.
extern TaskMessageEntry gAcropolisCafeteriaRoomEffectMessages[2];

/// Shared cafeteria puff gate (0 disabled, any nonzero signed word enabled).
///
/// Room-effect initialization clears it; control messages store their value
/// verbatim. Emitters and individual puffs observe the same gate and release
/// themselves when it clears. The gate belongs to the loaded room overlay.
extern s32 gAcropolisCafeteriaPuffEnabled;

/// Authored point lights contributing in every cafeteria view.
///
/// The room light collection borrows this complete array while the overlay is
/// loaded. Positions and falloff radii use integer world units; RGB intensities
/// use 12 fractional bits. Parent links, coordinate caches and per-query
/// attenuation remain writable.
extern WorldCoordPointLight gAcropolisCafeteriaPointLights[15];

/// Light collection shared by all four cafeteria room configurations.
///
/// Borrows fifteen writable point lights and one live cone light, with no
/// directional lights. Coordinate setup and lighting queries mutate the
/// borrowed arrays; the collection and its pointers live only while this
/// overlay is loaded. Inactive cone-light storage is outside the live count.
extern WorldCoordRoomLights gAcropolisCafeteriaRoomLights;

/// Minimum model ambient colours for the cafeteria's 24 session views.
///
/// Entry zero counts the following view entries, indexed by 1-based session
/// view rather than mapped background index. RGB uses 16 units per 8-bit level;
/// views 7 and 8 use a brighter, blue-biased minimum. The stored fourth colour
/// component is unused by lighting. All four room configurations borrow this
/// table only while the overlay is loaded.
extern WorldCoordRoomAmbientEntry gAcropolisCafeteriaViewAmbientColors[25];

/// Saved-area placement and map changes committed after the cafeteria follow-up scene.
///
/// Applies layout 2 to the patio and forked road in every save mode, clearing
/// the patio's map mark and setting the forked road's. Also discards both areas'
/// saved enemy poses. The third record is `AREA_APPLY_END`; the synchronous
/// update call borrows the list while the cafeteria overlay is loaded.
extern AreaApplyRec gAcropolisCafeteriaFollowUpAreaUpdates[3];

/// Singleton Stranger recoil X velocity in signed room units per controller tick.
///
/// Initialized to zero for each settle controller, then halved toward zero each
/// tick. Shared with the room's second TU; only one settle controller may use
/// it at a time, and storage is valid only while the cafeteria overlay is loaded.
extern s32 gAcropolisCafeteriaStrangerRecoilVelocityX;

/// Singleton Stranger recoil Y velocity in signed room units per controller tick.
///
/// Initialized to -20 for each settle controller; positive Y points down.
/// Gravity adds 5 per tick, capped at +20, even after the root reaches its floor.
/// Only one settle controller may use it while the cafeteria overlay is loaded.
extern s32 gAcropolisCafeteriaStrangerRecoilVelocityY;

/// Singleton Stranger recoil Z velocity in signed room units per controller tick.
///
/// Initialized to -20 for each settle controller, then halved toward zero each
/// tick. Only one settle controller may use it while the cafeteria overlay is
/// loaded; the controller ends once X/Z stop and the root reaches its floor.
extern s32 gAcropolisCafeteriaStrangerRecoilVelocityZ;

/// The cafeteria's cone lights: the room's light collection counts the first.
///
/// The other fourteen records keep stale bytes - sprite fields and a pointer
/// outside the room among them - that nothing reads. Coordinate caches, parent
/// links and attenuation remain writable while the overlay is loaded.
extern WorldCoordSpotLight gAcropolisCafeteriaSpotLightStorage[15];

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
