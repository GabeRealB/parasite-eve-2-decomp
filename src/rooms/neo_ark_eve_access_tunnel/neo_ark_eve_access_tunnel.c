#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_eve_access_tunnel.h"

/// Staging save location the room commits when the tunnel's save is taken.
extern GpSaveLoc D_neo_ark_eve_access_tunnel_801807A0;

/// CAP countdown the tunnel reloads while the save sequence runs.
extern s16 D_80114D08;

/// Set when the tunnel's save is written to the memory card.
extern s16 D_80071076;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", func_neo_ark_eve_access_tunnel_8017D6D4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", func_neo_ark_eve_access_tunnel_8017D810);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", RoomsShared8017d878Table);

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
                Task_Kill(task);
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
            void           (*resolve)(NaetUtilParam*, NaetUtilParam*) = func_neo_ark_eve_access_tunnel_8017D6D4;

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
            Task_Kill(task);
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
                Task_Kill(task);
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
            D_80071076          = 1;
            Mc_SaveData.field_6 = D_neo_ark_eve_access_tunnel_801807A0.field_2;
            Mc_SaveData.field_8 = D_neo_ark_eve_access_tunnel_801807A0.field_4;
            Mc_SaveData.field_5 = D_neo_ark_eve_access_tunnel_801807A0.field_1;
            Task_Spawn(0, 0x11, 0, 0);
            Task_Kill(task);
            break;
    }
}
