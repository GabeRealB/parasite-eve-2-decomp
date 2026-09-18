#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

/// Scene id byte at 0x8007272D; the tunnel stamps 0x18 when it hands the save
/// location off, the same way the neighbouring rooms stamp their own scenes.
extern s8 D_8007272D;

/// Staging save location the room commits when the tunnel's save is taken:
/// `field_2` / `field_4` / `field_1` hold what `func_neo_ark_eve_access_tunnel_8017DB18`
/// later copies into `Mc_SaveData.field_6` / `field_8` / `field_5`.
extern GpSaveLoc D_neo_ark_eve_access_tunnel_801807A0;

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

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
                        Task_SpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EAC4, 1, 0, 0);
                    }
                    break;
            }
            return 0;
    }
    return 1;
}

s32 func_neo_ark_eve_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (gGameSession->loc.place == 0xB) {
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
            Task_SpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EAC4, 2, 0x1AF, 0);
        } else {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_neo_ark_eve_access_tunnel_8017EAC4, 0, arg2->field_3, 0);
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
