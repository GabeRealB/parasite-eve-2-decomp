#ifndef SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
#define SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"

/// Gallery exit and abort decision-task descriptors (indices 0 and 1).
///
/// Bodyless priority-32 tasks wait for CAP replies and resolve session or room
/// transitions. Shared by gallery control fragments; spawn operands select the
/// exit operation or abort context. Keep the overlay loaded until they finish.
extern TaskDesc gActor215100GalleryDecisionTaskDescs[2];

/// Movement-triggered gallery exit gate (0 armed, 1 blocked).
///
/// Training startup and completed/aborted sessions block it. During an active
/// free-movement course, view 18 restores the firing barrier and rearms exit
/// confirmation by clearing it. Independent of the active-training latch.
extern s32 gActor215100GalleryExitInputBlocked;

/// Signed update-tick countdown before movement can request a gallery exit.
///
/// Active attachment modes 2/3 reload 60; each nonzero countdown is decremented
/// before exit checks and suppresses input for that update. Zero permits testing.
/// Retains signed behavior, including decrementing a negative nonzero value.
extern s32 gActor215100GalleryExitInputCooldownTicks;

extern AnimationSet gMistShootingGalleryPlayerGalleryAcceptClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIdleClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIntro6Clip;

extern AnimationSet gMistShootingGalleryPlayerGalleryIntro9Clip;

extern AnimationSet gMistShootingGalleryPlayerGalleryExcuseClip;

extern AnimationSet gMistShootingGalleryPlayerGalleryDeclineClip;

extern AnimationSet gAyaStandClip;

/// Waits for the accepted training offer's CAP cue before opening the course menu.
///
/// Three commands include END. The caller must already have selected the
/// acceptance caption; a three-frame wait follows its cue. Does not restore
/// HUD or player control. Both gallery overlays and CAP playback stay loaded.
extern EvsCommand gActor215100GalleryWaitTrainingAcceptanceScript[3];

/// Refusal dialogue and room restoration after declining gallery training.
///
/// Shared by the introduction and repeat-offer menu. Requires live gallery,
/// actor, player and CAP resources, with the refusal caption already selected.
/// Eighteen commands include the END sentinel; keep the script and its borrowed
/// requests loaded until completion. Installs the gallery player extension,
/// waits for CAP cues, returns to view 8, then restores HUD and player control.
extern EvsCommand gActor215100GalleryDeclineTrainingScript[18];

/// Stages Pierce and the player for direct entry to the training menus.
///
/// Nine commands include END. Hides the HUD, resets the player to clip 9,
/// selects menu view 15 and applies the ambient override. Requires live actors,
/// gallery CAP resources and an event session using `EVENT_SCRIPT_HUD_KEEP`.
extern EvsCommand gActor215100GalleryPrepareTrainingMenuScript[9];

/// Stages the repeat-entry training offer after its caption has been selected.
///
/// Thirteen commands include END. Installs gallery player clips, shows Pierce,
/// places both actors and waits for the offer's CAP cue. Holds view/HUD setup
/// for the menu's reply; both overlays and borrowed clip data stay loaded.
extern EvsCommand gActor215100GalleryOfferTrainingMenuScript[13];

/// Returns the player to fixed-view training for selected course levels 1..2.
///
/// Eight commands include END. Resets clip 1, places the player, selects view 14
/// and alternate room lights, then releases ambient and HUD suppression.
/// Scripted player control remains held for the course controller's handoff.
/// Requires the actor and gallery overlays and their live player/CAP tasks.
extern EvsCommand gActor215100GalleryStartFixedViewTrainingScript[8];

/// Returns the player to free movement for selected course levels 3..5.
///
/// Nine commands include END. Resets clip 1, removes the ambient override,
/// places the player in view 8, restores the HUD and ends scripted control.
/// Requires both gallery overlays and their live player/CAP tasks.
extern EvsCommand gActor215100GalleryStartFreeMovementTrainingScript[9];

/// Plays Pierce's response to the already-selected course-confirmation caption.
///
/// Six commands include END. Waits for a CAP cue, plays body clip 20 for
/// 35 frames, resets to clip 18 and waits for the next cue. Requires live
/// Pierce and CAP tasks and both overlays through playback.
extern EvsCommand gActor215100GalleryConfirmCourseScript[6];

/// Selected shooting-gallery training course, numbered 1..5.
///
/// Stored from the level-menu CAP reply before loadout setup and retained through
/// course completion or cancellation; it is not an active-session flag. The room
/// controller receives level minus one. Levels 1..2 use the fixed-view return
/// path, and movement exit checks apply to 3..5. Completion releases the
/// weapon-swap lock for 1..3; level 5 has the bonus-course abort path. The
/// zero-initialized word has no selected course until the menu stores a valid reply.
extern s32 gActor215100GallerySelectedCourseLevel;

/// Runs gallery briefing, music/weapon menus and the selected training handoff.
///
/// Start at state 0 with gallery CAP/scripts loaded and no active course.
/// spawnArg1 zero skips the repeat-entry confirmation; nonzero asks it, with
/// reply zero declining and other replies proceeding. The level reply must
/// be 1..5: it selects the loadout and becomes zero-based course spawnArg1.
/// Caption and event completion gate the workless task's states. The actor's
/// reply animations fire at countdown 0 and -22, with a retained -1000 floor.
/// Locks weapon swapping while staging training, then marks it live and ends.
void actor215100GalleryTrainingMenuTask(Task* task);

// Callbacks referenced by the overlay's shared data tables.
/// Resolves the gallery exit reply and restores training control or commits a room reload.
///
/// spawnArg1 is 0 to end training in place, 1 to confirm a retained room request,
/// or 2 to commit it without the first reply. Reply key 1 resumes training;
/// other replies end it. Deferred modes require the saved request and both
/// overlays to remain live until captions finish and the reload is queued.
/// The task has no work block and kills itself after its selected handoff.
void actor215100GalleryExitDecisionTask(Task* task);

/// Resolves abort confirmation for courses 1 and 2, then restores view and control.
///
/// Reply key 2 exits the live course controller, selects return view 8 on the
/// next tick and restores movement, weapon swapping and a held menu on the
/// following tick. Other replies only kill this decision task. Both overlays,
/// the player task and controller must remain live through the decision.
void actor215100GalleryAbortDecisionTask(Task* task);

#endif // SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
