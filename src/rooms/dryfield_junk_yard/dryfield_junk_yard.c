#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Block `func_dryfield_junk_yard_8017D658` carves off the scratch stack
/// (`0x1F8003FC`, one `addiu` of `-0x18`) to hold the model's world position
/// while `Gp_DrawEffGroundQuad` draws the ground quad there. Only `pos` is
/// written; the rest of the 0x18 bytes is not touched here.
typedef struct {
    VECTOR3 pos;
    byte    pad_C[0xC];
} DjyGroundQuadScratch;
STATIC_ASSERT_SIZEOF(DjyGroundQuadScratch, 0x18);

/// Resident routine at the fixed address `0x80724608`, outside every image
/// the build links. The room hands it the slot-0xA game pointer, two
/// constants and the `"DOG"` name below; what it does with them is unproven.
void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// Main-executable halfword the room task's second state waits on before it
/// calls `func_80724608`.
/// Signed byte of gameplay state the room's script callback stores into.

/// The room's message table, published in `Task::msgTable` for
/// `Gp_DispatchMsg` to walk.
extern GpMsgEntry D_dryfield_junk_yard_8017DD20[];
extern TaskDesc   D_dryfield_junk_yard_8017DD48[];
extern s32        D_dryfield_junk_yard_8017DD88;
extern s32        D_dryfield_junk_yard_8017DDD8;
extern s32        D_dryfield_junk_yard_8017DDEC;
extern s32        D_dryfield_junk_yard_8017DE00;
extern s32        D_dryfield_junk_yard_8017DE18;
extern s32        D_dryfield_junk_yard_8017DE30;
extern s32        D_dryfield_junk_yard_8017DE48;
extern s32        D_dryfield_junk_yard_8017E028;
extern s32        D_dryfield_junk_yard_8017E160;
extern s32        D_dryfield_junk_yard_8017E2B0;
extern s32        D_dryfield_junk_yard_8017E3D0;
extern s32        D_dryfield_junk_yard_8017E490;
extern s32        D_dryfield_junk_yard_8017E658;

void func_dryfield_junk_yard_8017D658(Task* task);
void func_dryfield_junk_yard_8017D708(Task* arg0);
void func_dryfield_junk_yard_8017DC60(Task* task);

/// The room task's states: set up, start the named sequence once the stream
/// is ready, then `taskKill`.
const TaskFuncTable3 D_dryfield_junk_yard_8017D5C4 = {
    { func_dryfield_junk_yard_8017D708, func_dryfield_junk_yard_8017DC60, taskKill },
};

/// Name the room task's second state hands to `func_80724608`.
const char D_dryfield_junk_yard_8017D5D0[] = "DOG";

/// Model task tick: reads the 2-bit game flag named by the spawn object's
/// `field_8`, clears the model's flags and sets them to 0x84 when the flag
/// reads 2 (otherwise zeroing `otOffset`), then runs the model's draw below.
void func_dryfield_junk_yard_8017D5F4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj        = (GpItemObj8*)task->spawnArg2;
    tmd        = task->extra.tmd;
    flag       = Gp_GetCurBit2Flag(obj->field_8);
    tmd->flags = 0;
    if (flag == 2) {
        tmd->flags = 0x84;
    } else {
        tmd->otOffset = 0;
    }
    func_dryfield_junk_yard_8017D658(task);
}

/// Model draw: unless the model's flags carry bit 0x80 or it has no buffer
/// yet, refreshes its world matrix and draws a 0x1A0 by 0xC0 ground-effect
/// quad at its world position.
void func_dryfield_junk_yard_8017D658(Task* task)
{
    DjyGroundQuadScratch* scratch;
    GpCoord*              coord;
    TmdObject*            tmd;

    tmd   = task->extra.tmd;
    coord = tmd->coords;
    if ((tmd->flags & 0x80) == 0 && tmd->buffer != 0) {
        scratch                            = SCRATCH_HEAD(DjyGroundQuadScratch) - 1;
        SCRATCH_HEAD(DjyGroundQuadScratch) = scratch;
        Gp_UpdateCoord(coord);
        scratch->pos.vx = coord->workm.t[0];
        scratch->pos.vy = coord->workm.t[1];
        scratch->pos.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1A0, 0xC0);
        SCRATCH_POP(DjyGroundQuadScratch);
    }
}

/// State 0 of the room task: publishes the message table and claims game
/// pointer slot 7. With a slot-0xA task present, it sends that task its
/// opening messages while nibble 0x38 is clear, then either latches nibble
/// 0x39 and starts the `func_800E8634` sequence (once nibble 0x28 has reached
/// 2) or, on a visit whose `warp` is 2, sends it message 0x3E9. Advances the
/// state either way.
void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->msgTable = D_dryfield_junk_yard_8017DD20;
    Game_SetPtrSlot(arg0, 7);
    if (gameGetPtrSlot(0xA) != 0) {
        if (GameFlag_GetNibble(0x38) == 0) {
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE00, 0);
            Gp_AllyAnimId(&D_dryfield_junk_yard_8017DD88);
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((GameFlag_GetNibble(0x39) == 0) && (GameFlag_GetNibble(0x28) >= 2)) {
            GameFlag_SetNibble(0x39, 1);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E490, 0, (s32)&D_dryfield_junk_yard_8017E658);
        } else if (gGameSession->at4.loc.warp == 2) {
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The sequence task `D_dryfield_junk_yard_8017DD48` describes, spawned by
/// the 0x13EF handler. State 0 starts a `func_800E8634` sequence and state 1
/// waits for `gGameSession->eventState` to clear. States 3, 5 and 7 send the
/// slot-0xA task a message (`0x3EE`, `0x3E8`, `0x3E8`) with a payload; states
/// 4 and 6 send the bare `0x3F0` / `0x3ED` and hold while it answers nonzero.
/// State 7, and state 2 directly, end in `taskKill`.
///
/// Every case writes its own `task->state + 1; return;`: cross jumping folds
/// those identical tails into the one increment block, and folds cases 4 and
/// 6's `Gp_DispatchMsg(..., 0, 0)` into one call.
void func_dryfield_junk_yard_8017D848(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634((s32)&D_dryfield_junk_yard_8017DE48, 0, (s32)&D_dryfield_junk_yard_8017E028);
            task->state = task->state + 1;
            return;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3EE, (s32)&D_dryfield_junk_yard_8017DE18, 0);
            task->state = task->state + 1;
            return;
        case 4:
            if (Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3F0, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 5:
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DDD8, 0);
            task->state = task->state + 1;
            return;
        case 6:
            if (Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3ED, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 7:
            Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DDEC, 0);
            /* fallthrough */
        case 2:
            taskKill(task);
            return;
    }
}

/// Handler for message 0x13F0 in the room's message table, keyed by `arg2`.
/// Point 6 plays CAP command 0xC until nibble 0x3A is set, and 6 after. Point 8
/// plays command 9 unless bit flag 0x1C is set; with it set, command 8 plays
/// only while nibble 0x73 is still clear and 0x7C is set, and otherwise the
/// point's own CAP slot starts. Always returns 0.
s32 func_dryfield_junk_yard_8017D994(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 6:
            Gp_RunCapCmd1(GameFlag_GetNibble(0x3A) <= 0 ? 0xC : 6);
            break;
        case 8:
            SOFT_TOUCH_REG(arg2);
            if (Gp_GetCurBit2Flag(0x1C) == 1) {
                if (GameFlag_GetNibble(0x73) != 0) {
                    Gp_StartCapSlot(arg2, 1, 0);
                } else if (GameFlag_GetNibble(0x7C) != 0) {
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
s32 func_dryfield_junk_yard_8017DA44(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. Copies the
/// incoming record to the outgoing one, then edits the copy: message 0x18
/// answers `field_3` 2 once nibble 0x7A has reached 4, else 1. Message 0x1B,
/// while nibble 0x38 is 1, returns 2, advancing the nibble to 2 and starting a
/// `func_800E8634` sequence; otherwise, with nibble 0x28 still clear, it
/// answers `field_2` 2 and sets nibbles 0x28 and 0x4B. Returns 1.
///
/// `field_5` non-zero means "report only", which suppresses every side effect.
s32 func_dryfield_junk_yard_8017DA4C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0x18 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
    }
    if (in->msgId == 0x1B) {
        if (GameFlag_GetNibble(0x38) == 1) {
            if (in->field_5 == 0) {
                GameFlag_SetNibble(0x38, 2);
                func_800E8634((s32)&D_dryfield_junk_yard_8017E3D0, 0, (s32)&D_dryfield_junk_yard_8017E2B0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x28) == 0 && in->field_5 == 0) {
            out->field_2 = 2;
            GameFlag_SetNibble(0x28, 1);
            GameFlag_SetNibble(0x4B, 4);
        }
    }
    return 1;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `field_2` is 1 and nibble 0x38 is clear, it latches the nibble to 1 and
/// spawns the sequence task. When it is 2, the slot-0xA task stands at x
/// 0x5209 or beyond and nibble 0x38 is 1, it advances the nibble to 2 and
/// starts a `func_800E8634` sequence. Always returns 0.
s32 func_dryfield_junk_yard_8017DB78(Task* task, s32 msgId, GpMsg13EF* msg)
{
    Task* player;

    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x38) == 0) {
            GameFlag_SetNibble(0x38, 1);
            Task_SpawnFromTable(D_dryfield_junk_yard_8017DD48, 0, 0, 0);
        }
    }
    if (msg->field_2 == 2) {
        player = gameGetPtrSlot(0xA);
        if ((player != NULL) && (player->extra.tmd->coords->coord.t[0] >= 0x5209) &&
            (GameFlag_GetNibble(0x38) == 1)) {
            GameFlag_SetNibble(0x38, 2);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E160, 0, (s32)&D_dryfield_junk_yard_8017E2B0);
        }
    }
    return 0;
}

/// Room script callback, named by two of the room's script records (command
/// 0xD, argument 5): stores its argument into `Mc_SaveData.sceneEvent`.
void func_dryfield_junk_yard_8017DC54(s8 arg0)
{
    Mc_SaveData.sceneEvent = arg0;
}

/// State 1 of the room task: once `gDisplayState.field_112` is non-zero and a slot-0xA
/// task exists, calls `func_80724608` on that task with the `"DOG"` name. The
/// state never advances, so it repeats every frame.
void func_dryfield_junk_yard_8017DC60(Task* task)
{
    if ((gDisplayState.field_112 != 0) && (gameGetPtrSlot(0xA) != 0)) {
        func_80724608(gameGetPtrSlot(0xA), -0x8C, 0xA, D_dryfield_junk_yard_8017D5D0);
    }
}

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_junk_yard_8017DCB4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_junk_yard_8017D5C4;
    sp.funcs[task->state](task);
}

/// Sets the room effect mode to 2.
void func_dryfield_junk_yard_8017DD0C(void)
{
    Gp_State1C->roomEffectMode = 2;
}
