#ifndef GAMEPLAY_SCENE_TASKS_H
#define GAMEPLAY_SCENE_TASKS_H

#include "common.h"

#include "main/task_types.h"

/// Task descriptor view used by `func_800E31E8` to compare flags and priority
/// as one word. The setup argument encodes a stage/area or stage/area/room key.
typedef union _GpTaskDesc {
    TaskDesc task;
    s32      flagsAndPriority;
} GpTaskDesc;
STATIC_ASSERT_SIZEOF(GpTaskDesc, 0xC);

#endif // GAMEPLAY_SCENE_TASKS_H
