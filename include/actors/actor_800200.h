#ifndef INCLUDE_ACTORS_ACTOR_800200_H
#define INCLUDE_ACTORS_ACTOR_800200_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern TmdSource gFlintBody;

/// Flint's native companion clip bank, selected by gameplay's bank index 6.
///
/// The 47 base words contain set pointers or NULL. The following 32 words start
/// NULL and remain writable for extension-copy messages, including raw payload
/// words that are not clips. Playback may follow only entries containing sets.
/// Keep actor_800200 and its animation data loaded throughout playback and copying.
extern AnimationSet* gActor800200CompanionAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// Dispatches Flint's setup, active update, exit staging or teardown task state.
///
/// Requires Task::state in 0..3 and the loaded actor_800200 overlay. State 0
/// initializes the model/work used by later states; state 3 kills the task.
/// The resident bank-7 descriptor supplies the live TMD task and body resource.
void actor800200Task(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800200_H
