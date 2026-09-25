#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern s32 D_80070F70;
extern u8  D_80072728;
/// Table of 0x80-byte actor config blocks; `Player_Status` is entry 1.
extern PlayerStatus D_80073B08[];
extern s16          D_80114D08;
extern s32          D_80115738;
extern s32          D_8011574C;

extern GpMsgEntry D_acropolis_bridge_80188E4C[];
extern TaskDesc   D_acropolis_bridge_80188E7C[];
extern s32        D_acropolis_bridge_80188EBC;
extern s32        D_acropolis_bridge_8018912C;
extern TaskDesc   D_acropolis_bridge_80189234;
extern SVECTOR    D_acropolis_bridge_80189240[];
extern TaskDesc   D_acropolis_bridge_80189830;

/// Three 16-entry rows, one per digit of the bridge code, mapping a nibble to
/// the SPRT command that renders it. Entries above 9 hold the row's blank
/// sentinel.
extern u8 D_acropolis_bridge_801898CC[3][16];

extern GpMsgEntry D_acropolis_bridge_801898FC[];
extern SVECTOR    D_acropolis_bridge_8018991C[7];
extern SVECTOR    D_acropolis_bridge_80189954[7];
extern SVECTOR    D_acropolis_bridge_8018998C[12];
extern u16        D_acropolis_bridge_801899EC[8];
extern u16        D_acropolis_bridge_801899FC[16];
extern u16        D_acropolis_bridge_80189A1C[12];
extern SVECTOR    D_acropolis_bridge_80189A34[2];
extern SVECTOR    D_acropolis_bridge_80189A44;
extern SVECTOR    D_acropolis_bridge_80189A4C;

/// The two 0x18-byte script work blocks `Gp_SpawnScript18` copies from when the
/// bridge cutscene starts.
extern s32 D_acropolis_bridge_80190B8C;
extern s32 D_acropolis_bridge_80190BA4;

extern u16 D_acropolis_bridge_80190C60;
extern s16 D_acropolis_bridge_801915E4[][6];

/// State handler table the per-frame tick dispatches through on
/// `AcropolisBridgeEnemyWork::field_0`.
extern void (*D_acropolis_bridge_8019175C[])(Task*);

extern Task* D_acropolis_bridge_80191794;
extern Task* D_acropolis_bridge_80191798;
extern Task* D_acropolis_bridge_8019179C;
extern s32   D_acropolis_bridge_801917A0;
extern u16   D_acropolis_bridge_801917A4;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);

void func_acropolis_bridge_8017D98C(Task* task);
void func_acropolis_bridge_8017D9FC(Task* task);
void func_acropolis_bridge_8017DA64(Task* task);
void func_acropolis_bridge_8017DB08(Task* task);
void func_acropolis_bridge_8017DB60(Task* task);
void func_acropolis_bridge_8017DBA0(Task* task);
void func_acropolis_bridge_8017DC1C(Task* task);
void func_acropolis_bridge_8017DC68(Task* task);
void func_acropolis_bridge_8017DD24(Task* task);
void func_acropolis_bridge_8017DD88(Task* task);
void func_acropolis_bridge_8017DD9C(Task* task);
void func_acropolis_bridge_8017DDEC(Task* task);
void func_acropolis_bridge_8017DE94(Task* task);
s16  func_acropolis_bridge_8017E024(void);
void func_acropolis_bridge_8017E04C(Task* task);
void func_acropolis_bridge_8017E1D0(Task* task);
void func_acropolis_bridge_8017E3A0(Task* task);
void func_acropolis_bridge_8017E4FC(Task* task);
void func_acropolis_bridge_8017E81C(void);
void func_acropolis_bridge_8017F198(s32 x, s32 y, s32 variant);
void func_acropolis_bridge_8017F2D0(s32 flags);
void func_acropolis_bridge_8017F404(Task* task);
void func_acropolis_bridge_8017F460(Task* task);
void func_acropolis_bridge_8017F4CC(Task* task);
void func_acropolis_bridge_8017F544(Task* task);
void func_acropolis_bridge_8017F658(Task* task);
s32  func_acropolis_bridge_8017F6D4(OverlayHotspot* table, s16 x, s16 y);
void func_acropolis_bridge_8017F808(Task* task);
void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_acropolis_bridge_80182F8C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_acropolis_bridge_801833A0(GsCOORDINATE2* arg0, u16 arg1, s16 arg2);
void func_acropolis_bridge_80183654(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_acropolis_bridge_8018581C(Task* task);
void func_acropolis_bridge_80185988(GpEnemy* enemy, Task* task);
void func_acropolis_bridge_80187850(GpEnemy* enemy, Task* task);

/// Work block of the bridge model task, stored at `Task::work`; it is exactly
/// the `memCalloc(4, 0)` the setup state allocates.
typedef struct _AcropolisBridgeModelWork {
    s32 field_0;
} _AcropolisBridgeModelWork;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeModelWork, 0x4);

/// Work block the bridge enemy's task keeps at `Task::work`. `field_4` is the
/// live flag every state handler of the enemy gates on. `body` and `hit` are
/// the two linked `GpObj`s -- kind 2 for the model and kind 3 for the hit box
/// -- whose `flags` bit 15 the handlers toggle to enable one and disable the
/// other, and `recs` / `hitRecs` are their collision record tables.
/// `lightMtx` / `colorMtx` are the matrices the model's `TmdObject` is pointed
/// at, `field_1F0` is the `GpEffArg` the death effect is spawned with and
/// `field_290` the death-sequence frame counter.
typedef struct AcropolisBridgeEnemyWork {
    /* 0x000 */ s16           field_0;
    /* 0x002 */ s16           field_2;
    /* 0x004 */ s16           field_4;
    /* 0x006 */ byte          pad_6[0x2];
    /* 0x008 */ s16           yaw;
    /* 0x00A */ byte          pad_A[0x2];
    /* 0x00C */ GpAnimCtx     anim;
    /* 0x020 */ GpAnimSlot    slots[4];
    /* 0x0C0 */ byte          pad_C0[0x40];
    /* 0x100 */ s16           field_100;
    /* 0x102 */ s16           field_102;
    /* 0x104 */ s16           field_104;
    /* 0x106 */ s16           field_106;
    /* 0x108 */ s16           field_108;
    /* 0x10A */ byte          pad_10A[0x2];
    /* 0x10C */ s16           field_10C;
    /* 0x10E */ s16           field_10E;
    /* 0x110 */ GpObj         body;
    /* 0x130 */ GpRec18       recs[3];
    /* 0x178 */ GpObj         hit;
    /* 0x198 */ GpRec18       hitRecs[1];
    /* 0x1B0 */ MATRIX        lightMtx;
    /* 0x1D0 */ MATRIX        colorMtx;
    /* 0x1F0 */ GpEffArg      field_1F0;
    /* 0x1F8 */ s16           field_1F8;
    /* 0x1FA */ s16           field_1FA;
    /* 0x1FC */ OverlayWalker walker;
    /* 0x290 */ u16           field_290;
    /* 0x292 */ u16           field_292;
} AcropolisBridgeEnemyWork;
STATIC_ASSERT_SIZEOF(AcropolisBridgeEnemyWork, 0x294);

/// 0xC-byte scratchpad block the per-frame tick carves off `G_SCRATCH_HEAD` to
/// stage the collision record it hands to the damage path: the record's three
/// packed coordinates followed by its `field_4` attack id, which is also the
/// "was there a hit" flag.
typedef struct AcropolisBridgeHitScratch {
    /* 0x0 */ s16  x;
    /* 0x2 */ s16  y;
    /* 0x4 */ s16  z;
    /* 0x6 */ byte pad_6[0x2];
    /* 0x8 */ s32  hit;
} AcropolisBridgeHitScratch;
STATIC_ASSERT_SIZEOF(AcropolisBridgeHitScratch, 0xC);

/// Ticks the walker task: steps its patrol route and drives its animation.
void func_acropolis_bridge_8018532C(OverlayWalker* walker);

/// Returns the patrol node nearest the given actor's coordinate, by squared
/// distance in the XZ plane.
u8 func_acropolis_bridge_801843A0(OverlayWalker* work, s32 actor);
/// Returns the patrol node nearest the walker, by squared distance in the
/// XZ plane between the node table and the walker's coordinate translation.
u8   func_acropolis_bridge_8018450C(OverlayWalker* work);
void func_acropolis_bridge_80184638(OverlayWalker* work, s16 arg1);
void func_acropolis_bridge_80184908(OverlayWalker* work);
void func_acropolis_bridge_80184B94(OverlayWalker* work);
void func_acropolis_bridge_80185104(OverlayWalker* work, SVECTOR3* pos);

/// Room message handler: answers msg 0xF (first use of the bridge) by running
/// the cutscene once and marking the area object, and msg 0xB by asking for
/// response 2 in the outgoing copy.
s32 func_acropolis_bridge_8017D6F4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    GpAreaKey key;

    *out = *in;
    if (in->msgId == 0xF) {
        if (GameFlag_GetNibble(0x10) == 0) {
            if (in->field_5 == 0) {
                GameFlag_SetNibble(0x10, 1);
                func_800E8634((s32)&D_acropolis_bridge_80188EBC, 0, (s32)&D_acropolis_bridge_8018912C);
                GameFlag_SetNibble(6, 1);
                key.stage = 1;
                key.area  = 0xC;
                Gp_SetAreaObjId(&key, 3, 1);
            }
            return 2;
        }
    }
    if ((in->msgId == 0xB) && (in->field_5 == 0)) {
        out->field_3 = 2;
    }
    return 1;
}

s32 func_acropolis_bridge_8017D7F0(void)
{
    return 0;
}

/// Slot-7 handler for the "player used the bridge switch" message: with the
/// room's progress nibble already at 3 it just restarts cap slot 7, otherwise
/// it clears the script step and spawns entry 1 of the room task table.
s32 func_acropolis_bridge_8017D7F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (GameFlag_GetNibble(2) == 3) {
            Gp_StartCapSlot(7, 1, 2);
            return 0;
        }
        func_acropolis_bridge_8017E60C(0xFFF, 1);
        Task_SpawnFromTable(D_acropolis_bridge_80188E7C, 1, 0, 0);
    }
    return 0;
}

s32 func_acropolis_bridge_8017D868(void)
{
    return 0;
}

s32 func_acropolis_bridge_8017D870(void)
{
    return 0;
}

/// State handlers of the room's own task.
const TaskFuncTable3 D_acropolis_bridge_8017D5C4 = {
    { func_acropolis_bridge_8017D98C, func_acropolis_bridge_8017D9FC, taskKill }
};

/// State handlers of the bridge model task.
const TaskFuncTable3 D_acropolis_bridge_8017D5D0 = {
    { func_acropolis_bridge_8017DA64, func_acropolis_bridge_8017DB08, taskKill }
};

/// Three-state dispatcher of the bridge model task: setup, per-frame update,
/// then `taskKill`. The table is copied onto the stack before the call.
void func_acropolis_bridge_8017D878(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D5D0;
    sp.funcs[task->state](task);
}

/// State handlers of the room's cutscene task.
const TaskFuncTable14 D_acropolis_bridge_8017D5DC = {
    { func_acropolis_bridge_8017DB60, func_acropolis_bridge_8017DBA0, func_acropolis_bridge_8017DD88,
      func_acropolis_bridge_8017DC1C, func_acropolis_bridge_8017DC68, func_acropolis_bridge_8017DD24,
      func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD88,
      func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD9C, func_acropolis_bridge_8017DDEC,
      func_acropolis_bridge_8017DE94, taskKill }
};

/// State handlers of the room's prompt script task.
const TaskFuncTable9 D_acropolis_bridge_8017D614 = {
    { func_acropolis_bridge_8017E04C, func_acropolis_bridge_8017F404, func_acropolis_bridge_8017E1D0,
      func_acropolis_bridge_8017F460, func_acropolis_bridge_8017F4CC, func_acropolis_bridge_8017F544,
      func_acropolis_bridge_8017E3A0, func_acropolis_bridge_8017E4FC, func_acropolis_bridge_8017F658 }
};

/// Fourteen-state dispatcher of the room's cutscene task: copies the handler
/// table onto the stack and calls the entry named by `Task::state`.
void func_acropolis_bridge_8017D8D0(Task* task)
{
    TaskFuncTable14 states;

    states = D_acropolis_bridge_8017D5DC;
    states.funcs[task->state](task);
}

void func_acropolis_bridge_8017D954(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_acropolis_bridge_8017D98C(Task* arg0)
{
    arg0->msgTable = D_acropolis_bridge_80188E4C;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_bridge_80191794 = Task_SpawnFromTable(D_acropolis_bridge_80188E7C, 0, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
}

void func_acropolis_bridge_8017D9FC(Task* task)
{
    char pad[0x10];
}

/// Three-state dispatcher of the room's own task: setup, an empty idle state,
/// then `taskKill`. The table is copied onto the stack before the call.
void func_acropolis_bridge_8017DA0C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D5C4;
    sp.funcs[task->state](task);
}

/// First state of the bridge model task: allocates its work block, parks the
/// model at (-0x23F0, 0x12C, -0xAF0) and parents it to the room's view
/// coordinate system. The task is killed if the allocation fails.
void func_acropolis_bridge_8017DA64(Task* task)
{
    TmdObject*                 extra;
    GsCOORDINATE2*             coord;
    _AcropolisBridgeModelWork* work;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    work  = (_AcropolisBridgeModelWork*)memCalloc(sizeof(_AcropolisBridgeModelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->flags      = 0;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = -0x23F0;
    coord->coord.t[1] = 0x12C;
    coord->coord.t[2] = -0xAF0;
    coord->flg        = 0;
    task->state++;
}

/// Per-frame state of the bridge model task: raises bit 0x80 of the object's
/// flags on camera views 8..10 and clears them elsewhere, then clears the root
/// coordinate's `flg` so its world matrix is rebuilt this frame.
void func_acropolis_bridge_8017DB08(Task* task)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    if ((u32)(Gp_GetViewIndex() - 8) < 3U) {
        extra->flags = 0x80;
    } else {
        extra->flags = 0;
    }
    coord->flg = 0;
}

void func_acropolis_bridge_8017DB60(Task* arg0)
{
    Gp_StartCapSlot(7, 1, 1);
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_bridge_8017DBA0(Task* arg0)
{
    if (Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA3, 0, 0) == 0) {
        Mc_SaveData.at4.loc.view = 8;
        gGameSession->hideHud    = 1;
        Gp_MsgPlayer3F3(0);
        Gp_MsgPlayerWeapon(0);
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_bridge_8017DC1C(Task* arg0)
{
    Task* temp_v0;

    temp_v0                     = Task_Spawn(2, 8, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    D_acropolis_bridge_80191798 = temp_v0;
}

void func_acropolis_bridge_8017DC68(Task* arg0)
{
    GpCmdArg msg = { { { 1, 0xB } }, 1 };

    if (Task_PollKill(D_acropolis_bridge_80191798, &D_acropolis_bridge_801917A0) != 0) {
        if (D_acropolis_bridge_801917A0 == 0) {
            Mc_SaveData.at4.loc.view = 6;
            gGameSession->hideHud    = 0;
            arg0->state              = (s32)(arg0->state + 1);
        } else {
            Mc_SaveData.at4.loc.view = 9;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            arg0->state = (s32)(arg0->state + 1);
        }
    }
}

void func_acropolis_bridge_8017DD24(Task* arg0)
{
    if (D_acropolis_bridge_801917A0 == 0) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        taskKill(arg0);
        return;
    }
    Gp_MsgPlayer3F3(1);
    arg0->state += 1;
}

void func_acropolis_bridge_8017DD88(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

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
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, 1, 0x7D5);
        Mc_SaveData.at4.loc.view    = 6;
        Mc_SaveData.at4.loc.room    = 2;
        gGameSession->at4.loc.room  = 2;
        gGameSession->roomObjsDirty = 1;
        GameFlag_SetNibble(2, 3);
        Gp_MsgPlayerWeapon(1);
        arg0->state = arg0->state + 1;
    }
}

void func_acropolis_bridge_8017DE94(Task* arg0)
{
    func_acropolis_bridge_8017F2D0(GameFlag_GetNibble(0x10) & 0xFF);
    gGameSession->hideHud = 0;
    arg0->state           = (s32)(arg0->state + 1);
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
    slotParam[0]     = Stream_FindSlot(&gGameSession->at4.loc.view, 0, 0);
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
    GpAreaKey*                 sess;
    OverlayHotspot*            hs;
    GpSprtRec*                 rec;
    s32                        view;

    work = (AcropolisBridgePromptWork*)memCalloc(0x10, 0);
    if (work == NULL) {
        Task_RequestKill(task, 0);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(&D_acropolis_bridge_80189830, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    work->field_0   = 0x14;
    work->field_4   = 0xFFF;
    sess            = &gGameSession->at4.loc;
    task->state++;
    view                                  = Gp_GetViewIndex();
    rec                                   = Gp_SprtTables[sess->stage - 1][gGameSession->sprtVariant - 1].field_0[sess->area - 1];
    rec[(u8)view - 1].field_4[35].field_4 = 1;
    gGameSession->cutsceneHold            = 1;
    Gp_MsgPlayer3F3(0);
    Display_AcquireRef();
    gGameSession->eventState = 1;
    gGameSession->hideHud    = 1;
    for (hs = D_acropolis_bridge_8018983C; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    D_acropolis_bridge_801917A8 = 0;
    func_acropolis_bridge_8017E60C(work->field_4, 0);
}

/// Runs one frame of the bridge's action prompt while the player is entering a
/// code: the cursor is hit-tested against the room's hotspot table, and a
/// confirm press on a hit hotspot either latches that hotspot for the caller
/// (when the prompt is idle) or shifts its id into `field_4`'s low nibble and
/// beeps. Hotspot id 0xA is the "clear" key, which re-arms the script at step
/// 0xFFF. Three entered digits end the script in state 5, a cancel press ends
/// it in state 8, and a busy cap suspends the whole scan for that frame.
void func_acropolis_bridge_8017E1D0(Task* task)
{
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;
    OverlayHotspot*            hs     = D_acropolis_bridge_8018983C;
    RoomActionPrompt*          prompt = &D_80114D28;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
    } else {
        prompt->targetId = 0x80;
        if (func_acropolis_bridge_8017F6D4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = 2;
            if (prompt->buttons[0].state == 2) {
                while (hs->id != -1) {
                    if (hs->hit != 0) {
                        if (work->promptBusy == 0) {
                            prompt->mode     = 0;
                            prompt->targetId = 0;
                            work->field_C    = hs->id;
                            work->promptKind = hs->promptKind;
                            task->state      = 3;
                            return;
                        }
                        if (hs->id == 0xA) {
                            work->field_4 = 0xFFF;
                            work->field_6 = 0;
                        } else {
                            work->field_4 <<= 4;
                            work->field_4   = (work->field_4 & 0xFF0) | hs->id;
                            work->field_6++;
                        }
                        SndEvt_EnqueueType6(0x510E0003, 0, 0);
                        break;
                    }
                    hs++;
                }
            }
        } else {
            prompt->mode = 1;
        }
        if (work->field_6 == 3) {
            task->state   = 5;
            work->field_A = 0;
        }
        if (prompt->buttons[1].state == 2) {
            task->state                 = 8;
            D_acropolis_bridge_801917A8 = 0;
        }
    }
    func_acropolis_bridge_8017E60C(work->field_4, 0);
}

/// Winds the bridge prompt back down, the mirror of
/// `func_acropolis_bridge_8017E04C`: it clears the "bridge is up" sprite
/// command of the camera the player is on, then runs the same twenty-frame
/// pass as `func_acropolis_bridge_8017E4FC` - the first ten frames re-arm the
/// script at step 0xFFF, the next ten replay the step the work block holds,
/// and the twentieth resets the frame counter and counts one completed pass.
/// The cursor is hit-tested against the room's hotspot table either way so
/// `mode` reports whether it sits over one, and the third pass ends the script
/// in state 8 with `D_acropolis_bridge_801917A8` raised.
void func_acropolis_bridge_8017E3A0(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    OverlayHotspot*            hs     = D_acropolis_bridge_8018983C;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;
    GpAreaKey*                 sess   = &gGameSession->at4.loc;
    GpSprtRec*                 rec;
    s32                        view;
    s16                        tick;
    s32                        step;

    view                                  = Gp_GetViewIndex();
    rec                                   = Gp_SprtTables[sess->stage - 1][gGameSession->sprtVariant - 1].field_0[sess->area - 1];
    rec[(u8)view - 1].field_4[35].field_4 = 0;

    tick = work->field_A;
    step = 0xFFF;
    if (tick >= 0xA) {
        if (tick >= 0x14) {
            goto reset;
        }
        step = work->field_4;
    }
    func_acropolis_bridge_8017E60C(step, 0);
    work->field_A++;
    goto after;

reset:
    work->field_A = 0;
    work->field_8++;

after:
    if (func_acropolis_bridge_8017F6D4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
    } else {
        prompt->mode = 1;
    }

    if (work->field_8 == 3) {
        task->state                 = 8;
        D_acropolis_bridge_801917A8 = 1;
    }
}

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
    OverlayHotspot*            hs     = D_acropolis_bridge_8018983C;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;
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

/// Draws the three-digit bridge code onto the current room's eighth SPRT
/// record. Each nibble of `digits` indexes one row of
/// `D_acropolis_bridge_801898CC`, which maps it to the single command left
/// drawing in that digit's band - commands 1..10, 11..20 and 21..30 - while
/// every other command in the band gets its skip-OT-link flag set. A nibble
/// above 9 maps to the row's sentinel (0x1F / 0x20 / 0x21), which blanks the
/// band and shows the placeholder at command 31, 32 or 33 instead, so
/// `func_acropolis_bridge_8017E60C(0xFFF, 0)` clears the whole display.
/// Command 34 is always hidden; `hidePrompt` also hides command 35.
void func_acropolis_bridge_8017E60C(s32 digits, s32 hidePrompt)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;
    s32        i;
    u8         hi;
    u8         mid;
    u8         lo;

    Gp_GetViewIndex();
    cmd = Gp_SprtTables[sess->stage - 1][gGameSession->sprtVariant - 1].field_0[sess->area - 1][7].field_4;

    if ((s16)hidePrompt != 0) {
        cmd[35].field_4 = 1;
    }

    hi  = D_acropolis_bridge_801898CC[0][((u32)digits & 0xF00) >> 8];
    mid = D_acropolis_bridge_801898CC[1][((u32)digits & 0xF0) >> 4];
    lo  = D_acropolis_bridge_801898CC[2][digits & 0xF];

    if (hi == 0x21) {
        for (i = 0x15; i < 0x1F; i++) {
            cmd[i].field_4 = 1;
        }
        cmd[33].field_4 = 0;
    } else {
        for (i = 0x15; i < 0x1F; i++) {
            if (hi == i) {
                cmd[i].field_4  = 0;
                cmd[33].field_4 = 1;
            } else {
                cmd[i].field_4 = 1;
            }
        }
    }

    if (mid == 0x20) {
        for (i = 0xB; i < 0x15; i++) {
            cmd[i].field_4 = 1;
        }
        cmd[32].field_4 = 0;
    } else {
        for (i = 0xB; i < 0x15; i++) {
            if (mid == i) {
                cmd[i].field_4  = 0;
                cmd[32].field_4 = 1;
            } else {
                cmd[i].field_4 = 1;
            }
        }
    }

    if (lo == 0x1F) {
        for (i = 1; i < 0xB; i++) {
            cmd[i].field_4 = 1;
        }
        cmd[31].field_4 = 0;
    } else {
        for (i = 1; i < 0xB; i++) {
            if (lo == i) {
                cmd[i].field_4  = 0;
                cmd[31].field_4 = 1;
            } else {
                cmd[i].field_4 = 1;
            }
        }
    }

    cmd[34].field_4 = 1;
}

/// Shows one frame of the bridge prompt: in the current room's eighth SPRT
/// record, every command from 1 to 33 gets its skip-OT-link flag set and only
/// command 34 is left drawing. `func_acropolis_bridge_8017E4FC` calls this on
/// each of the first ten frames of a pass.
void func_acropolis_bridge_8017E81C(void)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;
    s32        i;

    Gp_GetViewIndex();
    cmd = Gp_SprtTables[sess->stage - 1][gGameSession->sprtVariant - 1].field_0[sess->area - 1][7].field_4;

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

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2`s - top, right,
/// bottom and left edge - each linked into `gGpuCurrentOt[1]`.
void func_acropolis_bridge_8017E908(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

/// Slides one of three mutually exclusive bridge sprites in view 9 by
/// `(dx, dy)` and makes it the visible one. Each state owns three consecutive
/// `GpSprtElem` entries, which move together, and one of the three
/// `GpSprtCmd` slots; `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip
/// OT-linking", so the selected command gets 0 and the other two get 1. A
/// state outside 0..2 moves nothing and hides all three.
void func_acropolis_bridge_8017EB4C(s32 state, s8 dx, s8 dy)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtRec*   rec;
    GpSprtElem*  el;
    GpSprtCmd*   cmd;
    s32          mode;

    rec  = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];
    cmd  = rec[9].field_4;
    el   = rec[9].field_0;
    mode = state & 0xFF;

    if (mode == 0) {
        el[0].x0      += dx;
        el[0].y0      += dy;
        el[1].x0      += dx;
        el[1].y0      += dy;
        el[2].x0      += dx;
        el[2].y0      += dy;
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    } else if (mode == 1) {
        el[3].x0      += dx;
        el[3].y0      += dy;
        el[4].x0      += dx;
        el[4].y0      += dy;
        el[5].x0      += dx;
        el[5].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 1;
    } else if (mode == 2) {
        el[6].x0      += dx;
        el[6].y0      += dy;
        el[7].x0      += dx;
        el[7].y0      += dy;
        el[8].x0      += dx;
        el[8].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    }
}

/// State 1 of the room's prompt script task: moves the action-prompt cursors
/// from the pads and draws them.
///
/// `Task::spawnArg1` picks the ports: 1 drives port 0 only, 2 port 1 only,
/// anything else both. Each port's analog stick (pad status 0x12 linear, 0x73
/// squared) and then its d-pad, whose four bits pick one of eight headings,
/// move the prompt's 1/512-pixel position, which is clamped to the screen. The
/// confirm (0x40) and cancel (0xA0) buttons are classified into the prompt's
/// two button slots, a second press within `field_E` frames at an unmoved
/// cursor reporting state 4 instead of 2. `targetId` is the cursor speed here.
void func_acropolis_bridge_8017ED38(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_acropolis_bridge_8017F198(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the action-prompt cursor, a 16x24 textured quad, at (`x`, `y`) into
/// the head of the current OT. `variant` is the prompt's mode: 0 draws
/// nothing, 2 uses clut 0x3C87 and anything else 0x3C88.
void func_acropolis_bridge_8017F198(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Two-state dispatcher of the room's prompt script task: state 0 resets the
/// action prompts, state 1 moves and draws their cursors.
void func_acropolis_bridge_8017F280(Task* task)
{
    TaskFunc states[2] = { func_acropolis_bridge_8017F808, func_acropolis_bridge_8017ED38 };

    states[task->state](task);
}

/// Repaints the two bridge sprites that game flag nibble 0x10 governs: one
/// sprite command in view 2 of this room's sprite record and one in view 5.
/// `Gp_LinkViewSprts` reads `field_4` to decide whether to skip OT-linking a
/// command's prims, so a zero nibble draws both and a non-zero one hides them.
void func_acropolis_bridge_8017F2D0(s32 flags)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtRec*   rec;
    GpSprtCmd*   cmd;

    rec = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];

    cmd = rec[1].field_4;
    if ((flags & 0xFF) == 0) {
        cmd[11].field_4 = 0;
    } else {
        cmd[11].field_4 = 1;
    }

    cmd = rec[4].field_4;
    if ((flags & 0xFF) == 0) {
        cmd[16].field_4 = 0;
    } else {
        cmd[16].field_4 = 1;
    }
}

/// Picks which of three mutually exclusive bridge sprites view 9 of this room
/// draws. `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip OT-linking",
/// so the selected command gets 0 and the other two get 1; a state outside
/// 0..2 hides all three.
void func_acropolis_bridge_8017F358(s32 state)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtRec*   rec;
    GpSprtCmd*   cmd;
    s32          mode;

    rec  = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];
    cmd  = rec[9].field_4;
    mode = state & 0xFF;

    if (mode == 0) {
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 1;
    } else if (mode == 2) {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    }
}

/// Arms the action prompt for a fresh script step: parks the cursor at the top
/// left with the highlight mode on and the cursor speed at 0x80, tears down any
/// prompt still up, then advances the task to its next state.
void func_acropolis_bridge_8017F404(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    func_acropolis_bridge_8017E60C(0xFFF, 0);
    task->state++;
}

/// Spawns the action prompt for the script's current step: closes the previous
/// prompt, clears the highlight state, then re-spawns the prompt at the
/// coordinates the gameplay side left in `D_80114D28` with this step's display
/// mode, and advances the task to state 4.
void func_acropolis_bridge_8017F460(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;

    func_acropolis_bridge_8017E60C(work->field_4, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Closes the prompt the script's current step put up, clears the highlight
/// state, and advances the task to state 2. If `func_800D4EC0` still reports a
/// prompt on screen, the step is flagged busy in `promptBusy` (which the
/// hotspot scan in `func_acropolis_bridge_8017E1D0` gates on) and cap slot 9 is
/// started.
void func_acropolis_bridge_8017F4CC(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;

    func_acropolis_bridge_8017E60C(work->field_4, 0);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        work->promptBusy = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

/// Waits ten frames on the prompt the script's current step put up, then closes
/// it. Step 0x561 is the one the room answers with message 0x7DA before its
/// confirmation sound and state 6; every other step just clears the step's
/// counters, plays the cancel sound and goes to state 7. Either way the prompt
/// is torn down and the cursor is re-hit-tested against the room's hotspot
/// table, so `mode` reports whether it ended up over one.
void func_acropolis_bridge_8017F544(Task* task)
{
    RoomActionPrompt*          prompt = &D_80114D28;
    AcropolisBridgePromptWork* work   = (AcropolisBridgePromptWork*)task->work;
    OverlayHotspot*            hs     = D_acropolis_bridge_8018983C;

    if (work->field_A < 0xA) {
        work->field_A++;
        return;
    }

    if (work->field_4 != 0x561) {
        SndEvt_EnqueueType6(0x510E0004, 0, 0);
        work->field_8 = 0;
        work->field_A = 0;
        task->state   = 7;
    } else {
        GpCmdArg msg = { { { 1, 0xE } }, 2 };

        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        SndEvt_EnqueueType6(0x510E0009, 0, 0);
        task->state = 6;
    }
    func_acropolis_bridge_8017E60C(work->field_4, 0);
    if (func_acropolis_bridge_8017F6D4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
    } else {
        prompt->mode = 1;
    }
}

void func_acropolis_bridge_8017F658(Task* task)
{
    Display_ReleaseRef();
    func_acropolis_bridge_8017E60C(0xFFF, 0);
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, D_acropolis_bridge_801917A8);
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_80114D08                 = 0xA;
}

/// Hit-tests (`x`, `y`) against the hotspot table `table`, terminated by an
/// `id` of -1: raises `hit` on every entry whose rectangle contains the point
/// and clears it on the others, and answers whether any entry was hit.
s32 func_acropolis_bridge_8017F6D4(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Nine-state dispatcher of this room's script task: copies the handler table
/// out of the overlay's rodata onto the stack and tails into the entry named by
/// `Task::state`.
void func_acropolis_bridge_8017F788(Task* task)
{
    TaskFuncTable9 states;

    states = D_acropolis_bridge_8017D614;
    states.funcs[task->state](task);
}

/// State 0 of the prompt script task: resets both action-prompt slots - cursor
/// cleared, speed 0x100, double-press window 0xF frames, mode 1 - and steps the
/// task on one state.
void func_acropolis_bridge_8017F808(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// Per-frame driver for the bridge's ambient effect field, and the room's
/// message-table owner. On the first frame it publishes
/// `D_acropolis_bridge_801898FC` as slot 5's `Gp_DispatchMsg` table and seeds
/// `D_acropolis_bridge_80189A34` with the two tracked cable joints' world
/// positions.
///
/// Each frame it re-spawns the effects the current camera can see: the two
/// per-view bitmask tables (`D_acropolis_bridge_801899EC` /
/// `D_acropolis_bridge_80189A1C`) say which of the placed emitters are visible
/// from view `Gp_GetViewIndex()`, and each visible entry spawns its dust
/// (0x600B1 / 0x600B2) or spark (0x600B3) at the matching `SVECTOR`. View 9
/// lifts the dust 0x240 above the placed point.
///
/// On views 2, 5 and 6 (`bit & 0x62`) it also trails debris off the two moving
/// joints: `field_26` is the Manhattan distance the joint travelled since last
/// frame, biased by 0x20, and two `Gp_LcgState` rolls against that distance
/// decide whether this frame emits `D_8011574C` / `D_80115738`. The joint's
/// new position is written back for the next frame's delta.
///
/// `D_acropolis_bridge_801899FC` finally maps the view onto one of five
/// looping ambience effects (0x600B4..0x600B8): entering the view bursts 30
/// copies at once, and staying in it emits one per frame - one in two while
/// `Gp_State1C->battleState` says no battle is engaged, one in three while one
/// is, so that the ambience thins out during a fight.
void func_acropolis_bridge_8017F868(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;
    Task*          owner;
    SVECTOR        pos;
    u8             view;
    s32            bit;
    s32            i;
    s32            delta;
    s32            axis;
    s32            dist;
    s32            prev;
    s16            lastView;
    u16            rnd;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    owner = gameGetPtrSlot(3);
    part  = ((TmdObject*)owner->extra)->coords;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->eventState >= 4) {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->msgTable         = D_acropolis_bridge_801898FC;
        Game_SetPtrSlot(task, 5);
        D_8011574C  = 0x600B9;
        D_80115738  = 0x600BA;
        task->state = task->state + 1;
        for (i = 0; i < 2; i++) {
            part                              = &((TmdObject*)owner->extra)->coords[14 + i * 3];
            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    work->age = work->age + 1;
    switch (view) {
        case 6:
            func_acropolis_bridge_80183654(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 7:
            func_acropolis_bridge_80183654(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 3:
        case 4:
        case 9:
            func_acropolis_bridge_80183654(&D_acropolis_bridge_80189A4C, 0x100, 0x50C2);
            break;
    }

    bit = 1 << (view - 1);
    if (view == 9) {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                pos.vx = 0;
                pos.vy = -0x240;
                pos.vz = 0;
                pos.vx = D_acropolis_bridge_8018991C[i].vx;
                pos.vy = D_acropolis_bridge_8018991C[i].vy - 0x240;
                pos.vz = D_acropolis_bridge_8018991C[i].vz;
                Gp_SpawnEff(0x800600B1, coord, work->age + i, &pos);
            }
        }
    } else {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                Gp_SpawnEff(0x800600B1, coord, work->age + i, &D_acropolis_bridge_8018991C[i]);
                Gp_SpawnEff(0x600B2, coord, work->age + i, &D_acropolis_bridge_80189954[i]);
            }
        }
    }

    for (i = 0; i < 3; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(0x600B3, coord, 0, &D_acropolis_bridge_8018998C[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(0x600B3, coord, 1, &D_acropolis_bridge_8018998C[i]);
        }
        if (D_acropolis_bridge_80189A1C[i + 2] & bit) {
            Gp_SpawnEff(0x600B3, coord, 2, &D_acropolis_bridge_8018998C[i + 2]);
        }
    }
    if (D_acropolis_bridge_80189A1C[11] & bit) {
        Gp_SpawnEff(0x600B3, coord, 1, &D_acropolis_bridge_8018998C[11]);
    }

    if ((bit & 0x62) && Gp_State1C->eventState == 0 && part->coord.t[1] >= 0x201) {
        for (i = 0; i < 2; i++) {
            part  = &((TmdObject*)owner->extra)->coords[14 + i * 3];
            delta = D_acropolis_bridge_80189A34[i].vx - part->workm.t[0];
            dist  = delta < 0;
            if (dist) {
                delta = part->workm.t[0] - D_acropolis_bridge_80189A34[i].vx;
            }
            prev = D_acropolis_bridge_80189A34[i].vy;
            axis = prev - part->workm.t[1];
            if (axis < 0) {
                axis = part->workm.t[1] - prev;
            }
            dist        = delta + axis;
            prev        = D_acropolis_bridge_80189A34[i].vz;
            axis        = part->workm.t[2];
            delta       = prev - axis;
            delta       = ((delta >= 0) ? (dist + delta) : (dist + (axis - prev))) + 0x20;
            work->angle = delta;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rnd         = (u32)Gp_LcgState >> 16;
            if ((rnd & 0x1FF) < work->angle) {
                Gp_SpawnEff(D_8011574C, part, 0x40, NULL);
            }
            work->angle = work->angle - 0x20;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rnd         = (u32)Gp_LcgState >> 16;
            if ((rnd & 0x1FF) < work->angle) {
                Gp_SpawnEff(D_80115738, part, 0x1202180, NULL);
            }

            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    D_acropolis_bridge_801917AC =
        (DR_MOVE*)((u8*)D_8005C374 + (gDisplayState.otBuffer * 0x7000 + 0xA000));

    switch (D_acropolis_bridge_801899FC[view - 1]) {
        case 0:
            break;
        case 1:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B4, coord, view, NULL);
                }
            } else if (Gp_State1C->battleState != 1) {
                if (work->age & 0x200) {
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 1) == 0) {
                        Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                    }
                }
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                }
            }
            break;
        case 2:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B5, coord, view, NULL);
                }
            } else if (Gp_State1C->battleState != 1) {
                Gp_SpawnEff(0x600B5, coord, lastView, NULL);
                Gp_SpawnEff(0x600B5, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B5, coord, lastView, NULL);
                }
            }
            break;
        case 3:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B6, coord, view, NULL);
                }
            } else if (Gp_State1C->battleState != 1) {
                Gp_SpawnEff(0x600B6, coord, lastView, NULL);
                Gp_SpawnEff(0x600B6, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B6, coord, lastView, NULL);
                }
            }
            break;
        case 5:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B7, coord, view, NULL);
                }
            } else if (Gp_State1C->battleState != 1) {
                Gp_SpawnEff(0x600B7, coord, lastView, NULL);
                Gp_SpawnEff(0x600B7, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B7, coord, lastView, NULL);
                }
            }
            break;
        case 6:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B8, coord, view, NULL);
                }
            } else if (Gp_State1C->battleState != 1) {
                Gp_SpawnEff(0x600B8, coord, lastView, NULL);
                Gp_SpawnEff(0x600B8, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B8, coord, lastView, NULL);
                }
            }
            break;
    }

    work->scale = view;
}

/// The wide variant of the bridge's falling dust streak: same one-pixel `DR_MOVE`
/// smear as `func_acropolis_bridge_80180FF0`, rolled over the whole drop height
/// instead of the upper band. The first frame rolls the streak out of
/// `Gp_LcgState`: `move.vy` is the row it starts on (0x60..0xEF),
/// `scale` the lifetime in frames, `angle` the width and `period` the
/// number of frames each row of fall takes. The column window widens with the
/// starting row - it runs from `0x40 - (vy - 0x60) / 3` to `0xD0 + spread`,
/// where `spread` is half the drop from 0x60 capped at 0x20 - so streaks that
/// begin higher up stay nearer the middle of the screen.
/// `gDisplayState.drawBuffer` picks the buffer half, and the OT slot is the row
/// scaled into the 0x800-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_80180320(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        rndx;
    s32        range;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = (u32)rnd % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x             = (work->move.vy - 0x60) / 3;
            y             = work->move.vy < 0xA0 ? (work->move.vy - 0x60) / 2 : 0x20;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rndx          = (u32)Gp_LcgState >> 16;
            range         = y + 0x90;
            work->move.vx = rndx % (x + range) + (0x40 - x);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The mid variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, rolled over the whole
/// drop height and sorted by a squared depth ramp like
/// `func_acropolis_bridge_80180CC0`, but nearer the camera. The first frame
/// rolls the streak out of `Gp_LcgState`: `move.vy` is the row it starts on
/// (0x48..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is the whole drop from 0x48 capped at 0x58
/// and `spread` five thirds of it capped at 0x50 - so streaks that begin higher
/// up stay nearer the middle of the screen. `gDisplayState.drawBuffer` picks the
/// buffer half, and the OT slot grows with the *square* of the distance left to
/// fall, so a streak near the bottom of the screen sorts sharply in front of
/// one still high up. The task releases itself once the camera turns away, the
/// lifetime runs out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018063C(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        rndx;
    s32        col;
    s32        range;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x             = work->move.vy < 0xA0 ? work->move.vy - 0x48 : 0x58;
            y             = work->move.vy < 0x78 ? (work->move.vy - 0x48) * 5 / 3 : 0x50;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rndx          = (u32)Gp_LcgState >> 16;
            range         = y + 0x48;
            col           = rndx % (x + range) + 0x58;
            col          -= x;
            work->move.vx = col;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x2C0;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The narrow variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// lower part of the drop and sorted nearer the camera. The first frame rolls
/// the streak out of `Gp_LcgState`: `move.vy` is the row it starts on
/// (0x68..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x20 - drift` to
/// `0x60 + spread`, where `drift` is twice and `spread` nine times the drop
/// from 0x68, both capped once the streak starts at 0x78 or below - so streaks
/// that begin higher up stay nearer the middle of the screen.
/// `gDisplayState.drawBuffer` picks the buffer half, and the OT slot is the row
/// scaled into the 0x600-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018099C(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        rndx;
    s32        col;
    s32        range;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = (u32)rnd % 136 + 0x68;
            /* x and y double as the drift and spread of the column window here */
            x             = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 2 : 0x20;
            y             = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 9 : 0x90;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rndx          = (u32)Gp_LcgState >> 16;
            range         = y + 0x40;
            col           = rndx % (x + range) + 0x20;
            col          -= x;
            work->move.vx = col;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x600 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The tallest variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// whole screen height and sorted by a squared depth ramp. The first frame
/// rolls the streak out of `Gp_LcgState`: `move.vy` is the row it starts on
/// (0x48..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is a third and `spread` a half of the drop
/// from 0x48 - so streaks that begin higher up stay nearer the middle of the
/// screen. `gDisplayState.drawBuffer` picks the buffer half, and the OT slot
/// grows with the *square* of the distance left to fall, so a streak near the
/// bottom of the screen sorts sharply in front of one still high up. The task
/// releases itself once the camera turns away, the lifetime runs out, or the
/// streak falls off the bottom of the screen.
void func_acropolis_bridge_80180CC0(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        rndx;
    s32        col;
    s32        range;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x             = (work->move.vy - 0x48) / 3;
            y             = (work->move.vy - 0x48) / 2;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rndx          = (u32)Gp_LcgState >> 16;
            range         = y + 0x48;
            col           = rndx % (x + range) + 0x58;
            col          -= x;
            work->move.vx = col;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x400;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One falling dust streak on the bridge, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole streak out of `Gp_LcgState`: `move.vy` is the row it
/// starts on (0x68..0xE7), `move.vx` the column, `scale` the lifetime in
/// frames, `angle` the width and `period` the number of frames each row of
/// fall takes. The column is drawn from a range that widens with the starting
/// row - `(vy - 0x58) * 6`, capped at the full 240-pixel width once the streak
/// starts at 0x80 or below the horizon - so streaks that begin higher up stay
/// nearer the middle of the screen. `gDisplayState.drawBuffer` picks the buffer
/// half, and the OT slot is the row scaled into the 0x800-deep range so a
/// streak sorts against the room behind it. The task releases itself once the
/// camera turns away, the lifetime runs out, or the streak falls off the bottom
/// of the screen.
void func_acropolis_bridge_80180FF0(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        rndx;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x68;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rndx          = (u32)Gp_LcgState >> 16;
            work->move.vx = work->move.vy < 0x80 ? rndx % ((work->move.vy - 0x58) * 6) : rndx % 240;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of the bridge's twinkling dust spark: the task coordinate's
/// translation is projected through `GsWSMATRIX` with a single `RTPS` into an
/// `OverlaySpriteScratch` block taken from `G_SCRATCH_HEAD`, and two
/// `POLY_FT4`s are linked into the OT at that depth. The first is an upright
/// 0x1680 / otz square whose 0x10-wide texture cell is picked by
/// `work->age % 6`, drawn with texture blending off (`code |= 3`). The
/// second is the same point drawn as a spinning semi-transparent grey quad:
/// its two half-diagonals are `(0x3A80 / otz) * rsin` / `rcos` of
/// `work->scale`, which advances with `gDisplayState.animFrame`, and its tint
/// is a fresh random grey (0x20..0x7F) every frame. Depths under 0x11 drop
/// both quads. The task releases its work block each tick, so the spark lasts
/// one frame.
void func_acropolis_bridge_801812F4(Task* task)
{
    GsCOORDINATE2*        coord;
    GpEffWork*            work;
    void**                scratch;
    u8*                   head;
    OverlaySpriteScratch* blk;
    s32*                  otzp;
    POLY_FT4*             prim;
    s32                   grey;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    work->age   = task->spawnArg1;
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (OverlaySpriteScratch*)(head - 0x18);
    otzp        = &blk->otz;
    blk->vec.vx = coord->workm.t[0];
    blk->vec.vy = coord->workm.t[1];
    *scratch    = blk;
    blk->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4380;
        prim->code |= 3;
        prim->u0    = (work->age % 6) * 16;
        prim->v0    = 0;
        prim->u1    = (work->age % 6) * 16 + 0xF;
        prim->v1    = 0;
        prim->u2    = (work->age % 6) * 16;
        prim->v2    = 0xF;
        prim->u3    = (work->age % 6) * 16 + 0xF;
        prim->v3    = 0xF;
        blk->dx     = 0x1680 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->dx;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->dx;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->dx;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->dx;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);

        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut  = 0x4381;
        prim->tpage = 0x2B;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) % 96 + 0x20;
        prim->u0    = 0;
        prim->v0    = 0x10;
        prim->u1    = 0x27;
        prim->v1    = 0x10;
        prim->u2    = 0;
        prim->v2    = 0x37;
        prim->u3    = 0x27;
        prim->v3    = 0x37;
        prim->code |= 2;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;

        work->scale = gDisplayState.animFrame + work->age;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale)) >> 12;
        prim->x0    = blk->sxy.vx + blk->dx;
        prim->x3    = blk->sxy.vx - blk->dx;
        prim->y0    = blk->sxy.vy - blk->dy;
        prim->y3    = blk->sxy.vy + blk->dy;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale + 0x400)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale + 0x400)) >> 12;
        prim->x1    = blk->sxy.vx + blk->dx;
        prim->x2    = blk->sxy.vx - blk->dx;
        prim->y1    = blk->sxy.vy - blk->dy;
        prim->y2    = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
    Gp_ReleaseState1CMem(work, task);
}

/// The bridge's dust cloud: one semi-transparent `POLY_FT4` billboard placed at
/// the task's world position. The four corners are taken from the unit quad in
/// `D_acropolis_bridge_8018990C`, scaled by 0x300 and rotated by the
/// coordinate's `workm` - and then each rotated corner is overwritten with that
/// same `workm` translation, so all four collapse onto the object origin. The
/// quad is projected with one `RTPS` plus one `RTPT` directly into the
/// primitive, tinted a random grey, and linked into the OT at the `RTPS` depth
/// biased by 0x20; depths under 0x11 are dropped rather than drawn. The task
/// releases its work block on every tick, so the puff lasts one frame.
void func_acropolis_bridge_801819C8(Task* task)
{
    void**                      scratch;
    u8*                         head;
    AcropolisBridgeQuadScratch* block;
    AcropolisBridgeQuadCorner*  tbl;
    POLY_FT4*                   prim;
    GsCOORDINATE2*              coord;
    GpEffWork*                  work;
    MATRIX*                     m;
    SVECTOR*                    v;
    s32                         i;
    u8                          col;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);

    scratch   = (void**)G_SCRATCH_HEAD;
    i         = 0;
    m         = &coord->workm;
    tbl       = D_acropolis_bridge_8018990C;
    head      = SCRATCH_HEAD_AT(scratch, u8) - sizeof(AcropolisBridgeQuadScratch);
    work->age = ((GpEffSpawnArg*)&task->spawnArg1)->field_0;
    *scratch  = head;
    block     = (AcropolisBridgeQuadScratch*)*scratch;
    do {
        v                = ((AcropolisBridgeQuadScratch*)((SVECTOR*)block + i))->vec;
        block->vec[i].vx = tbl[i].x * 0x300;
        v->vy            = 0;
        v->vz            = tbl[i].y * 0x300;
        gte_SetRotMatrix(m);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        *(u16*)&block->vec[i].vx = *(u16*)&coord->workm.t[0];
        i++;
        *(u16*)&v->vy = *(u16*)&coord->workm.t[1];
        *(u16*)&v->vz = *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    setUV4(prim, 0, 0x10, 0x27, 0x10, 0, 0x37, 0x27, 0x37);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&block->otz);
    block->otz += 0x20;
    if (block->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4381;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        col         = ((u32)Gp_LcgState >> 16) & 0xF;
        setRGB0(prim, col, col, col);
        setSemiTrans(prim, 1);
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP(AcropolisBridgeQuadScratch);
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of a glow sprite: a camera-facing, semi-transparent `POLY_FT4`
/// centred on the task's own coordinate frame, drawn as a square of
/// half-extent `0x6180 / otz` around the projected point and dropped inside
/// `otz` 0x11.
///
/// `Task::spawnArg1` picks one of three lamps: the 0x27x0x27 texture cell at
/// `u = (arg + 1) * 0x28`, `v = 0x10` on tpage 0x2B, the clut
/// `0x4380 | ((arg + 2) & 0x3F)`, and the grey levels the sprite flickers
/// between on odd and even `gDisplayState.animFrame`. The work block is
/// released once the quad is queued, so the task lives for one frame.
void func_acropolis_bridge_80181D28(Task* task)
{
    GsCOORDINATE2*         coord;
    GpEffWork*             work;
    void**                 scratch;
    u8*                    head;
    RoomGlowSpriteScratch* blk;
    // The `gte_stszotz` operand is a second register holding the same pointer;
    // reload only emits that copy for a hard-register local (see
    // DECOMPILATION_LEARNINGS.md, "A `move` between two registers holding the
    // same pointer is a pin").
    register RoomGlowSpriteScratch* p asm("a0");
    POLY_FT4*                       prim;
    s32                             grey;
    s32                             clut;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (RoomGlowSpriteScratch*)(head - 0x14);
    blk->pos.vx = coord->workm.t[0];
    blk->pos.vy = coord->workm.t[1];
    *scratch    = blk;
    p           = blk;
    blk->pos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(&p->otz);
    if (blk->otz >= 0x11) {
        u8 base[3] = { 0x20, 0x60, 0x20 };
        u8 step[3] = { 0x08, 0x10, 0x0C };

        grey        = base[task->spawnArg1] + (gDisplayState.animFrame & 1) * step[task->spawnArg1];
        prim->code |= 2;
        prim->tpage = 0x2B;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        // Assigning through an `s32` keeps the load of `spawnArg1` in SImode;
        // storing the expression straight into the `u16` field lets the front
        // end shorten the whole chain and the load becomes an `lhu`.
        clut       = ((task->spawnArg1 + 2) & 0x3F) | 0x4380;
        prim->clut = clut;
        prim->u0   = (task->spawnArg1 + 1) * 0x28;
        prim->v0   = 0x10;
        prim->u1   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v1   = 0x10;
        prim->u2   = (task->spawnArg1 + 1) * 0x28;
        prim->v2   = 0x37;
        prim->u3   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v3   = 0x37;
        blk->half  = 0x6180 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->half;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->half;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->half;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->half;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x14);
    Gp_ReleaseState1CMem(work, task);
}

s32 func_acropolis_bridge_801820A0(Task* task)
{
    GsCOORDINATE2* coord;
    SVECTOR        pos;
    s32            i;

    coord = ((TmdObject*)task->extra)->coords;

    i = 0;
    do {
        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = 0xFA06 - (((u32)Gp_LcgState >> 16) & 0xF);
        Gp_SpawnEff(0x600BC, coord, 0, &pos);

        pos.vx      = -0x3E58;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = (((u32)Gp_LcgState >> 16) & 0xF) + 0xF830;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 + 0xF63C;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 0x20);

    i = 0;
    do {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vx      = -0x3E58;
        pos.vy      = ((u32)Gp_LcgState >> 16) % 1536 - 0x7D0;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = (u16)((u32)Gp_LcgState >> 16) % 970 - 0x9C4;
        Gp_SpawnEff(0x600BC, coord, 0, &pos);
        i++;
    } while (i < 8);

    return 0;
}

/// One falling mote of the bridge's ambient dust: drifts the task's coordinate
/// frame by the per-mote velocity in `GpEffWork::move`, projects the
/// result through `GsWSMATRIX` with a single `RTPS`, and links a 1x1 tile into
/// the OT at the resulting depth. The velocity and the grey level are rolled
/// once, on the first tick (`age == 0`); the mote is released after 0x1F
/// ticks or once it has fallen past y = -0x1D.
void func_acropolis_bridge_80182394(Task* task)
{
    void**           scratch;
    u8*              head;
    RoomMoteScratch* block;
    RoomMoteScratch* depth;
    TILE_1*          prim;
    GsCOORDINATE2*   coord;
    GpEffWork*       work;

    scratch  = (void**)G_SCRATCH_HEAD;
    coord    = ((TmdObject*)task->extra)->coords;
    head     = *scratch;
    block    = (RoomMoteScratch*)(head - 0xC);
    *scratch = block;
    depth    = block;
    work     = task->spawnArg2;
    Gp_UpdateCoord(coord);

    if (work->age == 0) {
        work->move.vz = 0;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->move.vx = ((u32)Gp_LcgState >> 16) & 0xF;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->move.vy = (((u32)Gp_LcgState >> 16) & 3) - 1;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->scale   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x30;
    }

    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;
    block->vec.vx      = *(u16*)&coord->workm.t[0];
    block->vec.vy      = *(u16*)&coord->workm.t[1];
    block->vec.vz      = *(u16*)&coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomMoteScratch*)(head - 0xC))->vec);
    gte_rtps();
    prim           = (TILE_1*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setTile1(prim);
    gte_stsxy(&prim->x0);
    gte_stszotz(&depth->otz);
    if (((RoomMoteScratch*)(head - 0xC))->otz >= 0x11) {
        setRGB0(prim, work->scale >> 1, work->scale, work->scale);
        addPrim((u_long*)(((((u32)((RoomMoteScratch*)(head - 0xC))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 0, ((RoomMoteScratch*)(head - 0xC))->otz);
        work->move.vy += 6;
    }
    SCRATCH_POP_BYTES_AT(scratch, 0xC);
    work->age++;
    if (work->age >= 0x1F || coord->coord.t[1] >= -0x1D) {
        Gp_ReleaseState1CMem(work, task);
    }
}

void func_acropolis_bridge_80182694(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_acropolis_bridge_801827EC(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0x40;
                work->angle = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
                coord->flg  = 0;
                task->state = 1;
                /* fallthrough */
            case 1:
                work->angle += 0x20;
                func_acropolis_bridge_801827EC(coord, work->angle, work->scale);
                if (work->scale >= 3) {
                    work->scale -= 2;
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the flash the bridge collapse throws off as a screen-facing quad: the
/// unit quad `D_80111E38` scaled to `arg1` half-size, rotated by the task's own
/// `GsCOORDINATE2` (`workm`) and then projected through `GsWSMATRIX` into a
/// 0x28-byte `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and
/// the other three through `rtpt`; a GTE error (`gte_stflg` sign bit) drops the
/// quad rather than drawing it. The `POLY_FT4` is the 0x38x0x38 cell at
/// `(0, 0x38)` of tpage 0x2B, modulated by the grey `arg2` and drawn
/// semi-transparent, and links into the OT at the projected depth.
void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    register GsCOORDINATE2*    coord asm("t7");
    void**                     scratch;
    u8*                        head;
    OverlayFlaggedQuadScratch* blk;
    POLY_FT4*                  prim;
    GpQuadCorner*              tbl;
    SVECTOR*                   sv;
    MATRIX*                    wm;
    s32                        i;

    coord = arg0;
    SOFT_TOUCH_REG(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = SCRATCH_HEAD_AT(scratch, u8) - sizeof(OverlayFlaggedQuadScratch);
    SOFT_TOUCH_REG(head);
    *scratch = head;
    blk      = (OverlayFlaggedQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &coord->workm;
    tbl = D_80111E38;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        sv           = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(OverlayFlaggedQuadScratch, v));
        sv->vy       = 0;
        sv->vz       = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        *(u16*)&blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        *(u16*)&sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&sv->vz = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    setUV4(prim, 0, 0x38, 0x37, 0x38, 0, 0x6F, 0x37, 0x6F);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        blk->otz++;
        prim->tpage = 0x2B;
        setRGB0(prim, arg2, arg2, arg2);
        prim->clut = 0x43D1;
        setSemiTrans(prim, 1);
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP(OverlayFlaggedQuadScratch);
}

/// Controller for one piece of the bridge's blown debris: it drifts the task's
/// coordinate frame by a velocity it rolls once, and hands the frame to
/// `func_acropolis_bridge_80182F8C` (state 1) or `func_acropolis_bridge_801833A0`
/// (state 2) to be drawn. Everything the piece needs is packed into
/// `Task::spawnArg1`: bits 0-11 become `scale`, bits 12-15 the number of
/// ticks each animation step lasts (`period`, 1 if zero), bits 16-23 the
/// speed the velocity is scaled to (`step`, 0x40 if zero), bits 24-27 the
/// launch pattern and bits 28-31 pick which of the two draw helpers runs. The
/// first tick also rolls the 12-bit `angle` out of `Gp_LcgState`; both it
/// and `scale` are passed to the draw helper every tick.
///
/// The launch pattern rolls `move` when the caller left it zero: 1 spreads
/// X and Z evenly over +/-0x80 and biases Y to -0x40..-0xBF, 2 spreads all
/// three evenly over +/-0x80, 3 keeps X and Z inside +/-0x10 and drives Y to
/// 0..-0xFF, 5 copies the velocity the caller staged at `pos`, and 0 stops
/// the piece from drifting at all by zeroing `step`. The rolled direction
/// is then normalized and scaled back up to `step` with one GTE `GPF`, so
/// the pattern only picks a direction and the packed speed sets the length.
///
/// Once running, a piece with a non-zero `step` adds its velocity onto the
/// coordinate's translation each tick and bends Y by 6 as it goes, and every
/// `period` ticks steps `index`; the eighth step releases the work block.
/// While `Gp_State1C->eventState` is set the room is fading out, so the piece only
/// keeps drawing, and releases itself once the fade reaches 4.
void func_acropolis_bridge_80182AF8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_acropolis_bridge_80182F8C(coord, work->index, work->scale, work->angle);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_acropolis_bridge_80182F8C(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_acropolis_bridge_801833A0(coord, work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws one piece of the bridge's blown debris as a screen-facing quad. The
/// piece's world position is copied out of `coord->workm.t` and projected
/// through `GsWSMATRIX` with a single `RTPS`; a GTE error (`gte_stflg` sign
/// bit) drops the piece rather than drawing it. The `POLY_FT4` is centred on
/// the projected point, its two diagonals `size * 31 / otz` long and turned by
/// `angle` and `angle + 0x400`, so the quad shrinks with distance and spins
/// with the piece. `frame` picks the animation cell: the texture window is the
/// 0x1F-wide column starting at `frame * 0x20` on rows 0xE0..0xFF of tpage
/// 0x2B. The primitive is semi-transparent with texture blending off
/// (`code |= 3`) and links into the OT at the projected depth.
void func_acropolis_bridge_80182F8C(GsCOORDINATE2* coord, u16 frame, s16 size, s16 angle)
{
    void**                        scratch;
    u8*                           head;
    AcropolisBridgeSpriteScratch* block;
    POLY_FT4*                     prim;
    s32                           ang;
    AcropolisBridgeSpriteScratch* depth;
    s32                           u;
    s32                           uu;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (AcropolisBridgeSpriteScratch*)(head - sizeof(AcropolisBridgeSpriteScratch));
    depth   = block;

    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&coord->workm.t[2];
    *scratch      = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->sx);
    gte_stflg(&((AcropolisBridgeSpriteScratch*)(head - 0x1C))->flag);

    if (block->flag >= 0) {
        gte_stszotz(&depth->otz);
        ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u           = frame << 5;
        uu          = u + 0x1F;
        setUV4(prim, u, 0xE0, uu, 0xE0, u, 0xFF, uu, 0xFF);
        setcode(prim, getcode(prim) | 3);

        ang       = angle;
        block->dx = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rsin(ang)) >> 12;
        block->dy = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rcos(ang)) >> 12;
        prim->x0  = block->sx + *(u16*)&block->dx;
        prim->x3  = block->sx - *(u16*)&block->dx;
        prim->y0  = block->sy - *(u16*)&block->dy;
        prim->y3  = block->sy + *(u16*)&block->dy;

        ang      += 0x400;
        block->dx = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rsin(ang)) >> 12;
        block->dy = (size * 31 / ((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz * rcos(ang)) >> 12;
        prim->x1  = block->sx + *(u16*)&block->dx;
        prim->x2  = block->sx - *(u16*)&block->dx;
        prim->y1  = block->sy - *(u16*)&block->dy;
        prim->y2  = block->sy + *(u16*)&block->dy;

        addPrim((u_long*)(((((u32)((AcropolisBridgeSpriteScratch*)(head - 0x1C))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(AcropolisBridgeSpriteScratch));
}

/// Draws one piece of the bridge's blown debris as an upright screen-facing
/// quad - the unrotated counterpart of `func_acropolis_bridge_80182F8C`. The
/// piece's world position is copied out of `coord->workm.t` and projected
/// through `GsWSMATRIX` with a single `RTPS`; a GTE error (`gte_stflg` sign
/// bit) drops the piece rather than drawing it. The `POLY_FT4` is centred on
/// the projected point and is `size * 55 / otz` wide, half that tall above the
/// centre and half below, so it shrinks with distance without ever spinning.
/// `frame` picks the animation cell out of a 4x2 grid of 0x38x0x38 cells on
/// tpage 0x2B: bits 0-1 pick the column and bit 2 the row. The primitive is
/// semi-transparent with texture blending off (`code |= 3`) and links into the
/// OT at the projected depth.
void func_acropolis_bridge_801833A0(GsCOORDINATE2* coord, u16 frame, s16 size)
{
    void**                        scratch;
    u8*                           head;
    AcropolisBridgeDebrisScratch* block;
    POLY_FT4*                     prim;
    AcropolisBridgeDebrisScratch* depth;
    u32                           cell;
    s32                           u;
    s32                           v;
    s8                            vTop;
    s8                            vBot;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (AcropolisBridgeDebrisScratch*)(head - sizeof(AcropolisBridgeDebrisScratch));
    depth   = block;

    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&coord->workm.t[2];
    *scratch      = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->sx);
    gte_stflg(&((AcropolisBridgeDebrisScratch*)(head - 0x18))->flag);

    if (block->flag >= 0) {
        gte_stszotz(&depth->otz);
        ((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = frame;
        u           = (cell & 3) * 0x38;
        v           = ((cell & 7) >> 2) * 0x38;
        vTop        = v + 0x70;
        vBot        = v + 0x70 + 0x37;
        setUV4(prim, u, vTop, u + 0x37, vTop, u, vBot, u + 0x37, vBot);
        setcode(prim, getcode(prim) | 3);

        block->d = size * 55 / ((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz;

        prim->x0 = prim->x2 = block->sx - *(u16*)&block->d;
        prim->x1 = prim->x3 = block->sx + *(u16*)&block->d;
        prim->y0 = prim->y1 = block->sy - *(u16*)&block->d - (block->d >> 1);
        prim->y2 = prim->y3 = block->sy + (block->d >> 1);

        addPrim((u_long*)(((((u32)((AcropolisBridgeDebrisScratch*)(head - 0x18))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(AcropolisBridgeDebrisScratch));
}

/// Debug message the walker's route search prints when no candidate beat its
/// initial best of 0xFF.
const char D_acropolis_bridge_8017D6CC[] = "s->root_cnt == 0xff about \n";

/// The bridge enemy's three state handlers: setup, per-frame tick and teardown.
const GpEnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8 = {
    { func_acropolis_bridge_80185988, func_acropolis_bridge_80187850, Gp_DestroyEnemy }
};

/// Draws a flickering star-shaped glow at the world-space point `arg0`. The
/// point is projected through `Gfx_ViewWorldMtx`, and when the GTE flag is
/// non-negative a sixteen-wedge gouraud disc of radius `(s16)arg1 * 64 / otz`
/// is queued around it, alternating full and half brightness, followed by a
/// four-armed inner cross of radius `(s16)arg1 * 8 / otz`. `arg2` packs one
/// nibble per channel - bits 8..11 red, 4..7 green, 0..3 blue - with bits
/// 12..15 the shift of a brightness flicker on odd `gDisplayState.animFrame`.
void func_acropolis_bridge_80183654(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

/// Reports whether the walker has reached the patrol node at `work->node`. It
/// stages the XZ delta between the node and the walker's coordinate
/// translation in an 8-byte scratch block, then accepts the node if the walker
/// is inside either of two radii: its own `field_5C * 4`, or a flat 300.
s16 func_acropolis_bridge_80184024(OverlayWalker* work)
{
    OverlayWalkerArrivalDelta* d;
    u8*                        head;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x8;
    d                = (OverlayWalkerArrivalDelta*)(head - 0x8);

    d->x = work->nav->nodes[work->node].x;
    d->y = work->nav->nodes[work->node].y;
    d->z = work->nav->nodes[work->node].z;
    d->x = d->x - *(u16*)&work->coord->coord.t[0];
    d->y = 0;
    d->z = d->z - *(u16*)&work->coord->coord.t[2];

    if (!overlayWalkerOutOfRange(d, work->field_5C * 4) ||
        !overlayWalkerOutOfRange(d, 300)) {
        SCRATCH_POP_BYTES(0x8);
        return 1;
    }
    SCRATCH_POP_BYTES(0x8);
    return 0;
}

/// Steers the walker along its patrol route. `pos` receives the position of the
/// node it is heading for; while it is still short of that node the route's
/// `arrived` flag stays clear, and on the frame it gets there the flag is
/// raised, the movement deltas are cleared and the cursor steps to the next
/// node -- wrapping back to the first when it hits the 0xFF terminator -- so
/// `pos` already describes the new node.
void func_acropolis_bridge_80184208(OverlayWalker* work, SVECTOR3* pos)
{
    OverlayWalkerRoute* route;
    OverlayWalkerRoute* step;
    OverlayWalkerRoute* wrap;
    OverlayWalkerRoute* next;
    u8                  node;

    route      = work->route;
    work->node = route->nodes[route->cursor];
    if (func_acropolis_bridge_80184024(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    work->route->arrived = 1;
    step                 = work->route;
    work->field_62       = 0;
    work->field_64       = 0;
    step->cursor++;

    wrap = work->route;
    if (wrap->nodes[wrap->cursor] == 0xFF) {
        wrap->cursor = 0;
    }

    next       = work->route;
    node       = next->nodes[next->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}

/// Scans the room's patrol node table for the node nearest actor `actor` and
/// returns its index. Same scan as `func_acropolis_bridge_8018450C`, but
/// measured from the translation of the actor config's matrix rather than
/// from the walker's own coordinate; the walker uses it with the player
/// (entry 1) to pick the node it retreats to.
u8 func_acropolis_bridge_801843A0(OverlayWalker* work, s32 actor)
{
    OverlayWalkerNearCfgScratch* block;
    u8*                          head;
    s16                          dz;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x18;
    block            = SCRATCH_HEAD(OverlayWalkerNearCfgScratch);

    block->cfg  = &D_80073B08[(s16)actor];
    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&block->cfg->coordMtx->t[0] - work->nav->nodes[block->node].x;
        block->dy   = *(u16*)&block->cfg->coordMtx->t[1] - work->nav->nodes[block->node].y;
        dz          = *(u16*)&block->cfg->coordMtx->t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_POP_BYTES(0x18);
    return block->nearest;
}

/// Scans the room's patrol node table for the node nearest the walker and
/// returns its index. Distance is the squared XZ distance between the node and
/// the low halfword of the walker coordinate's translation, staged in a 0x14
/// byte scratch block along with the cursor and the running best.
u8 func_acropolis_bridge_8018450C(OverlayWalker* work)
{
    OverlayWalkerNearScratch* block;
    u8*                       head;
    s16                       dz;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x14;
    block            = SCRATCH_HEAD(OverlayWalkerNearScratch);

    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&work->coord->coord.t[0] - work->nav->nodes[block->node].x;
        dz          = *(u16*)&work->coord->coord.t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    SCRATCH_POP_BYTES(0x14);
    return block->nearest;
}

/// Re-plans the walker's position in the room's route byte table so that it
/// heads towards actor `actor`. It collects every slot of that table naming
/// the node nearest the actor and every slot naming the node nearest the
/// walker, then picks the pair of slots that are closest together: the
/// walker's cursor becomes the slot on its own side, `field_75` records the
/// slot on the actor's side, and `field_73` becomes the +1 / -1 direction the
/// cursor has to travel along the table to close the gap -- which the caller
/// then applies, as does the last line here. Both lists hold at most eight
/// slots, so a table with more matches than that is silently truncated; if no
/// pair was found at all the routine only complains and leaves the cursor
/// where it was.
void func_acropolis_bridge_80184638(OverlayWalker* work, s16 actor)
{
    OverlayWalkerRouteScratch* s;
    u8*                        head;
    s32                        diff;
    s32                        best;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x1C;
    s                = (OverlayWalkerRouteScratch*)(head - 0x1C);

    s->nodeA  = func_acropolis_bridge_801843A0(work, actor);
    s->nodeB  = func_acropolis_bridge_8018450C(work);
    s->countA = 0;
    s->countB = 0;
    for (s->i = 0; s->i < work->nav->field_9; s->i++) {
        if (work->nav->field_4[s->i] == s->nodeA && s->countA < 8) {
            s->listA[s->countA] = s->i;
            s->countA++;
        }
        if (work->nav->field_4[s->i] == s->nodeB && s->countB < 8) {
            s->listB[s->countB] = s->i;
            s->countB++;
        }
    }

    s->listA[s->countA] = 0xFF;
    s->listB[s->countB] = 0xFF;
    s->best             = 0xFF;
    for (s->i = 0; s->i < 8; s->i++) {
        if (s->listA[s->i] == 0xFF) {
            break;
        }
        for (s->j = 0; s->j < 8; s->j++) {
            if (s->listB[s->j] == 0xFF) {
                break;
            }
            diff    = s->listA[s->i] - s->listB[s->j];
            best    = s->best;
            s->diff = diff;
            diff    = ABS(diff);
            if (diff < best) {
                s->best        = diff;
                work->cursor   = s->listB[s->j];
                work->field_75 = s->listA[s->i];
                if (s->diff < 0) {
                    work->field_73 = -1;
                } else {
                    work->field_73 = 1;
                }
            }
        }
    }

    if (s->best == 0xFF) {
        printf(D_acropolis_bridge_8017D6CC);
    }
    work->cursor += (u8)work->field_73;
    SCRATCH_POP_BYTES(0x1C);
}

/// Steps the walker toward its current patrol node. `func_800E0C10` produces
/// the 16.16 delta; the high half of each component becomes the whole-unit
/// step, rounded away from zero whenever a fraction is left over. While
/// `field_6B` is set the walker is pinned vertically, otherwise Y also carries
/// a constant 0x10 fall. Y is applied in three bands: a +8 hop above 0x20, a
/// -0x20 drop below -0x20, and the plain step in between. `moving` records
/// whether the frame produced any XZ motion at all.
void func_acropolis_bridge_80184908(OverlayWalker* work)
{
    u8*                       head;
    OverlayWalkerMoveScratch* s;
    s32                       valx;
    s32                       valy;
    s32                       valz;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       y;
    s32                       mag;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x18;
    s                = (OverlayWalkerMoveScratch*)(head - 0x18);
    if (func_800E0C10(work->recs, &s->delta, work->field_56, NULL) != 0) {
        dx         = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.h.hi;
        dz         = s->delta.vz.h.hi;
        s->move.vx = dx;
        s->move.vz = dz;
        valx       = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.w;
        if ((valx & 0xFFFF) != 0) {
            if (valx > 0) {
                s->move.vx++;
            } else {
                s->move.vx--;
            }
        }
        valz = s->delta.vz.w;
        if ((valz & 0xFFFF) != 0) {
            if (valz > 0) {
                s->move.vz++;
            } else {
                s->move.vz--;
            }
        }
        if (work->field_6B == 0) {
            dy         = s->delta.vy.h.hi;
            valy       = s->delta.vy.w;
            s->move.vy = s->move.vy + dy;
            if ((valy & 0xFFFF) != 0) {
                if (valy > 0) {
                    s->move.vy++;
                } else {
                    s->move.vy--;
                }
            }
        } else {
            s->move.vy = 0;
        }
    } else {
        s->move.vx = 0;
        s->move.vy = 0;
        s->move.vz = 0;
    }
    if (work->field_6B == 0) {
        s->move.vy += 0x10;
    }
    work->moveDelta          = s->move;
    work->coord->coord.t[0] += s->move.vx;
    if (s->move.vy >= 0x21) {
        work->coord->coord.t[1] += 8;
    }
    if (s->move.vy < -0x20) {
        work->coord->coord.t[1] -= 0x20;
    }
    y   = s->move.vy;
    mag = y;
    if (y < 0) {
        SOFT_TOUCH_REG(mag);
        mag = -mag;
    }
    if (mag < 0x20) {
        work->coord->coord.t[1] += y;
    }
    work->coord->coord.t[2] += s->move.vz;
    if (work->coord->coord.t[0] != 0 || work->coord->coord.t[2] != 0) {
        work->moving = 1;
    } else {
        work->moving = 0;
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Pushes the walker away from the obstacles in its collision record table.
/// Every occupied record is turned into a bearing relative to the direction
/// the walker faces; records whose `field_4` kind is neither 0x10000 (which
/// also raises `blocked`) nor 0x30000 only count while their low halfword is
/// clear, and at most eight are collected. Any two bearings closer together
/// than 0x401 cancel each other, since the walker is then wedged between them
/// and has nowhere to go. Each surviving bearing becomes a unit vector 10
/// units long (`GPF` by -10 of the normalised matrix column), which is added
/// to both `push` and the walker's own translation.
void func_acropolis_bridge_80184B94(OverlayWalker* work)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (Mc_SaveData.field_5C1 == 1) {
        return;
    }

    work->blocked = 0;
    work->push.vz = 0;
    work->push.vy = 0;
    work->push.vx = 0;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = SCRATCH_HEAD(OverlayAvoidScratch);

    Gfx_MatrixCol1(&work->coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-work->coord->workm.m[2][0], work->coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-work->coord->workm.m[0][2], work->coord->workm.m[1][2]);
    }

    s->eye.vx = *(u16*)&work->coord->workm.t[0];
    s->eye.vy = *(u16*)&work->coord->workm.t[1];
    s->eye.vz = *(u16*)&work->coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < work->avoidCount; s->i++) {
        if (work->avoidRecs[s->i].key == 0) {
            break;
        }
        s->kind = work->avoidRecs[s->i].key & 0xFFFF0000;
        if (s->kind != 0x10000) {
            if (s->kind != 0x30000 && (u16)work->avoidRecs[s->i].key != 0) {
                continue;
            }
        } else {
            work->blocked = 1;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] =
                overlayBearingXZ((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] =
                overlayBearingXY((SVECTOR3*)&work->avoidRecs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            work->push.vx           += s->dir.vx;
            work->push.vz           += s->dir.vz;
            work->coord->coord.t[0] += s->dir.vx;
            work->coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP(OverlayAvoidScratch);
}

/// Turns the walker toward `pos` by at most `field_5A` angle units per frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes an absolute yaw applied to the model's saved scale matrix.
void func_acropolis_bridge_80185104(OverlayWalker* work, SVECTOR3* pos)
{
    OverlayWalkerTurnScratch* s;
    GsCOORDINATE2*            coord;
    u8*                       head;
    s16                       diff, t;
    s32                       angle;
    u16                       frames;

    if (D_80072728 == 1)
        return;
    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x1C;
    s                = (OverlayWalkerTurnScratch*)(head - 0x1C);
    coord            = work->coord;
    diff             = overlayCoordBearingXZ(pos, coord) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    t                = diff;
    if (diff < 0) {
    wrapUp:
        if (t < -0x800) {
            t += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (t > 0x800) {
            t -= 0x1000;
            goto wrapDown;
        }
    }
    angle    = t;
    s->angle = angle;
    if (angle != 0)
        work->field_62++;
    else
        work->field_62 = 0;
    // Preserve the original's discarded counter read and reload of the
    // cleared turn adjustment; the read occupies v1 while v0 stays free.
    frames = (u16)work->field_62;
    CLOBBER_REG(v0);
    SOFT_USE_REG(frames);
    work->field_64 = 0;
    SOFT_COMPILER_BARRIER();
    if ((u16)work->field_5A + (u16)work->field_64 < s->angle)
        s->angle = (u16)work->field_5A + (u16)work->field_64;
    if (s->angle < -((u16)work->field_5A + (u16)work->field_64))
        s->angle = -((u16)work->field_5A + (u16)work->field_64);
    if ((u16)work->field_5A == 0)
        s->angle = 0;
    s->angle += ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
    __builtin_memcpy(work->coord->coord.m, work->scaleMtx.m, sizeof(work->scaleMtx.m));
    Gfx_RotMatrixY(&work->coord->coord, s->angle, 0);
    SCRATCH_POP_BYTES(0x1C);
}

/// Runs the walker's per-frame step inside the 0x28-byte scratch frame
/// `func_acropolis_bridge_8018532C` opened for it. `head` is the scratch head
/// as it was before the frame was carved off, so the `SVECTOR3` the states
/// steer towards is `head - 0x24` == `&block->pos`.
///
/// State 1 heads straight for the actor selected by the walker's spawn
/// variant -- the low halfword of each translation component of that actor's
/// coordinate matrix -- state 2 re-runs the patrol steering and re-reads the
/// route's byte table at `cursor` whenever the step or the state changed, and
/// state 3 follows the patrol route proper. The scalar at `field_5E` then
/// ramps towards `field_5C` by `field_60` a frame; while it is non-zero it
/// scales (`GPF`) the normalised facing column of the model matrix into the
/// per-frame world step, which is added to the coordinate's translation and
/// kept in `moveStep`. `Mc_SaveData.field_5C1` (a global freeze flag) zeroes the step
/// instead.
static __inline__ void walkerStep(OverlayWalker* walker, u8* head,
                                  OverlayWalkerTickScratch* block)
{
    u8*            head2;
    SVECTOR3*      pos;
    PlayerStatus*  cfg;
    SVECTOR*       sv;
    SVECTOR*       gsv;
    SVECTOR*       step;
    GsCOORDINATE2* coord;
    s16            sdiff;
    s32            diff;
    s16            speed;
    s32            cur;
    s32            target;
    s32            result;

    switch (walker->state) {
        case 0:
            break;
        case 1:
            cfg                            = &D_80073B08[walker->field_6E];
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = *(u16*)&cfg->coordMtx->t[0];
            pos->vy                        = *(u16*)&cfg->coordMtx->t[1];
            pos->vz                        = *(u16*)&cfg->coordMtx->t[2];
            break;
        case 2:
            SCRATCH_PUSH_BYTES(4);
            walker->field_6F = func_acropolis_bridge_801843A0(walker, 1);
            walker->field_70 = func_acropolis_bridge_8018450C(walker);
            if (walker->field_69 != walker->state || walker->field_70 != walker->field_72 ||
                walker->field_6F != walker->field_71) {
                func_acropolis_bridge_80184638(walker, 1);
                walker->node = walker->nav->field_4[walker->cursor];
            }
            walker->field_69 = walker->state;
            walker->field_72 = walker->field_70;
            walker->field_71 = walker->field_6F;
            if (func_acropolis_bridge_80184024(walker) != 0) {
                walker->cursor += (u8)walker->field_73;
                walker->node    = walker->nav->field_4[walker->cursor];
                SCRATCH_POP_BYTES(4);
            }
            break;
        case 3:
            func_acropolis_bridge_80184208(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    func_acropolis_bridge_80185104(walker, &block->pos);

    cur    = walker->field_5C;
    target = walker->field_5E;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->field_60) {
            result = target + walker->field_60;
        } else if (sdiff < -walker->field_60) {
            result = target - walker->field_60;
        } else {
            result = target + diff;
        }
        walker->field_5E = result;
    }

    coord = walker->coord;
    speed = walker->field_5E;
    step  = &walker->moveStep;
    if (Mc_SaveData.field_5C1 == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2            = SCRATCH_HEAD(u8);
        sv               = (SVECTOR*)(head2 - 8);
        SCRATCH_HEAD(u8) = (u8*)sv;
        /* The ROM keeps a second copy of the block address for the GTE
           transfers; without it `sv` and the copy share one register. */
        gsv = sv;
        if (speed != 0) {
            Gfx_MatrixCol2(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12();
            gte_stsv(gsv);
            coord->coord.t[0] += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1] += sv->vy;
            coord->coord.t[2] += sv->vz;
            walker->moveStep   = *(SVECTOR*)(head2 - 8);
            coord->flg         = 0;
        }
        SCRATCH_POP_BYTES(8);
    }
    if (walker->field_6C == 0) {
        func_acropolis_bridge_80184908(walker);
    }
    if (walker->field_6D == 0) {
        func_acropolis_bridge_80184B94(walker);
    }
}

void func_acropolis_bridge_8018532C(OverlayWalker* walker)
{
    u8*                       head;
    OverlayWalkerTickScratch* block;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x28;
    block            = SCRATCH_HEAD(OverlayWalkerTickScratch);
    walkerStep(walker, head, block);
    walker->coord->flg = 0;
    SCRATCH_POP_BYTES(0x28);
}

/// Handles the room's 0x7DB broadcast for the bridge enemy. Message 0x0B01/1
/// (the bridge is being lowered) restores the model's default flag set while
/// the enemy is still in one of its first three spawn variants, and message
/// 0x0E01/2 (the bridge run has ended) decides whether the enemy is armed for
/// this variant: variant 0 needs `Gp_StateF0.field_6` to be set at all, variant 1 needs
/// it to be at least 2 and variant 2 at least 3. When it is, the enemy and the
/// work block are given the stat block's starting HP and the behaviour state
/// advances to 4; otherwise the state resets to 0 and the mesh is hidden behind
/// the default flag set. Always reports success.
s32 func_acropolis_bridge_801856E0(Task* task, s32 msgId, GpCmdArg* msg)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->work;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    TmdObject*                extra = (TmdObject*)task->extra;
    s32                       variant;
    u16                       sub;

    if (msg->from.key == 0xB01 && msg->command == 1) {
        variant = enemy->placeKey >> 12;
        switch (variant) {
            case 0:
            case 1:
            case 2:
                extra->flags = 0;
                break;
        }
    }
    if (msg->from.key == 0xE01) {
        sub = msg->command;
        if (sub == 2) {
            variant = enemy->placeKey >> 12;
            switch (variant) {
                case 0:
                    if (Gp_StateF0.field_6 != 0) {
                        break;
                    }
                    work->field_0 = 0;
                    goto hide;
                case 1:
                    if (Gp_StateF0.field_6 >= 2) {
                        break;
                    }
                    work->field_0 = 0;
                    goto hide;
                case 2:
                    if (Gp_StateF0.field_6 < 3) {
                        goto reset;
                    }
                    break;
                default:
                    work->field_0 = 0;
                    goto hide;
            }
            enemy->hp       = D_acropolis_bridge_80190C60;
            work->field_10C = D_acropolis_bridge_80190C60;
            work->field_0   = 4;
            goto done;
        reset:
            work->field_0 = 0;
        hide:
            ((TmdObject*)task->extra)->flags = 0x80;
        }
    }
done:
    return 1;
}

/// Drives the bridge enemy's three animation slots from the state word at
/// `field_100`. State 1 restarts every slot on animation `field_104` with the
/// blend value the room's `D_acropolis_bridge_801915E4` table holds for the
/// (previous, next) animation pair, state 2 resets them without a blend, and
/// both then latch `field_104` as the previous animation and hand over to
/// state 3, which just ticks the slots once per frame and counts frames in
/// `field_106`. Every path first copies `field_108` into each slot's
/// `field_9` playback-rate byte.
void func_acropolis_bridge_8018581C(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    AcropolisBridgeEnemyWork* start;
    AcropolisBridgeEnemyWork* reset;
    AcropolisBridgeEnemyWork* tick;
    s32                       i;
    s32                       j;
    s32                       k;

    work = (AcropolisBridgeEnemyWork*)task->work;
    if (work->field_100 == 1) {
        start = (AcropolisBridgeEnemyWork*)task->work;
        for (i = 1; i < 4; i++) {
            start->slots[i].rate = start->field_108;
            func_800B4114(&start->anim, i, start->field_104, 0,
                          D_acropolis_bridge_801915E4[start->field_102][start->field_104]);
        }
        start->field_102 = start->field_104;
        goto advance;
    }
    if (work->field_100 == 2) {
        reset = (AcropolisBridgeEnemyWork*)task->work;
        for (j = 1; j < 4; j++) {
            reset->slots[j].rate = reset->field_108;
            Gp_AnimResetSlot(&reset->anim, j, reset->field_104);
        }
        reset->field_102 = reset->field_104;
    advance:
        work->field_100 = 3;
        work->field_106 = 0;
        return;
    }
    if (work->field_100 == 3) {
        work->field_106++;
        tick = (AcropolisBridgeEnemyWork*)task->work;
        for (k = 1; k < 4; k++) {
            tick->slots[k].rate = tick->field_108;
            Gp_AnimTickIndex(&tick->anim, k);
        }
    }
}

extern u8                D_8007218A;
extern GpPairSrcE        D_acropolis_bridge_80190C5C;
extern s32               D_acropolis_bridge_801915C8;
extern OverlayWalkerNode D_acropolis_bridge_8019162C[];
extern u8                D_acropolis_bridge_801916CC[];
extern u8*               D_acropolis_bridge_80191720[];
extern s32               D_acropolis_bridge_80191744;

/// Copies a scratch `SVECTOR3` onto a `GpObj`'s three position halfwords.
static __inline__ void bridge_set_obj_pos(GpObj* obj, SVECTOR3* pos)
{
    obj->pos.vx = pos->vx;
    obj->pos.vy = pos->vy;
    obj->pos.vz = pos->vz;
}

/// One-time setup for the bridge enemy: allocates the 0x294-byte work block,
/// points the model's light and colour matrices at it, hangs the enemy off the
/// room's stat block, starts the animation context on three slots, and links
/// the model and hit-box `GpObj`s (kinds 2 and 3) onto the part coordinates at
/// `field_8[3]` and `field_8[1]`. The walker half is seeded next: its patrol
/// tables are the copies embedded in the walker itself, the route is the entry
/// `D_acropolis_bridge_80191720` holds for this spawn variant, and the scale
/// matrix is rebuilt from `scale` through a `VECTOR` borrowed from the scratch
/// arena -- the same block is then reused for the world position handed to
/// `func_800D7A9C` before it is released. `field_1F8` is the 0x5DC entry
/// offset the model root is raised by, remembered in `field_1FA`. In the
/// third visit (`gGameSession->at4.loc.room == 2`) the three known variants start
/// in state 8 at a fixed position instead of state 1.
void func_acropolis_bridge_80185988(GpEnemy* enemy, Task* task)
{
    TmdObject*                obj;
    TmdObject*                obj2;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            coord2;
    GsCOORDINATE2*            coord3;
    AcropolisBridgeEnemyWork* work;
    OverlayWalker*            walker;
    GpObj*                    link;
    GpObj*                    link2;
    register u8*              head;
    register u8*              head2;
    register u8*              head3;
    VECTOR*                   scale;
    VECTOR*                   vec;
    s32                       amount;
    s32                       variant;
    s32                       step;
    u16                       hp;
    s32                       axisY;
    SVECTOR3                  pos;

    obj        = (TmdObject*)task->extra;
    coord      = obj->coords;
    work       = (AcropolisBridgeEnemyWork*)memCalloc(sizeof(AcropolisBridgeEnemyWork), 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    obj2            = (TmdObject*)task->extra;
    obj2->lightMtx  = &work->lightMtx;
    obj2->colorMtx  = &work->colorMtx;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->param    = &D_acropolis_bridge_80190C5C;
    hp              = D_acropolis_bridge_80190C5C.hpMax;
    enemy->recs     = work->recs;
    enemy->hp       = hp;
    work->field_10C = 1;
    work->field_10E = 1;
    enemy->hpMax    = work->field_10C;
    enemy->hp       = enemy->hpMax;
    func_800B3F84(&work->anim, &D_acropolis_bridge_801915C8, obj, work->pad_C0,
                  work->slots);
    work->field_108 = 0x10;
    link            = &work->body;
    link->coord     = &((TmdObject*)task->extra)->coords[3];
    link->ctx.recs  = work->recs;
    link->pos.vx    = 0;
    link->pos.vy    = 0;
    link->pos.vz    = 0;
    link->key       = 0x30029;
    link->radius    = 0x100;
    link->flags     = 1;
    Gp_LinkObj(2, link);
    link->flags |= 0x8000;
    Gp_InitRec18Table(link->ctx.recs, 3, 0);
    pos.vx          = 0;
    pos.vy          = 0;
    pos.vz          = 0;
    link2           = &work->hit;
    link2->coord    = &((TmdObject*)task->extra)->coords[1];
    link2->ctx.recs = work->hitRecs;
    bridge_set_obj_pos(link2, &pos);
    link2->radius = 0x100;
    link2->flags  = 1;
    Gp_LinkObj(3, link2);
    Gp_InitRec18Table(link2->ctx.recs, 1, 0);
    work->hit.key   = Gp_PackObjPair(enemy, 0);
    coord->sub      = &gGfxViewCoord;
    work->yaw       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->field_100 = 2;
    work->field_104 = 2;
    func_acropolis_bridge_8018581C(task);
    enemy->coord      = &((TmdObject*)task->extra)->coords[3];
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    task->msgTable            = &D_acropolis_bridge_80191744;
    work->field_2             = -1;
    work->field_0             = 1;
    work->field_1F8           = 0x5DC;
    work->field_1FA           = coord->coord.t[1];
    coord->coord.t[1]        += work->field_1F8;

    work->walker.navData.nodes    = D_acropolis_bridge_8019162C;
    work->walker.navData.count    = 0xA;
    work->walker.navData.field_4  = D_acropolis_bridge_801916CC;
    work->walker.navData.field_9  = 0xA;
    work->walker.routeData.nodes  = D_acropolis_bridge_80191720[enemy->placeKey >> 12];
    work->walker.nav              = &work->walker.navData;
    work->walker.routeData.cursor = 0;
    work->walker.route            = &work->walker.routeData;
    coord2                        = ((TmdObject*)task->extra)->coords;
    work->walker.avoidCount       = 3;
    walker                        = &work->walker;
    work->walker.recs             = 0;
    work->walker.avoidRecs        = work->recs;
    work->walker.scale            = 0x1000;
    work->walker.field_56         = 0;
    work->walker.field_5A         = 0x30;
    work->walker.coord            = coord2;
    walker->field_5C              = 0x30;
    walker->field_5E              = 0;
    walker->field_60              = 1;
    step                          = 3;
    work->walker.state            = step;
    work->walker.field_6B         = 1;
    work->walker.field_6C         = 1;
    work->walker.field_6E         = D_8007218A;

    __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
    head                     = *(u8**)(head + 0x3FC);
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD(VECTOR)     = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord3                                 = ((TmdObject*)task->extra)->coords;
    __asm__("lui %0, 0x1F80" : "=r"(head2) : "r"(coord3));
    head2 = *(u8**)(head2 + 0x3FC);
    vec   = (VECTOR*)head2;
    Gp_UpdateCoord(coord3);
    vec->vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    vec->vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    vec->vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, vec, 0, 3);
    __asm__ volatile("lui %0, 0x1F80" : "=r"(head3));
    head3            = *(u8**)(head3 + 0x3FC);
    SCRATCH_HEAD(u8) = head3 + 0x10;
    axisY            = 1;
    if (Gp_StateF0.field_6 < 3) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    if (gGameSession->at4.loc.room == 2) {
        variant = enemy->placeKey >> 12;
        switch (variant) {
            case 0:
                work->field_0                                 = 8;
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0x22C4;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x3E8;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x640;
                break;
            case 1:
                work->field_0                                     = 8;
                ((TmdObject*)task->extra)->coords->coord.t[0]     = -0x270F;
                ((TmdObject*)task->extra)->coords->coord.t[axisY] = -0x3E8;
                ((TmdObject*)task->extra)->coords->coord.t[2]     = -0x7D0;
                break;
            case 2:
                work->field_0                                 = 8;
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0x2EE0;
                ((TmdObject*)task->extra)->coords->coord.t[1] = -0x3E8;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x5DC;
                break;
            default:
                work->field_0 = 0;
                break;
        }
    }
    task->state++;
}

/// Resets the walker's scale matrix to a uniform `walker->scale` scale, through
/// a `VECTOR` borrowed from the scratch arena and released again. Identity is
/// left in place at the two ends of the ramp (0 and full size), where scaling
/// would be a no-op anyway. `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` both inline it on their first frame, where
/// the scratch block is taken and released around the whole matrix reset, and
/// the shrink variant below once per frame of the shrink, where the diagonal is
/// written before the block is taken.
static __inline__ void bridge_reset_scale_mtx_entry(AcropolisBridgeEnemyWork* work)
{
    OverlayWalker* walker;
    u8*            head;
    VECTOR*        scale;
    s32            amount;

    head                     = SCRATCH_HEAD(u8);
    walker                   = &work->walker;
    amount                   = walker->scale;
    scale                    = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD(VECTOR)     = scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// The same matrix reset as `bridge_reset_scale_mtx_entry`, in the statement
/// order the shrink halves of `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` use (and the one `bridge_scale_up` uses for
/// the spawn ramp).
static __inline__ void bridge_reset_scale_mtx_shrink(AcropolisBridgeEnemyWork* work)
{
    OverlayWalker* walker;
    u8*            head;
    VECTOR*        scale;
    s32            amount;

    walker                   = &work->walker;
    head                     = SCRATCH_HEAD(u8);
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    SCRATCH_HEAD(VECTOR) = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Runs the bridge enemy's approach state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, swaps bit 15
/// between the two behaviour flag words, rebuilds the scale matrix and restarts
/// the animation slots on animation 1. Every frame after that it raises the
/// model root by 0x2D until the entry offset at `field_1F8` reaches 0x708, and
/// shrinks the walker by 0x33 a frame down to 0x801 -- rebuilding the scale
/// matrix as it goes -- tagging the enemy's link node on every frame it is
/// already that small, then ticks the walker and the animation slots. The
/// behaviour state becomes 2 once the player is at or above the bridge.
void func_acropolis_bridge_80185F28(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    OverlayWalker*            walker;
    OverlayWalker*            walker2;
    GpEnemy*                  enemy;
    PlayerStatus*             cfg;

    cfg   = &Player_Status;
    work  = (AcropolisBridgeEnemyWork*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->walker.state            = 3;
        work->walker.routeData.cursor = 0;
        work->hit.flags              &= 0x7FFF;
        work->body.flags             |= 0x8000;
        bridge_reset_scale_mtx_entry(work);
        work->walker.field_5A = 0x60;
        walker                = &work->walker;
        walker->field_5C      = 0x20;
        walker->field_5E      = 0;
        walker->field_60      = 1;
        work->field_100       = 2;
        work->field_104       = 1;
        work->field_108       = 0x10;
    }
    if (work->walker.routeData.arrived == 1) {
        walker2           = &work->walker;
        walker2->field_5C = 0x20;
        walker2->field_5E = 0x60;
        walker2->field_60 = 2;
    }
    if (work->field_1F8 < 0x708) {
        work->field_1F8 += 0x2D;
        ((TmdObject*)task->extra)->coords->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (work->walker.scale >= 0x801) {
        work->walker.scale -= 0x33;
        bridge_reset_scale_mtx_shrink(work);
    } else {
        enemy->node.state.b.flags = 1;
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (cfg->coordMtx->t[1] >= 0x2BD) {
        work->field_0 = 2;
    }
}

/// Rebuilds the bridge enemy's model matrix for the spawn scale-up. `scale`
/// ramps 0x88 per frame until it reaches 0x1000, and until then the matrix is
/// reset to identity and scaled uniformly by it through a `VECTOR` taken from
/// the scratch stack.
static __inline__ void bridge_scale_up(AcropolisBridgeEnemyWork* work)
{
    OverlayWalker* walker;
    u8*            head;
    VECTOR*        scale;
    s32            amount;

    work->walker.scale      += 0x88;
    walker                   = &work->walker;
    head                     = SCRATCH_HEAD(u8);
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    SCRATCH_HEAD(VECTOR) = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Runs the bridge enemy's spawn state. On the first frame (work block still
/// live) it tags the link node while `Gp_PackObjPair` rebuilds the enemy's
/// pair table, sets bit 15 of both behaviour flag words, seeds the walker's
/// first patrol step and starts the reset animation. Every frame after that it
/// counts the entry delay at `field_1F8` down 0x3C at a time -- dropping the
/// model root by it while it runs -- scales the model up until it reaches full
/// size, ticks the walker and the animation slots, and finally advances to
/// state 3 once the work block reports it is done, or resets to state 1 when
/// the player has dropped below the bridge.
void func_acropolis_bridge_801861A0(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    OverlayWalker*            walker;
    GpEnemy*                  enemy;
    PlayerStatus*             cfg;
    s32                       done;
    u16                       height;

    cfg  = &Player_Status;
    work = (AcropolisBridgeEnemyWork*)task->work;
    if (work->field_4 != 0) {
        enemy = (GpEnemy*)task->spawnArg2;
        Gp_ArmStateF0(1);
        enemy->node.state.b.flags = 1;
        height                    = work->walker.field_5E;
        walker                    = &work->walker;
        work->walker.field_5A     = 0x100;
        walker->field_5C          = 0xA0;
        walker->field_60          = 6;
        walker->field_5E          = height;
        work->walker.state        = 1;
        work->hit.flags          |= 0x8000;
        work->body.flags         |= 0x8000;
        work->hit.key             = Gp_PackObjPair(enemy, 0);
        enemy->node.state.b.flags = 0;
        work->field_100           = 2;
        work->field_104           = 2;
        work->field_108           = 0x50;
    }
    if (work->field_1F8 > 0) {
        work->field_1F8 -= 0x3C;
        ((TmdObject*)task->extra)->coords->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (work->walker.scale < 0x1000) {
        bridge_scale_up(work);
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (((AcropolisBridgeEnemyWork*)task->work)->hitRecs[0].key == 0) {
        done = 0;
        SOFT_BARRIER();
    } else {
        done = 1;
    }
    if (done != 0) {
        work->field_0 = 3;
    }
    if (cfg->coordMtx->t[1] < 0x321) {
        work->field_0 = 1;
    }
}

/// Runs the bridge enemy's retreat state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, drops bit 15
/// of the hit box's flags, rebuilds the scale matrix and restarts the animation slots
/// on animation 1. Every frame after that it raises the model root by 0x2D
/// until the entry offset at `field_1F8` reaches 0x708, shrinks the walker by
/// 0x46 a frame down to 0x500 -- rebuilding the scale matrix as it goes, and
/// once it is that small tagging the enemy's link node instead -- then ticks
/// the walker and the animation slots. When the hand-over flag is set the
/// behaviour state becomes 1 while the player is below the bridge and 2
/// otherwise.
void func_acropolis_bridge_801863A8(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    OverlayWalker*            walker;
    GpEnemy*                  enemy;
    PlayerStatus*             cfg;

    cfg   = &Player_Status;
    work  = (AcropolisBridgeEnemyWork*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->walker.state            = 3;
        work->walker.routeData.cursor = 0;
        work->hit.flags              &= 0x7FFF;
        bridge_reset_scale_mtx_entry(work);
        work->walker.field_5A = 0x200;
        walker                = &work->walker;
        walker->field_5C      = 0x20;
        walker->field_5E      = 0x80;
        walker->field_60      = 3;
        work->field_100       = 2;
        work->field_104       = 1;
        work->field_108       = 0x10;
    }
    if (work->field_1F8 < 0x708) {
        work->field_1F8 += 0x2D;
        ((TmdObject*)task->extra)->coords->coord.t[1] =
            work->field_1FA + work->field_1F8;
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (work->walker.scale >= 0x500) {
        work->walker.scale -= 0x46;
        bridge_reset_scale_mtx_shrink(work);
    } else if (enemy->node.state.b.flags == 0) {
        enemy->node.state.b.flags = 1;
    }
    func_acropolis_bridge_8018532C(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (work->walker.routeData.cursor != 0) {
        if (cfg->coordMtx->t[1] < 0x321) {
            work->field_0 = 1;
        } else {
            work->field_0 = 2;
        }
    }
}

/// Reports whether the bridge enemy is standing on a kind-1 surface: the first
/// three collision records are scanned in order and the scan stops at the first
/// empty one, so an occupied record whose `key` high halfword is 1 has to
/// come before any gap in the table.
static __inline__ s32 bridge_rec_kind1(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (recs[i].key == 0) {
            return 0;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Plays one of the bridge enemy's positional sounds. The spawn variant in the
/// enemy's `field_8` high nibble picks the bank, so the event id is that nibble
/// shifted into byte 1 of `base`, and the pan and depth come from the model's
/// root coordinate.
static __inline__ void bridge_play_snd(Task* task, GpEnemy* enemy, s32 base)
{
    s32 snd;
    s32 pan;

    snd = ((enemy->placeKey >> 12) << 8) | base;
    pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(snd, pan,
                        (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
}

/// Runs the bridge enemy's plunge into the gorge. On the first frame (work
/// block still live) it disables the hit box, clears the enemy's link node tag,
/// seeds the three behaviour parameters and moves the model root out over the
/// gorge -- the X and Z it drops to depend on which of the three spawn variants
/// this is -- then loads the light-blend colour matrix and gives the root a
/// random yaw. Every frame after that the fall height comes from
/// `func_acropolis_bridge_8017E024`, and the model is spun on its own yaw and
/// scaled down once it is past 0x1F4, four units of scale per unit of depth.
/// Three chances to restart the scream animation are rolled on the way down --
/// on crossing 0x1F4, then one in sixteen frames while the animation has run
/// long enough, then one in thirty-two frames below 0x320 with the second
/// animation slot finished -- and the yaw is re-rolled while animation 4 is in
/// its fifth playback step. Landing on a kind-1 surface plays the impact sound
/// and hands over to state 7.
void func_acropolis_bridge_80186618(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    AcropolisBridgeEnemyWork* anim;
    GpEnemy*                  enemy;
    VECTOR                    scale;
    s32                       amount;
    s32                       height;

    work  = (AcropolisBridgeEnemyWork*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->hit.flags          &= 0x7FFF;
        enemy->node.state.b.flags = 0;
        work->field_108           = 0x20;
        work->field_100           = 2;
        work->field_104           = 1;
        switch (enemy->placeKey >> 12) {
            case 0:
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0x22C4;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x640;
                break;
            case 1:
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0x270F;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x7D0;
                break;
            case 2:
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0x2EE0;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x5DC;
                break;
        }
        work->colorMtx.t[1]    = 0x80;
        work->colorMtx.t[0]    = 0x80;
        work->colorMtx.t[2]    = 0x5A0;
        work->colorMtx.m[2][1] = 0xC0;
        work->colorMtx.m[2][0] = 0xC0;
        work->colorMtx.m[2][2] = 0x5A0;
        work->colorMtx.m[1][1] = 0xC0;
        work->colorMtx.m[1][0] = 0xC0;
        work->colorMtx.m[1][2] = 0x5A0;
        work->colorMtx.m[0][1] = 0xC0;
        work->colorMtx.m[0][0] = 0xC0;
        work->colorMtx.m[0][2] = 0x5A0;
        Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
        work->yaw              = (u32)Gp_LcgState >> 16;
    }
    ((TmdObject*)task->extra)->coords->coord.t[1] =
        func_acropolis_bridge_8017E024() - 0xC8;
    ((TmdObject*)task->extra)->coords->flg = 0;
    height                                 = ((TmdObject*)task->extra)->coords->coord.t[1];
    if (height < 0x1F4) {
        amount   = 0x1000;
        scale.vx = scale.vy = scale.vz = amount;
    } else {
        amount   = 0x1000;
        height  -= 0x1F4;
        height  *= 4;
        amount  -= height;
        scale.vx = scale.vy = scale.vz = amount;
    }
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, work->yaw, 1);
    ScaleMatrix(&((TmdObject*)task->extra)->coords->coord, &scale);
    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0x1F4 &&
        work->field_104 != 4) {
        work->field_100 = 2;
        work->field_104 = 4;
        bridge_play_snd(task, enemy, 0x40290003);
    }
    if (work->field_104 == 4) {
        if (((TmdObject*)task->extra)->coords->coord.t[1] >= -0x3DD &&
            (s32)((enemy->placeKey >> 12) + 8) < work->field_106) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                work->field_108 = ((enemy->placeKey >> 12) * 2) + 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0x320) {
        anim = (AcropolisBridgeEnemyWork*)task->work;
        if (anim->slots[1].curRec == anim->slots[1].nextRec) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0x1F) == 0) {
                work->field_108 = 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (*(s32*)&work->field_104 == 0x50004) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->yaw   = (u32)Gp_LcgState >> 16;
    }
    if (bridge_rec_kind1(work->recs) != 0) {
        if (work->field_10C > 0) {
            bridge_play_snd(task, enemy, 0x40290002);
        }
        work->field_0 = 7;
    }
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's fall at its current position. Seeds the hit-box,
/// animation and colour state on entry, then scales and spins the model by
/// height, rolls chances to restart its scream and enters state 7 on landing.
void func_acropolis_bridge_80186BBC(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    AcropolisBridgeEnemyWork* anim;
    GpEnemy*                  enemy;
    VECTOR                    scale;
    s32                       amount;
    s32                       height;

    work  = (AcropolisBridgeEnemyWork*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->hit.flags          &= 0x7FFF;
        enemy->node.state.b.flags = 0;
        work->field_108           = 0x20;
        work->field_100           = 2;
        work->field_104           = 1;
        work->colorMtx.t[1]       = 0x80;
        work->colorMtx.t[0]       = 0x80;
        work->colorMtx.t[2]       = 0x5A0;
        work->colorMtx.m[2][1]    = 0xC0;
        work->colorMtx.m[2][0]    = 0xC0;
        work->colorMtx.m[2][2]    = 0x5A0;
        work->colorMtx.m[1][1]    = 0xC0;
        work->colorMtx.m[1][0]    = 0xC0;
        work->colorMtx.m[1][2]    = 0x5A0;
        work->colorMtx.m[0][1]    = 0xC0;
        work->colorMtx.m[0][0]    = 0xC0;
        work->colorMtx.m[0][2]    = 0x5A0;
        Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
        work->yaw                 = (u32)Gp_LcgState >> 16;
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
    height                                 = ((TmdObject*)task->extra)->coords->coord.t[1];
    if (height < 0x1F4) {
        amount   = 0x1000;
        scale.vx = scale.vy = scale.vz = amount;
    } else {
        amount   = 0x1000;
        height  -= 0x1F4;
        height  *= 4;
        amount  -= height;
        scale.vx = scale.vy = scale.vz = amount;
    }
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, work->yaw, 1);
    ScaleMatrix(&((TmdObject*)task->extra)->coords->coord, &scale);
    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0x1F4 &&
        work->field_104 != 4) {
        work->field_100 = 2;
        work->field_104 = 4;
        bridge_play_snd(task, enemy, 0x40290003);
    }
    if (work->field_104 == 4) {
        if (((TmdObject*)task->extra)->coords->coord.t[1] >= -0x3DD &&
            (s32)((enemy->placeKey >> 12) + 8) < work->field_106) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                work->field_108 = ((enemy->placeKey >> 12) * 2) + 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (((TmdObject*)task->extra)->coords->coord.t[1] < 0x320) {
        anim = (AcropolisBridgeEnemyWork*)task->work;
        if (anim->slots[1].curRec == anim->slots[1].nextRec) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 0x1F) == 0) {
                work->field_108 = 0x10;
                work->field_100 = 2;
                work->field_104 = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (*(s32*)&work->field_104 == 0x50004) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->yaw   = (u32)Gp_LcgState >> 16;
    }
    if (bridge_rec_kind1(work->recs) != 0) {
        if (work->field_10C > 0) {
            bridge_play_snd(task, enemy, 0x40290002);
        }
        work->field_0 = 7;
    }
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's fall. On the first frame (work block still live) it
/// clears bit 15 of the hit box's flags and sets it in the model's, tags the link node,
/// seeds the three behaviour parameters and gives the model root coordinate a
/// random yaw from the shared LCG. Every frame after that it eases the entry
/// offset at `field_1F8` back to zero three units at a time, sets the model
/// root height from it and adds an `rsin` bob driven by the frame counter. Once
/// the enemy is resting on a kind-1 surface it also drifts the model root
/// towards the camera position by a sixteenth of the normalized direction,
/// yawing the root by 0x10 first.
void func_acropolis_bridge_80187078(Task* task)
{
    AcropolisBridgeEnemyWork* work;
    GpEnemy*                  enemy;
    GsCOORDINATE2*            coord;
    SVECTOR                   dir;
    SVECTOR*                  d;

    work  = (AcropolisBridgeEnemyWork*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work->field_4 != 0) {
        work->hit.flags          &= 0x7FFF;
        work->body.flags         |= 0x8000;
        enemy->node.state.b.flags = 1;
        work->field_100           = 1;
        work->field_104           = 3;
        work->field_108           = 0x10;
        Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, (u32)Gp_LcgState >> 16, 1);
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
    if (work->field_1F8 > 0) {
        work->field_1F8 -= 3;
    }
    ((TmdObject*)task->extra)->coords->coord.t[1] = work->field_1FA + work->field_1F8;
    ((TmdObject*)task->extra)->coords->coord.t[1] +=
        rsin((D_80070F70 << 5) + ((TmdObject*)task->extra)->coords->coord.t[0]) >> 6;
    if (bridge_rec_kind1(work->recs) != 0) {
        coord  = ((TmdObject*)task->extra)->coords;
        dir.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
        d      = &dir;
        d->vy  = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        d->vz  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, 0x10, 0);
        VectorNormalSS(d, d);
        gte_lddp(-0x10);
        gte_ldsv(d);
        gte_gpf12();
        gte_stsv(d);
        ((TmdObject*)task->extra)->coords->coord.t[0] += dir.vx;
        ((TmdObject*)task->extra)->coords->coord.t[2] += dir.vz;
    }
    ((TmdObject*)task->extra)->coords->flg = 0;
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's collapse sequence. On the first frame (work block
/// still live) it allocates the model's aux buffers, clears bit 15 of both
/// behaviour flag words, tags the link node, seeds the three behaviour
/// parameters, gives the model root coordinate a random yaw from the shared
/// LCG, drops the enemy's actor slots, arms the pending `Gp_StateF0` request
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 10, 22, 28
/// and 34, steps the light mode and model flags through the fade-out.
void func_acropolis_bridge_80187310(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->work;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       step;

    if (work->field_4 != 0) {
        Tmd_AllocBuffers((TmdObject*)task->extra);
        work->hit.flags          &= 0x7FFF;
        work->body.flags         &= 0x7FFF;
        enemy->node.state.b.flags = 1;
        work->field_100           = 1;
        work->field_104           = 5;
        work->field_108           = 0x10;
        Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, (u32)Gp_LcgState >> 16, 1);
        ((TmdObject*)task->extra)->coords->flg = 0;
        Gp_ClearNodeSlots(&enemy->node);
        if (Gp_StateF0.field_0 == 0 && Gp_StateF0.field_6 != 0) {
            Gp_ArmStateF0(1);
        }
        work->field_290 = 0;
    }
    if (work->field_290 < 0x65) {
        work->field_290++;
        func_acropolis_bridge_8018581C(task);
        step = work->field_290;
        switch (step) {
            case 10:
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, &((TmdObject*)task->extra)->coords[2], 1, NULL);
                break;
            case 28:
                ((TmdObject*)task->extra)->flags = 2;
                break;
            case 22:
                Gp_SetLightMode(enemy, 2);
                break;
            case 34:
                ((TmdObject*)task->extra)->flags = 0x80;
                break;
        }
    }
}

/// Runs the bridge enemy's death sequence. On the first frame (work block still
/// live) it clears bit 15 of both behaviour flag words, tags the link node and
/// drops its actor slots, arms the pending `Gp_StateF0` request, credits the
/// kill if the enemy still had HP, spawns the death effect on the model's
/// second part coordinate, switches the model to light mode 1, shakes the pad
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 2, 30 and
/// 44, steps the model flags / light mode through the fade-out.
void func_acropolis_bridge_801874DC(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->work;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       step;

    if (work->field_4 != 0) {
        work->hit.flags          &= 0x7FFF;
        work->body.flags         &= 0x7FFF;
        enemy->node.state.b.flags = 1;
        Gp_ClearNodeSlots(&enemy->node);
        if (Gp_StateF0.field_0 == 0 && Gp_StateF0.field_6 != 0) {
            Gp_ArmStateF0(1);
        }
        if (enemy->hp > 0) {
            Gp_ReleaseStateF0Add(task, 0x29);
        }
        work->field_1F0.coord      = &((TmdObject*)task->extra)->coords[1];
        work->field_1F0.spawnArgLo = 0xA0;
        work->field_1F0.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &((TmdObject*)task->extra)->coords[1], NULL,
                      &work->field_1F0);
        Gp_SetLightMode(enemy, 1);
        Gp_SpawnEff(0x600A5, &((TmdObject*)task->extra)->coords[1], 1, NULL);
        ((TmdObject*)task->extra)->flags = 2;
        work->field_290                  = 0;
        Gp_SpawnPadLerp(3, 0xFF, 8);
    }
    if (work->field_290 < 0x65) {
        work->field_290++;
        func_acropolis_bridge_8018581C(task);
        step = work->field_290;
        switch (step) {
            case 2:
                ((TmdObject*)task->extra)->flags = step;
                break;
            case 30:
                Gp_SetLightMode(enemy, 2);
                break;
            case 44:
                ((TmdObject*)task->extra)->flags = 0x80;
                break;
        }
    }
}

/// Applies one hit to the bridge enemy. While the work block still has HP it
/// rolls the damage for the incoming attack id, spawns the hit effect on the
/// model's second part coordinate, quadruples the damage on a critical roll,
/// credits it to the kill tally and the link node, and subtracts it from both
/// the work block's and the enemy's HP; the pending `Gp_StateF0` request is
/// armed once the HP runs out. When there is no HP left to take (before or
/// after the hit) it steps the behaviour state instead: 5 and 6 are already
/// reaction states and stay put, 4 and 8 advance to 6, everything else resets
/// to 5.
void func_acropolis_bridge_801876A8(Task* task, u32 attackId)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->work;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    s32                       damage;
    s16                       state;

    if (work->field_10C > 0) {
        damage                     = Gp_ComputeDamage(attackId, 0, 0, 0x1000);
        work->field_1F0.coord      = &((TmdObject*)task->extra)->coords[1];
        work->field_1F0.spawnArgLo = 0x80;
        work->field_1F0.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(attackId) & 0xFFFF, &((TmdObject*)task->extra)->coords[1],
                      NULL, &work->field_1F0);
        if (Gp_RollEnemyChance(enemy, attackId, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[1], 0, NULL);
        }
        func_800E2C78(enemy, attackId, damage, 0);
        enemy->hp -= damage;
        func_800DA6E8(&enemy->node, damage, 0);
        work->field_10C -= damage;
        enemy->hp        = work->field_10C;
        if (work->field_10C > 0) {
            return;
        }
        if (Gp_StateF0.field_6 != 0) {
            Gp_ReleaseStateF0Add(task, 0x29);
        }
        if (work->field_10C > 0) {
            return;
        }
    }
    state = work->field_0;
    if (state < 7) {
        if (state >= 5) {
            return;
        }
        if (state != 4) {
            work->field_0 = 5;
            return;
        }
        work->field_0 = 6;
    } else {
        if (state != 8) {
            work->field_0 = 5;
            return;
        }
        work->field_0 = 6;
    }
}

/// Ticks the bridge enemy once per frame. It refreshes the model's root
/// coordinate and relights it, then branches on the global pause mode
/// `Gp_StateF0.field_4`: mode 1 only releases the collision records, mode 2 also hides
/// the mesh, and mode 0 keeps the model's visibility in step with the camera
/// -- re-allocating or releasing the TMD's aux buffers when the view changes,
/// and remembering the view it last synced to in `field_292`. Outside the
/// death and cleanup states it then borrows a 0xC-byte scratch block, scans
/// the body's three collision records for a hit (high halfword 0x2), applies
/// it through `func_acropolis_bridge_801876A8`, raises `field_4` on the frame
/// the behaviour state changes, runs the state's handler from
/// `D_acropolis_bridge_8019175C`, clears both record tables and -- while no
/// `Gp_StateF0` request is pending -- resets any state other than 5 or 6 back
/// to 0.
void func_acropolis_bridge_80187850(GpEnemy* enemy, Task* task)
{
    AcropolisBridgeEnemyWork*  work;
    AcropolisBridgeEnemyWork*  cur;
    AcropolisBridgeHitScratch* block;
    TmdObject*                 extra;
    GpRec18*                   recs;
    VECTOR                     pos;
    s32                        mode;
    s32                        view;
    s32                        hit;
    u16                        state;
    s16                        i;

    work                                   = (AcropolisBridgeEnemyWork*)task->work;
    ((TmdObject*)task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    pos.vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    mode = Gp_StateF0.field_4;
    if (mode == 1) {
        goto paused;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto running;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto hidden;
    }
    goto body;

running:
    state = (u16)work->field_0;
    if ((u32)(state - 6) >= 2U) {
        if (state != 0) {
            view = Gp_GetViewIndex() & 0xFF;
            switch (view) {
                case 8:
                    if ((s32)work->field_292 == view) {
                        goto drop;
                    }
                    ((TmdObject*)task->extra)->flags = 0x80;
                    goto resync;
                case 22:
                    if (work->field_0 == 4) {
                        goto draw;
                    }
                drop:
                    ((TmdObject*)task->extra)->flags |= 4;
                    extra                             = (TmdObject*)task->extra;
                    if (extra->buffer != NULL) {
                        Tmd_FreeBuffers(extra);
                    }
                    goto resync;
                default:
                    Tmd_AllocBuffers((TmdObject*)task->extra);
                draw:
                    ((TmdObject*)task->extra)->flags = 0;
                    break;
            }
        resync:
            work->field_292 = Gp_GetViewIndex() & 0xFF;
        }
    }
    goto body;

paused:
    state = (u16)work->field_0;
    if ((u32)(state - 6) >= 2U && state != 0) {
        Gp_ClearRec18Occupied(&work->recs[0]);
        Gp_ClearRec18Occupied(&work->hitRecs[0]);
    }
    return;

hidden:
    ((TmdObject*)task->extra)->flags = 0x80;
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->hitRecs[0]);
    return;

body:
    SCRATCH_PUSH_BYTES(0xC);
    block = SCRATCH_HEAD(AcropolisBridgeHitScratch);
    recs  = work->recs;
    i     = 0;
    do {
        if (recs[i].key == 0) {
            goto missed;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            block->x = recs[i].point.vx;
            block->y = recs[i].point.vy;
            block->z = recs[i].point.vz;
            hit      = recs[i].key;
            goto hitTaken;
        }
        i++;
    } while (i < 3);
missed:
    hit = 0;
hitTaken:
    block->hit = hit;
    if (hit != 0) {
        func_acropolis_bridge_801876A8(task, hit);
    }

    cur = (AcropolisBridgeEnemyWork*)task->work;
    if (cur->field_2 != cur->field_0) {
        cur->field_4 = 1;
    } else {
        cur->field_4 = 0;
    }
    cur->field_2 = cur->field_0;
    D_acropolis_bridge_8019175C[work->field_0](task);
    Gp_ClearRec18Occupied(&work->recs[0]);
    Gp_ClearRec18Occupied(&work->hitRecs[0]);
    if (Gp_StateF0.field_6 == 0) {
        if ((u32)((u16)work->field_0 - 5) >= 2U) {
            work->field_0 = 0;
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Applies a visibility request to the bridge task's model flags: no request
/// restores the default flag set, bit 0 hides the mesh outright and bit 1 adds
/// the "skip drawing" bit to whatever flags are already set. Always reports
/// success.
s32 func_acropolis_bridge_80187BD0(Task* task, s32 arg1, s32 flags)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    if (flags == 0) {
        extra->flags = 0x80;
    } else if (flags & 1) {
        extra->flags = 0;
    } else if (flags & 2) {
        extra->flags |= 4;
    }
    return 1;
}

/// Relights the bridge enemy's model. Borrows a `VECTOR` from the scratchpad
/// arena, optionally refreshes the TMD's root coordinate first (`arg1 == 1`),
/// then feeds that part's world translation to `func_800D7A9C` so the object's
/// colour matrix is rebuilt for its current position, and releases the scratch.
void func_acropolis_bridge_80187C10(Task* task, s16 arg1)
{
    void**  scratch;
    u8*     head;
    VECTOR* pos;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    pos      = (VECTOR*)(head - 0x10);
    *scratch = pos;
    if (arg1 == 1) {
        ((TmdObject*)task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)task->extra)->coords);
    }
    ((VECTOR*)(head - 0x10))->vx = ((TmdObject*)task->extra)->coords->workm.t[0];
    pos->vy                      = ((TmdObject*)task->extra)->coords->workm.t[1];
    pos->vz                      = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, pos, 0, 3);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Shuts the bridge enemy's animation down. While the work block is still live
/// it hides the mesh behind the default flag set, tags the enemy's link node
/// and clears bit 15 of both behaviour flag words; once the work block has been
/// cleared it instead adds the "skip drawing" bit and releases the TMD's aux
/// buffers.
void func_acropolis_bridge_80187D04(Task* task)
{
    AcropolisBridgeEnemyWork* work  = (AcropolisBridgeEnemyWork*)task->work;
    TmdObject*                extra = (TmdObject*)task->extra;

    if (work->field_4 != 0) {
        GpEnemy* enemy = (GpEnemy*)task->spawnArg2;

        extra->flags              = 0x80;
        enemy->node.state.b.flags = 1;
        work->body.flags         &= 0x7FFF;
        work->hit.flags          &= 0x7FFF;
        return;
    }
    extra->flags |= 4;
    if (extra->buffer != NULL) {
        Tmd_FreeBuffers(extra);
    }
}

/// Runs the bridge enemy's current state handler - setup, per-frame tick or
/// teardown - copying the table onto the stack before the call.
void func_acropolis_bridge_80187D80(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D6E8;
    sp.funcs[task->state](task->spawnArg2, task);
}
