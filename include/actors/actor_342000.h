#ifndef INCLUDE_ACTORS_ACTOR_342000_H
#define INCLUDE_ACTORS_ACTOR_342000_H

#include "main/task_types.h"

/// Public entry index and complete extent of the incinerator exit-event spawn table.
enum {
    ACTOR_342000_TASK_EVENT       = 0,
    ACTOR_342000_EVENT_TASK_COUNT = 10,
};

/// Incinerator exit recipes: event 0, fade 1, Glutton body 2, parts 3..7, doors 8..9.
///
/// All run at priority `TASK_PRIORITY_CONTROL`, without a terminator. Event and fade are bodyless;
/// model entries skip automatic primitive-buffer allocation. Start entry 0 at
/// state zero with zero spawn arguments and the room's placed encounter actor
/// live. Body argument 1 is part id 0 and argument 2 is the event parent; parts
/// use ids 1..5 (right leg, left leg, then three fixed parts) and the body parent.
/// Doors borrow the event parent in argument 2; nonzero argument 1 adds depth
/// bias for the timed close. Fade argument 1 is unsigned-halfword intensity
/// removed per tick. Keep actor_342000, actor_444000's model/animation resources
/// and the incinerator room/CAP scene loaded through task and script teardown.
extern TaskDesc gActor342000IncineratorEventTaskDescs[ACTOR_342000_EVENT_TASK_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_342000_H
