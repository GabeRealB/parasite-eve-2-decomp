#ifndef STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_SHOOTING_GALLERY_PIERCE_PRIVATE_H
#define STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_SHOOTING_GALLERY_PIERCE_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"

/// Gallery exit and abort decision-task descriptors (indices 0 and 1).
extern TaskDesc gShootingGalleryPierceGalleryDecisionTaskDescs[2];

/// Movement-triggered gallery exit gate (0 armed, 1 blocked).
extern s32 gShootingGalleryPierceGalleryExitInputBlocked;

/// Signed update-tick countdown before movement can request a gallery exit.
extern s32 gShootingGalleryPierceGalleryExitInputCooldownTicks;

extern AnimationSet gMistShootingGalleryPlayerGalleryAcceptClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIdleClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIntro6Clip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIntro9Clip;

extern AnimationSet gMistShootingGalleryPlayerGalleryExcuseClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryDeclineClip;

extern AnimationSet gAyaStandClip;

/// Waits for the accepted training offer's CAP cue before opening the course menu.
extern EvsCommand gShootingGalleryPierceGalleryWaitTrainingAcceptanceScript[3];

/// Refusal dialogue and room restoration after declining gallery training.
extern EvsCommand gShootingGalleryPierceGalleryDeclineTrainingScript[18];

/// Stages Pierce and the player for direct entry to the training menus.
extern EvsCommand gShootingGalleryPierceGalleryPrepareTrainingMenuScript[9];

/// Stages the repeat-entry training offer after its caption has been selected.
extern EvsCommand gShootingGalleryPierceGalleryOfferTrainingMenuScript[13];

/// Returns the player to fixed-view training for selected course levels 1..2.
extern EvsCommand gShootingGalleryPierceGalleryStartFixedViewTrainingScript[8];

/// Returns the player to free movement for selected course levels 3..5.
extern EvsCommand gShootingGalleryPierceGalleryStartFreeMovementTrainingScript[9];

/// Plays Pierce's response to the already-selected course-confirmation caption.
extern EvsCommand gShootingGalleryPierceGalleryConfirmCourseScript[6];

/// Selected shooting-gallery training course, numbered 1..5.
extern s32 gShootingGalleryPierceGallerySelectedCourseLevel;

/// Runs gallery briefing, music/weapon menus and the selected training handoff.
void shootingGalleryPierceGalleryTrainingMenuTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Resolves the gallery exit reply and restores training control or commits a room reload.
void shootingGalleryPierceGalleryExitDecisionTask(Task* task);

/// Resolves abort confirmation for courses 1 and 2, then restores view and control.
void shootingGalleryPierceGalleryAbortDecisionTask(Task* task);

/// Pierce's index among the gallery's placed actors, which scripts and the menu address him by.
enum { PIERCE_PLACE_INDEX = 1 };

#endif // STAGES_AKROPOLIS_ACTORS_SHOOTING_GALLERY_PIERCE_SHOOTING_GALLERY_PIERCE_PRIVATE_H
