#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dcb8.h"
#include "rooms/rooms_shared_8017ff88.h"

/// Advances the gameplay LCG and yields the high half of the new state.
#define DRYFIELD_NIGHT_MAIN_STREET_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

/// Applies the patch list `table[GameFlag_GetNibble(nibble)]` to the current
/// area's view sprite commands. The list is a stream of byte pairs ended by a
/// 0xFF first byte: `(view, 0xFF)` selects that view's command list, and any
/// other `(cmd, value)` stores `value` in that command's `field_4`. The list
/// starts on the view named by its first byte.
#define DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(table, nibble)      \
    {                                                                   \
        GpAreaKey* sess;                                                \
        GpSprtRec* rec;                                                 \
        GpSprtCmd* cmd;                                                 \
        u8*        p;                                                   \
        s16        idx;                                                 \
        u8**       tbl;                                                 \
                                                                        \
        idx  = GameFlag_GetNibble(nibble);                              \
        tbl  = table;                                                   \
        p    = tbl[idx];                                                \
        sess = &gGameSession->at4.loc;                                  \
        rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1]; \
        cmd  = rec[p[0]].field_4;                                       \
        if (p[0] != 0xFF) {                                             \
            do {                                                        \
                if (p[1] == 0xFF) {                                     \
                    cmd = rec[p[0]].field_4;                            \
                    p  += 2;                                            \
                }                                                       \
                cmd[p[0]].field_4 = p[1];                               \
                p                += 2;                                  \
            } while (p[0] != 0xFF);                                     \
        }                                                               \
    }

extern s32 D_80070F70;
extern s16 D_80071076;
extern u8  D_80115598;
extern u8  D_80115690;
extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 D_80115744;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_dryfield_night_main_street_8018208C;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_night_main_street_80182098;

/// Descriptor of the task `func_dryfield_night_main_street_8017DE78` runs as.
extern TaskDesc D_dryfield_night_main_street_801820A4;

/// Message table installed at `Task::msgTable` by the room task's state 0
/// (ids `0x13EE`-`0x13F1`).
extern GpMsgEntry D_dryfield_night_main_street_801820B0[];

/// Per-nibble-value sprite patch lists, one table per game-flag nibble.
extern u8** D_dryfield_night_main_street_80182168;
extern u8** D_dryfield_night_main_street_8018216C;
extern u8** D_dryfield_night_main_street_80182170;
extern u8** D_dryfield_night_main_street_80182174;

/// The room effect mode of each view, indexed by view - 1.
extern u16 D_dryfield_night_main_street_80182178[];

/// Anchors of the room task's glows; the entries after the first are also
/// reached by their own names, and entry 16 is the spawn position scratch.
extern SVECTOR D_dryfield_night_main_street_801821A8[];
extern SVECTOR D_dryfield_night_main_street_801821B8;
extern SVECTOR D_dryfield_night_main_street_801821C8;
extern SVECTOR D_dryfield_night_main_street_801821D8;
extern SVECTOR D_dryfield_night_main_street_801821E8;

/// View masks of the room task's anchors.
extern s32 D_dryfield_night_main_street_80182230[];

/// Per-tint channel shifts for the halo task, indexed by the tint the spawn
/// argument selects.
extern RoomHaloShade D_dryfield_night_main_street_80182270[];

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpFadeWork D_dryfield_night_main_street_80188BA4;

/// The message and event the message handler latched for the room's event
/// task, and the flag it raises once it has spawned that task.
extern RoomEventMsg     D_dryfield_night_main_street_80188BAC;
extern s8               D_dryfield_night_main_street_80188BB4;
extern RoomLatchedEvent D_dryfield_night_main_street_80188BC8;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_night_main_street_80188BBC;
extern RoomEventReq D_dryfield_night_main_street_80188BD8;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_main_street_80188BC4;

void func_dryfield_night_main_street_8017E064(Task* arg0);
void func_dryfield_night_main_street_8017E0B8(Task* task);
void func_dryfield_night_main_street_8017E118(void);
void func_dryfield_night_main_street_8017E940(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_main_street_8017F128(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_main_street_8017F608(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_night_main_street_8017FD34(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
void func_dryfield_night_main_street_80180CF4(GpCoord* coord, s16 size);
void func_dryfield_night_main_street_80181220(GpCoord* arg0, s32 arg1);
void func_dryfield_night_main_street_80181598(GpCoord* arg0, s16 arg1, u8* arg2);

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_dryfield_night_main_street_8017D600(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_night_main_street_80188BC8.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_night_main_street_80188BC8.fade != 0) {
                    D_dryfield_night_main_street_80188BA4.field_0 = 0;
                    D_dryfield_night_main_street_80188BA4.field_1 = 0;
                    D_dryfield_night_main_street_80188BA4.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_night_main_street_80188BA4);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_night_main_street_80188BC8.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_night_main_street_80188BC8.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_night_main_street_80188BC8.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_main_street_80188BAC.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_main_street_80188BAC.field_2;
            Mc_SaveData.at4.loc.room = D_dryfield_night_main_street_80188BAC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.
s32 func_dryfield_night_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                  = req->flagId;
    D_dryfield_night_main_street_80188BC4 = 0;
    neg                                   = flag < 0;
    got                                   = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_night_main_street_80188BBC = *msg;
                D_dryfield_night_main_street_80188BD8 = *req;
                id                                    = req->flagId;
                mode                                  = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_main_street_80182098, 0, 0, 0);
                D_dryfield_night_main_street_80188BC4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's CAP command,
/// plays its two sound events in turn and waits for each to finish, then
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11 to load it.
void func_dryfield_night_main_street_8017D8FC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_main_street_80188BD8.field_0);
            if (D_dryfield_night_main_street_80188BD8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_main_street_80188BD8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_main_street_80188BD8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_main_street_80188BD8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_main_street_80188BD8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_main_street_80188BD8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_main_street_80188BBC.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_main_street_80188BBC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_night_main_street_80188BBC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_dryfield_night_main_street_8017D5F4 = {
    { func_dryfield_night_main_street_8017E064, func_dryfield_night_main_street_8017E0B8, taskKill },
};

/// Message handler for the room. Messages 0x19, 1 and 0xF answer in the copy's
/// `field_3` from story nibbles. Messages 0xB and 0xC run the room's own event
/// gate: unless the event's nibble is already set, the message and event are
/// latched, the nibble is set and the room's event task is spawned. Messages
/// 0xD and 0xE go through the event gate `func_dryfield_night_main_street_8017D798`
/// and, when it fires, swap collected bits
/// 0x10F / 0x112 for 0x113. Anything else is not consumed.
s32 func_dryfield_night_main_street_8017DA6C(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent ev;
    s32              ret;

    *out = *msg;
    if (msg->msgId == 0x19) {
        if (gGameSession->at4.loc.stage == 2) {
            if (msg->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = 2;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (msg->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (msg->msgId == 1 && msg->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = 1;
        } else if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 4;
        } else {
            out->field_3 = GameFlag_GetNibble(0x61) + 2;
        }
    }
    if (msg->msgId == 0xF && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->msgId == 0x19 && GameFlag_GetNibble(0x61) != 0) {
        if (msg->field_5 == 0) {
            Gp_SetNibbleIf(msg->field_6, 2);
            Gp_RunCapCmd1(0x13);
            return 2;
        }
        return 2;
    }
    if (msg->msgId == 0xB) {
        ev.capCmd                             = 3;
        ev.stageSnd                           = 0x52020005;
        ev.flagId                             = 0x57;
        ev.fade                               = 0;
        D_dryfield_night_main_street_80188BB4 = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC = *out;
                D_dryfield_night_main_street_80188BC8 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4 = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->msgId == 0xC) {
        ev.capCmd                             = 4;
        ev.stageSnd                           = 0x52020005;
        ev.flagId                             = 0x58;
        ev.fade                               = 0;
        D_dryfield_night_main_street_80188BB4 = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC = *out;
                D_dryfield_night_main_street_80188BC8 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4 = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->msgId == 0xD) {
        req.field_0 = 0xA;
        req.field_4 = 5;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x41;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x93) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    } else if (msg->msgId == 0xE) {
        req.field_0 = 0xB;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x42;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x94) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    }
    return 1;
}

/// Waits for the CAP script to go idle, then records the play time when the
/// script's event key is 1, drops collected bit 0x11A when 0x119 is also held,
/// and ends.
void func_dryfield_night_main_street_8017DE78(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (Gp_HasCollectedBit(0x119) != 0 && Gp_HasCollectedBit(0x11A) != 0) {
            Gp_ClearCollectedBit(0x11A);
        }
        taskKill(task);
    }
}

/// Plays the stage sound a CAP script cue asks for: cues 8, 9 and 0xC play
/// their own sound (9 also plays 0xC's), and cues 0x65 and 0x78 play sound
/// 0xD when the event key is 1 and 0 respectively.
s32 func_dryfield_night_main_street_8017DEF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x52020008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x52020009, 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(0x5202000C, 0, 0);
            break;
        case 0x65:
            if (Gp_GetCapEventKey() == 1) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
        case 0x78:
            if (Gp_GetCapEventKey() == 0) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
    }
    return 0;
}

/// Acts only on `arg2 == 1`. Once nibble 0x7B has reached 2 it ages flag
/// 0x119, sets current-bit flag 0x1B unless collected bit 0x119 is held,
/// spawns CAP entry 1 and the task above; before that it spawns CAP entry
/// 0x14 instead.
s32 func_dryfield_night_main_street_8017DFC8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&D_dryfield_night_main_street_801820A4, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}

s32 func_dryfield_night_main_street_8017E054(void)
{
    return 0;
}

s32 func_dryfield_night_main_street_8017E05C(void)
{
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// pointer slot 7, runs the room's message-table pass, then advances state and
/// raises the `D_80115598` flag.
void func_dryfield_night_main_street_8017E064(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_main_street_801820B0;
    Game_SetPtrSlot(arg0, 7);
    func_dryfield_night_main_street_8017E118();
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// The room entry task's idle state.
void func_dryfield_night_main_street_8017E0B8(Task* task)
{
}

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_dryfield_night_main_street_8017E0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_main_street_8017D5F4;
    sp.funcs[task->state](task);
}

/// Applies the sprite-command patch lists selected by game-flag nibbles 0x88,
/// 0x89, 0x8A and 0x8C, one table of lists per nibble.
void func_dryfield_night_main_street_8017E118(void)
{
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182168, 0x88);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_8018216C, 0x89);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182170, 0x8A);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182174, 0x8C);
}

/// Per-frame room task. On its first run it stores the ids 0x60286-0x60289 in
/// four gameplay globals. Each run it draws the anchors whose view mask in
/// `D_...80182230` contains the current view (entries 2 and 3 are cleared once
/// nibble 0x7F is set) and publishes the view's `roomEffectMode`. In views 8
/// and 0x13 it spawns 0x30 randomly placed 0x601B2 effects on entering the
/// view, and one more on each run with bit 0 of `D_80070F70` set while it
/// stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_night_main_street_8017E484(Task* task)
{
    s32 mask;
    s32 i;

    mask = 1 << (Gp_GetViewIndex() & 0xFF);
    if (task->state == 0) {
        D_80115728  = 0x60286;
        D_80115744  = 0x60287;
        D_8011573C  = 0x60288;
        D_80115720  = 0x60289;
        task->state = 1;
    }
    if (GameFlag_GetNibble(0x7F) != 0) {
        D_dryfield_night_main_street_80182230[3] = 0;
        D_dryfield_night_main_street_80182230[2] = 0;
    }
    if (mask & D_dryfield_night_main_street_80182230[0]) {
        func_dryfield_night_main_street_8017E940(D_dryfield_night_main_street_801821A8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[2]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821B8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[4]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821C8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[6]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821D8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[8]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821E8, 0x180);
    }
    for (i = 10; i < 16; i++) {
        if (mask & D_dryfield_night_main_street_80182230[i]) {
            func_dryfield_night_main_street_8017F128(&D_dryfield_night_main_street_801821A8[i], 1, 0x380);
        }
    }
    Gp_State1C->roomEffectMode = D_dryfield_night_main_street_80182178[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8 || (Gp_GetViewIndex() & 0xFF) == 0x13) {
        if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_night_main_street_801821A8[16]);
            }
        } else if (D_80070F70 & 1) {
            D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_night_main_street_801821A8[16]);
        }
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}

/// Draws a glow between the two world points `arg0[0]` and `arg0[1]`: a fan
/// of gouraud wedges around each projected point, joined by wedges spanning
/// the two, the sweep oriented along the screen-space line between them. Each
/// radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is drawn unless
/// both points project. The lit vertices take a brightness that flickers with
/// the frame counter.
void func_dryfield_night_main_street_8017E940(SVECTOR* arg0, s32 arg1)
{
    void**                   scratch;
    u8*                      head;
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = ((*(u8*)&ds->animFrame & 1) * 0x10) | 0x20;
            SOFT_BARRIER();
            angEnd = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a textured semi-transparent sprite centred on the world point `arg0`
/// when it projects. `arg1` picks the 40-texel column of the texture page and
/// its palette; `arg2` is the half-extent, scaled by 39 over the OTZ on
/// screen. The sprite's brightness flickers with the frame counter.
void func_dryfield_night_main_street_8017F128(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

void func_dryfield_night_main_street_8017F3B0(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    s32        vz;
    s16        f2a;
    u32        rng2;
    u32        rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = task->spawnArg1 & 0xFFF;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->angle = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->period = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step    = f2a;
        work->move.vy = 0;
        rng2          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng2;
        work->move.vx = -(((u32)rng2 >> 16) & 0x7F);
        rng3          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng3;
        vz            = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->move.vz = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_dryfield_night_main_street_8017F608(coord, (u16)work->index, work->scale, work->angle);

    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws one frame of a textured semi-transparent puff at the coordinate's
/// world position, skipped when it fails to project or sits too close
/// (OTZ below 0x41). `arg1` is the animation frame, a 48-texel cell of a
/// five-wide grid; `arg2` is the half-extent, scaled by 47 over the OTZ on
/// screen; `arg3` is the angle the quad is rotated by.
void func_dryfield_night_main_street_8017F608(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    GpEffFlareScratch* block;
    s32*               otzp;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                sine;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                vz;
    u16                tex;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head          = *scratch;
    block         = (GpEffFlareScratch*)(head - 0x1C);
    block->vec.vx = (u16)arg0->workm.t[0];
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    otzp          = &block->otz;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - 0x1C))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((GpEffFlareScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpEffFlareScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(otzp);
        if (((GpEffFlareScratch*)(head - 0x1C))->otz >= 0x41) {
            ang         = (s16)arg3;
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 3;
            tex         = arg1;
            u0          = (tex % 5) * 0x30;
            v0          = (tex / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            sine = rsin(ang);
            span = (s16)arg2 * 0x2F;
            block->dx =
                ((span / ((GpEffFlareScratch*)(head - 0x1C))->otz) * sine) >> 12;
            block->dy =
                ((span / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
            prim->x0 = block->sx + (u16)block->dx;
            prim->x3 = block->sx - (u16)block->dx;
            prim->y0 = block->sy - (u16)block->dy;
            prim->y3 = block->sy + (u16)block->dy;
            ang2     = ang + 0x400;
            block->dx =
                ((span / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rsin(ang2)) >> 12;
            block->dy =
                ((span / ((GpEffFlareScratch*)(head - 0x1C))->otz) * rcos(ang2)) >> 12;
            prim->x1 = block->sx + (u16)block->dx;
            prim->x2 = block->sx - (u16)block->dx;
            prim->y1 = block->sy - (u16)block->dy;
            prim->y2 = block->sy + (u16)block->dy;
            addPrim((u_long*)(((((u32)((GpEffFlareScratch*)(head - 0x1C))->otz
                                 << gDisplayState.otDepthShift) >>
                                2) &
                               0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// A drifting mote: the spawn argument gives its brightness flags, vertical
/// speed and lifetime. With neither of the two low bits set it starts dim and
/// rising, with some random extra speed, in state 1, which brightens it;
/// otherwise it starts bright in state 2, moving down, or up when bit 1 is
/// set. Every other tick it advances its
/// animation frame and draws; eight ticks before its lifetime ends it fades
/// out, and it releases its work block once dark or when the room's event
/// state reaches 4.
void func_dryfield_night_main_street_8017FA68(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        lifetime;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1 & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1 & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1 & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_dryfield_night_main_street_8017FD34(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_dryfield_night_main_street_8017FD34(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote as a textured semi-transparent square at the coordinate's
/// projected position, when it projects. The low two bits of `arg1` and the
/// top nibble of `arg2` pick the 24-texel texture cell; the rest of `arg2` is
/// the half-extent, scaled by 23 over the OTZ; the low byte of `arg3` is the
/// grey level and its top nibble picks the palette.
void func_dryfield_night_main_street_8017FD34(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    GpRingScratch* next;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;
    u16            vz;

    row                                     = arg2 >> 12;
    arg2                                   &= 0xFFF;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    pal                                     = arg3 >> 12;
    arg3                                   &= 0xFF;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    next                                    = (GpRingScratch*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(next));
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, between the radii `arg1` and `arg1 + arg2` (each scaled by 64
/// over the OTZ). The `arg1` edge is black and the other edge takes `rgb`, so
/// the ring fades across its width.
void func_dryfield_night_main_street_8017FFF8(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw09Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = (u16)arg0->workm.t[0];
        ((RoomDraw09Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw09Scratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw09Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw09Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw09Scratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glow at the coordinate's projected position: eight gouraud quads
/// fanned around it, of radius `arg1` scaled by 64 over the OTZ. The centre
/// takes `rgb` and the rim is black.
void func_dryfield_night_main_street_8018041C(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = (u16)arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A halo on an effect's anchor. State 0 parks the coordinate frame on the
/// anchor and derives the fade step from the spawn argument's duration.
/// State 1 grows and brightens a glow (with a half-bright echo on odd ticks)
/// and a shrinking ring around it for that duration; state 2 fades a
/// two-ring glow back out, then the work block is released. The tint's
/// channel shifts come from the table above.
void func_dryfield_night_main_street_801807B0(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index      = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->step       = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].r;
                rgb[1]           = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].g;
                rgb[2]           = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].b;
                func_dryfield_night_main_street_8018041C(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_dryfield_night_main_street_8018041C(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_dryfield_night_main_street_8017FFF8(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].r;
                    rgb[1] = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].g;
                    rgb[2] = mem->scale >> D_dryfield_night_main_street_80182270[mem->index].b;
                    func_dryfield_night_main_street_80181598(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// A burst on an effect's anchor. Each tick it grows a warm glow and the
/// flickering light glow at its coordinate; while its second level lasts it
/// also draws a widening ring that fades with that level, and afterwards the
/// main level runs down until the work block is released. The task also ends
/// when the room's event state reaches 4.
void func_dryfield_night_main_street_80180B48(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_dryfield_night_main_street_8018041C(coord, (s16)(step * 2), rgb);
        func_dryfield_night_main_street_80180CF4(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_dryfield_night_main_street_8017FFF8(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a flickering glow at the coordinate: an inner textured square of
/// half-extent `size` and an outer one half as large again (each scaled by
/// 0x37 over the OTZ), and, where the coordinate is over ground, a flat quad
/// on the ground beneath it. It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a random intensity. Nothing is drawn unless the coordinate
/// projects.
void func_dryfield_night_main_street_80180CF4(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
    block->vec.vx               = (u16)coord->workm.t[0];
    alias                       = block;
    vy                          = (u16)coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = (u16)coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = sc->sx - sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = sc->sx + sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = sc->sy - sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = sc->sy + sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = sc->sx - sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = sc->sx + sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = sc->sy - sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = sc->sy + sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_dryfield_night_main_street_80181220(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a textured semi-transparent quad of half-size `arg1` at the
/// coordinate's world position, turned to face the camera, when all four
/// corners project. The texture alternates between two frames with the frame
/// counter.
void func_dryfield_night_main_street_80181220(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = (u16)block->sxy0.vx;
        prim->y0    = (u16)block->sxy0.vy;
        prim->x1    = (u16)block->sxy1.vx;
        prim->y1    = (u16)block->sxy1.vy;
        prim->x2    = (u16)block->sxy2.vx;
        prim->y2    = (u16)block->sxy2.vy;
        prim->x3    = (u16)block->sxy3.vx;
        prim->y3    = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Draws a star-shaped glow at the coordinate's projected position: a
/// half-bright fan of radius `arg1` (scaled by 64 over the OTZ), a full-bright
/// fan of half that radius over it, and four half-bright spikes. Every quad
/// takes `arg2` at the centre and is black at its rim.
void func_dryfield_night_main_street_80181598(GpCoord* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = (u16)arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Emits sparks from the task's coordinate frame: each tick it turns its angle
/// on by a random amount, spawns effect `D_80115728` moving outward along that
/// angle and upward faster as the task ages, and releases its work block after
/// 0x15 ticks or when the room's event state reaches 4.
void func_dryfield_night_main_street_80181F58(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}
