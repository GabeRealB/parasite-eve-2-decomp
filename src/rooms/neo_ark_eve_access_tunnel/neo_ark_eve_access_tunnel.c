#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

/// Staging save location the room commits when the tunnel's save is taken.
extern GpSaveLoc D_neo_ark_eve_access_tunnel_801807A0;

/// CAP countdown the tunnel reloads while the save sequence runs.
extern s16 D_80114D08;

/// Set when the tunnel's save is written to the memory card.
extern s16 D_80071076;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", func_neo_ark_eve_access_tunnel_8017D6D4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", func_neo_ark_eve_access_tunnel_8017D810);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel", func_neo_ark_eve_access_tunnel_8017D980);

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
