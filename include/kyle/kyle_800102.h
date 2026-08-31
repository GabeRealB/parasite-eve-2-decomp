#ifndef KYLE_800102_H
#define KYLE_800102_H

#include "common.h"

/// 0x20-byte `GpObj` list node. Two of them lead Kyle's work block and are
/// unlinked together when his task tears down.
typedef struct Kyle800102Obj {
    /* 0x00 */ byte pad_0[0x20];
} Kyle800102Obj;
STATIC_ASSERT_SIZEOF(Kyle800102Obj, 0x20);

typedef struct Kyle800102Work {
    /* 0x00 */ Kyle800102Obj field_0;
    /* 0x20 */ Kyle800102Obj field_20;
} Kyle800102Work;

/// The overlay's view of the owning `Task`: only the work pointer is known.
typedef struct Kyle800102Task {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Kyle800102Work* field_1C;
} Kyle800102Task;

void func_kyle_800102_80168270(Kyle800102Task* arg0);

#endif
