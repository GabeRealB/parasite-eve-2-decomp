#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

/// Message handlers this room's task answers, installed into pointer slot 7.
extern GpMsgEntry D_neo_ark_submarine_tunnel_80181A50[];

/// Message 0x13F0 handler: while the session is in area 1, spawns the
/// capacity-resident enemy for slot ids 4 and 5.
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 < 6) {
        if (arg2 >= 4) {
            if (Game_Session->field_9 == 1) {
                Gp_SpawnIfCapIdle(arg2, 0);
            }
        }
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", func_neo_ark_submarine_tunnel_8017F318);

void func_neo_ark_submarine_tunnel_8017F398(s32 arg0)
{
    GameFlag_SetNibble(0xBC, arg0);
}

/// Publishes the room's message table on the task, plays the room's arrival
/// sound event, then hands over to the next state.
void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0)
{
    arg0->field_24 = D_neo_ark_submarine_tunnel_80181A50;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550C0003, 0, 0);
    arg0->state = arg0->state + 1;
}

void func_neo_ark_submarine_tunnel_8017F414(void)
{
    Game_GetPtrSlot(3);
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_tunnel/neo_ark_submarine_tunnel_2", jtbl_neo_ark_submarine_tunnel_8017D620);
