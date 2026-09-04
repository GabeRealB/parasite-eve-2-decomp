#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"

extern TaskDesc D_acropolis_bridge_80189234;
extern TaskDesc D_acropolis_bridge_80189830;
extern SVECTOR  D_acropolis_bridge_80189240[];
extern Task*    D_acropolis_bridge_8019179C;
extern u16      D_acropolis_bridge_801917A4;

/// The two 0x18-byte script work blocks `Gp_SpawnScript18` copies from when the
/// bridge cutscene starts.
extern s32 D_acropolis_bridge_80190B8C;
extern s32 D_acropolis_bridge_80190BA4;

void func_acropolis_bridge_8017F2D0(s32 arg0);
void func_acropolis_bridge_8017E81C(void);
s32  func_acropolis_bridge_8017F6D4(AcropolisBridgeHotspot* table, s16 x, s16 y);

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

/// Brings the bridge's action-prompt script online: allocates its
/// `AcropolisBridgePromptWork` block, spawns the prompt task it drives, arms
/// the script at step 0xFFF, raises the "bridge is up" sprite command of the
/// camera the player is on, and clears every hotspot's hit flag so the first
/// hit test starts clean. A failed allocation kills the task instead.
void func_acropolis_bridge_8017E04C(Task* task)
{
    AcropolisBridgePromptWork* work;
    GameSessionFrom4*          sess;
    AcropolisBridgeHotspot*    hs;
    GpSprtRec*                 rec;
    s32                        view;

    work = (AcropolisBridgePromptWork*)Mem_Calloc(0x10, 0);
    if (work == NULL) {
        Task_RequestKill(task, 0);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(&D_acropolis_bridge_80189830, 0, 1, 0);
    task->idMap     = (TaskIdMap*)work;
    work->field_0   = 0x14;
    work->field_4   = 0xFFF;
    sess            = (GameSessionFrom4*)&Game_Session->field_4;
    task->state++;
    view                                  = Gp_GetViewIndex();
    rec                                   = Gp_SprtTables[sess->field_3 - 1][Game_Session->field_74 - 1].field_0[sess->field_2 - 1];
    rec[(u8)view - 1].field_4[35].field_4 = 1;
    Game_Session->field_66                = 1;
    Gp_MsgPlayer3F3(0);
    Display_AcquireRef();
    Game_Session->field_1  = 1;
    Game_Session->field_68 = 1;
    for (hs = D_acropolis_bridge_8018983C; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    D_acropolis_bridge_801917A8 = 0;
    func_acropolis_bridge_8017E60C(work->field_4, 0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E1D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E3A0);

/// Idles the bridge prompt for twenty frames per pass: the first ten frames
/// keep the prompt task ticking through `func_acropolis_bridge_8017E81C`, the
/// next ten hold it closed, and the twentieth counts one completed pass in
/// `field_8`. Either way the cursor is re-hit-tested against the room's
/// hotspot table so `mode` reports whether it sits over one. After three
/// passes the script rewinds to state 2 for another attempt, and once three
/// attempts have been spent it gives up into state 8.
void func_acropolis_bridge_8017E4FC(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgeHotspot*    hs     = D_acropolis_bridge_8018983C;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->idMap;
    s16                        tick;
    u8                         retry;

    Gp_GetViewIndex();
    tick = work->field_A;
    if (tick < 0xA) {
        func_acropolis_bridge_8017E81C();
        work->field_A++;
    } else if (tick < 0x14) {
        func_acropolis_bridge_8017E60C(0xFFF, 0);
        work->field_A++;
    } else {
        work->field_A = 0;
        work->field_8++;
    }

    if (func_acropolis_bridge_8017F6D4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
    } else {
        prompt->mode = 1;
    }

    if (work->field_8 == 3) {
        task->state      = 2;
        work->field_6    = 0;
        work->field_4    = 0xFFF;
        retry            = work->retryCount + 1;
        work->retryCount = retry;
        if (retry >= 3) {
            task->state                 = 8;
            D_acropolis_bridge_801917A8 = 0;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_bridge/acropolis_bridge_5", func_acropolis_bridge_8017E60C);

/// Shows one frame of the bridge prompt: in the current room's eighth SPRT
/// record, every command from 1 to 33 gets its skip-OT-link flag set and only
/// command 34 is left drawing. `func_acropolis_bridge_8017E4FC` calls this on
/// each of the first ten frames of a pass.
void func_acropolis_bridge_8017E81C(void)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&Game_Session->field_4;
    GpSprtCmd*        cmd;
    s32               i;

    Gp_GetViewIndex();
    cmd = Gp_SprtTables[sess->field_3 - 1][Game_Session->field_74 - 1].field_0[sess->field_2 - 1][7].field_4;

    for (i = 0x15; i < 0x1F; i++) {
        cmd[i].field_4 = 1;
    }
    for (i = 0xB; i < 0x15; i++) {
        cmd[i].field_4 = 1;
    }
    for (i = 1; i < 0xB; i++) {
        cmd[i].field_4 = 1;
    }
    cmd[34].field_4 = 0;
    cmd[33].field_4 = 1;
    cmd[32].field_4 = 1;
    cmd[31].field_4 = 1;
}
