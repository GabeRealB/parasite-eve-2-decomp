#ifndef ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
#define ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"

/// The room's three-state table for its event task: open the message table,
/// idle, then kill the task.
extern const TaskFuncTable3 D_dryfield_night_breezeway_8017D5C4;

/// The room's message table: 0x13EE, 0x13F1, 0x13EF and 0x13F0 to their
/// handlers, terminated by 0x7FFFFFFF.
extern GpMsgEntry D_dryfield_night_breezeway_8017E67C[];

void func_dryfield_night_breezeway_8017D634(Task* task);
void func_dryfield_night_breezeway_8017D680(Task* task);
void func_dryfield_night_breezeway_8017D6D8(SVECTOR* arg0, s16 arg1, s32 arg2);
void func_dryfield_night_breezeway_8017DB4C(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_breezeway_8017E334(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_BREEZEWAY_H
