#ifndef KYLE_800102_H
#define KYLE_800102_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/weapon.h"

/// One entry of the task's state table; the dispatcher passes the task itself.
typedef void (*Kyle800102StateFn)(Task* task);

/// Launch offset per attachment index, in the muzzle coordinate's local space.
extern SVECTOR D_kyle_800102_80177424[2];
/// Launch speed per attachment index, shifted left 16 into `field_88`.
extern u8 D_kyle_800102_8017743C[4];
/// Impact clip id per attachment, indexed by `sfx - 0xA`.
extern u16 D_kyle_800102_80177434[4];

void func_kyle_800102_80167A84(Task* arg0);
void func_kyle_800102_80167DE0(Task* arg0);
void func_kyle_800102_80168244(Task* arg0);
void func_kyle_800102_80168270(Task* arg0);
void func_kyle_800102_801682B4(Task* arg0);

#endif
