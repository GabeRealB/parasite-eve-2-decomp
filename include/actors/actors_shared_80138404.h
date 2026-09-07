#ifndef ACTORS_SHARED_80138404_H
#define ACTORS_SHARED_80138404_H

#include "common.h"

#include "main/task.h"

/// The actor's own three state handlers - spawn/setup, per-frame tick and
/// teardown. Every carrier holds its own table at its own address, named there
/// by the family's symbol maps, so the shared object owns no data.
extern TaskFuncTable3 ActorsShared80138404Table;

/// Runs the actor's current state handler, copying the table onto the stack
/// before the call. 15 actor overlays carry this body once and share this
/// object; the ones carrying it several times land it as plain C instead.
void ActorsShared80138404(Task* task);

#endif // ACTORS_SHARED_80138404_H
