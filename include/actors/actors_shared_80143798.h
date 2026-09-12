#ifndef ACTORS_SHARED_80143798_H
#define ACTORS_SHARED_80143798_H

#include "common.h"

#include "main/task.h"

/// Rebuilds the task model's root world matrix and reads the resulting
/// world-space translation back into a local the function never uses.
void ActorsShared80143798(void* arg0, Task* arg1);

#endif
