#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_H

#include "types.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Live gallery training-session latch (0 inactive, 1 active).
extern s32 gShootingGalleryPierceGalleryTrainingActive;

/// Descriptor slots for the gallery's introduction, training menu and red-flash loop.
enum {
    SHOOTING_GALLERY_PIERCE_GALLERY_INTRO_TASK          = 0,
    SHOOTING_GALLERY_PIERCE_GALLERY_MENU_TASK           = 1,
    SHOOTING_GALLERY_PIERCE_GALLERY_RED_FLASH_LOOP_TASK = 2,
    SHOOTING_GALLERY_PIERCE_GALLERY_TASK_COUNT          = 3,
};

/// Bodyless gallery event tasks, indexed by `SHOOTING_GALLERY_PIERCE_GALLERY_*_TASK`.
extern TaskDesc gShootingGalleryPierceGalleryTaskDescs[SHOOTING_GALLERY_PIERCE_GALLERY_TASK_COUNT];

/// Descriptor slots for Pierce's body and its attached paired model.
enum {
    SHOOTING_GALLERY_PIERCE_PIERCE_BODY_TASK  = 0,
    SHOOTING_GALLERY_PIERCE_PIERCE_PAIR_TASK  = 1,
    SHOOTING_GALLERY_PIERCE_PIERCE_TASK_COUNT = 2,
};

/// Pierce's body and paired-model spawn descriptors.
extern TaskDesc gShootingGalleryPiercePierceTaskDescs[SHOOTING_GALLERY_PIERCE_PIERCE_TASK_COUNT];

/// Pierce's one-time story scene at shooting-gallery action point 4.
extern EvsCommand gShootingGalleryPiercePierceStoryScript[117];

/// Restores the gallery after skipping `gShootingGalleryPiercePierceStoryScript`.
extern EvsCommand gShootingGalleryPiercePierceStorySkipScript[15];

/// Gallery exit-gate replies to the room-transition resolver.
enum {
    SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_ALLOW = 1,
    SHOOTING_GALLERY_PIERCE_GALLERY_EXIT_DEFER = 2,
};

/// Checks movement input near the gallery exit and starts a training-exit prompt.
void shootingGalleryPierceCheckGalleryExitInput(void);

/// Ends the completed gallery course's interaction and restores its barrier or view.
void shootingGalleryPierceFinishGalleryCourse(void);

/// Requests course-dependent gallery cancellation or abort confirmation.
void shootingGalleryPierceRequestGalleryAbort(void);

/// Resolves a gallery exit request, deferring it through confirmation when needed.
s32 shootingGalleryPierceResolveGalleryExit(const RoomEventMsg* request);

/// Handles the gallery's action point by signaling training or starting its CAP event.
void shootingGalleryPierceHandleGalleryAction(void);

/// Selects the gallery caption CAP payload and its font texture-page origin.
void shootingGalleryPierceSelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

/// Selects a keyed caption record and caches its screen-pixel text layout.
s32 shootingGalleryPierceCapCaptionSelectRecord(s16 commandIndex, s16 key, s32 bottomBaselineY);

/// Starts Pierce's next shooting-gallery conversation from persistent talk progress.
void shootingGalleryPierceStartPierceConversation(void);

/// Queues the actor's selected caption, background, title and continuation caret.
void shootingGalleryPierceCapCaptionDrawCurrent(void);

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_H
