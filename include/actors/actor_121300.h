#ifndef INCLUDE_ACTORS_ACTOR_121300_H
#define INCLUDE_ACTORS_ACTOR_121300_H

#include "main/task_types.h"

/// Spawn recipes for Eve's Dryfield lamp-shattering scene and its children.
///
/// Indices 0..10 are the double, fade in, fade out, debris spawner, five debris
/// models, blackout and texture uploader. There is no terminal descriptor.
/// All run at priority `TASK_PRIORITY_CONTROL`; the body and debris defer automatic model buffers.
/// The room loads actor_121300 as resource 132 before spawning the double at 0.
/// Debris arguments select lamp 0..11 and placement 0..14; texture uploads
/// borrow their live double in argument 2. Keep the actor and room resources
/// loaded until every child and borrowed scene callback has finished.
extern TaskDesc gActor121300SceneTasks[11];

#endif // INCLUDE_ACTORS_ACTOR_121300_H
