#ifndef INCLUDE_ACTORS_ACTOR_01200_H
#define INCLUDE_ACTORS_ACTOR_01200_H

#include "main/task_types.h"

/// Bone Suckler TMD descriptor shared by the three actor-slot builds.
///
/// Priority `TASK_PRIORITY_ACTOR` with automatic model buffers; spawn at index 0. Room imports
/// use the corresponding slot alias and must keep that slot's resources loaded.
extern TaskDesc gActor01200BoneSucklerTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 1.
///
/// Spawn at index 0; priority `TASK_PRIORITY_ACTOR` with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor101200BoneSucklerTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 2.
///
/// Spawn at index 0; priority `TASK_PRIORITY_ACTOR` with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor201200BoneSucklerTaskDesc;

/// Bone Suckler TMD spawn descriptor exported by actor slot 3.
///
/// Spawn at index 0; priority `TASK_PRIORITY_ACTOR` with automatic model buffers. Keep the
/// slot's callback and borrowed model geometry loaded while its tasks run.
extern TaskDesc gActor301200BoneSucklerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_01200_H
