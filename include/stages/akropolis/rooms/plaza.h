#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_PLAZA_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_PLAZA_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// The clips the plaza places for its scenes, which police_officers's banks bind: the rookie officer's at the tower's
// entrance, the sergeant's at the barricade, and Aya's. Each is named for the line or step it plays on.

/// The officer's clips; the rest of his are police_officers's own.
extern AnimationSet gPoliceRookieStandClip;
extern AnimationSet gPoliceRookieLookAtBadgeClip;
extern AnimationSet gPoliceRookieShiftStanceClip;
extern AnimationSet gPoliceRookieIdleClip;
extern AnimationSet gPoliceRookieExplainSwatClip;

/// The sergeant's clips.
extern AnimationSet gPoliceSergeantClearedForEntryClip;
extern AnimationSet gPoliceSergeantGoRightInClip;
extern AnimationSet gPoliceSergeantTalkIdleClip;
extern AnimationSet gPoliceSergeantOuttaControlClip;
extern AnimationSet gPoliceSergeantOuttaControlGestureClip;
extern AnimationSet gPoliceSergeantHelicopterDownClip;
extern AnimationSet gPoliceSergeantSomethingBigClip;
extern AnimationSet gPoliceSergeantWatchYourselfClip;
extern AnimationSet gPoliceSergeantRaiseArmClip;
extern AnimationSet gPoliceSergeantLowerArmClip;
extern AnimationSet gPoliceSergeantListenClip;
extern AnimationSet gPoliceSergeantTurnHeadClip;
extern AnimationSet gPoliceSergeantTurnHeadBackClip;
extern AnimationSet gPoliceSergeantHeardOfThemClip;
extern AnimationSet gPoliceSergeantTurnedIdleClip;
extern AnimationSet gPoliceSergeantDamnRookiesClip;
extern AnimationSet gPoliceSergeantTurnAwayClip;
extern AnimationSet gPoliceSergeantHunterFromMistClip;
extern AnimationSet gPoliceSergeantIdleClip;

/// Aya's clips, with the idle her bank starts with.
extern AnimationSet gAyaIdleClip;
extern AnimationSet gAyaShowBadgeClip;
extern AnimationSet gAyaPutAwayBadgeClip;
extern AnimationSet gAyaTurnToSergeantClip;
extern AnimationSet gAyaFaceSergeantClip;
extern AnimationSet gAyaAskSituationClip;
extern AnimationSet gAyaTurnFromSergeantClip;
extern AnimationSet gAyaAskAboutCiviliansClip;
extern AnimationSet gAyaThankOfficerClip;
extern AnimationSet gAyaAskAboutRoofClip;
extern AnimationSet gAyaLookAtWreckClip;

/// The plaza's tasks in `gAkropolisPlazaSequenceTaskDescriptors`: the room's sequence, the traversal, each scene's
/// task and their presentation.
enum {
    AKROPOLIS_PLAZA_TASK_SEQUENCE = 0,
    AKROPOLIS_PLAZA_TASK_TRAVERSAL,
    AKROPOLIS_PLAZA_TASK_HELICOPTER_WRECK,
    AKROPOLIS_PLAZA_TASK_ENTRANCE,
    AKROPOLIS_PLAZA_TASK_SERGEANT,
    AKROPOLIS_PLAZA_TASK_OPENING_MOVIE_SETUP,
    AKROPOLIS_PLAZA_TASK_SERGEANT_AGAIN,
    AKROPOLIS_PLAZA_TASK_FADE_OUT,
    AKROPOLIS_PLAZA_TASK_FADE_IN,
    AKROPOLIS_PLAZA_TASK_CAPTION,
    AKROPOLIS_PLAZA_TASK_OPENING_MOVIE,
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

/// The plaza's tasks, spawned by index (`AKROPOLIS_PLAZA_TASK_*`).
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

/// Spawns the plaza's lights on its first frame, one per marker of `_gAkropolisPlazaAmbientLightPlacements`: the
/// police lights' six siren beams (markers 1..6), four rotating flares (7..10) and the street's glows (12 on).
void akropolisPlazaSpawnAmbientLightsTask(Task* task);

/// A glow at a light that does not move: a flickering yellow street lamp below marker 16, a pulsing red one from it.
void akropolisPlazaLightGlowTask(Task* task);

/// A rotating siren, blue below marker 5 and red from it: its beam sweeps round, with a glow at the lamp, a far glow
/// while the beam is long, and a point light on the scene while the lamp is on screen.
void akropolisPlazaSirenLightTask(Task* task);

/// A rotating light seen as a pulsing four-ray flare, blue below marker 9 and red from it, with a flickering glow
/// ahead of it.
void akropolisPlazaLightFlareTask(Task* task);

/// Main's CD queue handler for the offset-movie commands: plays or resets a movie at a sector offset, or resumes one
/// at a saved position, then polls it. Only the plaza issues these, so main calls into this overlay for them.
void akropolisPlazaPollStreamCommands(void);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_PLAZA_H
