#ifndef SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H
#define SRC_ACTORS_ACTOR_215100_ACTOR_215100_PRIVATE_H

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/evs.h"

#include "main/task_types.h"

extern TaskDesc D_actor_215100_8014CF6C[2];

extern s32 D_actor_215100_8014D038;

extern s32 D_actor_215100_8014D03C;

extern s32 D_actor_215100_8014D044;

extern AnimationSet gActor215100Animation034E4;

extern AnimationSet gActor215100Animation03754;

extern AnimationSet gActor215100Animation039AC;

extern AnimationSet gActor215100Animation03B48;

extern AnimationSet gActor215100Animation03D98;

extern AnimationSet gActor215100Animation03FF0;

extern AnimationSet gActor215100Animation042F4;

extern EvsCommand D_actor_215100_8014EB98[3];

extern EvsCommand D_actor_215100_8014EBE0[18];

extern EvsCommand D_actor_215100_8014ED90[9];

extern EvsCommand D_actor_215100_8014EE68[13];

extern EvsCommand D_actor_215100_8014EFA0[8];

extern EvsCommand D_actor_215100_8014F060[9];

extern EvsCommand D_actor_215100_8014F138[6];

extern s32 D_actor_215100_8015E670;

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
