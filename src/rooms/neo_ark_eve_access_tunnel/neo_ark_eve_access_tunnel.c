#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_eve_access_tunnel.h"

/// Staging save location the room commits when the tunnel's save is taken.
extern GpSaveLoc D_neo_ark_eve_access_tunnel_801807A0;

/// CAP countdown the tunnel reloads while the save sequence runs.
extern s16 D_80114D08;

/// Set when the tunnel's save is written to the memory card.
extern s16 D_80071076;

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
/// `field_4` to the task in pointer slot 3 as message 0x3EE, skipping to state 2
/// when it is 0xFFFF; state 1 waits until that task answers 0x3F0 with 0. States
/// 2 and 3 queue the sound event `field_8`, if any, and wait for its voice to go
/// quiet. State 4 queues type-7 sound event 0x80000000, commits the save
/// location in the descriptor's first four bytes (stage, area, warp, room) to
/// `Mc_SaveData`, spawns task type 0x11 and ends the task.
void func_neo_ark_eve_access_tunnel_8017D810(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_neo_ark_eve_access_tunnel_801807A8.field_4;
            if (msg.field_12 == -1) {
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
            if (D_neo_ark_eve_access_tunnel_801807A8.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_neo_ark_eve_access_tunnel_801807A8.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_neo_ark_eve_access_tunnel_801807A8.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_neo_ark_eve_access_tunnel_801807A8.field_0;
            Mc_SaveData.at4.loc.area  = D_neo_ark_eve_access_tunnel_801807A8.field_1;
            Mc_SaveData.at4.loc.warp  = D_neo_ark_eve_access_tunnel_801807A8.field_2;
            Mc_SaveData.at4.loc.room  = D_neo_ark_eve_access_tunnel_801807A8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", D_neo_ark_eve_access_tunnel_8017D688);

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
            NaetEventDesc  work;
            NaetUtilParam  param;
            NaetEventDesc* wp;
            s32            (*resolve)(NaetUtilParam*, NaetUtilParam*) = func_neo_ark_eve_access_tunnel_8017D6D4;

            work.field_0 = 4;
            work.field_1 = (u8)task->spawnArg1;
            work.field_3 = 1;
            work.field_2 = 2;
            work.field_8 = 0;
            work.field_4 = 0x800;
            Gp_MsgPlayerWeapon(0);
            wp            = &work;
            param.field_0 = wp->field_1;
            param.field_2 = wp->field_2;
            param.field_3 = wp->field_3;
            param.field_5 = 0;
            resolve(&param, &param);
            wp->field_1                          = param.field_0;
            wp->field_2                          = param.field_2;
            wp->field_3                          = param.field_3;
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
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_neo_ark_eve_access_tunnel_801807A0.field_2;
            Mc_SaveData.at4.loc.warp = D_neo_ark_eve_access_tunnel_801807A0.field_4;
            Mc_SaveData.at4.loc.room = D_neo_ark_eve_access_tunnel_801807A0.field_1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
