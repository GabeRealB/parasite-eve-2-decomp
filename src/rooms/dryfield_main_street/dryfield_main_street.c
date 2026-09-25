#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
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

/// Advances the gameplay LCG and yields the high half of the new state.
#define DRYFIELD_MAIN_STREET_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern s32 D_80070F70;
extern s16 D_80071076;
extern u8  D_80115598;
extern u8  D_80115690;
extern u8  D_801156F9;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_dryfield_main_street_80180E7C;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_main_street_80180E88;

/// Descriptor of the task `func_dryfield_main_street_8017DFC8` spawns.
extern TaskDesc D_dryfield_main_street_80180E94;

/// Message table the room entry task installs at `Task::msgTable`.
extern GpMsgEntry D_dryfield_main_street_80180EA0[];

/// Payload of message 0x7DA the room entry task sends to pointer slot 4.
extern s32 D_dryfield_main_street_80180ED0;

/// The two arguments `func_dryfield_main_street_8017E05C` hands to
/// `func_800E8634`.
extern s32 D_dryfield_main_street_80181624;
extern s32 D_dryfield_main_street_80181A14;

/// Descriptor of the task `func_dryfield_main_street_8017E320` spawns.
extern TaskDesc D_dryfield_main_street_8018156C;

/// The room effect mode of each view, indexed by view - 1.
extern u16 D_dryfield_main_street_80181B94[];

/// Spawn position scratch of the room task's effects.
extern SVECTOR D_dryfield_main_street_80181BA4;

/// The two anchors of the trail task's coordinate trails; the second is also
/// reached by its own name.
extern SVECTOR D_dryfield_main_street_80181BAC[];
extern SVECTOR D_dryfield_main_street_80181BB4;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpStateBD8 D_dryfield_main_street_8018560C;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg     D_dryfield_main_street_80185614;
extern RoomLatchedEvent D_dryfield_main_street_80185634;

/// Set by the message handler when its last 0xB/0xC message latched an event
/// and spawned the room's event task; every such message clears it first.
extern s8 D_dryfield_main_street_8018561C;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_main_street_80185624;
extern RoomEventReq D_dryfield_main_street_80185644;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_main_street_8018562C;

/// The task `func_dryfield_main_street_8017E320` spawned, until it is killed
/// or forgotten.
extern Task* D_dryfield_main_street_80185630;

void func_dryfield_main_street_8017E0D8(Task* task);
void func_dryfield_main_street_8017E158(Task* task);
void func_dryfield_main_street_8017E4A4(s32 arg0);
void func_dryfield_main_street_8017EA88(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_main_street_8017F18C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_main_street_8017F5B8(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_dryfield_main_street_8017FE3C(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_dryfield_main_street_801804BC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_dryfield_main_street_8017D600(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_main_street_80185634.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_main_street_80185634.fade != 0) {
                    D_dryfield_main_street_8018560C.field_0 = 0;
                    D_dryfield_main_street_8018560C.field_1 = 0;
                    D_dryfield_main_street_8018560C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_main_street_8018560C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_main_street_80185634.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_main_street_80185634.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_main_street_80185634.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_main_street_80185614.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_main_street_80185614.field_2;
            Mc_SaveData.at4.loc.room = D_dryfield_main_street_80185614.field_3;
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
s32 func_dryfield_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_main_street_8018562C = 0;
    neg                             = flag < 0;
    got                             = (s16)flag;
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
                D_dryfield_main_street_80185624 = *msg;
                D_dryfield_main_street_80185644 = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_main_street_80180E88, 0, 0, 0);
                D_dryfield_main_street_8018562C = 1;
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
void func_dryfield_main_street_8017D8FC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_main_street_80185644.field_0);
            if (D_dryfield_main_street_80185644.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_main_street_80185644.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_main_street_80185644.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_main_street_80185644.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_main_street_80185644.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_main_street_80185644.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_main_street_80185624.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_main_street_80185624.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_main_street_80185624.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room entry task's three states: set the room up, idle, end.
const TaskFuncTable3 D_dryfield_main_street_8017D5F4 = {
    { func_dryfield_main_street_8017E0D8, func_dryfield_main_street_8017E158, taskKill },
};

/// The room's message handler; it copies the message to `out` first. Messages
/// 0x19, 1 and 0xF answer in the copy's `field_3` from story nibbles, and 0x19
/// is consumed outright once nibble 0x61 is set. Messages 0xB and 0xC latch a
/// fixed event and spawn the room's event task unless the event's nibble is
/// already set. Messages 0xD and 0xE build a request for the event gate
/// `func_dryfield_main_street_8017D798` and, when it fires, swap collected
/// bits 0x10F / 0x112 for 0x113. Anything else is not consumed.
s32 func_dryfield_main_street_8017DA6C(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
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
        ev.capCmd                       = 3;
        ev.stageSnd                     = 0x52020005;
        ev.flagId                       = 0x57;
        ev.fade                         = 0;
        D_dryfield_main_street_8018561C = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_main_street_80185614 = *out;
                D_dryfield_main_street_80185634 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_main_street_80180E7C, 0, 0, 0);
                D_dryfield_main_street_8018561C = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->msgId == 0xC) {
        ev.capCmd                       = 4;
        ev.stageSnd                     = 0x52020005;
        ev.flagId                       = 0x58;
        ev.fade                         = 0;
        D_dryfield_main_street_8018561C = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_main_street_80185614 = *out;
                D_dryfield_main_street_80185634 = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_main_street_80180E7C, 0, 0, 0);
                D_dryfield_main_street_8018561C = 1;
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
        ret         = func_dryfield_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_main_street_8018562C != 0) {
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
        ret         = func_dryfield_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_main_street_8018562C != 0) {
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
void func_dryfield_main_street_8017DE78(Task* task)
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
s32 func_dryfield_main_street_8017DEF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
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
s32 func_dryfield_main_street_8017DFC8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&D_dryfield_main_street_80180E94, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}

/// Does nothing and answers 0.
s32 func_dryfield_main_street_8017E054(void)
{
    return 0;
}

/// On a message whose `field_2` is 1, the first time only (nibble 0x5F still
/// clear): forgets the task `func_dryfield_main_street_8017E320` spawned, calls
/// `func_800E8634` with the room's two data blocks, and sets nibbles 0x5F and
/// 0x155 and clears nibble 3. Always answers 0.
s32 func_dryfield_main_street_8017E05C(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    if ((msg->field_2 == 1) && (GameFlag_GetNibble(0x5F) == 0)) {
        func_dryfield_main_street_8017E4A4(0);
        func_800E8634((s32)&D_dryfield_main_street_80181624, 0, (s32)&D_dryfield_main_street_80181A14);
        GameFlag_SetNibble(0x5F, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
    }
    return 0;
}

/// The room entry task's first state: installs the room's message table,
/// hands the task to pointer slot 7, raises `D_80115598` and, until nibble
/// 0x5F is set, sends message 0x7DA to the task in pointer slot 4.
void func_dryfield_main_street_8017E0D8(Task* task)
{
    task->msgTable = D_dryfield_main_street_80180EA0;
    Game_SetPtrSlot(task, 7);
    D_80115598 = 1;
    if (GameFlag_GetNibble(0x5F) == 0) {
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_dryfield_main_street_80180ED0, 0x7DB);
    }
    task->state++;
}

/// The room entry task's idle state; it only opens and closes a stack frame.
void func_dryfield_main_street_8017E158(Task* task)
{
    char pad[0x10];
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_main_street_8017E168(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_main_street_8017D5F4;
    sp.funcs[task->state](task);
}

/// Per-frame task that turns the player (`gameGetPtrSlot(3)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the translation of the *second*
/// `GsCOORDINATE2` node of the target's model (`field_8[1]`) minus the
/// player's own (`field_8[0]`); the delta against `GameActor::field_52` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `taskKill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_dryfield_main_street_8017E1C0(Task* task)
{
    Task*          player;
    GameActor*     actor;
    GpWorkObj*     work;
    GsCOORDINATE2* self;
    GsCOORDINATE2* target;
    s32            angle;
    s32            delta;
    s32            magnitude;
    s32            step;
    s32            wrapped;

    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    work   = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
    if ((work != NULL) && (gGameSession->eventState != 0)) {
        self      = ((TmdObject*)player->extra)->coords;
        target    = &((TmdObject*)((Task*)work->field_0)->extra)->coords[1];
        angle     = ratan2(target->coord.t[0] - self->coord.t[0], target->coord.t[2] - self->coord.t[2]);
        delta     = angle - actor->field_52;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->field_52 = (s16)((u16)actor->field_52 + step);
            return;
        }
        actor->field_52 = angle;
    }
    taskKill(task);
}

/// Passes `arg0` to `Gp_ArmStateF0` and marks item 0x10A as seen.
void func_dryfield_main_street_8017E2F4(s32 arg0)
{
    Gp_ArmStateF0(arg0);
    Gp_SetItemSeenBit(0x10A, 1);
}

/// Spawns the ramp task described at `D_dryfield_main_street_8018156C` and
/// keeps it in `D_dryfield_main_street_80185630`.
void func_dryfield_main_street_8017E320(void)
{
    D_dryfield_main_street_80185630 = Task_SpawnFromTable(&D_dryfield_main_street_8018156C, 1, 0, 0);
}

/// Steers the task `func_dryfield_main_street_8017E320` spawned, if there is
/// one: 0 or 1 becomes its `spawnArg1`, any other value kills and forgets it.
void func_dryfield_main_street_8017E354(s32 arg0)
{
    Task* t = D_dryfield_main_street_80185630;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_dryfield_main_street_80185630);
    D_dryfield_main_street_80185630 = NULL;
}

/// Per-frame ramp task. While `D_801156F9` is clear, state 0 ramps
/// `killCountdown` by 0x100 a frame towards 0x1000 while `spawnArg1` is set,
/// or towards 0 while it is clear, and passes it to `func_800B0928` with the
/// player and the current area's work object. Any other state ends the task.
void func_dryfield_main_street_8017E3A8(Task* task)
{
    GpWorkObj* work;
    u16        tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            work = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
            func_800B0928(gameGetPtrSlot(3), (Task*)work->field_0, 0x300, 0x200, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Forgets the task `func_dryfield_main_street_8017E320` spawned, without
/// killing it. The argument is unused; its caller passes 0.
void func_dryfield_main_street_8017E4A4(s32 arg0)
{
    D_dryfield_main_street_80185630 = 0;
}

/// Per-frame room task. On its first run it stores the ids 0x60293-0x60295 in
/// three gameplay globals. Each run it publishes the current view's
/// `roomEffectMode`. In view 8 it spawns 0x30 randomly placed 0x601B1 effects
/// on entering the view, and one more on each run with bit 0 of `D_80070F70`
/// set while it stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_main_street_8017E4B0(Task* task)
{
    s32 i;

    if (task->state == 0) {
        D_80115758  = 0x60293;
        D_8011572C  = 0x60294;
        D_80115750  = 0x60295;
        task->state = 1;
    }
    Gp_State1C->roomEffectMode = D_dryfield_main_street_80181B94[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8) {
        if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(0x601B1, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_main_street_80181BA4);
            }
        } else if (D_80070F70 & 1) {
            D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(0x601B1, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_main_street_80181BA4);
        }
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}

/// A spark the room spawns: each frame it draws tile `field_20` of the 5-wide
/// sprite grid with `func_dryfield_main_street_8017EA88`, at half-extent
/// `field_24` (the low 12 bits of `spawnArg1`) and spin `field_26` (random),
/// and drifts along `field_10`. The first run picks a random direction,
/// normalises it and scales it by `field_2A` (bits 16-23 of `spawnArg1`,
/// default 0x40). Every `field_28` frames (bits 12-14, default 1) the tile
/// advances; after tile 9 the spark releases itself.
void func_dryfield_main_street_8017E830(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->field_22++;
    if (task->state == 0) {
        work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;
        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
        work->field_26 = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->field_28 = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->field_28 = 1;
        }

        work->field_22 = 0;
        task->state    = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->field_2A    = f2a;
        work->field_10.vy = 0;
        rng2              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng2;
        work->field_10.vx = -(((u32)rng2 >> 16) & 0x7F);
        rng3              = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng3;
        vz                = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->field_10.vz = vz;
        VectorNormalSS(&work->field_10, &work->field_10);

        gte_lddp(work->field_2A);
        gte_ldsv(&work->field_10);
        gte_gpf12();
        gte_stsv(&work->field_10);
    }

    func_dryfield_main_street_8017EA88(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);

    coord->coord.t[0] += work->field_10.vx;
    coord->coord.t[1] += work->field_10.vy;
    coord->coord.t[2] += work->field_10.vz;
    coord->flg         = 0;

    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative and `otz` is at least 0x41, queues one
/// semi-transparent shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4383) rotated
/// about the projected centre. `arg1` selects a 48-texel UV tile in a 5-wide
/// grid: u = `(arg1 % 5) * 48`, v = `(arg1 / 5) * 48 - 0x80`. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 47 / otz`.
/// `arg3` is the spin angle, applied at `arg3` and `arg3 + 0x400` through
/// `rsin`/`rcos`.
void func_dryfield_main_street_8017EA88(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw39Scratch* block;
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
    block         = (RoomDraw39Scratch*)(head - 0x1C);
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    otzp          = &block->otz;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw39Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw39Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw39Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(otzp);
        if (((RoomDraw39Scratch*)(head - 0x1C))->otz >= 0x41) {
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
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * sine) >> 12;
            block->dy =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
            prim->x0 = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3 = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0 = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y3 = *(u16*)&block->sy + *(u16*)&block->dy;
            ang2     = ang + 0x400;
            block->dx =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rsin(ang2)) >> 12;
            block->dy =
                ((span / ((RoomDraw39Scratch*)(head - 0x1C))->otz) * rcos(ang2)) >> 12;
            prim->x1 = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x2 = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y1 = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y2 = *(u16*)&block->sy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)((RoomDraw39Scratch*)(head - 0x1C))->otz
                                 << gDisplayState.otDepthShift) >>
                                2) &
                               0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// A flash on an effect's anchor, lasting `spawnArg1` frames. State 1 grows a
/// warm glow twice over and a shrinking ring around it; when it ends it hands
/// the tint to `Gp_DrawFadeQuad`, and state 2 fades a two-ring glow back out
/// before the work block is released. The task also ends when the room's event
/// state reaches 4.
void func_dryfield_main_street_8017EEE8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_dryfield_main_street_8017F5B8(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_dryfield_main_street_8017F5B8(coord, (s16)((u16)work->angle * 2), rgb);
                func_dryfield_main_street_8017F18C(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_dryfield_main_street_801804BC(coord, (s16)((s16)work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
void func_dryfield_main_street_8017F18C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
void func_dryfield_main_street_8017F5B8(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
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
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// A beam between two anchors, the two entries of
/// `D_dryfield_main_street_80181BAC` on the effect's parent frame. State 0
/// allocates two eight-slot coordinate trails and fills both with the anchors'
/// frames; each later frame records the anchors into the next slot and draws
/// the trails with `func_dryfield_main_street_8017FE3C`. The work block is
/// released after `spawnArg1` frames. Nothing runs once the room's event state
/// reaches 2.
void func_dryfield_main_street_8017F94C(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_dryfield_main_street_80181BAC[0].vx;
                objCoord->coord.t[1] = D_dryfield_main_street_80181BAC[0].vy;
                objCoord->coord.t[2] = D_dryfield_main_street_80181BAC[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_dryfield_main_street_80181BAC[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_dryfield_main_street_80181BB4.vx;
                coord.coord.t[1] = D_dryfield_main_street_80181BB4.vy;
                coord.coord.t[2] = D_dryfield_main_street_80181BB4.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_main_street_8017FE3C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
void func_dryfield_main_street_8017FE3C(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// A burst on an effect's anchor. It spawns effect 0x60076 and then either,
/// with a non-zero `spawnArg1`, a spray of randomly moving 0x60070 sparks for
/// seven frames, or two 0x6007C effects and a widening, fading double ring for
/// seven frames; then the work block is released. The task also ends when the
/// room's event state reaches 4.
void func_dryfield_main_street_80180234(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_dryfield_main_street_8017F18C(objCoord, 0x100, 0x100, rgb);
            func_dryfield_main_street_8017F18C(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
void func_dryfield_main_street_801804BC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
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
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
