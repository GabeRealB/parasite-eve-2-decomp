#ifndef INCLUDE_ACTORS_ACTOR_120300_H
#define INCLUDE_ACTORS_ACTOR_120300_H

#include "main/task_types.h"

/// Live indices in Gary Douglas's garage task descriptor table, without a terminator.
enum {
    ACTOR_120300_TASK_BODY              = 0,
    ACTOR_120300_TASK_JUNK_YARD_REFUSAL = 1,
    ACTOR_120300_TASK_HEAD              = 2,
    ACTOR_120300_TASK_RIFLE             = 3,
    ACTOR_120300_TASK_FADE_OUT          = 4,
    ACTOR_120300_TASK_COUNT             = 5,
};

/// Gary Douglas's garage body, refusal scene, head/hat, rifle and fade spawn recipes.
///
/// All run at priority 192. Body, head and rifle allocate TMD bodies with
/// automatic primitive buffers disabled; their callbacks allocate the buffers.
/// The refusal and fade are bodyless. Head/rifle spawn argument 2 borrows the
/// live body task, which owns their teardown; fade argument 1's unsigned low
/// halfword is its intensity increment per tick (zero holds indefinitely).
/// Body/refusal arguments are unused. Spawn only a live
/// `ACTOR_120300_TASK_*` index. Keep actor_120300 and the garage/player resources
/// loaded while its callbacks, scripts or borrowed model/animation data are used.
extern TaskDesc gActor120300TaskDescriptors[ACTOR_120300_TASK_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_120300_H
