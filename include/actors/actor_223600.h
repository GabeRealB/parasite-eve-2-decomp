#ifndef ACTOR_223600_H
#define ACTOR_223600_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's per-instance work block, reached through `Task::idMap`. Only
/// the fields the decompiled message handlers touch are modelled so far: the
/// state word at 0x0 that every handler drives, the animation pair at
/// 0x4A/0x58 the state dispatchers read, the motion state at 0x174, the three
/// bytes at 0x180 that a handler copies out of the event packet and the flag
/// at 0x20C the tick latches while the model is parked.
typedef struct Actor223600Work {
    /* 0x000 */ s16  field_0;  ///< state
    /* 0x002 */ s16  field_2;  ///< state at the previous dispatch
    /* 0x004 */ s16  field_4;  ///< set when `field_0` moved away from `field_2`
    /* 0x006 */ byte pad_6[0x44];
    /* 0x04A */ u16  field_4A; ///< low ten bits: current animation id
    /* 0x04C */ byte pad_4C[0xC];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x11A];
    /* 0x174 */ s16  field_174; ///< motion state
    /* 0x176 */ byte pad_176[0xA];
    /* 0x180 */ u8   field_180;
    /* 0x181 */ u8   field_181;
    /* 0x182 */ u8   field_182;
    /* 0x183 */ byte pad_183[0x85];
    /* 0x208 */ u16  field_208; ///< animation id that last raised the reaction
    /* 0x20A */ byte pad_20A[0x2];
    /* 0x20C */ s8   field_20C; ///< 1 while the model's coordinate is zeroed
} Actor223600Work;

/// Event packet handed to this actor's message handlers. Its first three bytes
/// are copied into the work block, and its first four are then re-read as two
/// little-endian `u16` words: a command word and a sub-command.
typedef union Actor223600Event {
    /* 0x0 */ u8  bytes[4];
    /* 0x0 */ u16 words[2];
} Actor223600Event;
STATIC_ASSERT_SIZEOF(Actor223600Event, 0x4);

/// Game mode word the tick switches on: 0 and 1 drive the model's `field_C`
/// from the work block, 2 forces 0x80.
extern u8 D_801153F4;

/// The three state handlers `func_actor_223600_8014CA00` copies onto its stack
/// before the indirect call, in the order the dispatcher indexes them: entry 0
/// is the shared idle handler, 1 the show handler and 2 the hide handler. The
/// entries are `GpEnemyTaskFunc`s, so the tick is an `ActorsShared80135df4`
/// handler alongside `func_actor_223600_8014B540` and `Gp_DestroyEnemy` in
/// `ActorsShared80135df4Table`.
extern const GpEnemyTaskFuncTable3 D_actor_223600_80149E4C;

s32  func_actor_223600_8014B464(Actor223600Work* arg0);
void func_actor_223600_8014B840(GpEnemy* enemy, Task* task);
void func_actor_223600_8014BBF4(GpEnemy* enemy, Task* task);
void func_actor_223600_8014CA00(GpEnemy* enemy, Task* task);
s32  func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2);
s32  func_actor_223600_8014CCD4(Task* task, s32 arg1, Actor223600Event* event);

#endif // ACTOR_223600_H
