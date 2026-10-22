#ifndef INCLUDE_ACTORS_ACTOR_800300_H
#define INCLUDE_ACTORS_ACTOR_800300_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models those descriptors attach.
extern TmdSource gEveCompanionBody;

/// Native companion clip bank selected by gameplay's companion bank index 7.
///
/// Holds 47 base clip words and a writable 32-word extension, initially NULL.
/// Loaded base slots contain set pointers or NULL; copied extension words may
/// also carry non-pointer payloads. Follow only slots known to contain clips.
/// Playback and extension copies require actor_800300 and its sets to stay loaded.
extern AnimationSet* gActor800300CompanionAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Runs the noncombatant companion's initialization, frame update and teardown states.
///
/// The resident model-task descriptor supplies the model; spawning supplies
/// zeroed GameActor/CompanionWork storage and loaded native animation resources.
/// Task state must be 0..3. Initialization publishes the companion task and
/// borrows its work/model coordinates for collision until teardown unlinks them.
void actor800300Task(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800300_H
