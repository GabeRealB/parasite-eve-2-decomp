#ifndef KYLE_800102_H
#define KYLE_800102_H

#include "common.h"

/// 0x20-byte `GpObj` list node. Two of them lead Kyle's work block and are
/// unlinked together when his task tears down.
typedef struct Kyle800102Obj {
    /* 0x00 */ byte pad_0[0x20];
} Kyle800102Obj;
STATIC_ASSERT_SIZEOF(Kyle800102Obj, 0x20);

/// Kyle's work block: two list nodes up front and a frame countdown near the
/// end.
typedef struct Kyle800102Work {
    /* 0x00 */ Kyle800102Obj field_0;
    /* 0x20 */ Kyle800102Obj field_20;
    /* 0x40 */ byte          pad_40[0x48];
    /* 0x88 */ s32           timer;
} Kyle800102Work;

/// The overlay's view of the owning `Task`: the work pointer and the state
/// field the countdown advances when it runs out.
typedef struct Kyle800102Task {
    /* 0x00 */ byte            pad_0[0x1C];
    /* 0x1C */ Kyle800102Work* field_1C;
    /* 0x20 */ byte            pad_20[0x10];
    /* 0x30 */ s32             state;
} Kyle800102Task;

void func_kyle_800102_80168244(Kyle800102Task* arg0);
void func_kyle_800102_80168270(Kyle800102Task* arg0);

#endif
