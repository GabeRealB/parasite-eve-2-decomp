#ifndef INCLUDE_ROOMS_AKROPOLIS_PLAZA_H
#define INCLUDE_ROOMS_AKROPOLIS_PLAZA_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Native animation sets shared with the companion actor overlay.
extern AnimationSet gAkropolisPlazaExtraAnimClip;

extern AnimationSet gAkropolisPlazaExtraAnim2Clip;

extern AnimationSet gAkropolisPlazaExtraAnim3Clip;

extern AnimationSet gAkropolisPlazaExtraAnim4Clip;

extern AnimationSet gAkropolisPlazaExtraAnim5Clip;

extern AnimationSet gAkropolisPlazaExtraAnim6Clip;

extern AnimationSet gAkropolisPlazaExtraAnim7Clip;

extern AnimationSet gAkropolisPlazaExtraAnim8Clip;

extern AnimationSet gAkropolisPlazaExtraAnim9Clip;

extern AnimationSet gAkropolisPlazaExtraAnim10Clip;

extern AnimationSet gAkropolisPlazaExtraAnim11Clip;

extern AnimationSet gAkropolisPlazaExtraAnim12Clip;

extern AnimationSet gAkropolisPlazaExtraAnim13Clip;

extern AnimationSet gAkropolisPlazaExtraAnim14Clip;

extern AnimationSet gAkropolisPlazaExtraAnim15Clip;

extern AnimationSet gAkropolisPlazaExtraAnim16Clip;

extern AnimationSet gAkropolisPlazaExtraAnim17Clip;

extern AnimationSet gAkropolisPlazaExtraAnim18Clip;

extern AnimationSet gAkropolisPlazaExtraAnim19Clip;

extern AnimationSet gAkropolisPlazaExtraAnim20Clip;

extern AnimationSet gAkropolisPlazaExtraAnim21Clip;

extern AnimationSet gAkropolisPlazaExtraAnim22Clip;

extern AnimationSet gAkropolisPlazaExtraAnim23Clip;

extern AnimationSet gAkropolisPlazaExtraAnim24Clip;

extern AnimationSet gAyaIdleClip;

extern AnimationSet gAkropolisPlazaExtraAnim25Clip;

extern AnimationSet gAkropolisPlazaExtraAnim26Clip;

extern AnimationSet gAkropolisPlazaExtraAnim27Clip;

extern AnimationSet gAkropolisPlazaExtraAnim28Clip;

extern AnimationSet gAkropolisPlazaExtraAnim29Clip;

extern AnimationSet gAkropolisPlazaExtraAnim30Clip;

extern AnimationSet gAkropolisPlazaExtraAnim31Clip;

extern AnimationSet gAkropolisPlazaExtraAnim32Clip;

extern AnimationSet gAkropolisPlazaExtraAnim33Clip;

extern AnimationSet gAkropolisPlazaExtraAnim34Clip;

/// Entry indices of the plaza's direct-spawn sequence task table.
enum {
    AKROPOLIS_PLAZA_TASK_SEQUENCE = 0,
    AKROPOLIS_PLAZA_TASK_STREAMED_SCENE,
    AKROPOLIS_PLAZA_TASK_STREAM_SCENE,
    AKROPOLIS_PLAZA_TASK_FINAL_SCENE,
    AKROPOLIS_PLAZA_TASK_FIRST_SCENE,
    AKROPOLIS_PLAZA_TASK_DISPLAY_SETUP,
    AKROPOLIS_PLAZA_TASK_REPEAT_SCENE,
    AKROPOLIS_PLAZA_TASK_FADE_OUT,
    AKROPOLIS_PLAZA_TASK_FADE_IN,
    AKROPOLIS_PLAZA_TASK_CAPTION,
    AKROPOLIS_PLAZA_TASK_MOVIE_DISPLAY,
    AKROPOLIS_PLAZA_TASK_LETTERBOX,
    AKROPOLIS_PLAZA_TASK_COUNT
};

/// Extents of the plaza's exported room, view, warp and surface tables.
enum {
    AKROPOLIS_PLAZA_ROOM_COUNT          = 1,
    AKROPOLIS_PLAZA_VIEW_COUNT          = 8,
    AKROPOLIS_PLAZA_WARP_COUNT          = 1,
    AKROPOLIS_PLAZA_SURFACE_CLASS_COUNT = 8
};

/// Direct-spawn recipes for the streamed plaza sequence and its presentation tasks.
extern TaskDesc gAkropolisPlazaSequenceTaskDescriptors[AKROPOLIS_PLAZA_TASK_COUNT];

/// Collision resources for the plaza's single room.
extern WorldCollisionRoomResources gAkropolisPlazaRoomCollisionResources[AKROPOLIS_PLAZA_ROOM_COUNT];

/// Logical-view maps for the plaza's single room.
extern u8* gAkropolisPlazaViewMaps[AKROPOLIS_PLAZA_ROOM_COUNT];

/// Logical-view count for the plaza's single room, in view elements.
extern ViewCount gAkropolisPlazaViewCounts[AKROPOLIS_PLAZA_ROOM_COUNT];

/// Lighting descriptors for the plaza's single room.
extern WorldCoordRoomLighting gAkropolisPlazaRoomLightingTable[AKROPOLIS_PLAZA_ROOM_COUNT];

/// Static room cameras selected by the plaza's eight logical views.
extern ViewCamera gAkropolisPlazaViewCameras[AKROPOLIS_PLAZA_VIEW_COUNT];

/// Image-presentation choices for the plaza's eight logical views.
extern SpriteView gAkropolisPlazaSpriteViews[AKROPOLIS_PLAZA_VIEW_COUNT];

/// The plaza's single arrival/departure endpoint, selected by warp ID 1.
extern DirectionWarpEntry gAkropolisPlazaWarpEntries[AKROPOLIS_PLAZA_WARP_COUNT];

/// Collision and footstep records selected by room-local surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisPlazaSurfacePropertiesByClass[AKROPOLIS_PLAZA_SURFACE_CLASS_COUNT];

/// Slots in the plaza's area-layout table, including the absent variants 0 and 2.
enum { AKROPOLIS_PLAZA_AREA_VARIANT_COUNT = 3 };

/// Area layouts of the plaza, indexed directly by placement variant 0..2.
extern AreaVariant gAkropolisPlazaAreaVariants[AKROPOLIS_PLAZA_AREA_VARIANT_COUNT];

/// Spawns the plaza's ambient glows, flares and siren beams on the first tick.
void akropolisPlazaSpawnAmbientLightsTask(Task* task);

/// Draws a stationary flickering or pulsing additive glow at a placed light.
void akropolisPlazaLightGlowTask(Task* task);

/// Sweeps a siren beam, its point light and its near and far additive glows.
void akropolisPlazaSirenLightTask(Task* task);

/// Sweeps a pulsing four-ray flare and a flickering glow ahead of a placed light.
void akropolisPlazaLightFlareTask(Task* task);

/// Services the plaza's queued play, reset and resume movie commands.
void akropolisPlazaPollStreamCommands(void);

#endif // INCLUDE_ROOMS_AKROPOLIS_PLAZA_H
