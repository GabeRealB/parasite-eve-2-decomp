#ifndef ACTORS_SHARED_801436CC_H
#define ACTORS_SHARED_801436CC_H

#include "common.h"

#include "main/task.h"

/// Per-frame refresh of the task model's texture page and CLUT row: the area
/// key is copied off the session, synced, and the nested area record indexed
/// by the parent's work-object id (`GpWorkObj::field_8 >> 12`) supplies the
/// two bytes. The TMD stream is walked twice when the aux buffer exists, and
/// the task's own state counter is bumped.
void ActorsShared801436cc(void* arg0, Task* task);

#endif
