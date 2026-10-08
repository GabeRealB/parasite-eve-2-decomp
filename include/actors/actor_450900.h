#ifndef INCLUDE_ACTORS_ACTOR_450900_H
#define INCLUDE_ACTORS_ACTOR_450900_H

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
