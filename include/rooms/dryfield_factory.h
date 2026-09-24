#ifndef ROOMS_DRYFIELD_FACTORY_H
#define ROOMS_DRYFIELD_FACTORY_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// The room's handler table for its script task: the seven states the task
/// runs through `func_dryfield_factory_8018169C`.
extern const TaskFuncTable7 D_dryfield_factory_8017D678;

/// The room's event gate: answers 1 when the request's flag says the event
/// already happened, 0 (after running the request's cap command) when its
/// item prerequisite is missing, and otherwise latches the request, writes
/// the flag and spawns the room's event task, for 2. A non-zero `field_5` on
/// the message only asks for the answer.
s32 func_dryfield_factory_8017D6F8(RoomEventReq* req, RoomEventMsg* msg);

/// Model handlers the factory model's per-frame state picks by progress
/// nibble 0x49. Each returns non-zero once its movement has finished.
s32 func_dryfield_factory_8017E33C(Task* task);
s32 func_dryfield_factory_8017E6BC(Task* task);
s32 func_dryfield_factory_8017EA24(Task* task);
s32 func_dryfield_factory_8017ED68(Task* task);

/// Kills the task; the factory model's exit callback.
void func_dryfield_factory_80180430(Task* task);

/// Binds the model to the light and colour matrices in the task's work block
/// and rebuilds its lighting.
void func_dryfield_factory_80180450(Task* task);

/// Sends message 0x13F3 to `task`, if there is one.
void func_dryfield_factory_801804B0(Task* task);

/// Runs cap step `step` of the room's script, picking the sound, the progress
/// flags and the cap slot for the step.
void func_dryfield_factory_80180DE8(Task* task, s16 step);

/// Moves both action-prompt cursors from the pads and draws them.
void func_dryfield_factory_801810D8(Task* task);

/// Shows (non-zero) or hides (zero) the second sprite command of view 9 of the
/// current room, in stage 2 only.
void func_dryfield_factory_80181620(s32 show);

/// Marks every hotspot of `table` under (`x`, `y`) as hit; answers whether any
/// was.
s32 func_dryfield_factory_80181778(RoomHotspot* table, s16 x, s16 y);

/// As `func_dryfield_factory_80181620`, for view 11.
void func_dryfield_factory_80181B38(s32 show);

/// Resets both action-prompt slots and steps the caller on one state.
void func_dryfield_factory_80181BB4(Task* task);

/// Draws a tinted, flickering glow disc at the world-space point `pos`.
void func_dryfield_factory_80181C14(SVECTOR* pos, s32 size, s32 tint);

#endif // ROOMS_DRYFIELD_FACTORY_H
