#ifndef GAMEPLAY_1BC_H
#define GAMEPLAY_1BC_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// 0x60-byte enemy work object allocated by `func_800B0494`
/// ("new_enemy ---> NULL"). Stored in `Task::spawnArg2`; `task` back-points
/// at the owner. `node` is the `func_800DAB38` list entry at +0x10.
typedef struct _GpEnemy {
    /* 0x00 */ Task*      task;
    /* 0x04 */ byte       pad_4[0xC];
    /* 0x10 */ GpLinkNode node;
} GpEnemy;

void func_800B01F0(GpEnemy* enemy, Task* task);

#endif // GAMEPLAY_1BC_H
