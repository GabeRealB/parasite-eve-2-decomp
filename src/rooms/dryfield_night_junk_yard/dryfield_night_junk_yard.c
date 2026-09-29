#include "rooms/dryfield_night_junk_yard.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/sprites.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk.
extern GpMsgEntry D_dryfield_night_junk_yard_8018055C[];
/// Payload of the 0x7DA message the entry task sends to the slot-4 task.
extern s32      D_dryfield_night_junk_yard_801805A0;
extern GpEvsCmd D_dryfield_night_junk_yard_801805A4[];

s32 func_dryfield_night_junk_yard_8017D5F4(Task*, s32, s32, GpMessageArg);
s32 func_dryfield_night_junk_yard_8017D6A4(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_junk_yard_8017D6AC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_junk_yard_8017D82C(Task*, s32, RoomEventMsg*, GpMessageArg);

extern AnimationPlayRequest D_dryfield_night_junk_yard_80180584;
extern GpCmdArg             D_dryfield_night_junk_yard_80180598;
extern GpCmdArg             D_dryfield_night_junk_yard_8018059C;
void                        func_dryfield_night_junk_yard_8017D894(u8);

GpMsgEntry D_dryfield_night_junk_yard_8018055C[5] = {
    { 5102, func_dryfield_night_junk_yard_8017D6AC },
    { 5105, func_dryfield_night_junk_yard_8017D6A4 },
    { 5103, func_dryfield_night_junk_yard_8017D82C },
    { 5104, func_dryfield_night_junk_yard_8017D5F4 },
    { 0x7FFFFFFF, NULL },
};

AnimationPlayRequest D_dryfield_night_junk_yard_80180584 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

GpCmdArg D_dryfield_night_junk_yard_80180598 = { { .loc = { 3, 26 } }, 0 };

GpCmdArg D_dryfield_night_junk_yard_8018059C = { { .loc = { 3, 26 } }, 1 };

s32 D_dryfield_night_junk_yard_801805A0 = 0x21A03;

GpEvsCmd D_dryfield_night_junk_yard_801805A4[17] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_junk_yard_80180584 }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_junk_yard_80180598 }, { .value = 2011 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_dryfield_night_junk_yard_8017D894 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_dryfield_night_junk_yard_8018059C }, { .value = 2011 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

static void func_dryfield_night_junk_yard_8017D8B0(Task* task);
static void func_dryfield_night_junk_yard_8017D958(Task* task);

/// Handler for message 0x13F0 in the room's message table, keyed by `arg2`.
/// Point 6 plays CAP command 0xC until nibble 0x3A is set, and 6 after. Point 8
/// plays command 9 unless bit flag 0x1C is set; with it set, command 8 plays
/// only while nibble 0x73 is still clear and 0x7C is set, and otherwise the
/// point's own CAP slot starts. Always returns 0.
s32 func_dryfield_night_junk_yard_8017D5F4(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 6:
            Gp_RunCapCmd1(GameFlag_GetNibble(0x3A) <= 0 ? 0xC : 6);
            break;
        case 8:
            if (Gp_GetCurBit2Flag(0x1C) == 1) {
                if (GameFlag_GetNibble(0x73) == 0 && GameFlag_GetNibble(0x7C) != 0) {
                    Gp_RunCapCmd1(8);
                } else {
                    Gp_StartCapSlot(arg2, 1, 0);
                }
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_night_junk_yard_8017D6A4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. Copies the incoming
/// record to the outgoing one, then edits the copy according to the message id
/// and the game's progress nibbles.
///
/// Message 0x18 first picks the copy's `field_3` answer from nibble 0x7A (2
/// once it has reached 4, else 1). Message 0x1B, while nibble 0x46 is 1, runs
/// the CAP command 4 and arms the nibble in `field_6`, consuming the message
/// (returns 0); otherwise, with nibble 0x64 still clear, it answers 2 and
/// latches that nibble. It then walks the 0x73 / 0x61 / 0x8F chain - only while
/// 0x73 is 1, 0x61 agrees with it and 0x8F is clear - and answers 3 when nibble
/// 0x7A has reached it, latching 0x8F.
///
/// `field_5` non-zero means "report only", which suppresses every side effect.
s32 func_dryfield_night_junk_yard_8017D6AC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    s32          state;
    s32          value;

    if (in->prefix.packed == 0x18 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
    }
    *out = *in;
    if (in->prefix.packed != 0x1B) {
        return 1;
    }
    if (GameFlag_GetNibble(0x46) == 1) {
        if (in->field_5 != 0) {
            return 0;
        }
        Gp_RunCapCmd1(4);
        Gp_SetNibbleIf(in->field_6, 2);
        return 0;
    }
    if (GameFlag_GetNibble(0x64) == 0 && in->field_5 == 0) {
        out->field_2 = 2;
        GameFlag_SetNibble(0x64, 1);
    }
    state = GameFlag_GetNibble(0x73);
    if (state != 1) {
        return 1;
    }
    if (GameFlag_GetNibble(0x61) != state) {
        return 1;
    }
    if (GameFlag_GetNibble(0x8F) != 0) {
        return 1;
    }
    value = GameFlag_GetNibble(0x7A);
    if (value != 3) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 1;
    }
    out->field_2 = value;
    GameFlag_SetNibble(0x8F, 1);
    return 1;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `field_2` is 3 on the visit whose `place` is 1, it latches nibble 0x9F once
/// and passes `D_dryfield_night_junk_yard_801805A4` to `func_800E8614`. Always
/// returns 0.
s32 func_dryfield_night_junk_yard_8017D82C(Task* arg0, s32 arg1, RoomEventMsg* in, GpMessageArg arg3)
{
    if ((in->field_2 == 3) && (gGameSession->at4.loc.variant == 1) && (GameFlag_GetNibble(0x9F) == 0)) {
        GameFlag_SetNibble(0x9F, 1);
        func_800E8614(D_dryfield_night_junk_yard_801805A4, 0);
    }
    return 0;
}

/// Stores `arg0` as the session's current room number and mirrors it into the
/// main-executable byte `Mc_SaveData[0].state.at4.loc.room`.
void func_dryfield_night_junk_yard_8017D894(u8 arg0)
{
    gGameSession->at4.loc.room        = arg0;
    Mc_SaveData[0].state.at4.loc.room = arg0;
}

/// Room entry task tick: publish the message table, claim game pointer slot 7,
/// and, on the visit whose sub-id (`gGameSession::at4.loc.variant`) is 1 and that has
/// already latched nibble 0x9F, announce the room to the slot-4 task with
/// message 0x7DA. The nibble is then applied to the current sprite-table entry
/// either way, and the state advances.
static void func_dryfield_night_junk_yard_8017D8B0(Task* task)
{
    u8 subId;

    task->msgTable = D_dryfield_night_junk_yard_8018055C;
    Game_SetPtrSlot(task, 7);
    subId = gGameSession->at4.loc.variant;
    if (subId == 1 && GameFlag_GetNibble(0x9F) == subId) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &D_dryfield_night_junk_yard_801805A0, 0x7DB);
    }
    func_dryfield_night_junk_yard_8017D9B8(GameFlag_GetNibble(0x9F));
    task->state = task->state + 1;
}

/// Entry task state 1: does nothing, and nothing here advances the state.
static void func_dryfield_night_junk_yard_8017D958(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_junk_yard_8017D5C4 = {
    { func_dryfield_night_junk_yard_8017D8B0, func_dryfield_night_junk_yard_8017D958, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_night_junk_yard_8017D960(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}

/// Applies game flag nibble 0x9F to the sixth sprite command of view 0 in the
/// current room's sprite record: a zero nibble draws the command, a nonzero one
/// hides it (`Gp_LinkViewSprts` skips OT-linking when `field_4` is set).
void func_dryfield_night_junk_yard_8017D9B8(u8 arg0)
{
    GameLocationKey* sess = &gGameSession->at4.loc;
    GpSprtCmd*       cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][6].field_4;
    if (arg0 == 0) {
        cmd[5].field_4 = 0;
    } else {
        cmd[5].field_4 = 1;
    }
}
