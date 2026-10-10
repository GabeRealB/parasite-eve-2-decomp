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

extern s32 D_actor_215100_8014D044;

extern AnimationSet gActor215100Animation034E4;

extern AnimationSet gActor215100Animation03754;

extern AnimationSet gActor215100Animation039AC;

extern AnimationSet gActor215100Animation03B48;

extern AnimationSet gActor215100Animation03D98;

extern AnimationSet gActor215100Animation03FF0;

extern AnimationSet gActor215100Animation042F4;

extern EvsCommand D_actor_215100_8014EB98[3];

/// Refusal dialogue and room restoration after declining gallery training.
///
/// Shared by the introduction and repeat-offer menu. Requires live gallery,
/// actor, player and CAP resources, with the refusal caption already selected.
/// Eighteen commands include the END sentinel; keep the script and its borrowed
/// requests loaded until completion. Installs the gallery player extension,
/// waits for CAP cues, returns to view 8, then restores HUD and player control.
extern EvsCommand gActor215100GalleryDeclineTrainingScript[18];

extern EvsCommand D_actor_215100_8014ED90[9];

extern EvsCommand D_actor_215100_8014EE68[13];

extern EvsCommand D_actor_215100_8014EFA0[8];

extern EvsCommand D_actor_215100_8014F060[9];

extern EvsCommand D_actor_215100_8014F138[6];

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
