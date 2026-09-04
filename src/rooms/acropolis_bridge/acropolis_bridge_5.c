#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

extern TaskDesc D_acropolis_bridge_80189234;
extern SVECTOR  D_acropolis_bridge_80189240[];
extern Task*    D_acropolis_bridge_8019179C;
extern u16      D_acropolis_bridge_801917A4;

/// The two 0x18-byte script work blocks `Gp_SpawnScript18` copies from when the
/// bridge cutscene starts.
extern s32 D_acropolis_bridge_80190B8C;
extern s32 D_acropolis_bridge_80190BA4;

void func_acropolis_bridge_8017F2D0(s32 arg0);

void func_acropolis_bridge_8017DD9C(Task* arg0)
{
    Task* task = Task_SpawnFromTable(&D_acropolis_bridge_80189234, 0, 0, 0);
    s32   next = arg0->state + 1;

    D_acropolis_bridge_8019179C = task;
    arg0->state                 = next;
}

void func_acropolis_bridge_8017DDEC(Task* arg0)
{
    s32 unused[2]; // never read; the target still reserves sp+0x10..sp+0x18 for it
    s32 killed;

    if (Task_PollKill(D_acropolis_bridge_8019179C, &killed) != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, 1, 0x7D5);
        Mc_SaveData.field_4    = 6;
        Mc_SaveData.field_5    = 2;
        Game_Session->field_5  = 2;
        Game_Session->field_76 = 1;
        GameFlag_SetNibble(2, 3);
        Gp_MsgPlayerWeapon(1);
        arg0->state = arg0->state + 1;
    }
}

void func_acropolis_bridge_8017DE94(Task* arg0)
{
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
    Game_Session->field_68 = 0;
    arg0->state            = (s32)(arg0->state + 1);
}

void func_acropolis_bridge_8017DEE4(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;
    Task*       task;
    s16         count;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
    }
    goto tail;

L_case0:
    queue->field_1EA = 1;
    slotParam[0]     = Stream_FindSlot(&Game_Session->field_4, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case1:
    if (queue->field_1FA == 0) {
        goto tail;
    }
    Task_Reparent(task, Gp_SpawnScript18((s32)&D_acropolis_bridge_80190B8C, (s32)&D_acropolis_bridge_80190BA4));
    goto advance;

L_case2:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto tail;
    }
advance:
    task->state = task->state + 1;
    goto tail;

L_case3:
    count               = task->killCountdown + 1;
    task->killCountdown = count;
    if (count >= 0x1F) {
        Task_RequestKill(task, 0);
        return;
    }
tail:
    D_acropolis_bridge_801917A4 = queue->field_1EA;
}

s16 func_acropolis_bridge_8017E024(void)
{
    return D_acropolis_bridge_80189240[D_acropolis_bridge_801917A4 + 1].vy;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E04C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E1D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E3A0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E4FC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E60C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E81C);
