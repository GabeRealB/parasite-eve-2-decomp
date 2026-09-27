#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/loadui.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "main/wipsys.h"

void        Gp_FinishLoadWait(Task* task);
static u16  func_800AA120(void);
static void func_800AA548(s32 arg0);
static void func_800AD024(void);
static void func_800AD620(Task* task);
static void func_800AD65C(Task* task);
void        D_8017DA78(s32 arg0, s32 arg1);
void        D_8017EF60(s32 arg0, s32 arg1);
void        func_80724748(GpAreaKey* arg0);
void        func_80724E2C(void);

extern TaskDesc             D_80183824[];
static const TaskFuncTable3 Gp_SessionStates;
static const TaskFuncTable8 Gp_LoadStateFns;
static const TaskFuncTable3 Gp_RoomObjStates;
extern s16                  D_80114CD0;
extern u16                  Gp_DirFlags;
extern u16                  D_80114CD4;
extern u16                  Gp_DirPhase;
extern u8                   Gp_DirByte;
extern u8                   Gp_DirNibble;
extern u8                   Gp_DirAlt;
extern u8                   Gp_DirAltNibble;
extern u8                   D_80114CDC;
extern u8                   D_80114CDD;
extern u8                   D_80114CDE;
extern s32                  D_80114CF0;
extern s16                  D_80114CF4;
extern u16                  Gp_DirFadeLevel;
extern u8                   D_80114CF8;
extern s16                  D_80114D08;

static void Gp_ApplyNewGameAreaFlags(void);
static void Gp_ApplyNpcRoomSnd(void);
static void Gp_InitStageVisit(GpAreaKey* arg0);
static void Gp_LinkSprtCmd(GpSprtElem* arg0, GpSprtCmd* arg1);
static void Gp_LoadWaitCdBusy(Task* task);
static void Gp_LoadWaitDone(Task* task);
static void Gp_LoadWaitIdle(Task* task);
static void Gp_MarkAreaVisited(GpAreaKey* arg0);
static s32  Gp_PickCompanion(void);
static void Gp_SetupCompanionActor(GpActorArg* arg0, u16* arg1);

static const TaskFuncTable6 Gp_LoadWaitFns = { {
    Gp_ViewBeginLoad,
    Gp_EnqueueViewCd,
    Gp_ViewLoadImage,
    Gp_LoadWaitCdBusy,
    Gp_LoadWaitIdle,
    Gp_LoadWaitDone,
} };

static const GpTbl5 Gp_ConfigCdTable = { { 4, 3, 2, 5, 6 } };

/// Maps `Player_Status.weapon` / `field_22` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `field_21` is 0 or the mapped byte is 0.
static void Gp_EnqueueWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u16 item;
    s32 val;
    s32 attach;
    s32 temp;
    s32 flag;

    item = Player_Status.weapon;
    if (item == 0) {
        return;
    }

    param1[0] = 0;
    switch (item) {
        case 0xB:
            param1[0] = 1;
            if (Player_Status.weaponSlotItem == 0xB) {
                param1[0] = 2;
            }
            if (Player_Status.weaponSlotItem == 0xC) {
                param1[0] = 3;
            }
            break;
        case 0xC:
            param1[0] = 4;
            if (Player_Status.weaponSlotItem == 0xB) {
                param1[0] = 5;
            }
            if (Player_Status.weaponSlotItem == 0xC) {
                param1[0] = 6;
            }
            break;
        case 0xD:
            param1[0] = 7;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 8;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 9;
            }
            break;
        case 0xE:
            param1[0] = 0xA;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 0xB;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 0xC;
            }
            break;
        case 0xF:
            param1[0] = 0xD;
            val       = Player_Status.weaponSlotItem;
            if (val == 0xE) {
                param1[0] = val;
            }
            if (val == 0xF) {
                param1[0] = val;
            }
            break;
        case 0x17:
            param1[0] = 0x13;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 0x14;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 0x15;
            }
            break;
        case 0x1B: {
            register McItemSlot* slot asm("a0");

            param1[0] = 0x10;
            slot      = Gp_GetItemSlot(item + 0x7F);
            TOUCH_REG(slot);
            attach = slot->attachId;
            if (attach != 0 && attach != 0xFF) {
                temp = attach;
                TOUCH_REG(temp);
                attach = temp - 0x9F;
                if (attach == 0xB) {
                    param1[0] = 0x11;
                }
                if (attach == 0xC) {
                    param1[0] = 0x12;
                }
            }
            break;
        }
    }

    if (param1[0] == 0) {
        return;
    }

    flag      = 1;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 0xA;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = flag;
}

void Gp_EnqueueViewCd(Task* task)
{
    GpAreaKey* sess;
    u8         param1[8];
    u8         param2[8];

    sess = &gGameSession->at4.loc;
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = sess->stage;
        param1[2] = sess->area;
        param1[0] = Gp_GetViewIndex();
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        task->state++;
    }
}

static void Gp_LoadWaitCdBusy(Task* task)
{
    if (CdCmd_Queue.field_1FA != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitIdle(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = -2;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitDone(Task* task)
{
    if (CdCmd_Queue.field_1FE == 0xFF) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadViewImages(void)
{
    u8 view;
    u8 i;

    view = Gp_GetViewIndex();
    for (i = 0; i < 50; i++) {
        if (D_8006C338[i].field_0 == 2) {
            if (view - 1 == i) {
                while (Fs_LoadImageChunk(D_8006C338[i].field_4, 1)) {
                }
                break;
            }
        }
    }
}

void Gp_FinishLoadWait(Task* task)
{
    Pad_ClearCooldown(0);
    if (task->spawnArg1 == 0) {
        Stage_RequestSpecialFlag(1);
        gGameSession->viewDirty = 0;
        taskKill(task);
        Display_ResetHeapWrapper();
    } else {
        if (task->spawnArg1 == 1) {
            gDisplayState.at100.flags.flipMode = 1;
        }
        gDisplayState.at100.flags.imageSource = 2;
        Task_Spawn(0, 0x17, 0, 0);
        gGameSession->viewReady = 1;
        taskKill(task);
    }
}

void Gp_LoadWaitDispatch(Task* task)
{
    TaskFuncTable6 sp;

    sp = Gp_LoadWaitFns;
    Pad_SetCooldown(0);
    if (task->state < 0) {
        Gp_FinishLoadWait(task);
    } else {
        sp.funcs[task->state](task);
    }
}

static void Gp_ReloadFromSave(void)
{
    Task*       slot;
    McSaveData* save;

    slot            = gameGetPtrSlot(1);
    save            = &Mc_SaveData[0];
    slot->spawnArg1 = save->at4.loc.view;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    gGameSession->at4.loc.view = save->at4.loc.view;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(2);
    gGameSession->viewReady = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

static void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                        = gameGetPtrSlot(1);
    Mc_SaveData[0].at4.loc.view = arg0;
    gGameSession->at4.loc.view  = arg0;
    slot->spawnArg1             = (u8)arg0;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(1);
    gDisplayState.at100.flags.imageSource = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void Gp_CommitSpawnLoc(Task* task)
{
    u8 val;

    val                         = (u8)task->spawnArg1;
    Mc_SaveData[0].at4.loc.view = val;
    gGameSession->at4.loc.view  = val;
    taskKill(task);
}

void func_800A99B4(void)
{
    Display_SpawnWithOtSmall(0, 0x26, 0, 0);
}

void Gp_SetupSprtDisplay(Task* task)
{
    DisplayState* ds;
    s32           flag;

    ds                       = &gDisplayState;
    flag                     = ds->keepGraphics;
    ds->at100.flags.flipMode = 2;
    if (flag == 0) {
        Gpu_ResetGraphAndOt();
        Tmd_AllocMissingBuffers();
    }
    Gp_AllocSprtLists();
    taskKill(task);
    Display_ResetHeapWrapper();
}

void Gp_LoadViewAndCd(u8 arg0)
{
    u8           view;
    u8           i;
    GameSession* session;
    u8           param2[8];
    u8           param1[8];

    view = Gp_GetViewIndex();
    for (i = 0; i < 50; i++) {
        if (D_8006C338[i].field_0 == 2) {
            if (view - 1 == i) {
                while (Fs_LoadImageChunk(D_8006C338[i].field_4, 1)) {
                }
                break;
            }
        }
    }
    session   = gGameSession;
    param1[3] = session->at4.loc.stage;
    param1[2] = session->at4.loc.area;
    param1[0] = Gp_GetViewIndex();
    param2[0] = 1;
    if (arg0 != 0) {
        param2[1] = 4;
    } else {
        param2[1] = 0;
    }
    param2[3] = 0;
    param2[2] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
}

void Gp_EnqueueConfigCd(s32 arg0)
{
    u8     param1[8];
    u8     param2[8];
    GpTbl5 table;

    table = Gp_ConfigCdTable;
    if (Mc_SaveData[0].characterId != 0) {
        param1[3] = 0;
        param1[2] = 1;
        param1[0] = 0;
        param2[0] = table.field_0[Player_Status.field_26 - 1];
        if ((u8)arg0 == 0) {
            param2[1] = 0;
        } else {
            param2[1] = 5;
        }
        param2[2] = 6;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }
}

void Gp_EnqueueHeldWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u8  val;
    s32 flag;

    val = Player_Status.weapon;
    if (val == 0) {
        val = 1;
    }
    flag      = 1;
    param1[0] = val;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 3;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = flag;
    Gp_EnqueueWeaponCd();
}

static void Gp_EnqueueStageCd(void)
{
    u8 param1[8];
    u8 param2[8];

    CdCmd_Enqueue(0x54, &gGameSession->at4.loc.view, NULL);
    param1[3] = 0;
    param1[2] = 0x5A;
    param1[0] = gGameSession->at4.loc.stage;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    param2[0] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
}

static void Gp_EnqueueCompanionCd(u8 type, u8 variant)
{
    u8  param2[4];
    u8* param1;

    if (type == 0) {
        return;
    }

    param1                 = SCRATCH_PUSH_BYTES(8);
    gGameSession->field_80 = 0;
    param1[3]              = 0;
    param1[2]              = 0x50;
    param1[0]              = 0;
    param2[0]              = type;
    param2[1]              = 0;
    param2[2]              = 4;
    param2[3]              = 6;
    CdCmd_Enqueue(0x21, param1, param2);

    if (variant != 0) {
        param1[3] = 0;
        param1[2] = 0x50;
        param1[0] = variant;
        param2[0] = type;
        param2[1] = 0;
        param2[2] = 4;
        param2[3] = 6;
        CdCmd_Enqueue(0x21, param1, param2);
        if (variant == 5) {
            gGameSession->companionVariant  = 3;
            Mc_SaveData[0].companionVariant = 3;
        }
    }

    SCRATCH_POP_BYTES(8);
}

void Gp_PumpTmdStream(Task* task)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (task->spawnType == 1) {
        obj->tpage = 4;
        obj->clut  = 6;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
}

/// Walk the inner area rec's 0x10-byte CdCmd 0x21 list (`Gp_CdRecCur`),
/// matching each id against the 0xC-byte list (`D_80114C68`). Returns 1
/// when the list is exhausted or missing, else 0 (still in flight).
static u16 Gp_PollAreaCdLoads(void)
{
    u8           param1[8];
    u8           param2[8];
    GpCdAreaRec* rec;
    GpCdRec0C*   rec12;
    s32          val;

    switch (Gp_AreaCdPhase) {
        case 0:
            rec         = (GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&Mc_SaveData[0].at4.loc.view);
            D_80114C64  = rec;
            Gp_CdRecCur = rec->field_0;
            if (rec == NULL) {
                return 1;
            }
            if (D_80114C68 == NULL) {
                return 1;
            }
            if (Gp_CdRecCur == NULL) {
                return 1;
            }
            Gp_AreaCdPhase++;
        case 1:
            while (Gp_CdRecCur->entryId != 0xFF) {
                if (Gp_CdRecCur->entryId == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                for (D_80114C68 = D_80114C64->field_4; D_80114C68->field_0 != 0xFF; D_80114C68++) {
                    if (Gp_CdRecCur->entryId == D_80114C68->field_0) {
                        break;
                    }
                }
                if (Gp_CdRecCur->pad_C == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                param1[3] = 0;
                param1[0] = Gp_CdRecCur->pad_C;
                rec12     = D_80114C68;
                val       = (s16)rec12->field_2;
                if (val >= 0x64) {
                    param2[0] = val % 100;
                    param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                } else {
                    param2[0] = rec12->field_2;
                    param1[2] = D_8010CAD0[rec12->field_4].field_0;
                }
                param2[1] = 0;
                param2[2] = Gp_CdRecCur->tpage;
                param2[3] = Gp_CdRecCur->clut;
                CdCmd_Enqueue(0x21, param1, param2);
                Gp_AreaCdPhase++;
                break;
            }
            if (Gp_CdRecCur->entryId == 0xFF) {
                return 1;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Gp_CdRecCur++;
                Gp_AreaCdPhase--;
            }
            break;
    }
    return 0;
}

static u16 func_800AA120(void)
{
    u8           param1[8];
    u8           param2[8];
    GpCdAreaRec* rec;
    GpCdRec0C*   rec12;
    u16          key;
    s32          val;
    s16          d;
    s16          e;

    switch (D_80114C70) {
        case 0:
            rec        = (GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&Mc_SaveData[0].at4.loc.view);
            D_80114C64 = rec;
            D_80114C68 = rec->field_4;
            if (rec == NULL) {
                goto finished;
            }
            if (rec->field_4 == NULL) {
                return 1;
            }
            D_80114C70++;
        case 1:
            if (D_80114C68->field_0 == 0xFF) {
                return 1;
            }
            do {
                Gp_CdRecCur = D_80114C64->field_0;
                D_80114C72  = 0;
                if (Gp_CdRecCur->entryId != 0xFF) {
                    key = D_80114C68->field_0;
                    while (Gp_CdRecCur->entryId != 0xFF) {
                        if (Gp_CdRecCur->entryId == key && Gp_CdRecCur->pad_C == 0) {
                            D_80114C72 = 1;
                            break;
                        }
                        Gp_CdRecCur++;
                    }
                }
                rec12 = D_80114C68;
                if (rec12->field_4 != 5) {
                    if (D_80114C72 != 0) {
                        d         = (s8)Gp_CdRecCur->tpage;
                        e         = (s8)Gp_CdRecCur->clut;
                        param1[3] = 0;
                        param1[0] = 0;
                        val       = (s16)rec12->field_2;
                        if (val >= 0x64) {
                            param2[0] = val % 100;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                        } else {
                            param2[0] = rec12->field_2;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0;
                        }
                        param2[1] = 0;
                        param2[2] = d;
                        param2[3] = e;
                        CdCmd_Enqueue(0x21, param1, param2);
                        goto queued;
                    } else {
                        d         = 0;
                        e         = 0;
                        param1[3] = 0;
                        param1[0] = 0;
                        val       = (s16)rec12->field_2;
                        if (val >= 0x64) {
                            param2[0] = val % 100;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                        } else {
                            param2[0] = rec12->field_2;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0;
                        }
                        param2[1] = 0;
                        param2[2] = d;
                        param2[3] = e;
                        CdCmd_Enqueue(0x21, param1, param2);
                        goto queued;
                    }
                } else if (D_80114C72 != 0) {
                    d         = (s8)Gp_CdRecCur->tpage;
                    e         = (s8)Gp_CdRecCur->clut;
                    param1[3] = 0;
                    param1[0] = 0;
                    val       = (s16)rec12->field_2;
                    if (val >= 0x64) {
                        param2[0] = val % 100;
                        param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                    } else {
                        param2[0] = rec12->field_2;
                        param1[2] = D_8010CAD0[rec12->field_4].field_0;
                    }
                    param2[1] = 0;
                    param2[2] = d;
                    param2[3] = e;
                    CdCmd_Enqueue(0x21, param1, param2);
                queued:
                    D_80114C70++;
                    break;
                } else {
                    D_80114C68 = rec12 + 1;
                }
            } while (rec12[1].field_0 != 0xFF);
            if (D_80114C68->field_0 == 0xFF) {
            finished:
                return 1;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                D_80114C68++;
                D_80114C70--;
            }
            break;
    }
    return 0;
}

static void func_800AA548(s32 arg0)
{
    GpWarpRec    rec;
    GpActorFlags flags;
    TmdObject*   model;
    GpAreaKey*   sess;
    GameSession* session;
    PlayerPos*   pos;
    s32          stage;
    s32          warp;
    u32          playerId;

    session                    = gGameSession;
    session->deathVariant      = 0;
    gDisplayState.otDepthShift = 0;
    sess                       = &session->at4.loc;
    if (Player_Status.hp <= 0) {
        Player_Status.hp = 1;
    }
    if ((Mc_SaveData[0].companionType != 0) && (Mc_SaveData[0].companionHp <= 0)) {
        Mc_SaveData[0].companionHp = 1;
    }
    Gp_LoadRoomParams();
    gGameSession->cutsceneHold = 0;
    Gp_ResetMenuLock();
    Display_ClampField126(0);
    Task_Spawn(0, 0x1D, 0, 0);
    Task_Spawn(0, 0x1A, 0, 0);
    Game_SetPtrSlot(Task_Spawn(4, 5, 0, 0), 9);
    Task_Spawn(0, 0x14, 0, 0);
    if ((arg0 & 0xFFFF) != 1) {
        Game_SetPtrSlot(Task_Spawn(0, 0x16, 0, 0), 1);
    }
    Game_SetPtrSlot(Task_Spawn(0, 0x10, 0, 0), 2);
    stage = sess->stage;
    warp  = sess->warp;
    rec   = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if (!(gDisplayState.at100.word & 0xFFFF00)) {
        if (((GP_LOC_WORD(gGameSession->at4.loc) & ~0xFF) == GP_LOC_KEY(3, 24, 2, 0)) && (gGameSession->at4.loc.warp == 2)) {
            Mc_SaveData[0].at4.loc.view = gGameSession->at4.loc.view = 2;
        } else {
            Mc_SaveData[0].at4.loc.view = gGameSession->at4.loc.view = rec.field_34;
        }
    }
    Gp_ActorSlots[0] = NULL;
    Gp_ActorSlots[1] = NULL;
    if (gDisplayState.at100.flags.pendingPlayerPos == 1) {
        pos                = &D_80073B08[Mc_SaveData[0].characterId].pos;
        D_80114CB0.field_0 = (s32)pos->yaw;
        D_80114CB0.field_4 = (s32)pos->x;
        D_80114CB0.field_8 = (s32)pos->y;
        D_80114CB0.field_C = (s32)pos->z;
        flags.field_0      = 0x23;
        flags.field_2      = 0;
        Gp_SpawnPlayer((GpActorArg*)&D_80114CB0, Mc_SaveData[0].characterId & 0xFFFF, 0, &flags);
        Gp_SetupCompanionActor((GpActorArg*)&rec.field_14, &flags.field_0);
        gDisplayState.at100.flags.pendingPlayerPos = 0;
    } else {
        playerId      = (u8)Mc_SaveData[0].characterId;
        flags.field_0 = 1;
        flags.field_2 = rec.field_35 & 1;
        Gp_SpawnPlayer((GpActorArg*)&rec, (s8)playerId & 0xFFFF, 0, &flags);
        flags.field_2 = 0;
        Gp_SetupCompanionActor((GpActorArg*)&rec.field_14, &flags.field_0);
    }
    model        = (gameGetPtrSlot(3))->extra.tmd;
    model->tpage = 6;
    model->clut  = 0;
    tmdProcessStream(model);
    tmdProcessStream(model);
    Gp_LoadStageView();
    Game_SetPtrSlot(Task_Spawn(1, 0x23, 0, 0), 4);
    Game_SetPtrSlot(Task_Spawn(6, 4, 0, 0), 5);
    Task_Spawn(9, 6, 0, 0);
    Task_Spawn(9, 0x11, 0, 0);
    if ((Mc_SaveData[0].demoScene != 0) && (Mc_SaveData[0].demoScene != 0xB)) {
        Task_Spawn((s32)Mc_SaveData[0].demoScene, 1, 0, 0);
    }
    Gp_SpawnPlaces(sess);
    Gp_SpawnArea((GpAreaKey*)sess);
    Gp_InitStateF0();
    Task_Spawn(1, 0xF, 0, 0);
    Task_Spawn(1, 0x10, 0, 0);
    stage = sess->stage;
    warp  = sess->warp;
    rec   = Gp_WarpTables[stage - 1][sess->area - 1][warp - 1];
    if ((u8)gGameSession->areaSetupDone != 0) {
        if (rec.field_28 != 0) {
            SndEvt_EnqueueType6(rec.field_28, 0, 0);
        }
        if (rec.field_36 != 0) {
            GameFlag_SetNibble((s32)rec.field_36, 1);
        }
    } else {
        gGameSession->areaSetupDone = 1;
    }
    CdCmd_Queue.field_210        = 0;
    gGameSession->freezeRoomObjs = 0;
}

static void Gp_BeginSessionTask(Task* arg0)
{
    CdCmdQueue*   queue;
    DisplayState* ds;
    u16           one;

    queue = &CdCmd_Queue;
    Game_ClearPtrSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    one = 1;
    Mem_Init();
    CdCmd_ActivatePhase1();
    gGameSession->at4.raw     = Mc_SaveData[0].at4.raw;
    gGameSession->sprtVariant = ds->roomVariant;
    queue->field_20A          = one;
    if ((arg0->spawnArg1 & 0xF) == 0) {
        MoveImage(
            (RECT*)&gDisplayState.dispEnv[ds->drawBuffer ^ 1],
            ds->dispEnv[ds->drawBuffer].disp.x,
            ds->dispEnv[ds->drawBuffer].disp.y);
        ds->at100.flags.imageSource = 0;
        Display_SetMode(0xD010);
    }
    Task_Spawn(0, 0x1C, arg0->spawnArg1 & 0xF, 0);
    ds->skipDraw     = 0;
    queue->field_244 = one;
    queue->field_248 = one;
    D_8007A394       = 0;
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// Sets `Pad_RemapState->field_3`. When the CD queue is idle and
/// `func_80042500` returns 0: sets `CdCmd_Queue.field_22E`, starts the
/// boot load if a command is queued, clears `Stream_Slots`, refreshes
/// `GameSession.loadedWeaponFamily` / `loadedConfigSet` from save/config (enqueueing
/// CdCmd 0x21 via `Gp_EnqueueConfigCd` / `Gp_EnqueueHeldWeaponCd` if stale), then
/// `Gp_EnqueueAttach7Cd` and advances `task->state`.
static void Gp_LoadWaitBoot(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    McSaveData*   save;
    GameSession*  session;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;

    Pad_RemapState->field_3 = 1;
    queue                   = &CdCmd_Queue;
    if (CdCmd_IsIdle() & 0xFFFF) {
        if ((u8)func_80042500()) {
            return;
        }
        queue->field_22E = 1;
        if (queue->field_224 != 0) {
            Fs_EnsureBootLoadStarted();
        }
        Mem_Set(Stream_Slots, 0, sizeof(Stream_Slots));
        session = gGameSession;
        save    = &Mc_SaveData[0];
        if (session->loadedWeaponFamily != save->characterId || session->loadedConfigSet != Player_Status.field_26) {
            GameSession* sess;

            Gp_EnqueueConfigCd(0);
            Gp_EnqueueHeldWeaponCd();
            sess                     = gGameSession;
            sess->loadedWeaponFamily = save->characterId;
            sess->loadedConfigSet    = Player_Status.field_26;
        }
        Gp_EnqueueAttach7Cd();
        task->state++;
    }
    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, enqueues a stage reload if
/// `GameSession.at4.loc.stage` differs from the cached `loadedStage`, then
/// advances `task->state`.
static void Gp_LoadWaitStage(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;

    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        if (gGameSession->at4.loc.stage != gGameSession->loadedStage) {
            Gp_EnqueueStageCd();
            gGameSession->loadedStage = gGameSession->at4.loc.stage;
        }
        task->state++;
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle: `Gp_InitStageVisit` on the save location,
/// `Mem_ConfigureAuxHeap(loc.stage, loc.area)`, `Mem_SetActiveAuxHeap(1)` when
/// the save is in stage 5 / area 1, `Mem_InitAux`, `Gp_ApplyNpcRoomSnd`,
/// `Snd_InitFromStage`. Sets `gStageSceneMusicEntry` when in stage 3 with game flag
/// nibble 0x7A >= 4, primes `GameSession.areaBgmCountdown` / `field_12E` /
/// `deathRestartDelay` (1 / -0x80 / 0x1E) and `D_8007A39C` (0x3C / 0), spawns table `D_80062774` entry 0,
/// then advances `task->state`.
static void Gp_LoadState2(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;
    McSaveData*   save;
    GpSndParam*   pair;
    GpAreaKey*    sess;

    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        sess = &Mc_SaveData[0].at4.loc;
        Gp_InitStageVisit(sess);
        save = &Mc_SaveData[0];
        Mem_ConfigureAuxHeap(save->at4.loc.stage, save->at4.loc.area);
        if ((GP_LOC_WORD(save->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            Mem_SetActiveAuxHeap(true);
        }
        Mem_InitAux();
        Gp_ApplyNpcRoomSnd();
        Snd_InitFromStage(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
        if (gGameSession->at4.loc.stage == 3 && GameFlag_GetNibble(0x7A) >= 4) {
            gStageSceneMusicEntry = 1;
        } else {
            gStageSceneMusicEntry = 0;
        }
        gGameSession->areaBgmCountdown  = 1;
        *(s8*)&gGameSession->field_12E  = -0x80;
        gGameSession->deathRestartDelay = 0x1E;
        pair                            = (GpSndParam*)&D_8007A39C;
        pair->field_0                   = 0x3C;
        pair->field_2                   = 0;
        Task_SpawnFromTable(&D_80062774, 0, 0, 0);
        task->state++;
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, enqueues CdCmd 0x21 with the current
/// session location (`at4.loc.room` / `at4.loc.area` / `at4.loc.stage`), then
/// `Gp_PickCompanion`. If that returns a companion type, stores it in
/// `GameSession.companionType` and calls `Gp_EnqueueCompanionCd` with
/// `Mc_SaveData[0].companionType` / `companionVariant`. Then advances `task->state`.
static void Gp_LoadWaitCompanion(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;
    u8            param1[8];
    u8            param2[8];
    u8            flag;

    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = gGameSession->at4.loc.stage;
        param1[2] = gGameSession->at4.loc.area;
        param1[1] = gGameSession->at4.loc.room;
        param1[0] = 0;
        param1[4] = 0;
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        flag = Gp_PickCompanion();
        if (flag != 0) {
            gGameSession->companionType = flag;
            Gp_EnqueueCompanionCd(Mc_SaveData[0].companionType, Mc_SaveData[0].companionVariant);
        }
        task->state++;
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// When the CD queue is idle, if the session location high word is
/// `0x3010000` and `loc.room >= 4`, re-inits stage sound and enqueues
/// CdCmd 0x21 (`param1[0] = 0x16`). If `applySavePlace` is 1, applies
/// `Mc_SaveData[0].at4.loc.place` via `Gp_SetAreaObjId` and clears the flag. Then
/// applies the save location (`Gp_MarkAreaVisited` / `Gp_SyncAreaKeyIndex`), copies
/// `Mc_SaveData[0].at4.loc.place` into `GameSession.at4.loc.place`, builds the stream
/// VLC, clears `D_80114C74`, and advances `task->state`.
static void Gp_LoadWaitSave(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;
    u8            param1[8];
    u8            param2[8];
    GpAreaKey*    saveKey;
    GameSession*  sess;

    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        GameSession* session;

        session = gGameSession;
        if ((GP_LOC_WORD(session->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(3, 1, 0, 0)) {
            if (session->at4.loc.room >= 4) {
                Snd_InitFromStage(session->at4.loc.stage, session->at4.loc.area);
                param1[3] = gGameSession->at4.loc.stage;
                param1[2] = gGameSession->at4.loc.area;
                param1[0] = 0x16;
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }
        }
        sess = gGameSession;
        if (sess->applySavePlace == 1) {
            Gp_SetAreaObjId((GpAreaKey*)&sess->at4.loc, Mc_SaveData[0].at4.loc.place, -1);
            gGameSession->applySavePlace = 0;
        }
        saveKey = (GpAreaKey*)&Mc_SaveData[0].at4.loc.view;
        Gp_MarkAreaVisited(saveKey);
        Gp_SyncAreaKeyIndex(saveKey);
        gGameSession->at4.loc.place = saveKey->place;
        CdCmd_BuildVlcIfStream();
        D_80114C74 = 0;
        task->state++;
    }
}

/// Steps the area CD load through its phases, one per frame: phase 0 resets
/// `D_80114C70` and falls into phase 1, which runs `func_800AA120` until it
/// finishes and then moves to phase 2, which polls `Gp_PollAreaCdLoads`.
/// Returns 1 once phase 2 reports every load complete, 0 otherwise.
static inline u16 _gpAdvanceAreaCd(void)
{
    switch (D_80114C74) {
        case 0:
            D_80114C70 = 0;
            D_80114C74 = 1;
        case 1:
            if (func_800AA120()) {
                Gp_AreaCdPhase = 0;
                D_80114C74++;
            }
            return 0;
        case 2:
            if (Gp_PollAreaCdLoads()) {
                return 1;
            }
        default:
            return 0;
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (RGB 8), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0.
/// Then walks `D_80114C74`: phase 0 resets `D_80114C70` and falls into
/// phase 1 (`func_800AA120`); when that finishes, phase 2 runs
/// `Gp_PollAreaCdLoads`. On success, resets TMD lists / the current OT,
/// advances `task->state`, and if `Mc_SaveData[0].interlace` is set enables
/// interlace on both `DISPENV` slots.
static void Gp_LoadWaitAreaCd(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    DisplayState* ds2;
    s32           color;
    s32           queued;
    s32           buf;
    s8            yoff;

    color  = 8;
    queued = CdCmd_Queue.field_224;
    ds     = &gDisplayState;
    buf    = ds->otBuffer;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    if (queued == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }

    if (_gpAdvanceAreaCd()) {
        Gp_ClearObjHeads();
        Tmd_InitLists();
        ds2 = &gDisplayState;
        Gp_DrawActorTmdActive(&Gpu_OtBuffers[ds2->drawBuffer]);
        task->state++;
        if (Mc_SaveData[0].interlace != 0) {
            ds2->dispEnv[1].isinter = 1;
            ds2->dispEnv[0].isinter = 1;
        }
    }
}

/// Dual-buffer TILE / DR_TPAGE overlay (gray 0x64), indexed by
/// `gDisplayState.otBuffer`. Draws while `CdCmd_Queue.field_224` is 0,
/// then after 7 frames clears `CdCmd_Queue.field_22E` and advances state.
static void Gp_FadeGrayHold(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    s32           color;
    s32           buf;
    s8            yoff;

    queue = &CdCmd_Queue;
    ds    = &gDisplayState;
    color = 0x64;
    buf   = ds->otBuffer;
    tile  = &Gp_FadeTiles[buf];
    dr    = &Gp_FadeTpages[buf];
    if (queue->field_224 == 0) {
        setlen(tile, 3);
        setcode(tile, 0x62);
        tile->r0 = color;
        tile->g0 = color;
        tile->b0 = color;
        tile->x0 = -0xA0;
        yoff     = ds->vramYOffset;
        tile->w  = 0x140;
        tile->h  = 0xF0;
        tile->y0 = -0x78 - yoff;
        addPrim(gGpuCurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(gGpuCurrentOt - 0x10, dr);
    }
    task->killCountdown++;
    if (task->killCountdown >= 7) {
        queue->field_22E = 0;
        task->state++;
    }
}

static void Gp_InitStageVisit(GpAreaKey* arg0)
{
    McSaveData*  save;
    GpFlagBank** banks;
    GpFlagBank*  bank;

    banks = Gp_FlagBanks;
    save  = &Mc_SaveData[0];
    if ((save->visitFlags & 1) == 0) {
        save->visitFlags = 1;
        Gp_ClearAllFlagNibbles();
        Gp_ApplyNewGameAreaFlags();
        save->companionHpMax = 0x64;
        save->companionHp    = 0x64;
        func_800B8014();
    }
    if ((((s8)save->visitFlags >> arg0->stage) & 1) == 0) {
        bank             = banks[arg0->stage];
        bank->field_4[0] = 0;
        bank->field_4[1] = 0;
        Gp_ApplyBit2Bank(arg0->stage);
        if (gDisplayState.field_112 != 0) {
            func_80724748(arg0);
        }
    }
}

/// Pick companion type into `Mc_SaveData[0].companionType` from the NPC room tables.
/// Returns 0 if already current or none; else 1/2/3 for the caller to store
/// in `GameSession.companionType`.
static s32 Gp_PickCompanion(void)
{
    McSaveData*           save;
    McSaveData*           p;
    McSaveData*           q;
    GameSession*          sess;
    register GameSession* session asm("a0");
    u8*                   bytes;
    s32                   stage;
    u8                    hi;
    s32                   arg;

    arg   = 0x4B;
    save  = &Mc_SaveData[0];
    stage = save->at4.loc.stage;
    bytes = D_80114198[GameFlag_GetNibble(arg)].field_0;
    if (bytes != NULL) {
        if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
            if (bytes[save->at4.loc.area - 1] != 0) {
                sess                   = gGameSession;
                save->companionType    = 2;
                save->companionVariant = 0;
                return (sess->companionType != 2) * 2;
            }
        }
    }

    bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
    if (bytes != NULL) {
        if (D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage) {
            p = &Mc_SaveData[0];
            if (bytes[p->at4.loc.area - 1] & 0xF) {
                session          = gGameSession;
                p->companionType = 1;
                if (session->companionType == 1) {
                    hi = bytes[p->at4.loc.area - 1] >> 4;
                    if (session->companionVariant == hi) {
                        p->companionVariant = hi;
                        return 0;
                    }
                }
                q                              = &Mc_SaveData[0];
                q->companionVariant            = bytes[q->at4.loc.area - 1] >> 4;
                gGameSession->companionVariant = bytes[q->at4.loc.area - 1] >> 4;
                return 1;
            }
        }
    }

    bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
    if (bytes != NULL) {
        if (D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage) {
            p = &Mc_SaveData[0];
            if (bytes[p->at4.loc.area - 1] != 0) {
                session             = gGameSession;
                p->companionType    = 3;
                p->companionVariant = 0;
                if (session->companionType == 3) {
                    return 0;
                }
                return 3;
            }
        }
    }

    Mc_SaveData[0].companionType    = 0;
    Mc_SaveData[0].companionVariant = 0;
    gGameSession->companionType     = 0;
    gGameSession->companionVariant  = 0;
    return 0;
}

static void Gp_ApplyNpcRoomSnd(void)
{
    McSaveData* save;
    u8*         bytes;
    s32         stage;
    s32         flag;

    save  = &Mc_SaveData[0];
    stage = save->at4.loc.stage;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(3, 32, 0, 0)) {
        bytes = D_80114198[GameFlag_GetNibble(0x4B)].field_0;
        if (bytes != NULL) {
            if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
                if (bytes[save->at4.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
        if (bytes != NULL) {
            if (D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage) {
                if (bytes[Mc_SaveData[0].at4.loc.area - 1] & 0xF) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
        if (bytes != NULL) {
            if (D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage) {
                if (bytes[Mc_SaveData[0].at4.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
    }
    flag = 0;
done:
    Snd_SetModeFlag(flag);
}

static void Gp_SetupCompanionActor(GpActorArg* arg0, u16* arg1)
{
    McSaveData* save;
    s32         field;

    save  = &Mc_SaveData[0];
    field = save->companionType;
    if (field != 0) {
        if (field == 2) {
            Gp_SpawnAlly(arg0, save->companionType, GameFlag_GetNibble(0x4B), arg1);
        } else {
            Gp_SpawnAlly(arg0, field, 0, arg1);
        }
    }
}

static void Gp_ClearFlagBank(s32 arg0)
{
    GpFlagBank* bank;

    bank             = Gp_FlagBanks[arg0];
    bank->field_4[0] = 0;
    bank->field_4[1] = 0;
}

static void Gp_MarkAreaVisited(GpAreaKey* arg0)
{
    McSaveData* save;
    GpFlagBank* bank;
    s32         which;
    s32         bit;
    s32         mask;
    s32         flags;

    bank = Gp_FlagBanks[arg0->stage];
    save = &Mc_SaveData[0];
    if ((((s8)save->visitFlags >> arg0->stage) & 1) == 0) {
        save->visitFlags |= 1 << arg0->stage;
        if (gDisplayState.field_112 != 0) {
            func_80724E2C();
        }
    }

    which = 0;
    if (arg0->area >= 0x21) {
        which = 1;
        bit   = arg0->area - 0x21;
    } else {
        bit = arg0->area - 1;
    }

    mask  = 1;
    flags = bank->field_4[which];
    if (((mask << bit) & flags) == 0) {
        bank->field_4[which] = flags | (mask << bit);
        Gp_SetAreaFlag0(arg0);
    }
}

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

static void Gp_SessionState1(Task* task)
{
    DisplayState* ds;
    s32           temp;

    ds             = &gDisplayState;
    ds->skipDraw   = 1;
    ds->holdState |= 0x80;
    temp           = task->spawnArg1 & 0xF;
    if (temp != 0) {
        if (temp == 1) {
            ds->at100.flags.imageSource = 0;
        }
    }
    task->state++;
}

static void Gp_ResumeSessionTask(Task* task)
{
    SndBank_SetEnableFlags(0, 0x40000000);
    if (gGameSession->deathVariant != 0) {
        taskKill(task);
        return;
    }
    if ((task->spawnArg1 & 0x10) == 0) {
        if (Gp_StateF0.field_0 == 2) {
            Gp_StateF0.field_0 = 3;
        }
        Gp_TriggerPeIfArmed();
    }
    task->state++;
}

void func_800AC0F0(Task* task)
{
    TaskFuncTable3 sp;

    sp = Gp_SessionStates;
    Pad_SetCooldown(0);
    *(volatile u8*)&Gp_StateF0.field_4 = 1;
    sp.funcs[((volatile Task*)task)->state](task);
}

static void Gp_LoadFinishTask(Task* task)
{
    if (CdCmd_Queue.field_224 == 0) {
        Gpu_ClearOTag(0);
        Gpu_ClearOTag(1);
        Pad_RemapState->field_3 = 0;
        taskKill(task);
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            func_800AA548(1);
        } else {
            func_800AA548(0);
        }
        gDisplayState.holdCount  = 0;
        gDisplayState.holdState &= 0x7F;
        Display_AcquireRef();
        Task_Spawn(0, 0x21, 0, 0);
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            Task_SpawnFromTable(D_80183824, 0, 0, 0);
            CdCmd_SetupMdecBuffers();
            CdCmd_SelectMdecBuffer();
        }
    }
}

void Gp_LoadStateTask(Task* task)
{
    TaskFuncTable8 sp;
    DisplayState*  ds;

    sp = Gp_LoadStateFns;
    Pad_SetCooldown(0);
    ds = &gDisplayState;
    if (ds->demoScene != 0) {
        if (Pad_ReadButtonsInv(0) & 0x800) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                Wip_SysFlags.field_4 = 1;
                ds->gameMode         = 1;
                return;
            }
        }
    }
    sp.funcs[task->state](task);
}

void Gp_FlashWhiteTask(Task* task)
{
    CdCmdQueue* queue;
    u8          fade;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            task->killCountdown = 0;
            task->state++;
        case 1:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            task->killCountdown++;
            if (task->killCountdown < 3) {
                return;
            }
            task->killCountdown = 0xFF;
            task->state++;
            break;
        case 2:
            fade = task->killCountdown;
            Fade_DrawOverlay(fade, fade, fade, 2);
            task->killCountdown -= 0x1E;
            if (task->killCountdown > 0) {
                return;
            }
            if ((s16)queue->field_248 != 0) {
                queue->field_248 = 0;
                queue->field_244 = 0;
            }
            Display_ReleaseRef();
            taskKill(task);
            break;
    }
}

s32 Gp_DispatchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpMsgEntry* temp;
    GpMsgEntry* entry;

    temp = arg0->msgTable;
    if (temp == NULL) {
        return 0;
    }
    entry = temp;
    if (entry->id != arg1) {
        do {
            if (entry->id == 0x7FFFFFFF) {
                return 0;
            }
            entry++;
        } while (entry->id != arg1);
    }
    return entry->handler(arg0, arg1, arg2, arg3);
}

/// Same room-object link as `Gp_LinkRoomObjects`, then spawn type 0x1B as a
/// child, clear `GameSession.roomObjsDirty`, and increment `task->state`.
static void Gp_LinkRoomObjectsSpawn(Task* task)
{
    GpAreaKey*    sess;
    GpRoomObjRec* recs;
    GpRoomObjRec* rec;
    GpGridParams* grid;
    GpObj4A*      list1;
    GpObj4A*      list2;
    GpObj3A*      list3;
    GpObj4A*      obj;
    GpObj3A*      obj3;
    GpCoord*      coord;
    u8            flags;
    Task*         spawned;

    sess = &gGameSession->at4.loc;
    recs = Gp_RoomObjTables[sess->stage - 1]->field_0[sess->area - 1];
    if (recs != NULL) {
        rec   = (GpRoomObjRec*)(sess->room * sizeof(GpRoomObjRec) + (s32)recs);
        recs  = rec - 1;
        grid  = rec[-1].field_0;
        list1 = recs->field_4;
        list2 = recs->field_8;
        list3 = recs->field_C;
        if (grid != NULL) {
            grid->field_0 = &gGfxViewCoord;
            Gp_GridParams = grid;
        }
        if (list1 != NULL) {
            coord = &gGfxViewCoord;
            obj   = list1;
            do {
                obj->field_8 = coord;
                Gp_LinkObj4A(1, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                TOUCH_REG(obj);
                obj++;
            } while (!(flags & 0x80));
        }
        if (list2 != NULL) {
            coord = &gGfxViewCoord;
            obj   = list2;
            do {
                obj->field_8 = coord;
                Gp_LinkObj4A(0, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                TOUCH_REG(obj);
                obj++;
            } while (!(flags & 0x80));
        }
        if (list3 != NULL) {
            obj3 = list3;
            do {
                Gp_LinkObj3A(0, obj3);
                flags          = obj3->field_3A | 0x40;
                obj3->field_3A = flags;
                TOUCH_REG(obj3);
                obj3++;
            } while (!(flags & 0x80));
        }
    }
    spawned = Task_Spawn(0, 0x1B, 0, 0);
    if (spawned != NULL) {
        Task_Reparent(task, spawned);
    }
    gGameSession->roomObjsDirty = 0;
    task->state++;
}

void Gp_LinkViewSprts(void)
{
    GpAreaKey*    sess;
    s32           view;
    DisplayState* ds;
    GpSprtPrim**  table;
    GpSprtTbl*    tbl;
    GpSprtRec*    recs;
    GpSprtCmd*    rec;
    GpSprtElem*   base;

    sess          = &gGameSession->at4.loc;
    view          = Gp_GetViewIndex();
    table         = Gp_SprtLists;
    ds            = &gDisplayState;
    Gp_SprtCursor = table[ds->drawBuffer];
    tbl           = Gp_SprtTables[sess->stage - 1];
    recs          = tbl->field_0[sess->area - 1];
    rec           = recs[(u8)view - 1].field_4;
    base          = recs[(u8)view - 1].field_0;
    if (rec->field_2 == 0) {
        rec++;
    } else {
        ds->at100.flags.imageSource = 0;
    }
    if (rec->field_0 != 0xFFFF) {
        do {
            if (rec->field_5 == 0) {
                Gp_LinkSprtCmd(base, rec);
            }
            rec++;
        } while (rec->field_0 != 0xFFFF);
    }
}

/// Build merged `DR_TPAGE`+`SPRT` packets into `gGpuPrimCursor` from
/// `arg0[arg1->field_0]` for `arg1->field_2` entries, and OT-link each.
static void Gp_EmitSprts(GpSprtElem* arg0, GpSprtCmd* arg1)
{
    register u32  i asm("s4");
    GpTpageSprt*  dest;
    GpSprtElem*   elem;
    GpSprtElem*   cur;
    DisplayState* ds;
    register u32  maskHi asm("s6");
    u32           mask;
    SPRT*         sprt;
    u32           tpage;

    i              = 0;
    dest           = (GpTpageSprt*)gGpuPrimCursor;
    elem           = arg0 + arg1->field_0;
    gGpuPrimCursor = dest + arg1->field_2;
    if (arg1->field_2 != 0) {
        ds     = &gDisplayState;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        cur    = elem;
        do {
            sprt = &dest->sprt;
            if ((cur->flags & 1) == 0) {
                PRIM_COLOR_WORD(sprt, 0) = PRIM_COLOR_WORD(cur, 0);
            }
            tpage = elem->tpage;
            setlen(&dest->tpage, 1);
            setlen(sprt, 4);
            setcode(sprt, 0x64);
            dest->tpage.code[0] = 0xE1000000 | (tpage & 0x9FF);
            MargePrim(dest, sprt);
            sprt->code           |= cur->flags;
            *(u16*)&sprt->u0      = *(u16*)&cur->u0;
            sprt->clut            = cur->clut;
            PRIM_XY_WORD(sprt, 0) = PRIM_XY_WORD(cur, 0);
            i++;
            TOUCH_REG(i);
            *(u32*)&sprt->w = *(u32*)&cur->w;
            elem++;
            dest->tpage.tag = (dest->tpage.tag & maskHi) | (*(u_long*)(((((u32)cur->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) & mask);
            *(u_long*)(((((u32)cur->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) =
                (*(u_long*)(((((u32)cur->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) & maskHi) | ((u32)dest & mask);
            dest++;
            cur++;
        } while (i < arg1->field_2);
    }
}

static void Gp_SetSprtShadeBits(s32 arg0)
{
    GpAreaKey*           sess;
    s32                  view;
    register GpSprtPrim* prim asm("a1");
    GpSprtTbl*           tbl;
    GpSprtRec*           recs;
    GpSprtCmd*           rec;
    u8*                  p;
    u32                  i;
    u8                   flags;

    sess          = &gGameSession->at4.loc;
    view          = Gp_GetViewIndex();
    prim          = Gp_SprtLists[gDisplayState.drawBuffer];
    Gp_SprtCursor = prim;
    tbl           = Gp_SprtTables[sess->stage - 1];
    recs          = tbl->field_0[sess->area - 1];
    rec           = recs[(u8)view - 1].field_4;
    if (rec->field_0 != 0xFFFF) {
        do {
            if (rec->field_5 == 0) {
                if (Gp_SprtLists[0] != NULL) {
                    for (i = 0; i < rec->field_2;) {
                        p = &prim->field_F;
                        do {
                            if (arg0 != 0) {
                                flags = *p | 1;
                            } else {
                                flags = *p & 0xFE;
                            }
                            *p = flags;
                            p += 0x1C;
                            prim++;
                        } while (++i < rec->field_2);
                    }
                }
            }
            rec++;
        } while (rec->field_0 != 0xFFFF);
    }
}

void Gp_AllocSprtLists(void)
{
    GpAreaKey*   sess;
    u8           view;
    u32          count;
    s32          i;
    GpSprtRec*   recs;
    GpSprtCmd*   rec;
    GpSprtElem*  elems;
    GpSprtElem*  elem;
    s32          bufIdx;
    GpTpageSprt* buf[2];
    GpTpageSprt* dest;
    SPRT*        sprt;
    u32          tpage;

    sess  = &gGameSession->at4.loc;
    count = 0;
    view  = Gp_GetViewIndex();
    recs  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    rec   = recs[view - 1].field_4;
    elems = recs[view - 1].field_0;
    while (rec->field_0 != 0xFFFF) {
        count += rec->field_2;
        rec++;
    }
    count *= 0x38;
    if (count == 0) {
        Gp_SprtLists[0] = NULL;
        return;
    }
    Gp_SprtLists[0] = memCalloc(count, 1);
    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    count         >>= 1;
    count          += (u32)Gp_SprtLists[0];
    Gp_SprtLists[1] = (GpSprtPrim*)count;
    buf[0]          = (GpTpageSprt*)Gp_SprtLists[0];
    buf[1]          = (GpTpageSprt*)count;
    for (rec = recs[view - 1].field_4; rec->field_0 != 0xFFFF; rec++) {
        if (rec->field_5 != 0) {
            continue;
        }
        for (bufIdx = 0; bufIdx < 2; bufIdx++) {
            elem = elems + rec->field_0;
            for (i = 0; i < rec->field_2; i++) {
                dest                     = buf[bufIdx];
                sprt                     = &dest->sprt;
                PRIM_COLOR_WORD(sprt, 0) = PRIM_RGBC(0, 0x80, 0, 0);
                setlen(&dest->tpage, 1);
                tpage = elem->tpage;
                setlen(&dest->sprt, 4);
                setcode(&dest->sprt, 0x65);
                dest->tpage.code[0] = 0xE1000000 | (tpage & 0x9FF);
                MargePrim(dest, sprt);
                sprt->code           |= elem->flags;
                *(u16*)&sprt->u0      = *(u16*)&elem->u0;
                sprt->clut            = elem->clut;
                PRIM_XY_WORD(sprt, 0) = PRIM_XY_WORD(elem, 0);
                *(u32*)&sprt->w       = *(u32*)&elem->w;
                elem++;
                buf[bufIdx]++;
            }
        }
    }
}

static void Gp_LinkRoomObjects(Task* task)
{
    GpAreaKey*    sess;
    GpRoomObjRec* recs;
    GpGridParams* grid;
    GpObj4A*      list1;
    GpObj4A*      list2;
    GpObj3A*      list3;
    s32           i;

    sess = &gGameSession->at4.loc;
    Gp_LoadStageView();
    Gp_GridParams = NULL;
    Gp_ClearObj4AList(1);
    Gp_ClearObj4AList(0);
    Gp_ClearObj3AList(0);
    recs = Gp_RoomObjTables[sess->stage - 1]->field_0[sess->area - 1];
    if (recs != NULL) {
        grid  = recs[sess->room - 1].field_0;
        list1 = recs[sess->room - 1].field_4;
        list2 = recs[sess->room - 1].field_8;
        list3 = recs[sess->room - 1].field_C;
        if (grid != NULL) {
            grid->field_0 = &gGfxViewCoord;
            Gp_GridParams = grid;
        }
        if (list1 != NULL) {
            for (i = 0;; i++) {
                list1[i].field_8 = &gGfxViewCoord;
                Gp_LinkObj4A(1, &list1[i]);
                list1[i].field_4A |= 0x40;
                if (list1[i].field_4A & 0x80) {
                    break;
                }
            }
        }
        if (list2 != NULL) {
            for (i = 0;; i++) {
                list2[i].field_8 = &gGfxViewCoord;
                Gp_LinkObj4A(0, &list2[i]);
                list2[i].field_4A |= 0x40;
                if (list2[i].field_4A & 0x80) {
                    break;
                }
            }
        }
        if (list3 != NULL) {
            for (i = 0;; i++) {
                Gp_LinkObj3A(0, &list3[i]);
                list3[i].field_3A |= 0x40;
                if (list3[i].field_3A & 0x80) {
                    break;
                }
            }
        }
    }
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
}

s8 Gp_FindViewIndex(s32 arg0)
{
    s16        idx;
    GpAreaKey* sess;
    s16        limit;
    u8*        bytes;

    idx   = 0;
    sess  = &gGameSession->at4.loc;
    limit = *(s16*)&Gp_ViewCountTables[sess->stage - 1]->field_0[sess->area - 1][sess->room - 1];
    bytes = Gp_ViewIndexTables[sess->stage - 1]->field_0[sess->area - 1][sess->room - 1];
    if (limit > 0) {
        do {
            if (bytes[idx] == (u8)arg0) {
                return idx + 1;
            }
            idx++;
        } while (idx < limit);
    }
    return 0;
}

static s32 Gp_ViewSprtCmdEmpty(void)
{
    GameSession*    session;
    GpAreaKey*      sess;
    GpSprtTbl**     tbl68;
    s32             i;
    GpViewIndexTbl* tbl;
    u8***           mid;
    u8**            inner;
    u8*             bytes;
    u8              idx;
    GpSprtTbl*      tbl2;
    GpSprtRec**     mid2;
    GpSprtRec*      recs;

    session = gGameSession;
    tbl68   = Gp_SprtTables;
    sess    = &session->at4.loc;
    i       = sess->stage - 1;
    tbl68   = &tbl68[i];
    tbl     = Gp_ViewIndexTables[i];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    idx     = bytes[sess->view - 1];
    tbl2    = *tbl68;
    mid2    = tbl2->field_0;
    recs    = mid2[sess->area - 1];
    return recs[idx - 1].field_4->field_2 == 0;
}

static void func_800AD024(void)
{
    RECT            rect;
    GameSession*    session;
    GpAreaKey*      sess;
    GpViewIndexTbl* tbl;
    u8***           mid;
    u8**            inner;
    u8*             bytes;
    u8              idx;
    GpSprtTbl*      tbl2;
    GpSprtRec**     mid2;
    GpSprtRec*      recs;
    GpDrawAreaRec*  area;
    DR_AREA*        prim;

    session = gGameSession;
    sess    = &session->at4.loc;
    tbl     = Gp_ViewIndexTables[sess->stage - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    idx     = bytes[sess->view - 1];
    tbl2    = Gp_SprtTables[sess->stage - 1];
    mid2    = tbl2->field_0;
    recs    = mid2[sess->area - 1];
    area    = recs[idx - 1].field_8;
    if (area != NULL) {
        for (; area->depth != 0xFFFF; area++) {
            rect = area->rect;
            if (gDisplayState.drawBuffer != 0) {
                rect.y += 0x110;
            }
            prim           = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            SetDrawArea(prim, &rect);
            addPrim(&gGpuCurrentOt[0x3FF], prim);
            if (gDisplayState.drawBuffer != 0) {
                rect.y = 0x110;
            } else {
                rect.y = 0;
            }
            rect.x         = 0;
            rect.w         = 0x140;
            rect.h         = 0xF0;
            prim           = (DR_AREA*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            SetDrawArea(prim, &rect);
            addPrim((u_long*)((((u32)area->depth << gDisplayState.otDepthShift) >> 2 & 0xFFC) + (u32)gGpuCurrentOt), prim);
        }
    }
}

s32 Gp_GetViewIndex(void)
{
    GameSession*    session;
    GpAreaKey*      sess;
    GpViewIndexTbl* tbl;
    u8***           mid;
    u8**            inner;
    u8*             bytes;

    session = gGameSession;
    sess    = &session->at4.loc;
    tbl     = Gp_ViewIndexTables[sess->stage - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    return bytes[sess->view - 1];
}

void* Gp_GetViewSprtExtra(void)
{
    GameSession*    session;
    GpAreaKey*      sess;
    GpViewIndexTbl* tbl;
    u8***           mid;
    u8**            inner;
    u8*             bytes;
    u8              idx;
    GpSprtTbl*      tbl2;
    GpSprtRec**     mid2;
    GpSprtRec*      recs;

    session = gGameSession;
    sess    = &session->at4.loc;
    tbl     = Gp_ViewIndexTables[sess->stage - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->area - 1];
    bytes   = inner[sess->room - 1];
    idx     = bytes[sess->view - 1];
    tbl2    = Gp_SprtTables[sess->stage - 1];
    mid2    = tbl2->field_0;
    recs    = mid2[sess->area - 1];
    return recs[idx - 1].field_8;
}

static void Gp_RoomObjState1(Task* task)
{
    if (task->spawnArg1 != (u8)gGameSession->at4.loc.view) {
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        task->spawnArg1 = (u8)gGameSession->at4.loc.view;
    }
    if (gGameSession->roomObjsDirty != 0) {
        Gp_LinkRoomObjects(task);
        gGameSession->roomObjsDirty = 0;
    }
    func_800AD024();
}

static void Gp_LinkSprtCmd(GpSprtElem* arg0, GpSprtCmd* arg1)
{
    register u32  i asm("t0");
    GpSprtPrim*   prim;
    GpSprtElem*   elem;
    DisplayState* ds;
    u_long*       otBase;

    i = 0;
    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    prim = Gp_SprtCursor;
    elem = arg0 + arg1->field_0;
    if (arg1->field_2 != 0) {
        ds     = &gDisplayState;
        otBase = gGpuCurrentOt;
        do {
            if (arg1->field_4 == 0) {
                addPrim(&otBase[((u32)elem->otz << ds->otDepthShift) >> 4 & 0x3FF], prim);
            }
            prim++;
            i++;
            elem++;
        } while (i < arg1->field_2);
    }
    Gp_SprtCursor = prim;
}

void func_800AD50C(Task* task)
{
    TaskFuncTable3 funcs;

    funcs = Gp_RoomObjStates;
    if (gGameSession->freezeRoomObjs == 0) {
        funcs.funcs[task->state](task);
    } else {
        gDisplayState.at100.flags.imageSource = 0;
    }
}

void Gp_AllocSprtListsTask(Task* task)
{
    Gp_AllocSprtLists();
    taskKill(task);
}

void func_800AD5B8(Task* task)
{
    TaskFunc funcs[2] = { func_800AD620, func_800AD65C };

    if (gGameSession->freezeRoomObjs == 0) {
        funcs[task->state](task);
    }
}

static void func_800AD620(Task* task)
{
    s32 val;

    val = Gp_ViewSprtCmdEmpty();
    do {
        gDisplayState.at100.flags.imageSource = val;
    } while (0);
    task->state++;
}

static void func_800AD65C(Task* task)
{
    DisplayState* ds;
    s32           val;

    ds = &gDisplayState;
    if ((ds->displayOwner != 2) && (ds->skipDraw == 0)) {
        Gp_LinkViewSprts();
    } else {
        val                                   = Gp_ViewSprtCmdEmpty();
        gDisplayState.at100.flags.imageSource = val;
    }
}

void func_800AD6BC(void)
{
    Task*            slot;
    PlayerStatus*    cfg;
    u32              flags;
    u32              action;
    u32              mask;
    GpDirActionTable funcs;

    funcs = Gp_DirActionFns;
    cfg   = &Player_Status;
    slot  = gameGetPtrSlot(1);
    if (slot != NULL) {
        if (slot->spawnArg1 != Mc_SaveData[0].at4.loc.view) {
            func_800A7F24();
            D_80114D08 = 0xA;
        }
    }
    if (gDisplayState.pendingMode != 0) {
        D_80114D08 = 0xA;
    }
    if (D_80114CF8 == 0) {
        if (Gp_StateC08.field_A == 0) {
            gGameSession->dirActionBusy = 0;
            if (D_80114D08 != 0) {
                D_80114D08 = (u16)D_80114D08 - 1;
            }
            if (Gp_TakePendingObj4C(&Gp_DirFlags, &Gp_DirByte, &Gp_DirNibble) != 0) {
                if (D_80114CD0 != (s16)Gp_DirFlags) {
                    D_80114CDC = 1;
                } else {
                    D_80114CDC = 0;
                }
                Gp_DirPhase = 0;
                flags       = Gp_DirFlags;
                mask        = flags & 0x8000;
                if (Gp_StateF0.field_1 == 0) {
                    if (mask && (gDisplayState.pendingMode == 0) && !(gGameSession->padPrev & 0x10)) {
                        if (!(flags & 0x4000)) {
                            D_80114CF8 = 1;
                        } else if (Gp_StateF0.field_0 != 1) {
                            D_80114CF8 = 1;
                        }
                    } else if (cfg->field_24 != 0) {
                        if (!(gGameSession->padPrev & 0x10)) {
                            if (!(Gp_DirFlags & 0x4000)) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            } else if (Gp_StateF0.field_0 != 1) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = 0xA;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    D_80114CD0 = (s16)Gp_DirFlags;
    if (D_80114CF8 != 0) {
        gGameSession->dirActionBusy = 1;
        action                      = (u8)Gp_DirFlags;
        if (action != 0xFF) {
            funcs.funcs[action]();
        } else {
            Gp_DirNibble    = 0;
            Gp_DirByte      = 0;
            Gp_DirAltNibble = 0;
            Gp_DirAlt       = 0;
            Gp_DirFlags     = 0;
            D_80114CD4      = 0;
            D_80114CF8      = 0;
        }
    } else {
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        Gp_DirFlags     = 0;
        D_80114CD4      = 0;
    }
    D_80114CDE = Gp_StateF0.field_0;
}

void Gp_SetupDirWarp(void)
{
    Task*         slot7;
    Task*         slot3;
    PlayerStatus* cfg;
    GameActor*    actor;
    GpAreaKey*    sess;
    GpWarpRec     rec;
    GpXformArg    msg;
    SVECTOR       pos;
    SVECTOR       pos2;
    s32           stage;
    s32           room;
    s16           ret;

    sess  = &gGameSession->at4.loc;
    stage = sess->stage;
    room  = sess->area;
    slot7 = gameGetPtrSlot(7);
    slot3 = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    actor = slot3->work;

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        return;
    }

    D_80114CF4      = 0;
    Gp_DirFadeLevel = 0;
    rec             = Gp_WarpTables[stage - 1][room - 1][(Gp_DirNibble >> 4) - 1];

    Gp_WarpLoc.field_4 = 1;
    Gp_WarpLoc.field_3 = 1;
    Gp_WarpLoc.field_5 = 1;
    *(u16*)&Gp_WarpLoc = Gp_DirByte;
    Gp_WarpLoc.field_2 = Gp_DirNibble & 0xF;
    Gp_WarpLoc.field_6 = rec.field_36;

    ret        = Gp_DispatchMsg(slot7, 0x13EE, (s32)&Gp_WarpLoc, (s32)&Gp_WarpLoc);
    D_80114CF4 = ret;

    switch (ret) {
        case 1:
            if (rec.field_2C != 0) {
                D_80114CF0 = rec.field_2C;
            } else {
                D_80114CF0 = 0;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (rec.field_0 + 0x800) & 0xFFF;
            if (rec.field_0 == 0x7800 || rec.field_0 == 0x7FFF) {
                pos.vx     = -0x5C1;
                pos.vy     = 0;
                pos.vz     = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ((Task*)Gp_ActorSlots[0], (GpPosXZ*)&pos);
            } else if (rec.field_0 == 0x7FFE) {
                msg.rot.vy = actor->field_52;
            }
            Gp_DispatchMsg(slot3, 0x3EE, (s32)&msg, 0);
            if (rec.field_35 & 2) {
                Gp_DirFadeLevel = 0x1E;
            }
            Gp_DirPhase++;
            break;

        case 0:
            if (rec.field_30 != 0) {
                D_80114CF0 = rec.field_30;
            } else {
                D_80114CF0 = 0;
            }
            if (Gp_StateF0.field_0 == 1) {
                Gp_WarpLoc.field_4 = Gp_StateF0.field_0;
                Gp_WarpLoc.field_3 = Gp_StateF0.field_0;
                Gp_WarpLoc.field_5 = 0;
                *(u16*)&Gp_WarpLoc = Gp_DirByte;
                Gp_WarpLoc.field_2 = Gp_DirNibble & 0xF;
                Gp_WarpLoc.field_6 = rec.field_36;
                Gp_DispatchMsg(slot7, 0x13EE, (s32)&Gp_WarpLoc, (s32)&Gp_WarpLoc);
                D_80114CF8    = 0;
                Gp_DirNibble  = 0;
                Gp_DirByte    = 0;
                Gp_DirFlags   = 0;
                cfg->field_24 = 0;
                if (D_80114CF0 != 0 && cfg->hp > 0) {
                    SndEvt_EnqueueType6(D_80114CF0, 0, 0);
                }
                return;
            }
            msg.rot.vx = 0;
            msg.rot.vz = 0;
            msg.rot.vy = (rec.field_0 + 0x800) & 0xFFF;
            if (rec.field_0 == 0x7800 || rec.field_0 == 0x7FFF) {
                pos2.vx    = -0x5C1;
                pos2.vy    = 0;
                pos2.vz    = 0x9C1;
                msg.rot.vy = Gp_YawToPosXZ((Task*)Gp_ActorSlots[0], (GpPosXZ*)&pos2);
            } else if (rec.field_0 == 0x7FFE) {
                msg.rot.vy = actor->field_52;
            }
            Gp_DispatchMsg(slot3, 0x3EE, (s32)&msg, 0);
            Gp_DirPhase++;
            break;

        case 2:
            Gp_WarpLoc.field_4 = 1;
            Gp_WarpLoc.field_3 = 1;
            Gp_WarpLoc.field_5 = 0;
            *(u16*)&Gp_WarpLoc = Gp_DirByte;
            Gp_WarpLoc.field_2 = Gp_DirNibble & 0xF;
            Gp_WarpLoc.field_6 = rec.field_36;
            Gp_DispatchMsg(slot7, 0x13EE, (s32)&Gp_WarpLoc, (s32)&Gp_WarpLoc);
            D_80114CF8    = 0;
            Gp_DirNibble  = 0;
            Gp_DirByte    = 0;
            Gp_DirFlags   = 0;
            cfg->field_24 = 0;
            break;
    }
}

void Gp_FadeDirWaitMsg(void)
{
    void* slot;
    u8    fade;

    slot = gameGetPtrSlot(3);
    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
        if (D_80114CF4 != 0) {
            Gp_StateF0.field_4 = 1;
        }
        Gp_DirPhase++;
    }
}

void Gp_CommitWarp(void)
{
    Task*         slot3;
    Task*         slot7;
    PlayerStatus* cfg;
    GpAreaKey*    sess;
    GpWarpRec     rec;
    GpSaveLoc*    loc;
    u8            fade;

    slot3 = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    slot7 = gameGetPtrSlot(7);

    sess = &gGameSession->at4.loc;
    rec  = Gp_WarpTables[sess->stage - 1][sess->area - 1][(Gp_DirNibble >> 4) - 1];

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }

    loc                = &Gp_WarpLoc;
    loc->field_4       = 1;
    loc->field_3       = 1;
    loc->field_5       = 0;
    *(u16*)&Gp_WarpLoc = Gp_DirByte;
    loc->field_2       = Gp_DirNibble & 0xF;
    loc->field_6       = rec.field_36;
    Gp_DispatchMsg(slot7, 0x13EE, (s32)loc, (s32)loc);

    if (D_80114CF0 != 0) {
        if (cfg->hp > 0) {
            SndEvt_EnqueueType6(D_80114CF0, 0, 0);
        }
    }

    if (D_80114CF4 == 0) {
        Gp_DispatchMsg(slot3, 0x3F1, 0, 0);
        D_80114CF8    = 0;
        Gp_DirNibble  = 0;
        Gp_DirByte    = 0;
        Gp_DirFlags   = 0;
        cfg->field_24 = 0;
    } else {
        Gp_DirPhase++;
    }
}

void Gp_WarpPhase4(void)
{
    u8 fade;

    if (*(s16*)&Gp_DirFadeLevel != 0) {
        fade = *(u8*)&Gp_DirFadeLevel;
        Fade_DrawOverlay(fade, fade, fade, 2);
        Gp_DirFadeLevel += 0x1E;
        if ((s16)Gp_DirFadeLevel >= 0x100) {
            Gp_DirFadeLevel = 0xFF;
        }
    }
    if (D_80114CF0 == 0 || SndVoice_HasActiveId(D_80114CF0) == 0) {
        Gp_DirPhase++;
    }
}

void Gp_MsgPlayerDirFacing(void)
{
    Task*      slot;
    GameActor* actor;
    u8         flags;
    s32        facing;
    u8*        row;

    actor = gameGetPtrSlot(3)->work;
    flags = Gp_DirByte;
    if (flags & 0x80) {
        facing = actor->field_82;
        if (Gp_DirFlags & 0x100) {
            row              = D_801149FC[(flags & 0x70) >> 4].field_4;
            actor->field_930 = row[(flags & 0xF) - facing];
        } else {
            row              = D_801149FC[(flags & 0x70) >> 4].field_0;
            actor->field_930 = row[(flags & 0xF) - facing];
        }
    } else {
        actor->field_930 = (flags & 0x70) >> 4;
    }

    slot = gameGetPtrSlot(3);
    if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
        Gp_DispatchMsg(slot, 0x3F1, 0, 0);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else if (Gp_TakePendingObj4C(&D_80114CD4, &Gp_DirAlt, &Gp_DirAltNibble)) {
        if ((u8)D_80114CD4 == 0) {
            Gp_StateF0.field_4 = 1;
            Gp_DirPhase++;
        }
    }
}

void Gp_CommitDirWarp(void)
{
    Task*       slot;
    GpSaveLoc*  loc;
    McSaveData* save;

    slot = gameGetPtrSlot(7);
    loc  = &Gp_WarpLoc;

    /* first two bytes as one halfword (field_1 cleared) */
    *(u16*)&Gp_WarpLoc = Gp_DirAlt;
    loc->field_2       = Gp_DirAltNibble & 0xF;
    loc->field_4       = 1;
    loc->field_3       = 1;
    loc->field_5       = 0;
    Gp_DispatchMsg(slot, 0x13EE, (s32)loc, (s32)loc);

    save               = &Mc_SaveData[0];
    save->at4.loc.area = Gp_WarpLoc.field_0;
    save->at4.loc.warp = loc->field_2;
    save->at4.loc.room = loc->field_3;
    Task_Spawn(0, 0x11, 0, 0);

    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    D_80114CD4      = 0;
    D_80114CDD      = 0;
}

void Gp_PostDirIfCapIdle(void)
{
    if (gGameSession->eventState == 0) {
        if (Gp_CapBusy() == 0) {
            if (Gp_DirNibble == 0xFF) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, Gp_DirByte, 0);
            } else {
                Gp_SpawnIfCapIdle(Gp_DirByte, Gp_DirNibble);
            }
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

void Gp_RunDirAction(void)
{
    void (*fns[2])(s32, s32) = { D_8017DA78, D_8017EF60 };

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    } else {
        fns[(Gp_DirFlags >> 8) & 0x7F](Gp_DirByte, Gp_DirNibble);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    }
}

void Gp_ApplyAreaRecs(GpAreaApplyRec* arg0)
{
    GpAreaKey                key;
    register GpAreaApplyRec* rec asm("s0");
    GpAreaRec*               tbl;
    GpAreaObj*               obj;
    GpAreaKey*               sess;
    McSaveData*              save;
    GpAreaRec**              tables;
    s32                      mask;
    s32                      apply;
    s32                      expected;
    s32                      cond;
    s8                       mode;
    u8                       idx;
    u8                       temp;

    apply = 0;
    sess  = &gGameSession->at4.loc;
    if (arg0->field_0 != 0xFF) {
        tables = Gp_AreaTables;
        save   = &Mc_SaveData[0];
        rec    = arg0;
        idx    = *(volatile u8*)&rec->field_0;
        do {
            tbl       = tables[idx];
            key.stage = idx;
            temp      = rec->field_1;
            key.room  = 1;
            key.area  = temp;
            key.view  = sess->view;
            mask      = rec->field_3 & 0xF0;
            if (mask == 0) {
                goto set_apply;
            }
            mode = save->gameMode;
            if (mode == 0 || mode == 2) {
                expected = 0x10;
                goto cmp;
            }
            if (mode == 1 || mode == 3) {
                expected = 0x20;
            } else {
                goto set_zero;
            }
        cmp:
            if (mask != expected) {
                cond = apply;
                goto test;
            }
        set_apply:
            apply = 1;
            goto join;
        set_zero:
            apply = 0;
        join:
            cond = apply;
        test:
            if (cond != 0) {
                Gp_SetAreaObjId(&key, rec->field_2, 1);
                if (tbl != NULL) {
                    obj = tbl[rec->field_1].field_4;
                    if (obj != NULL) {
                        if (rec->field_3 & 0xF) {
                            obj->field_1 |= 4;
                        } else {
                            obj->field_1 &= 0xFB;
                        }
                    }
                }
            }
            rec++;
            apply = 0;
            idx   = rec->field_0;
        } while (idx != 0xFF);
    }
}

/// New-game init: for each of the four stage flag lists, OR bit 2 into
/// `GpAreaObj.field_1` on every record whose apply flag is set.
static void Gp_ApplyNewGameAreaFlags(void)
{
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg1;
        tbl = Gp_AreaTableStg1;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->field_1 |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg2;
        tbl = Gp_AreaTableStg2;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->field_1 |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg4;
        tbl = Gp_AreaTableStg4;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->field_1 |= 0x4;
                    }
                }
            }
        }
    }
    {
        GpAreaRec*     tbl;
        GpAreaObj*     obj;
        GpAreaFlagRec* rec;

        rec = Gp_NewGameFlagsStg5;
        tbl = Gp_AreaTableStg5;
        if (tbl != NULL) {
            for (; rec->field_0 != 0xFF; rec++) {
                if (rec->field_1 != 0) {
                    obj = tbl[rec->field_0].field_4;
                    if (obj != NULL) {
                        obj->field_1 |= 0x4;
                    }
                }
            }
        }
    }
}

/// Bit 2 of the area object's flags byte, as 0 or 1; 0 when the stage has no
/// table or the area no object. The bit-2 counterpart of `Gp_GetAreaFlag2`.
static inline s32 _gpGetAreaFlag4(GpAreaKey* key)
{
    GpAreaRec* rec;
    GpAreaObj* obj;
    s32        val;

    rec = Gp_AreaTables[key->stage];
    if (rec != NULL) {
        obj = rec[key->area].field_4;
        if (obj != NULL) {
            val = obj->field_1 & 4;
            return val != 0;
        }
    }
    return 0;
}

void Gp_RebuildAreaIdBits(void)
{
    GpAreaKey  key;
    GpAreaKey* sess;
    s32        count;
    s32        i;
    u8         stage;

    sess      = &gGameSession->at4.loc;
    stage     = sess->stage;
    key.room  = 1;
    key.view  = 2;
    key.stage = stage;
    if (gGameSession->at4.loc.stage - 1 < 5) {
        count = Gp_AreaIdCounts[sess->stage - 1];
        for (i = 1; i <= count; i++) {
            key.area = i;
            if (_gpGetAreaFlag4(&key) == 1) {
                if (Gp_GetAreaFlag2(&key) == 1) {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                    } else {
                        Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                    }
                } else {
                    if (key.area <= 32) {
                        Gp_AreaIdBits[0] |= 1 << (key.area - 1);
                    } else {
                        Gp_AreaIdBits[1] |= 1 << (key.area - 33);
                    }
                }
            } else {
                if (key.area <= 32) {
                    Gp_AreaIdBits[0] &= ~(1 << (key.area - 1));
                } else {
                    Gp_AreaIdBits[1] &= ~(1 << (key.area - 33));
                }
            }
        }
    }
}

static const TaskFuncTable3 Gp_SessionStates = { {
    Gp_ResumeSessionTask,
    Gp_SessionState1,
    Gp_BeginSessionTask,
} };

static const TaskFuncTable8 Gp_LoadStateFns = { {
    Gp_LoadWaitBoot,
    Gp_LoadWaitStage,
    Gp_LoadState2,
    Gp_LoadWaitCompanion,
    Gp_LoadWaitSave,
    Gp_LoadWaitAreaCd,
    Gp_FadeGrayHold,
    Gp_LoadFinishTask,
} };

static const TaskFuncTable3 Gp_RoomObjStates = { {
    Gp_LinkRoomObjectsSpawn,
    Gp_RoomObjState1,
    taskKill,
} };
