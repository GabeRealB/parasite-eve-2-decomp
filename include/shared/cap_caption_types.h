#ifndef INCLUDE_SHARED_CAP_CAPTION_TYPES_H
#define INCLUDE_SHARED_CAP_CAPTION_TYPES_H

#include "main/task_types.h"

/* The stored callback takes a schedule argument. The task scheduler views the
 * same record as a TaskDesc; keep both views without casting its address. */
typedef union {
    TaskDesc tasks[1];
    struct {
        u16 flags;
        u16 priority;
        union {
            TaskFunc task;
            void     (*withArg)(Task*, s32);
        } callback;
        TaskSpawnArg arg;
    } native[1];
} CapCaptionTaskTable;
STATIC_ASSERT_SIZEOF(CapCaptionTaskTable, 12);

#endif // INCLUDE_SHARED_CAP_CAPTION_TYPES_H
