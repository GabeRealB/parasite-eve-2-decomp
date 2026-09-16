#ifndef ACTOR_223600_H
#define ACTOR_223600_H

#include "common.h"

#include "main/task.h"

/// The actor's per-instance work block, reached through `Task::idMap`. Only
/// the fields the decompiled message handlers touch are modelled so far: the
/// 16-bit state word at 0x0 that every handler drives, and the three bytes at
/// 0x180 that a handler copies out of the event packet.
typedef struct Actor223600Work {
    /* 0x000 */ s16  field_0; ///< state
    /* 0x002 */ byte pad_2[0x17E];
    /* 0x180 */ u8   field_180;
    /* 0x181 */ u8   field_181;
    /* 0x182 */ u8   field_182;
} Actor223600Work;

/// Event packet handed to this actor's message handlers. Its first three bytes
/// are copied into the work block, and its first four are then re-read as two
/// little-endian `u16` words: a command word and a sub-command.
typedef union Actor223600Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor223600Event;
STATIC_ASSERT_SIZEOF(Actor223600Event, 0x4);

s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2);
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event);

#endif // ACTOR_223600_H
