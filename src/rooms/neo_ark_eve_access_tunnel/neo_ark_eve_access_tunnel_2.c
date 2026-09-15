#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern TaskDesc D_neo_ark_eve_access_tunnel_8017EAC4;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel_2", func_neo_ark_eve_access_tunnel_8017DC6C);

s32 func_neo_ark_eve_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (Game_Session->field_9 == 0xB) {
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
