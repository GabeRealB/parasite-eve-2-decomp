#ifndef GAMEPLAY_EVS_H
#define GAMEPLAY_EVS_H

#include "common.h"

/// 0x18-byte event-script command executed by `Gp_ScriptTaskState1`. `op` is
/// the opcode (-1 ends the script; 43/44/45 are jump / call / return with
/// `arg0` as the target command); the remaining words are per-op arguments.
typedef struct _GpEvsCmd {
    /* 0x00 */ s32 op;
    /* 0x04 */ s32 arg0;
    /* 0x08 */ s32 arg1;
    /* 0x0C */ s32 arg2;
    /* 0x10 */ s32 arg3;
    /* 0x14 */ s32 arg4;
} GpEvsCmd;
STATIC_ASSERT_SIZEOF(GpEvsCmd, 0x18);

/// Event-script entry points and serialized command operands share one word.
typedef union GpEvsAddress {
    s32       address;
    GpEvsCmd* commands;
    void*     storage;
} GpEvsAddress __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(GpEvsAddress, 4);

#endif // GAMEPLAY_EVS_H
