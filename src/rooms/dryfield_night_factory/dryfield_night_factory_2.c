#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

extern s16 D_80114D08;

/// The pending event message and request the gate latched, the flag saying
/// one was latched, and the descriptor of the task the gate spawns to play it.
extern RoomEventMsg D_dryfield_night_factory_8018A7D4;
extern RoomEventReq D_dryfield_night_factory_8018A7EC;
extern u8           D_dryfield_night_factory_8018A7DC;
extern TaskDesc     D_dryfield_night_factory_80186E40;

/// The spawn tables the room entry task selected for the stage variant -- the
/// one its poller spawns the script from, and the one the command handler and
/// the entry task spawn the room's actors from -- and the `memCalloc(4, 0)`
/// slot the poller parks the script task in.
extern TaskDesc* D_dryfield_night_factory_8018A7E0;
extern TaskDesc* D_dryfield_night_factory_8018A7E4;
extern Task**    D_dryfield_night_factory_8018A7E8;

extern TaskDesc   D_dryfield_night_factory_80186DE0[];
extern TaskDesc   D_dryfield_night_factory_80186E28[];
extern TaskDesc   D_dryfield_night_factory_80186E4C[];
extern GpMsgEntry D_dryfield_night_factory_80186E64[];
extern TaskDesc   D_dryfield_night_factory_80186EA0[];

/// The world-space points the room's three glow discs are drawn at.
extern SVECTOR D_dryfield_night_factory_80186F04;
extern SVECTOR D_dryfield_night_factory_80186F0C;
extern SVECTOR D_dryfield_night_factory_80186F14;

void func_dryfield_night_factory_801802C8(Task* task);
void func_dryfield_night_factory_80180438(Task* arg0);
void func_dryfield_night_factory_801809EC(Task* task);
void func_dryfield_night_factory_80180A4C(Task* task);
void func_dryfield_night_factory_80181538(s32 x, s32 y, s32 variant);
void func_dryfield_night_factory_80181938(Task* task);
void func_dryfield_night_factory_8018196C(Task* task);
void func_dryfield_night_factory_801819BC(Task* task);
void func_dryfield_night_factory_80181A24(Task* task);
void func_dryfield_night_factory_80181AB8(Task* task);

s32 func_dryfield_night_factory_80180164(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                              = req->flagId;
    D_dryfield_night_factory_8018A7DC = 0;
    neg                               = flag < 0;
    got                               = (s16)flag;
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
                D_dryfield_night_factory_8018A7D4 = *msg;
                D_dryfield_night_factory_8018A7EC = *req;
                id                                = req->flagId;
                mode                              = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_factory_80186E40, 0, 0, 0);
                D_dryfield_night_factory_8018A7DC = 1;
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

/// The event task the gate spawns: plays the latched request's cap command and
/// its two voice lines in turn, then warps to the area, warp point and room
/// the latched message names.
void func_dryfield_night_factory_801802C8(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_factory_8018A7EC.field_0);
            if (D_dryfield_night_factory_8018A7EC.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_factory_8018A7EC.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_factory_8018A7EC.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_factory_8018A7EC.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_factory_8018A7EC.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_factory_8018A7EC.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_night_factory_8018A7D4.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_night_factory_8018A7D4.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_night_factory_8018A7D4.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// State handlers of the room entry task: set-up, an empty tick and
/// `taskKill`.
const TaskFuncTable3 D_dryfield_night_factory_8017D638 = {
    { func_dryfield_night_factory_80180438, func_dryfield_night_factory_801809EC, taskKill },
};

/// Room entry task set-up: publish the room's message table in `Task::msgTable`
/// and claim game pointer slot 7, then park the `memCalloc` slot the poller
/// `func_dryfield_night_factory_8018076C` watches in it. Session variant
/// `gGameSession::at4.loc.stage == 2` (the night factory) picks the larger spawn
/// tables and the second progress-nibble interpretation; every other variant
/// picks the day set. The entry's own task and callback are spawned from the
/// selected table at index 4 and 5, then nibble 0x48 is read -- under variant
/// 2 through a branch whose two arms are the same call, which is why the
/// target keeps both copies of it.
///
/// `slot` and the store to `D_..._A7E8` are one chained assignment on purpose:
/// that makes GCC materialise the global's address ahead of `memCalloc`, so
/// the address quantity's live range spans the call. `local-alloc.c`'s
/// `QTY_CMP_PRI` divides by the range length, which drops it below the
/// `gGameSession` load quantity, and that load then wins `$v1` -- the target's
/// allocation. Split into two statements it takes `$v1` itself and the load
/// falls to `$a0`.
void func_dryfield_night_factory_80180438(Task* arg0)
{
    Task** slot;

    arg0->msgTable = D_dryfield_night_factory_80186E64;
    Game_SetPtrSlot(arg0, 7);
    slot       = (D_dryfield_night_factory_8018A7E8 = memCalloc(4, 0));
    arg0->work = (TaskIdMap*)slot;
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186E28;
    } else {
        D_dryfield_night_factory_8018A7E4 = D_dryfield_night_factory_80186DE0;
    }
    if (gGameSession->at4.loc.stage == 2) {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186E94;
    } else {
        D_dryfield_night_factory_8018A7E0 = D_dryfield_night_factory_80186EA0;
    }
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 4, 0, (s32)D_dryfield_night_factory_8018A7E8);
    Task_SpawnFromTable(D_dryfield_night_factory_8018A7E4, 5, 0, 0);
    if (gGameSession->at4.loc.stage == 2) {
        func_dryfield_night_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
        SOFT_BARRIER();
    } else {
        func_dryfield_night_factory_80181620(GameFlag_GetNibble(0x48) & 0xFF);
    }
    arg0->state++;
}

/// Filters a warp request: copies `in` to `out`, choosing the destination room
/// for area 0x19 from the stage variant and progress flags, and for area 0x18
/// from game flag 0x7A. Area 0x18 is refused with cap slot 4 until game flag
/// 0x4A reaches 2, area 0x16 with cap command 0xD while game flag 0x37 is
/// clear, and area 0x19 goes through the event gate with the room's own
/// request. Any other warp answers 1.
s32 func_dryfield_night_factory_80180574(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u8           variant;

    *out = *in;
    if (in->msgId == 0x19) {
        variant = gGameSession->at4.loc.stage;
        if (variant == 2) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = variant;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->msgId == 0x18) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(0x7A) < 4) {
                out->field_3 = 1;
            } else {
                out->field_3 = 2;
            }
        }
        if (in->msgId == 0x18) {
            if (GameFlag_GetNibble(0x4A) != 2) {
                if (in->field_5 != 0) {
                    return 0;
                }
                Gp_StartCapSlot(4, 1, 0);
                Gp_SetNibbleIf(in->field_6, 2);
                return 0;
            }
        }
    }
    if (in->msgId == 0x16) {
        if (GameFlag_GetNibble(0x37) == 0) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(0xD);
            return 0;
        }
    }
    if (in->msgId == 0x19) {
        req.field_0 = 0xE;
        req.field_4 = 0xE;
        req.field_8 = 0x52170013;
        req.field_C = 0x52170003;
        req.flagId  = -0x30;
        req.itemId  = 0;
        return func_dryfield_night_factory_80180164(&req, in);
    }
    return 1;
}

/// Spawns the script task from the table the room entry task selected, parks it
/// in the entry task's slot, and kills itself once that task has gone.
void func_dryfield_night_factory_8018076C(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_night_factory_8018A7E8 = Task_SpawnFromTable(D_dryfield_night_factory_8018A7E0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_night_factory_8018A7E8, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

s32 func_dryfield_night_factory_8018080C(void)
{
    return 0;
}

/// Command handler for the night factory room, reached from the room's command
/// table (`D_dryfield_night_factory_80186E64`, id 0x13F0) with the command in
/// `$a2`.
///
/// Cases 1/2/3/5/12 spawn an actor out of whichever spawn table the session
/// selected (`D_..._A7E4`, written by `func_dryfield_night_factory_80180438`)
/// at index 2/3/1/0/6, handing the command on as `Task_SpawnFromTable`'s third
/// argument. Case 6 silences both characters' weapons and spawns the factory's
/// own table `D_..._80186E4C` at index 0 instead -- that table's task is the
/// `func_dryfield_night_factory_8018076C` poller. Case 12 only acts while
/// progress flag 0x49 is 1, and silences the player's and the ally's weapon
/// before spawning. Every other command does nothing.
///
/// The `goto`s are the target's shape: every path shares the single `return 0`
/// at `end`, so the exit block is the only place `$v0` is zeroed.
s32 func_dryfield_night_factory_80180814(s32 arg0, s32 arg1, s32 cmd)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 2;
            break;
        case 2:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 3;
            break;
        case 3:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 1;
            break;
        case 5:
            table = D_dryfield_night_factory_8018A7E4;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(D_dryfield_night_factory_80186E4C, 0, 0, 0);
            goto end;
        case 12:
            if (GameFlag_GetNibble(0x49) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = D_dryfield_night_factory_8018A7E4;
                idx   = 6;
                break;
            }
            goto end;
        default:
            goto end;
    }
    Task_SpawnFromTable(table, idx, cmd, 0);
end:
    return 0;
}

/// Message handler: command 7 plays sound 0x52170007, and command 21 plays
/// 0x52170015 and sets game flag 0x4A to 2.
s32 func_dryfield_night_factory_80180914(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            Gp_EnqueueStageSnd6(0x52170007, 0, 0);
            break;
        case 21:
            Gp_EnqueueStageSnd6(0x52170015, 0, 0);
            GameFlag_SetNibble(0x4A, 2);
            break;
    }
    return 0;
}

/// Message handler: the first message with `field_2` 1 while game flag 0x2C is
/// clear starts cap 0xB, sets the flag and plays sound 0x5217000A.
s32 func_dryfield_night_factory_80180980(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x2C) == 0)) {
        Gp_SpawnIfCapIdle(0xB, 1);
        GameFlag_SetNibble(0x2C, 1);
        func_800E3FAC(0xA2, 0xA);
        SndEvt_EnqueueType6(0x5217000A, 0, 0);
    }
    return 0;
}

/// The room entry task's per-frame state, which does nothing.
void func_dryfield_night_factory_801809EC(Task* task)
{
}

/// Runs the room entry task's current state, through a copy of its handler
/// table on the stack.
void func_dryfield_night_factory_801809F4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_factory_8017D638;
    sp.funcs[task->state](task);
}

/// Idle state of the room's script, state 2 of
/// `D_dryfield_night_factory_8017D678`. It holds the prompt idle for the
/// `field_8` frames the prompt states armed -- decrementing that countdown
/// first and bailing out while it is still non-zero or while a cap is playing
/// -- and otherwise hit-tests the room's hotspot table.
///
/// A confirmed hit (`buttons[0].state == 2`) copies the hotspot's `id` and
/// `promptKind` into the work block and advances to state 3; with nothing under
/// the cursor the prompt merely highlights (`mode` 1). `buttons[1].state == 2`
/// leaves the scan by advancing to state 5.
void func_dryfield_night_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    OverlayHotspot*         hs     = D_dryfield_night_factory_80186EBC;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_dryfield_night_factory_80181778(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// State handlers of the room's script task, run by
/// `func_dryfield_night_factory_8018169C`: set-up, prompt arming, the idle
/// hotspot scan, prompt spawning, the prompt state, the exit and the wait for
/// the message handler's trigger.
const TaskFuncTable7 D_dryfield_night_factory_8017D678 = {
    {
        func_dryfield_night_factory_8018182C,
        func_dryfield_night_factory_80181938,
        func_dryfield_night_factory_80180A4C,
        func_dryfield_night_factory_8018196C,
        func_dryfield_night_factory_801819BC,
        func_dryfield_night_factory_80181A24,
        func_dryfield_night_factory_80181AB8,
    },
};

/// Outlines `rect` in (`r`, `g`, `b`): four flat `LINE_F2`s along its top,
/// right, bottom and left edges, each linked into `gGpuCurrentOt[1]`.
void func_dryfield_night_factory_80180BA4(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

void func_dryfield_night_factory_80180DE8(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (GameFlag_GetNibble(0x48) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (!(GameFlag_GetNibble(0x49) & 2)) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) | 2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(8, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (GameFlag_GetNibble(0x49) & 2) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) & ~2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(9, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) ^ 1);
                if (GameFlag_GetNibble(0x47) == 0) {
                    Mc_SaveData.at4.loc.view = 0x12;
                } else {
                    Mc_SaveData.at4.loc.view = 0x13;
                }
                state       = 6;
                task->state = state;
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 1);
                state       = 2;
                task->state = state;
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                state       = 2;
                task->state = state;
                break;
        }
    } else {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->at4.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(0xA, 0, 0);
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 0);
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                break;
        }
        task->state = 2;
    }
}

void func_dryfield_night_factory_801810D8(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_dryfield_night_factory_80181538(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Draws the action-prompt cursor icon at (`x`, `y`): one 16x24 textured quad
/// linked into the head of the current OT, with palette 0x3C87 for `variant` 2
/// and 0x3C88 otherwise. `variant` 0 draws nothing.
void func_dryfield_night_factory_80181538(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

void func_dryfield_night_factory_80181620(s32 show)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(show & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Runs the script task's current state. The seven handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_dryfield_night_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_night_factory_8017D678;
    sp.funcs[task->state](task);
}

/// The prompt task the script spawns: resets both action-prompt slots, then
/// moves and draws the cursors every frame.
void func_dryfield_night_factory_80181718(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_factory_80181BB4, func_dryfield_night_factory_801810D8 };

    states[task->state](task);
}

/// Script message handler: raises the one-shot trigger the cursor state
/// consumes.
void func_dryfield_night_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}

s32 func_dryfield_night_factory_80181778(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

void func_dryfield_night_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    OverlayHotspot*         hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_dryfield_night_factory_80186E94, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    task->msgTable  = D_dryfield_night_factory_80186EAC;
    if (GameFlag_GetNibble(0x48) == 0) {
        Mc_SaveData.at4.loc.view = 0xC;
    } else {
        Mc_SaveData.at4.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_night_factory_80186EBC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}

/// Script state: highlights the action prompt (`mode` 1, target id 0x80),
/// clears its screen position and steps the script on one state.
void func_dryfield_night_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Script state: drops the prompt's highlight and spawns the action prompt at
/// the cursor position with the display mode of the confirmed hotspot, then
/// moves the script to state 4.
void func_dryfield_night_factory_8018196C(Task* task)
{
    RoomActionPrompt*       prompt = &D_80114D28;
    NightFactoryScriptWork* work   = (NightFactoryScriptWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}

/// Runs the prompt state of the night factory script: drops the highlight the
/// previous state left in `D_80114D28` and, while `func_800D4EC0` still reports
/// a prompt on screen, hands the task to the cap step `field_C` names. Once the
/// prompt is gone the task advances to state 2 instead, and either way the work
/// block's `field_8` is set to 0xA.
void func_dryfield_night_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_night_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}

/// Script state that ends the scene: gives the player back their weapon and
/// the HUD, releases the display, kills the prompt task and asks for this one
/// to be killed.
void func_dryfield_night_factory_80181A24(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

/// Script state that waits for the one-shot trigger: keeps the prompt hidden
/// and, once `field_A` is raised, consumes it, re-arms the countdown and goes
/// back to the idle state.
void func_dryfield_night_factory_80181AB8(Task* task)
{
    RoomActionPrompt*       prompt;
    NightFactoryScriptWork* work;

    prompt           = &D_80114D28;
    work             = (NightFactoryScriptWork*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            Mc_SaveData.at4.loc.view = 0xC;
        } else {
            Mc_SaveData.at4.loc.view = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}

void func_dryfield_night_factory_80181B38(s32 show)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(show & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

void func_dryfield_night_factory_80181BB4(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when it lands in front of
/// the camera, draws a disc of sixteen gouraud wedges and a four-pointed inner
/// cross around it. The radii are `(s16)arg1` scaled by 64 and 8 over the
/// depth. `arg2` packs the tint as four nibbles `[shift][r][g][b]`; bit 0 of
/// the animation frame, shifted by the top nibble, is added to every channel
/// so the disc flickers on alternate frames.
void func_dryfield_night_factory_80181C14(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                packed;
    s32                blend;
    s32                size;
    s32                otz;
    s32                rOuter;
    s32                rInner;
    s32                ang;
    s32                t;
    s32                t2;
    s32                r;
    s32                g;
    s32                b;
    s32                rh;
    s32                gh;
    s32                bh;

    {
        void** scratch;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        block   = (RoomDraw05Scratch*)(*scratch = head - 0x14);
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        size          = (s16)arg1;
        otz           = block->otz + 1;
        rOuter        = (size * 64) / otz;
        block->otz    = otz;
        ds            = &gDisplayState;
        blend         = ds->animFrame;
        block->rOuter = rOuter;
        rInner        = (size * 8) / block->otz;
        packed        = arg2 << 16;
        blend         = blend & 1;
        blend         = blend << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = rInner;
        ang           = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            rh = (u8)r >> 1;
            gh = (u8)g >> 1;
            bh = (u8)b >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rh, gh, bh);
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
            setRGB2(prim, r, g, b);
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

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang - 0x400)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ang - 0x400)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x400)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x400)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang + 0x400)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang + 0x400)) >> 11);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x800)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x800)) >> 12);
            ang     += 0x800;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Per-frame effect: refreshes the world matrix of the task's model, then draws
/// one of three glowing discs at fixed points in the room. The draw set is
/// selected by the stage-visit byte `gGameSession->at4.loc.view` taken as a bit
/// index, and each of the three groups also gates on a story flag, so a disc
/// only appears on the visits and after the event that the flag records.
void func_dryfield_night_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(task->extra.tmd->coords);
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        func_dryfield_night_factory_80181C14(&D_dryfield_night_factory_80186F04, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            func_dryfield_night_factory_80181C14(&D_dryfield_night_factory_80186F0C, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            func_dryfield_night_factory_80181C14(&D_dryfield_night_factory_80186F14, 0x80, 0x50A0);
        }
    }
}
