#ifndef GAMEPLAY_1BC_H
#define GAMEPLAY_1BC_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x60-byte enemy work object allocated by `func_800B0494`
/// ("new_enemy ---> NULL"). Stored in `Task::spawnArg2`; `task` back-points
/// at the owner. `node` is the `func_800DAB38` list entry at +0x10.
typedef struct _GpEnemy {
    /* 0x00 */ Task*      task;
    /* 0x04 */ byte       pad_4[0x8];
    /* 0x0C */ s32        field_C;
    /* 0x10 */ GpLinkNode node;
} GpEnemy;

/// Source object for `func_800B3CCC` / `func_800B3F60`. Word at 0x30 is
/// copied into the dest context; the address of 0x34 is stored as dest
/// `field_4` (base of 0x50-byte records in `func_800B3448`).
typedef struct _GpAnimObj {
    /* 0x00 */ byte  pad_0[0x30];
    /* 0x30 */ void* field_30;
    /* 0x34 */ byte  field_34;
} GpAnimObj;

/// 0x14-byte context filled by `func_800B3CCC` (no `field_C`) and
/// `func_800B3F60` (also writes `field_C`). Nearby helpers index
/// `field_C` as a 0x28-byte slot array and `field_8` at a 0x10 stride.
typedef struct _GpAnimCtx {
    /* 0x00 */ void* field_0;
    /* 0x04 */ void* field_4;
    /* 0x08 */ void* field_8;
    /* 0x0C */ void* field_C;
    /* 0x10 */ void* field_10;
} GpAnimCtx;
STATIC_ASSERT_SIZEOF(GpAnimCtx, 0x14);

GpEnemy* func_800B0168(s32 bank, s32 type, s32 arg2, GpEnemy* parent);
GpEnemy* func_800B01AC(TaskDesc* table, s32 idx, s32 arg2, GpEnemy* parent);
void     func_800B01F0(GpEnemy* enemy, Task* task);
void     func_800B0234(Task* task);
GpEnemy* func_800B0494(Task* task, GpEnemy* parent);
void     func_800B0544(GpEnemy* enemy, Task* task);
void     func_800B3CCC(GpAnimCtx* arg0, void* arg1, GpAnimObj* arg2, void* arg3);
void     func_800B58D4(TmdObject* arg0, s32 arg1, s32 arg2);

#endif // GAMEPLAY_1BC_H
