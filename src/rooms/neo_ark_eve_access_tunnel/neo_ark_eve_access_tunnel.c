#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_eve_access_tunnel.h"
#include "rooms/room.h"

/// Parameter block of `func_neo_ark_eve_access_tunnel_8017D6D4`, the room-local
/// resolver `func_neo_ark_eve_access_tunnel_8017D980` calls with one pointer as
/// both its input and its output.
///
/// `field_0` is the code the resolver switches on, `field_2` passes through
/// unchanged, `field_3` is the byte it writes, and a non-zero `field_5` makes it
/// return without touching anything. The caller stages the block from the
/// `RoomDeparture` it is about to publish and copies `field_3` back into it.
typedef struct NaetUtilParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
} NaetUtilParam;
STATIC_ASSERT_SIZEOF(NaetUtilParam, 0x6);

/// Descriptor the tunnel's outgoing task is spawned from, index 0 of the table
/// `func_neo_ark_eve_access_tunnel_8017D980` hands `Task_SpawnFromTable`.
extern TaskDesc D_neo_ark_eve_access_tunnel_8017EA88;

/// The staged event descriptor, read by the task spawned above.
extern RoomDeparture D_neo_ark_eve_access_tunnel_801807A8;

/// Scene id byte; the tunnel stamps 0x18 when it hands the save location off.
extern s8 D_8007272D;

/// CAP countdown the tunnel reloads while its sequences run.
extern s16 D_80114D08;

/// Set when the tunnel's save is written to the memory card.

/// Staging save location the room commits when the tunnel's save is taken:
/// `field_2` / `field_4` / `field_1` hold what `func_neo_ark_eve_access_tunnel_8017DB18`
/// later copies into `Mc_SaveData.at4.loc.area` / `warp` / `room`.
extern GpSaveLoc D_neo_ark_eve_access_tunnel_801807A0;

/// The room's message table, installed in `Task::msgTable` by the room task's
/// first state. Terminated by the `0x7FFFFFFF` id.
extern GpMsgEntry D_neo_ark_eve_access_tunnel_8017EA94[];

/// The room's three task descriptors: the departure sequence, the save
/// sequence and the CAP-wait flag setter, spawned by the message handlers.
extern TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4[];

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

void func_neo_ark_eve_access_tunnel_8017DF24(Task* arg0);
void func_neo_ark_eve_access_tunnel_8017DFC0(Task* task);

/// Resolves the code in `arg0->field_0` into a state byte in `arg1->field_3`,
/// unless `arg0->field_5` is set. Only six codes produce one, each from a
/// game-flag nibble: codes 5, 41 and 45 take their nibble plus one, codes 2, 16
/// and 20 fold theirs into a fixed set of states. Every other code leaves
/// `arg1` untouched. Always returns 1.
s32 func_neo_ark_eve_access_tunnel_8017D6D4(NaetUtilParam* arg0, NaetUtilParam* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->field_3 = 3;
                }
                break;
            case 5:
                arg1->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        arg1->field_3 = 1;
                        break;
                    case 1:
                        arg1->field_3 = 6;
                        break;
                    case 2:
                        arg1->field_3 = 7;
                        break;
                    case 3:
                        arg1->field_3 = 8;
                        break;
                    default:
                        arg1->field_3 = 1;
                        break;
                }
                break;
            case 45:
                arg1->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// The tunnel's outgoing task, run on the descriptor staged in
/// `D_neo_ark_eve_access_tunnel_801807A8`. State 0 sends the descriptor's
/// `facing` to the task in pointer slot 3 as message 0x3EE, skipping to state 2
/// when it is -1; state 1 waits until that task answers 0x3F0 with 0. States
/// 2 and 3 queue the sound event `sndEvent`, if any, and wait for its voice to go
/// quiet. State 4 queues type-7 sound event 0x80000000, commits the save
/// location in the descriptor's first four bytes (stage, area, warp, room) to
/// `Mc_SaveData`, spawns task type 0x11 and ends the task.
void func_neo_ark_eve_access_tunnel_8017D810(Task* arg0)
{
    GpXformArg msg;
    Task*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_neo_ark_eve_access_tunnel_801807A8.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_neo_ark_eve_access_tunnel_801807A8.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_neo_ark_eve_access_tunnel_801807A8.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_neo_ark_eve_access_tunnel_801807A8.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.stage = D_neo_ark_eve_access_tunnel_801807A8.stage;
            Mc_SaveData.at4.loc.area  = D_neo_ark_eve_access_tunnel_801807A8.area;
            Mc_SaveData.at4.loc.warp  = D_neo_ark_eve_access_tunnel_801807A8.warp;
            Mc_SaveData.at4.loc.room  = D_neo_ark_eve_access_tunnel_801807A8.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room task's three states: install the message table, adjust the views
/// each frame, and end the task. `func_neo_ark_eve_access_tunnel_8017E038` runs
/// them through a stack copy.
const TaskFuncTable3 D_neo_ark_eve_access_tunnel_8017D688 = {
    func_neo_ark_eve_access_tunnel_8017DF24,
    func_neo_ark_eve_access_tunnel_8017DFC0,
    taskKill,
};

/// Tunnel departure sequence, advanced one step per call: step 0 raises CAP
/// command 3, step 1 waits for the CAP system to go idle, step 2 arms the CAP
/// countdown at 0xA and waits for the event key it answers with - 0xC kills the
/// sequence and messages the player weapon - and step 3 falls through to the
/// shared advance. Step 4 stages `D_neo_ark_eve_access_tunnel_801807A8` (the
/// message halfword 0x800 and the code in the task's `spawnArg1`, run once more
/// through the room's resolver, and no sound) and spawns the tunnel's outgoing
/// task, whose callback is `func_neo_ark_eve_access_tunnel_8017D810`.
void func_neo_ark_eve_access_tunnel_8017D980(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(3);
            goto L_advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            goto L_advance;
        case 2:
            D_80114D08 = 0xA;
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            goto L_advance;
        case 3:
        L_advance:
            task->state++;
            return;
        case 4: {
            RoomDeparture  work;
            NaetUtilParam  param;
            RoomDeparture* wp;
            s32            (*resolve)(NaetUtilParam*, NaetUtilParam*) = func_neo_ark_eve_access_tunnel_8017D6D4;

            work.stage    = 4;
            work.area     = (u8)task->spawnArg1;
            work.room     = 1;
            work.warp     = 2;
            work.sndEvent = 0;
            work.facing   = 0x800;
            Gp_MsgPlayerWeapon(0);
            wp            = &work;
            param.field_0 = wp->area;
            param.field_2 = wp->warp;
            param.field_3 = wp->room;
            param.field_5 = 0;
            resolve(&param, &param);
            wp->area                             = param.field_0;
            wp->warp                             = param.field_2;
            wp->room                             = param.field_3;
            D_neo_ark_eve_access_tunnel_801807A8 = work;
            Task_SpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EA88, 0, 0, 0);
            taskKill(task);
            break;
        }
    }
}

/// Tunnel save sequence, advanced one step per call: step 0 raises CAP command
/// 2, step 1 waits for the CAP system to go idle, step 2 latches the save flag
/// into CAP and waits for the event key it answers with, step 3 waits for the
/// queued sound to finish, and step 4 commits the staged save location to
/// `Mc_SaveData` and spawns the outgoing task.
void func_neo_ark_eve_access_tunnel_8017DB18(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(2);
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            D_80114D08 = 0xA;
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            Gp_MsgPlayerWeapon(0);
            task->state++;
            SndEvt_EnqueueType6(0x55080003, 0, 0);
            return;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x55080003);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_neo_ark_eve_access_tunnel_801807A0.field_2;
            Mc_SaveData.at4.loc.warp  = D_neo_ark_eve_access_tunnel_801807A0.field_4;
            Mc_SaveData.at4.loc.room  = D_neo_ark_eve_access_tunnel_801807A0.field_1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message handler for id 0x13F1 in the room's message table: accepts the
/// message and does nothing.
s32 func_neo_ark_eve_access_tunnel_8017DC64(void)
{
    return 0;
}

/// Tunnel message handler. Message 9 either raises the CAP command that opens
/// the tunnel (nibble 0xB9 still clear) or, once that nibble is set, latches the
/// save location the outgoing message carries and starts the cutscene that
/// leads to the EVE encounter. The two `switch`es are load-bearing: the
/// equivalent `if` / `else` chain makes reorg fill the second field_5 branch's
/// delay slot from the return block instead of the fall-through.
s32 func_neo_ark_eve_access_tunnel_8017DC6C(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    switch (*(u16*)src) {
        case 9:
            switch (GameFlag_GetNibble(0xB9)) {
                case 0:
                    if (src->field_5 == 0) {
                        Gp_SetNibbleIf(src->field_6, 2);
                        Gp_RunCapCmd1(1);
                    }
                    break;
                default:
                    if (src->field_5 == 0) {
                        D_8007272D                                   = 0x18;
                        D_neo_ark_eve_access_tunnel_801807A0.field_2 = dst->field_0;
                        D_neo_ark_eve_access_tunnel_801807A0.field_4 = dst->field_2;
                        D_neo_ark_eve_access_tunnel_801807A0.field_1 = dst->field_3;
                        Gp_MsgPlayerWeapon(0);
                        Task_SpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 1, 0, 0);
                    }
                    break;
            }
            return 0;
    }
    return 1;
}

s32 func_neo_ark_eve_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (gGameSession->at4.loc.place == 0xB) {
        switch (arg2) {
            case 6:
                if (GameFlag_GetNibble(0x142) == 0) {
                    if (Gp_StateF0.field_0 == 1) {
                        Gp_RunCapCmd1(6);
                    }
                } else {
                    Gp_RunCapCmd1(8);
                }
                break;
            case 7:
                if (GameFlag_GetNibble(0x143) == 0) {
                    if (Gp_StateF0.field_0 == 1) {
                        Gp_RunCapCmd1(7);
                    }
                } else {
                    Gp_RunCapCmd1(9);
                }
                break;
        }
    }
    return 0;
}

s32 func_neo_ark_eve_access_tunnel_8017DE1C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0xA) {
        if (GameFlag_GetNibble(0xF8) != 0) {
            Gp_RunCapCmd1(5);
            Task_SpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 2, 0x1AF, 0);
        } else {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_neo_ark_eve_access_tunnel_8017EAC4, 0, arg2->field_3, 0);
        }
        return 0;
    }
    return 0;
}

s32 func_neo_ark_eve_access_tunnel_8017DE9C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x55080000 | 1, 0, 0);
    }
    return 0;
}

/// Third entry of the room's task table: waits for the CAP command to finish,
/// then, unless it ended on event key 0xC, sets the game-flag nibble named by
/// the task's spawn argument to 2, and ends the task.
void func_neo_ark_eve_access_tunnel_8017DED0(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() != 0xC) {
            GameFlag_SetNibble(arg0->spawnArg1, 2);
        }
        taskKill(arg0);
    }
}

/// State 0 of the tunnel's message task: park the room's message table in
/// `Task::msgTable` and publish the task in pointer slot 7, as every room-entry
/// task does. Then, once the session has reached state 0xB, set bit 15 of every
/// 16-bit half of the 0x25800-byte image buffer and latch `GameSession::flowFlags`
/// bit 0 - the flag that suppresses the bank-load spawn when the task ends.
void func_neo_ark_eve_access_tunnel_8017DF24(Task* arg0)
{
    arg0->msgTable = D_neo_ark_eve_access_tunnel_8017EA94;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 0xB) {
        u16* ptr = (u16*)Fs_ImgBuffers;
        s32  i   = 0;

        do {
            *ptr = (u16)(*ptr | 0x8000);
            i   += 1;
            ptr += 1;
        } while (i <= 0x12BFF);
        gGameSession->flowFlags = 1;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the tunnel's message task, run every frame: while the session is
/// below state 4 it sets both runs of view flags, and at state 0xB it sets the
/// CD command queue's `field_22A` to 2.
void func_neo_ark_eve_access_tunnel_8017DFC0(Task* task)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (gGameSession->at4.loc.place < 4U) {
        func_neo_ark_eve_access_tunnel_8017E090(0, 0);
        func_neo_ark_eve_access_tunnel_8017E090(1, 0);
    }
    if (gGameSession->at4.loc.place == 0xB) {
        queue->field_22A = 2;
    }
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_neo_ark_eve_access_tunnel_8017E038(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_access_tunnel_8017D688;
    sp.funcs[task->state](task);
}
