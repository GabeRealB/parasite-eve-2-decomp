#include "common.h"

#include "main/fs.h"
#include "main/session.h"

void func_neo_ark_eve_access_tunnel_8017E090(s32 arg0, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel_3", func_neo_ark_eve_access_tunnel_8017DF24);

void func_neo_ark_eve_access_tunnel_8017DFC0(void)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (Game_Session->field_9 < 4U) {
        func_neo_ark_eve_access_tunnel_8017E090(0, 0);
        func_neo_ark_eve_access_tunnel_8017E090(1, 0);
    }
    if (Game_Session->field_9 == 0xB) {
        queue->field_22A = 2;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_eve_access_tunnel/neo_ark_eve_access_tunnel_3", jtbl_neo_ark_eve_access_tunnel_8017D6C0);
