#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

/// Message handlers this room's task answers, installed into pointer slot 7.
extern GpMsgEntry D_neo_ark_submarine_tunnel_80181A50[];

/// Parameter block handed to the task spawned from
/// `D_neo_ark_submarine_tunnel_80181A34` as `Task::spawnArg2`;
/// `func_neo_ark_submarine_tunnel_8017F318` fills it before the spawn call.
///
/// The target carries two symbols for this block: the compiler reached the
/// word at 0x4 as `%hi(base + 4)`, and splat names a relocation by the address
/// it resolves to, so it split the block at `D_neo_ark_submarine_tunnel_80187A24`.
/// Every other field is reached as a displacement off the 0x80187A20 base, so
/// the two are declared apart: writing 0x4 through the base gives a `%hi(base+4)`
/// relocation that links to the same byte but is not what the target holds.
typedef struct NeoArkSubmarineTunnelSpawn {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u8  pad_4[4]; /* 0x4 is D_neo_ark_submarine_tunnel_80187A24 */
    /* 0x8 */ u8  field_8;
    /* 0x9 */ u8  field_9;
    /* 0xA */ u8  field_A;
    /* 0xB */ u8  field_B;
} NeoArkSubmarineTunnelSpawn;
STATIC_ASSERT_SIZEOF(NeoArkSubmarineTunnelSpawn, 0xC);

extern TaskDesc                   D_neo_ark_submarine_tunnel_80181A34;
extern NeoArkSubmarineTunnelSpawn D_neo_ark_submarine_tunnel_80187A20;
extern s16                        D_neo_ark_submarine_tunnel_80187A24;

/// Message 0x13F0 handler: while the session is in area 1, spawns the
/// capacity-resident enemy for slot ids 4 and 5.
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 < 6) {
        if (arg2 >= 4) {
            if (gGameSession->field_9 == 1) {
                Gp_SpawnIfCapIdle(arg2, 0);
            }
        }
    }
    return 0;
}

/// Rear entry to the room: while the submarine is still parked it selects the
/// MDEC decode mode, fills the spawn block and starts the tunnel task;
/// afterwards it just records the requested room id.
void func_neo_ark_submarine_tunnel_8017F318(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                            = 2;
        D_neo_ark_submarine_tunnel_80187A20.field_0 = 1;
        D_neo_ark_submarine_tunnel_80187A20.field_2 = 0x60;
        D_neo_ark_submarine_tunnel_80187A20.field_9 = 0x40;
        D_neo_ark_submarine_tunnel_80187A20.field_8 = 1;
        D_neo_ark_submarine_tunnel_80187A20.field_A = 0x80;
        D_neo_ark_submarine_tunnel_80187A20.field_B = 0x80;
        Task_SpawnFromTable(&D_neo_ark_submarine_tunnel_80181A34, 0, 0, (s32)&D_neo_ark_submarine_tunnel_80187A20);
        return;
    }
    D_neo_ark_submarine_tunnel_80187A24 = arg0;
}

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
