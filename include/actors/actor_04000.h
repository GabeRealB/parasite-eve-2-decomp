#ifndef INCLUDE_ACTORS_ACTOR_04000_H
#define INCLUDE_ACTORS_ACTOR_04000_H

#include "main/task_types.h"

/// Blood Suckler TMD spawn descriptor; priority `TASK_PRIORITY_ACTOR` with automatic model buffers.
///
/// Spawn at index 0. Keep this package loaded while its callback and borrowed
/// model geometry are in use.
extern TaskDesc gActor04000BloodSucklerTaskDesc;

/// Blood Suckler hanging-pool controller, with no body, at priority 96.
///
/// Spawn at index 0 with unused arguments. It borrows hanging actor tasks,
/// keeps two drop slots filled during battle, and ends when candidates run out.
/// Keep the selected actor slot loaded while the controller and pool are live;
/// the night-toilet room imports the actor_104000 instance.
extern TaskDesc gActor04000HangingPoolTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_204000, with the same descriptor contract.
extern TaskDesc gActor204000BloodSucklerTaskDesc;

/// Slot export of `gActor04000BloodSucklerTaskDesc` from actor_304000, with the same descriptor contract.
extern TaskDesc gActor304000BloodSucklerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_04000_H
