#ifndef INCLUDE_ACTORS_ACTOR_450900_H
#define INCLUDE_ACTORS_ACTOR_450900_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Spawn-table indices for the growth-room companion scene tasks.
enum {
    ACTOR_450900_TASK_EXIT               = 0,
    ACTOR_450900_TASK_COMPANION_DISTRESS = 1,
    ACTOR_450900_TASK_PLAYER_REACTION    = 2,
    ACTOR_450900_TASK_CHOICE_ACTION      = 3,
    ACTOR_450900_TASK_DEPARTURE          = 4,
    ACTOR_450900_TASK_HEAD_AIM           = 5,
    ACTOR_450900_TASK_COUNT              = 6,
};

/// Task recipes for the growth-room distress, choice, departure and head aiming.
///
/// Indices are `ACTOR_450900_TASK_*`; all recipes allocate no task body and
/// use zero descriptor data. Keep this package loaded until spawned tasks end.
/// Head aiming also requires live player and companion models.
extern TaskDesc gActor450900Tasks[ACTOR_450900_TASK_COUNT];

/// Skippable growth-room entry scene that installs clips and places both actors.
///
/// Starts CAP command 10, coordinates views 7 and 2 with its cues, then restores
/// player control. Requires this package, room CAP resources and live actors
/// through playback; installed clip data must stay loaded while actors use it.
extern EvsCommand gActor450900EntryScript[];

/// Entry-scene skip target that restores actor placement, view and player control.
///
/// Fades across cleanup and settles the player on its native idle animation.
/// Use with `gActor450900EntryScript`; keep the package and live actors through
/// execution. The commands end with `EVENT_SCRIPT_OPCODE_END`.
extern EvsCommand gActor450900EntrySkipScript[];

/// Starts the growth-room companion conversation selected by saved progress.
///
/// Before the confirming choice, distress elapsed updates below 780 select
/// CAP dialogue 3 and later updates select dialogue 4 through event scripts.
/// After that choice, the first call starts dialogue 5 and latches it as seen;
/// later calls request CAP command 12 only while playback is idle, pausing actors.
/// Requires this package, the growth-room CAP resources and live player/companion
/// models through playback. Event scripts hide the HUD and restore it on exit.
void actor450900StartCompanionConversation(void);

/// Handles the growth-room departure trigger using the companion's current root Z.
///
/// Below -1900 world units, spawns the departure dialogue task; otherwise starts
/// CAP command 11 with variant 0. Requires this actor package, the growth room's
/// CAP resources, and a live companion TMD with up-to-date root coordinates.
/// The package and playback resources must remain loaded until the task ends.
/// Spawn failure is ignored; overlapping calls can queue several departures.
void actor450900HandleDepartureTrigger(void);

#endif // INCLUDE_ACTORS_ACTOR_450900_H
