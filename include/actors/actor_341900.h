#ifndef INCLUDE_ACTORS_ACTOR_341900_H
#define INCLUDE_ACTORS_ACTOR_341900_H

#include "main/task_types.h"

/// Public entry index and complete extent of the Glutton entrance spawn table.
enum {
    ACTOR_341900_TASK_EVENT          = 0,
    ACTOR_341900_ENTRANCE_TASK_COUNT = 10,
};

/// Glutton entrance recipes: event 0, fade 1, body 2, five attached parts 3..7, doors 8..9.
///
/// All run at priority `TASK_PRIORITY_CONTROL`, without a terminator. The event and fade are
/// bodyless; model entries skip automatic primitive-buffer allocation. The
/// incinerator room starts entry 0 with unused zero spawn arguments. Internally,
/// body argument 1 is part id 0 and argument 2 is the event parent; child parts
/// use ids 1..5 and the body parent. Doors borrow the event parent in argument 2.
/// Fade argument 1 supplies unsigned-halfword intensity units per tick.
/// Keep actor_341900, actor_444000's borrowed model/animation resources and the
/// selected room/CAP scene loaded while their tasks and script callbacks run.
extern TaskDesc gActor341900GluttonEntranceTaskDescs[ACTOR_341900_ENTRANCE_TASK_COUNT];

#endif // INCLUDE_ACTORS_ACTOR_341900_H
