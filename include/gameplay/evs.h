#ifndef GAMEPLAY_EVS_H
#define GAMEPLAY_EVS_H

#include "common.h"

#include "gameplay/cap.h"
#include "gameplay/message.h"

struct _GpEvsCmd;
struct PadScriptCmd;
struct PadScriptVibrationSegment;

/// Key an event script uses to select a scene/audio stream.
///
/// Opcode 12 installs the pointer for caption playback. `group` and `streamId`
/// match the stream key, and `subId` matches the stream's first qualifier. The
/// second qualifier is absent from this record; playback matches it as zero.
/// Group zero selects the stage-zero stream table, and any other group is
/// matched in the current folder's table. Debug output prints the three
/// numbers as `evs<group>_<streamId>_<subId>.txt`.
typedef struct {
    u16 group;    // Scene/audio selection group (0 selects the stage-zero table)
    u16 streamId; // Stream ID matched in that table
    u16 subId;    // First exact stream qualifier
} EvsSceneKey;
STATIC_ASSERT_SIZEOF(EvsSceneKey, 6);

/// An event operand is either a value or an address, according to its opcode.
typedef union GpEvsOperand {
    s32                               value;
    void*                             storage;
    struct _GpEvsCmd*                 commands;
    AnimationPlayRequest*             animation;
    EvsSceneKey*                      overlays;
    struct PadScriptCmd*              padCommands;
    struct PadScriptVibrationSegment* padRecords;
    TaskMessageArg                    message;
    TaskSpawnArg                      spawn;
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
