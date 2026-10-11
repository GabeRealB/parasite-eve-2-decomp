#ifndef INCLUDE_ACTORS_ACTOR_142900_H
#define INCLUDE_ACTORS_ACTOR_142900_H

#include "gameplay/evs.h"

/// First-visit Shelter B2 elevator scene with player/companion choreography and screen shake.
///
/// The room starts this with HUD hide/restore and the paired skip script.
/// Installs scene clips, follows CAP cues, operates the door, then reloads the
/// elevator hall and releases both actors. All 87 commands include END.
/// Keep actor_142900, the elevator room, both actors and CAP resources loaded
/// while the interpreter borrows commands, callbacks and animation data.
extern EvsCommand gActor142900ElevatorFirstVisitScript[87];

/// Recovery path for skipping the first-visit Shelter B2 elevator scene.
///
/// Under the primary fade, stops shaking, runs the short CAP continuation,
/// reloads the hall, restores player placement and both actors' weapon effects,
/// and releases scripted control. All 18 commands include END. The same room,
/// actor and CAP resources as the normal path must remain loaded through END.
extern EvsCommand gActor142900ElevatorFirstVisitSkipScript[18];

#endif // INCLUDE_ACTORS_ACTOR_142900_H
