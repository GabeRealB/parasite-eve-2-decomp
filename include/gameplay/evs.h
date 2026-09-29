#ifndef GAMEPLAY_EVS_H
#define GAMEPLAY_EVS_H

#include "common.h"

#include "gameplay/cap.h"
#include "gameplay/message.h"

struct _GpEvsCmd;
struct _GpScriptCmd;
struct _GpScriptRec;

/// Overlay ids passed to the loader by an event-script command.
typedef struct _GpOverlayIds {
    u16 field_0;
    u16 field_2;
    u16 field_4;
} GpOverlayIds;
STATIC_ASSERT_SIZEOF(GpOverlayIds, 6);

/// An event operand is either a value or an address, according to its opcode.
typedef union GpEvsOperand {
    s32                   value;
    void*                 storage;
    struct _GpEvsCmd*     commands;
    AnimationPlayRequest* animation;
    GpOverlayIds*         overlays;
    struct _GpScriptCmd*  padCommands;
    struct _GpScriptRec*  padRecords;
    GpMessageArg          message;
    TaskSpawnArg          spawn;
    // The exported callback address uses the word-register event ABI. Some
    // callbacks ignore that register or consume only its low byte/halfword;
    // these members retain their source declarations in script initializers.
    void        (*callback)(s32);
    void        (*callbackNoArg)(void);
    void        (*callbackS8)(s8);
    void        (*callbackU8)(u8);
    void        (*callbackS16)(s16);
    void        (*callbackU16)(u16);
    void        (*callbackU32)(u32);
    s32         (*callbackResult)(s32);
    void        (*callbackSetText)(GpCapTextCb);
    GpCapTextCb captionText;
} GpEvsOperand;
STATIC_ASSERT_SIZEOF(GpEvsOperand, 4);

/// 0x18-byte event-script command executed by `Gp_ScriptTaskState1`. `op` is
/// the opcode (-1 ends the script; 43/44/45 jump, call, and return).
/// The opcode determines which operand members the interpreter reads.
typedef struct _GpEvsCmd {
    /* 0x00 */ s32          op;
    /* 0x04 */ GpEvsOperand arg0;
    /* 0x08 */ GpEvsOperand arg1;
    /* 0x0C */ GpEvsOperand arg2;
    /* 0x10 */ GpEvsOperand arg3;
    /* 0x14 */ GpEvsOperand arg4;
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
