#ifndef INCLUDE_ACTORS_ACTOR_450900_H
#define INCLUDE_ACTORS_ACTOR_450900_H

/// Handles the growth-room departure trigger using the companion's current root Z.
///
/// Below -1900 world units, spawns the departure dialogue task; otherwise starts
/// CAP command 11 with variant 0. Requires this actor package, the growth room's
/// CAP resources, and a live companion TMD with up-to-date root coordinates.
/// The package and playback resources must remain loaded until the task ends.
/// Spawn failure is ignored; overlapping calls can queue several departures.
void actor450900HandleDepartureTrigger(void);

#endif // INCLUDE_ACTORS_ACTOR_450900_H
