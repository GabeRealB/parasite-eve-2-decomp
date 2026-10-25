#ifndef STAGES_AKROPOLIS_ROOMS_CAFETERIA_CAFETERIA_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_CAFETERIA_CAFETERIA_PRIVATE_H

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
    AKROPOLIS_CAFETERIA_MOVIE_TASK_LAUNCH   = 0,
    AKROPOLIS_CAFETERIA_MOVIE_TASK_PLAYBACK = 2,
    AKROPOLIS_CAFETERIA_MOVIE_TASK_COUNT    = 3,
};

/// Bodyless priority-192 movie launcher, blackout and playback spawn recipes.
extern TaskDesc gAkropolisCafeteriaMovieTasks[AKROPOLIS_CAFETERIA_MOVIE_TASK_COUNT];

/// Cafeteria room-effect dispatch for puff control, followed by its end marker.
extern TaskMessageEntry gAkropolisCafeteriaRoomEffectMessages[2];

/// Shared cafeteria puff gate (0 disabled, any nonzero signed word enabled).
extern s32 gAkropolisCafeteriaPuffEnabled;

/// Authored point lights contributing in every cafeteria view.
extern WorldCoordPointLight gAkropolisCafeteriaPointLights[15];

/// Light collection shared by all four cafeteria room configurations.
extern WorldCoordRoomLights gAkropolisCafeteriaRoomLights;

/// Minimum model ambient colors for the cafeteria's 24 session views.
extern WorldCoordRoomAmbientEntry gAkropolisCafeteriaViewAmbientColors[25];

/// Saved-area placement and map changes committed after the cafeteria follow-up scene.
extern AreaApplyRec gAkropolisCafeteriaFollowUpAreaUpdates[3];

/// Singleton Stranger recoil X velocity in signed room units per controller tick.
extern s32 gAkropolisCafeteriaStrangerRecoilVelocityX;

/// Singleton Stranger recoil Y velocity in signed room units per controller tick.
extern s32 gAkropolisCafeteriaStrangerRecoilVelocityY;

/// Singleton Stranger recoil Z velocity in signed room units per controller tick.
extern s32 gAkropolisCafeteriaStrangerRecoilVelocityZ;

/// The cafeteria's cone lights: the room's light collection counts the first.
extern WorldCoordSpotLight gAkropolisCafeteriaSpotLightStorage[15];

// Callbacks referenced by the overlay's shared data tables.
/// Plays the cafeteria movie with its area-music cue, then restores game presentation.
void akropolisCafeteriaPlayMovieTask(Task* movieTask);

/// Holds the frame black during the cafeteria's movie transition.
void akropolisCafeteriaBlackoutTask(Task* task);

/// Hands frame presentation to cafeteria movie playback and releases the launcher.
void akropolisCafeteriaStartMovieTask(Task* task);

/// Room-effect message whose first integer payload controls cafeteria puff emission.
enum { AKROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED = 3000 };

/// Sets the cafeteria puff gate and creates an emitter for each nonzero request.
s32 akropolisCafeteriaSetPuffEnabled(Task* task, s32 messageId, s32 enabled, s32 unused);

#endif // STAGES_AKROPOLIS_ROOMS_CAFETERIA_CAFETERIA_PRIVATE_H
