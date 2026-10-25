#ifndef INCLUDE_ACTORS_ACTOR_143900_H
#define INCLUDE_ACTORS_ACTOR_143900_H

#include "main/task_types.h"

#include "gameplay/evs.h"

/// Shelter R49 entry scene leading to the Neo Ark observatory transition.
///
/// The 32 commands include the END record. CAP command 1 supplies the cue waits;
/// the script holds the player's equipped-bank idle and plays walker clips 1..4.
/// It restores ambient lighting and weapons before requesting the room movie
/// and transition. Borrowed requests, callbacks and CAP/room resources must stay
/// loaded while the interpreter runs; demo scene 9 does not start the scene.
extern EvsCommand gActor143900ObservatoryEntryScript[32];

/// Skip path for the observatory entry scene, including its END record.
///
/// Twelve commands stop scene music, fade out and release the scene, restore
/// weapons and ambient lighting, then request the same movie/transition. After
/// the transition wait it fades in and resumes player control. Keep both actor
/// and room resources loaded while this borrowed command table is running.
extern EvsCommand gActor143900ObservatoryEntrySkipScript[12];

/// First scripted walker TMD recipe, priority `TASK_PRIORITY_CONTROL` with automatic model buffers.
///
/// Spawn at index 0 with its live Enemy owner in spawn argument 2. Keep
/// actor_143900 geometry, clips and callbacks loaded throughout the singleton walker's life.
extern TaskDesc gActor143900ScriptedWalkerTaskDesc;

/// Second scripted walker spawn recipes: 0 body, 1 attachment 1, 2 attachment 2.
///
/// All are TMD bodies at priority `TASK_PRIORITY_CONTROL`, without a terminator. Body placement
/// supplies a live Enemy in spawn argument 2. Attachment spawn argument 1
/// names the borrowed body part (1 or 12); both attachment tasks and the
/// singleton body require actor_143900 resources to remain loaded.
extern TaskDesc gActor143900SecondScriptedWalkerTaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_143900_H
