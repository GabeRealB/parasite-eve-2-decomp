#ifndef INCLUDE_ACTORS_ACTOR_160600_H
#define INCLUDE_ACTORS_ACTOR_160600_H

#include "main/task_types.h"

#include "gameplay/evs.h"

/// Sterilization-room entry event when no companion is present.
///
/// Runs the shared opening and conversation, then CAP command 12's departure
/// and normal restoration. Start with `evsStartScriptWithSkip` and
/// `gActor160600SceneSkipScript`. Requires the player, placed actor 0, CAP
/// command slots 11..12 and their room resources. Keep this actor package loaded
/// through the event and while its installed player animation clips are used.
/// The 29 commands terminate with END.
extern EvsCommand gActor160600SceneWithoutCompanionScript[29];

/// Sterilization-room entry event with the companion present.
///
/// Adds the companion's opening run and CAP command 13's exchange to the
/// shared opening, conversation and restoration. Start with
/// `evsStartScriptWithSkip` and `gActor160600SceneSkipScript`. Requires live
/// player, companion, placed actor 0 and CAP command slots 11 and 13.
/// Keep this actor package and the room resources loaded through the event
/// and while either actor uses its installed scene clips.
/// The 52 commands terminate with END.
extern EvsCommand gActor160600SceneWithCompanionScript[52];

/// Skip target restoring the sterilization room after either entry-event variant.
///
/// A primary fade covers scene cleanup, view 6, companion dismissal, weapon
/// restoration and player/placed-actor placement. Restores player control and
/// terminates the event with END, bypassing pending script returns. Requires
/// this actor package and the same live room/player resources as the event.
/// The complete skip path has 16 commands.
extern EvsCommand gActor160600SceneSkipScript[16];

/// Twenty-part model task for the sterilization room's placed scene actor.
///
/// Creates a TMD body at priority `TASK_PRIORITY_ACTOR` and runs the paced walker's spawn/frame
/// states. Scene placement supplies a live owning `Enemy` through `spawnArg2.pointer`.
/// The `actor_160600` model, clips and callbacks must remain loaded until teardown.
extern TaskDesc gActor160600PlacedActorTask;

#endif // INCLUDE_ACTORS_ACTOR_160600_H
