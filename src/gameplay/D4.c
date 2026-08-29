#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
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

void Gp_FinishLoadWait(Task* task);
s32  func_800AA120(void);
void func_800AA548(s32 arg0);
void func_800AD024(void);
void func_800AD620(Task* task);
void func_800AD65C(Task* task);
void D_8017DA78(s32 arg0, s32 arg1);
void D_8017EF60(s32 arg0, s32 arg1);
void func_80724748(GameSessionFrom4* arg0);
void func_80724E2C(void);

extern TaskDesc       D_80183824[];
extern TaskFuncTable6 Gp_LoadWaitFns;
extern TaskFuncTable3 Gp_SessionStates;
extern TaskFuncTable8 Gp_LoadStateFns;
extern TaskFuncTable3 Gp_RoomObjStates;
extern u16            Gp_DirFlags;
extern u16            D_80114CD4;
extern u16            Gp_DirPhase;
extern u8             Gp_DirByte;
extern u8             Gp_DirNibble;
extern u8             Gp_DirAlt;
extern u8             Gp_DirAltNibble;
extern u8             D_80114CDC;
extern u8             D_80114CDD;
extern s32            D_80114CF0;
extern s16            D_80114CF4;
extern u16            Gp_DirFadeLevel;
extern u8             D_80114CF8;

void Gp_EnqueueWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u16 item;
    s32 val;
    s32 attach;
    s32 temp;
    s32 flag;

    item = Wip_SysConfig.field_21;
    if (item == 0) {
        return;
    }

    param1[0] = 0;
    switch (item) {
        case 0xB:
            param1[0] = 1;
            if (Wip_SysConfig.field_22 == 0xB) {
                param1[0] = 2;
            }
            if (Wip_SysConfig.field_22 == 0xC) {
                param1[0] = 3;
            }
            break;
        case 0xC:
            param1[0] = 4;
            if (Wip_SysConfig.field_22 == 0xB) {
                param1[0] = 5;
            }
            if (Wip_SysConfig.field_22 == 0xC) {
                param1[0] = 6;
            }
            break;
        case 0xD:
            param1[0] = 7;
            if (Wip_SysConfig.field_22 == 0xE) {
                param1[0] = 8;
            }
            if (Wip_SysConfig.field_22 == 0xF) {
                param1[0] = 9;
            }
            break;
        case 0xE:
            param1[0] = 0xA;
            if (Wip_SysConfig.field_22 == 0xE) {
                param1[0] = 0xB;
            }
            if (Wip_SysConfig.field_22 == 0xF) {
                param1[0] = 0xC;
            }
            break;
        case 0xF:
            param1[0] = 0xD;
            val       = Wip_SysConfig.field_22;
            if (val == 0xE) {
                param1[0] = val;
            }
            if (val == 0xF) {
                param1[0] = val;
            }
            break;
        case 0x17:
            param1[0] = 0x13;
            if (Wip_SysConfig.field_22 == 0xE) {
                param1[0] = 0x14;
            }
            if (Wip_SysConfig.field_22 == 0xF) {
                param1[0] = 0x15;
            }
            break;
        case 0x1B: {
            register GpItemSlot* slot asm("a0");

            param1[0] = 0x10;
            slot      = Gp_GetItemSlot(item + 0x7F);
            TOUCH_REG(slot);
            attach = slot->field_2;
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
    GameSessionFrom4* sess;
    u8                param1[8];
    u8                param2[8];

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = sess->field_3;
        param1[2] = sess->field_2;
        param1[0] = Gp_GetViewIndex();
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        task->state++;
    }
}

void Gp_LoadWaitCdBusy(Task* task)
{
    if (CdCmd_Queue.field_1FA != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadWaitIdle(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = -2;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadWaitDone(Task* task)
{
    if (CdCmd_Queue.field_1FE == 0xFF) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadViewImages(void)
{
    s32           raw;
    s32           i;
    s32           target;
    register s32  type2 asm("v0");
    FsFolderSlot* table;
    FsFolderSlot* slot;

    raw    = Gp_GetViewIndex();
    i      = 0;
    table  = D_8006C338;
    target = (u8)raw - 1;
    for (; (u8)i < 50; i++) {
        type2 = 2;
        if (table[(u8)i].field_0 == type2) {
            if (target == (u8)i) {
                slot = &table[(u8)i];
                while (Fs_LoadImageChunk((FsImageChunk*)slot->field_4, 1) & 0xFF) {
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
        Game_Session->field_52 = 0;
        Task_Kill(task);
        Display_ResetHeapWrapper();
    } else {
        if (task->spawnArg1 == 1) {
            Display_State.field_103 = 1;
        }
        Display_State.field_100 = 2;
        Task_Spawn(0, 0x17, 0, 0);
        Game_Session->field_4D = 1;
        Task_Kill(task);
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

void Gp_ReloadFromSave(void)
{
    Task*       slot;
    McSaveData* save;

    slot            = Game_GetPtrSlot(1);
    save            = &Mc_SaveData;
    slot->spawnArg1 = save->field_4;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Game_Session->field_4 = save->field_4;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(2);
    Game_Session->field_4D = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                  = Game_GetPtrSlot(1);
    Mc_SaveData.field_4   = arg0;
    Game_Session->field_4 = arg0;
    slot->spawnArg1       = (u8)arg0;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(1);
    Display_State.field_100 = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void Gp_CommitSpawnLoc(Task* task)
{
    u8 val;

    val                   = *(u8*)&task->spawnArg1;
    Mc_SaveData.field_4   = val;
    Game_Session->field_4 = val;
    Task_Kill(task);
}

void func_800A99B4(void)
{
    Display_SpawnWithOtSmall(0, 0x26, 0, 0);
}

void Gp_SetupSprtDisplay(Task* task)
{
    DisplayState* ds;
    s32           flag;

    ds            = &Display_State;
    flag          = (s8)ds->field_122;
    ds->field_103 = 2;
    if (flag == 0) {
        Gpu_ResetGraphAndOt();
        Tmd_AllocMissingBuffers();
    }
    Gp_AllocSprtLists();
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

void Gp_LoadViewAndCd(s32 arg0)
{
    s32           raw;
    s32           i;
    s32           target;
    register s32  type2 asm("v0");
    FsFolderSlot* table;
    FsFolderSlot* slot;
    GameSession*  session;
    u8            param2[8];
    u8            param1[8];

    raw    = Gp_GetViewIndex();
    i      = 0;
    table  = D_8006C338;
    target = (u8)raw - 1;
    for (; (u8)i < 50; i++) {
        type2 = 2;
        if (table[(u8)i].field_0 == type2) {
            if (target == (u8)i) {
                slot = &table[(u8)i];
                while (Fs_LoadImageChunk((FsImageChunk*)slot->field_4, 1) & 0xFF) {
                }
                break;
            }
        }
    }
    session   = Game_Session;
    param1[3] = session->field_7;
    param1[2] = session->field_6;
    param1[0] = Gp_GetViewIndex();
    param2[0] = 1;
    if ((u8)arg0 != 0) {
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
    if (Mc_SaveData.field_22 != 0) {
        param1[3] = 0;
        param1[2] = 1;
        param1[0] = 0;
        param2[0] = table.field_0[Wip_SysConfig.field_26 - 1];
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

    val = Wip_SysConfig.field_21;
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

void Gp_EnqueueStageCd(void)
{
    u8 param1[8];
    u8 param2[8];

    CdCmd_Enqueue(0x54, &Game_Session->field_4, NULL);
    param1[3] = 0;
    param1[2] = 0x5A;
    param1[0] = Game_Session->field_7;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    param2[0] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
}

void Gp_EnqueueCompanionCd(s32 arg0, s32 arg1)
{
    u8           param2[4];
    u8*          param1;
    void**       scratch;
    void*        head;
    void*        temp;
    s32          c50;
    s32          c4;
    s32          c6;
    register s32 saved1 asm("s6");
    register s32 flag asm("s3");

    saved1 = arg1;
    flag   = saved1;
    if ((u8)arg0 == 0) {
        return;
    }

    scratch  = (void**)G_SCRATCH_HEAD;
    c50      = 0x50;
    c4       = 4;
    c6       = 6;
    head     = *scratch;
    temp     = (u8*)head - 8;
    param1   = temp;
    *scratch = temp;

    Game_Session->field_80 = 0;
    param1[3]              = 0;
    param1[2]              = c50;
    ((u8*)head)[-8]        = 0;
    param2[0]              = arg0;
    param2[1]              = 0;
    param2[2]              = c4;
    param2[3]              = c6;
    CdCmd_Enqueue(0x21, param1, param2);

    if ((u8)flag != 0) {
        param1[3]       = 0;
        param1[2]       = c50;
        ((u8*)head)[-8] = saved1;
        param2[0]       = arg0;
        param2[1]       = 0;
        param2[2]       = c4;
        param2[3]       = c6;
        CdCmd_Enqueue(0x21, param1, param2);
        if ((u8)flag == 5) {
            Game_Session->field_125 = 3;
            Mc_SaveData.field_5C7   = 3;
        }
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void Gp_PumpTmdStream(Task* task)
{
    TmdObject* obj;

    obj = task->extra;
    if (task->spawnType == 1) {
        obj->field_24 = 4;
        obj->field_25 = 6;
        if (obj->field_18 != NULL) {
            Tmd_ProcessStream(obj);
            Tmd_ProcessStream(obj);
        }
    }
}

s32 Gp_PollAreaCdLoads(void)
{
    u8                  param1[8];
    u8                  param2[8];
    GpCdAreaRec*        rec;
    register GpCdRec10* rec10 asm("a0");
    register GpCdRec10* match asm("a1");
    register GpCdRec10* next asm("v0");
    GpCdRec0C*          rec12;
    s32                 val;
    u8                  temp;

    switch (Gp_AreaCdPhase) {
        case 0:
            rec         = (GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&Mc_SaveData.field_4);
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
            if (Gp_CdRecCur->field_0 != 0xFF) {
                do {
                    rec10 = Gp_CdRecCur;
                    if (rec10->field_0 == 0) {
                        next        = rec10 + 1;
                        Gp_CdRecCur = next;
                    } else {
                        D_80114C68 = D_80114C64->field_4;
                        if (D_80114C68->field_0 != 0xFF) {
                            SCHED_BARRIER();
                            match = rec10;
                            for (; D_80114C68->field_0 != 0xFF; D_80114C68++) {
                                if (match->field_0 == D_80114C68->field_0) {
                                    break;
                                }
                            }
                        }
                        if (Gp_CdRecCur->field_C == 0) {
                            Gp_CdRecCur++;
                        } else {
                            param1[3] = 0;
                            param1[0] = Gp_CdRecCur->field_C;
                            rec12     = D_80114C68;
                            val       = (s16)rec12->field_2;
                            if (val >= 0x64) {
                                param2[0] = val % 100;
                                temp      = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                            } else {
                                param2[0] = rec12->field_2;
                                temp      = D_8010CAD0[rec12->field_4].field_0;
                            }
                            param1[2] = temp;
                            COMPILER_BARRIER();
                            param2[1] = 0;
                            param2[2] = Gp_CdRecCur->field_D;
                            param2[3] = Gp_CdRecCur->field_E;
                            CdCmd_Enqueue(0x21, param1, param2);
                            Gp_AreaCdPhase++;
                            break;
                        }
                    }
                    {
                        register s32 hi asm("a0");
                        GpCdRec10*   p;
                        asm("lui %0, %%hi(Gp_CdRecCur)" : "=r"(hi));
                        asm("lw %0, %%lo(Gp_CdRecCur)(%1)" : "=r"(p) : "r"(hi));
                        if (p->field_0 == 0xFF) {
                            break;
                        }
                    }
                } while (1);
            }
            if (Gp_CdRecCur->field_0 == 0xFF) {
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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA120);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA548);

void Gp_BeginSessionTask(Task* arg0)
{
    CdCmdQueue*   queue;
    DisplayState* ds;
    u16           one;

    queue = &CdCmd_Queue;
    Game_ClearPtrSlots();
    ds            = &Display_State;
    ds->field_10b = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    one = 1;
    Mem_Init();
    CdCmd_ActivatePhase1();
    ((SessionBytesAt4*)Game_Session)->field_4 =
        ((SessionBytesAt4*)&Mc_SaveData)->field_4;
    Game_Session->field_74 = ds->field_10e;
    queue->field_20A       = one;
    if ((arg0->spawnArg1 & 0xF) == 0) {
        MoveImage(
            (RECT*)&Display_State.dispEnv[ds->field_1f ^ 1],
            ds->dispEnv[ds->field_1f].disp.x,
            ds->dispEnv[ds->field_1f].disp.y);
        ds->field_100 = 0;
        Display_SetMode(0xD010);
    }
    Task_Spawn(0, 0x1C, arg0->spawnArg1 & 0xF, 0);
    ds->field_104    = 0;
    queue->field_244 = one;
    queue->field_248 = one;
    D_8007A394       = 0;
}

void Gp_LoadWaitBoot(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    McSaveData*   save;
    GameSession*  session;
    register s32  color asm("a2");
    register s32  qhi asm("a1");
    register s32  queued asm("a0");
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
        session = Game_Session;
        save    = &Mc_SaveData;
        if (session->field_11C != save->field_22 || session->field_11E != Wip_SysConfig.field_26) {
            register GameSession* sess asm("v1");

            Gp_EnqueueConfigCd(0);
            Gp_EnqueueHeldWeaponCd();
            sess            = Game_Session;
            sess->field_11C = save->field_22;
            sess->field_11E = Wip_SysConfig.field_26;
        }
        Gp_EnqueueAttach7Cd();
        task->state++;
    }
    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi) : "r"(color), "r"(ds));
    buf  = ds->field_114;
    tile = &Gp_FadeTiles[buf];
    SOFT_USE_REG2(qhi, tile);
    dr     = &Gp_FadeTpages[buf];
    queued = *(u16*)((s32)qhi + (s16)0x91C4);
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }
}

void Gp_LoadWaitStage(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    register s32  color asm("a2");
    register s32  qhi asm("a1");
    register s32  queued asm("a0");
    s32           buf;
    s8            yoff;

    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi));
    buf    = ds->field_114;
    tile   = &Gp_FadeTiles[buf];
    dr     = &Gp_FadeTpages[buf];
    queued = *(u16*)((s32)qhi + (s16)0x91C4);
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        if (Game_Session->field_7 != Game_Session->field_78) {
            Gp_EnqueueStageCd();
            Game_Session->field_78 = Game_Session->field_7;
        }
        task->state++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", Gp_LoadState2);

void Gp_LoadWaitCompanion(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    register s32  color asm("a2");
    register s32  qhi asm("a1");
    register s32  queued asm("a0");
    s32           buf;
    s8            yoff;
    u8            param1[8];
    u8            param2[8];
    s32           flag;
    McSaveData*   save;

    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi) : "r"(color), "r"(ds));
    buf  = ds->field_114;
    tile = &Gp_FadeTiles[buf];
    SOFT_USE_REG2(qhi, tile);
    dr     = &Gp_FadeTpages[buf];
    queued = *(u16*)((s32)qhi + (s16)0x91C4);
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        register GameSession* session asm("v1");
        register s32          cmd asm("a0");
        register u8*          p1 asm("a1");
        register u8*          p2 asm("a2");
        register s32          tmp asm("v0");
        register s32          room asm("v1");

        session   = Game_Session;
        cmd       = 0x21;
        tmp       = session->field_7;
        p1        = param1;
        param1[3] = tmp;
        tmp       = session->field_6;
        p2        = param2;
        param1[2] = tmp;
        room      = session->field_5;
        param1[0] = 0;
        param1[4] = 0;
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        param1[1] = room;
        CdCmd_Enqueue(cmd, p1, p2);
        flag = Gp_PickCompanion();
        if ((u8)flag) {
            Game_Session->field_124 = flag;
            save                    = &Mc_SaveData;
            Gp_EnqueueCompanionCd((u8)save->field_13, (u8)save->field_5C7);
        }
        task->state++;
    }
}

void Gp_LoadWaitSave(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    register s32  color asm("a2");
    register s32  qhi asm("a1");
    register s32  queued asm("a0");
    s32           buf;
    s8            yoff;
    u8            param1[8];
    u8            param2[8];
    GpAreaKey*    saveKey;

    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi) : "r"(color), "r"(ds));
    buf  = ds->field_114;
    tile = &Gp_FadeTiles[buf];
    SOFT_USE_REG2(qhi, tile);
    dr     = &Gp_FadeTpages[buf];
    queued = *(u16*)((s32)qhi + (s16)0x91C4);
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        register GameSession* session asm("a1");

        session = Game_Session;
        if ((*(u32*)&session->field_4 & 0xFFFF0000) == 0x3010000) {
            if (session->field_5 >= 4) {
                register GameSession* sess asm("v1");
                register s32          cmd asm("a0");
                register u8*          p1 asm("a1");
                register u8*          p2 asm("a2");
                register s32          tmp asm("v0");
                register s32          loc asm("v1");

                Snd_InitFromStage(session->field_7, session->field_6);
                cmd       = 0x21;
                sess      = Game_Session;
                tmp       = sess->field_7;
                p1        = param1;
                param1[3] = tmp;
                loc       = sess->field_6;
                p2        = param2;
                param1[0] = 0x16;
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                param1[2] = loc;
                CdCmd_Enqueue(cmd, p1, p2);
            }
        }
        {
            register GameSession* sess asm("a0");

            sess = Game_Session;
            if (sess->field_4C == 1) {
                Gp_SetAreaObjId((GpAreaKey*)&sess->field_4, Mc_SaveData.field_9, -1);
                Game_Session->field_4C = 0;
            }
        }
        saveKey = (GpAreaKey*)&Mc_SaveData.field_4;
        Gp_MarkAreaVisited(saveKey);
        Gp_SyncAreaKeyIndex(saveKey);
        Game_Session->field_9 = saveKey->field_5;
        CdCmd_BuildVlcIfStream();
        D_80114C74 = 0;
        task->state++;
    }
}

void Gp_LoadWaitAreaCd(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    DisplayState* ds2;
    register s32  color asm("a2");
    register s32  qhi asm("a1");
    register s32  queued asm("a0");
    s32           buf;
    s8            yoff;

    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi) : "r"(color), "r"(ds));
    buf  = ds->field_114;
    tile = &Gp_FadeTiles[buf];
    SOFT_USE_REG2(qhi, tile);
    dr     = &Gp_FadeTpages[buf];
    queued = *(u16*)((s32)qhi + (s16)0x91C4);
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }

    {
        register s32 done asm("v0");

        switch (D_80114C74) {
            case 0:
                D_80114C70 = 0;
                D_80114C74 = 1;
            case 1:
                if (func_800AA120() & 0xFFFF) {
                    Gp_AreaCdPhase = 0;
                    D_80114C74++;
                }
                done = 0;
                break;
            case 2:
                done = Gp_PollAreaCdLoads() & 0xFFFF;
                if (done) {
                    done = 1;
                    break;
                }
            default:
                done = 0;
                break;
        }

        if (done & 0xFFFF) {
            Gp_ClearObjHeads();
            Tmd_InitLists();
            ds2 = &Display_State;
            Gp_DrawActorTmdActive(&Gpu_OtBuffers[ds2->field_1f]);
            task->state++;
            if (Mc_SaveData.field_5C3 != 0) {
                ds2->dispEnv[1].isinter = 1;
                ds2->dispEnv[0].isinter = 1;
            }
        }
    }
}

void Gp_FadeGrayHold(Task* task)
{
    TILE*         tile;
    DR_TPAGE*     dr;
    DisplayState* ds;
    CdCmdQueue*   queue;
    s32           color;
    s32           buf;
    s8            yoff;

    queue = &CdCmd_Queue;
    ds    = &Display_State;
    color = 0x64;
    buf   = ds->field_114;
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
        addPrim(Gpu_CurrentOt - 0x10, tile);
        setlen(dr, 1);
        dr->code[0] = 0xE1000000 | 0x240;
        addPrim(Gpu_CurrentOt - 0x10, dr);
    }
    task->killCountdown++;
    if (task->killCountdown >= 7) {
        queue->field_22E = 0;
        task->state++;
    }
}

void Gp_InitStageVisit(GameSessionFrom4* arg0)
{
    McSaveData*  save;
    GpFlagBank** banks;
    GpFlagBank*  bank;

    banks = Gp_FlagBanks;
    save  = &Mc_SaveData;
    if ((save->field_10 & 1) == 0) {
        save->field_10 = 1;
        Gp_ClearAllFlagNibbles();
        Gp_ApplyNewGameAreaFlags();
        save->field_6CA = 0x64;
        save->field_6C8 = 0x64;
        func_800B8014();
    }
    if ((((s8)save->field_10 >> arg0->field_3) & 1) == 0) {
        bank             = banks[arg0->field_3];
        bank->field_4[0] = 0;
        bank->field_4[1] = 0;
        Gp_ApplyBit2Bank(arg0->field_3);
        if (Display_State.field_112 != 0) {
            func_80724748(arg0);
        }
    }
}

s32 Gp_PickCompanion(void)
{
    McSaveData*           save;
    McSaveData*           p;
    register McSaveData*  q asm("a0");
    register GameSession* sess asm("v1");
    register GameSession* session asm("a0");
    register u8*          bytes asm("s0");
    s32                   stage;
    u8                    hi;
    register s32          arg asm("a0");

    arg   = 0x4B;
    save  = &Mc_SaveData;
    stage = save->field_7;
    bytes = D_80114198[GameFlag_GetNibble(arg)].field_0;
    if (bytes != NULL) {
        if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
            if (bytes[save->field_6 - 1] != 0) {
                sess            = Game_Session;
                save->field_13  = 2;
                save->field_5C7 = 0;
                return (sess->field_124 != 2) * 2;
            }
        }
    }

    bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
    if (bytes != NULL) {
        if (D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage) {
            p = &Mc_SaveData;
            if (bytes[p->field_6 - 1] & 0xF) {
                session     = Game_Session;
                p->field_13 = 1;
                if (session->field_124 == 1) {
                    hi = bytes[p->field_6 - 1] >> 4;
                    if (session->field_125 == hi) {
                        p->field_5C7 = hi;
                        return 0;
                    }
                }
                q                       = &Mc_SaveData;
                q->field_5C7            = bytes[q->field_6 - 1] >> 4;
                Game_Session->field_125 = bytes[q->field_6 - 1] >> 4;
                return 1;
            }
        }
    }

    bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
    if (bytes != NULL) {
        if (D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage) {
            p = &Mc_SaveData;
            if (bytes[p->field_6 - 1] != 0) {
                session      = Game_Session;
                p->field_13  = 3;
                p->field_5C7 = 0;
                if (session->field_124 == 3) {
                    return 0;
                }
                return 3;
            }
        }
    }

    Mc_SaveData.field_13    = 0;
    Mc_SaveData.field_5C7   = 0;
    Game_Session->field_124 = 0;
    Game_Session->field_125 = 0;
    return 0;
}

void Gp_ApplyNpcRoomSnd(void)
{
    McSaveData* save;
    u8*         bytes;
    s32         stage;
    s32         flag;

    save  = &Mc_SaveData;
    stage = save->field_7;
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x3200000) {
        bytes = D_80114198[GameFlag_GetNibble(0x4B)].field_0;
        if (bytes != NULL) {
            if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
                if (bytes[save->field_6 - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
        if (bytes != NULL) {
            if (D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage) {
                if (bytes[Mc_SaveData.field_6 - 1] & 0xF) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
        if (bytes != NULL) {
            if (D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage) {
                if (bytes[Mc_SaveData.field_6 - 1] != 0) {
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

void Gp_SetupCompanionActor(GpActorArg* arg0, u16* arg1)
{
    McSaveData* save;
    s32         field;

    save  = &Mc_SaveData;
    field = save->field_13;
    if (field != 0) {
        if (field == 2) {
            Gp_SpawnAlly(arg0, save->field_13, GameFlag_GetNibble(0x4B), arg1);
        } else {
            Gp_SpawnAlly(arg0, field, 0, arg1);
        }
    }
}

void Gp_ClearFlagBank(s32 arg0)
{
    GpFlagBank* bank;

    bank             = Gp_FlagBanks[arg0];
    bank->field_4[0] = 0;
    bank->field_4[1] = 0;
}

void Gp_MarkAreaVisited(GpAreaKey* arg0)
{
    McSaveData* save;
    GpFlagBank* bank;
    s32         which;
    s32         bit;
    s32         mask;
    s32         flags;

    bank = Gp_FlagBanks[arg0->field_3];
    save = &Mc_SaveData;
    if ((((s8)save->field_10 >> arg0->field_3) & 1) == 0) {
        save->field_10 |= 1 << arg0->field_3;
        if (Display_State.field_112 != 0) {
            func_80724E2C();
        }
    }

    which = 0;
    if (arg0->field_2 >= 0x21) {
        which = 1;
        bit   = arg0->field_2 - 0x21;
    } else {
        bit = arg0->field_2 - 1;
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

void Gp_SessionState1(Task* task)
{
    DisplayState* ds;
    s32           temp;

    ds            = &Display_State;
    ds->field_104 = 1;
    ds->field_1d |= 0x80;
    temp          = task->spawnArg1 & 0xF;
    if (temp != 0) {
        if (temp == 1) {
            ds->field_100 = 0;
        }
    }
    task->state++;
}

void Gp_ResumeSessionTask(Task* task)
{
    SndBank_SetEnableFlags(0, 0x40000000);
    if (Game_Session->field_0 != 0) {
        Task_Kill(task);
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
    *(volatile u8*)&D_801153F4 = 1;
    sp.funcs[((volatile Task*)task)->state](task);
}

void Gp_LoadFinishTask(Task* task)
{
    if (CdCmd_Queue.field_224 == 0) {
        Gpu_ClearOTag(0);
        Gpu_ClearOTag(1);
        Pad_RemapState->field_3 = 0;
        Task_Kill(task);
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x1050000) {
            func_800AA548(1);
        } else {
            func_800AA548(0);
        }
        Display_State.field_11d = 0;
        Display_State.field_1d &= 0x7F;
        Display_AcquireRef();
        Task_Spawn(0, 0x21, 0, 0);
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x1050000) {
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
    ds = &Display_State;
    if (ds->field_12c != 0) {
        if (Pad_ReadButtonsInv(0) & 0x800) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                Wip_SysFlags.field_4 = 1;
                ds->field_11e        = 1;
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
            Task_Kill(task);
            break;
    }
}

s32 Gp_DispatchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpMsgEntry* temp;
    GpMsgEntry* entry;

    temp = arg0->field_24;
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

void Gp_LinkRoomObjectsSpawn(Task* task)
{
    GameSessionFrom4* sess;
    GpRoomObjRec*     recs;
    GpRoomObjRec*     rec;
    GpGridParams*     grid;
    GpObj4A*          list1;
    GpObj4A*          list2;
    GpObj3A*          list3;
    GpObj4A*          obj;
    GpObj3A*          obj3;
    GsCOORDINATE2*    coord;
    u8                flags;
    Task*             spawned;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    recs = Gp_RoomObjTables[sess->field_3 - 1]->field_0[sess->field_2 - 1];
    if (recs != NULL) {
        rec   = (GpRoomObjRec*)(sess->field_1 * sizeof(GpRoomObjRec) + (s32)recs);
        recs  = rec - 1;
        grid  = rec[-1].field_0;
        list1 = recs->field_4;
        list2 = recs->field_8;
        list3 = recs->field_C;
        if (grid != NULL) {
            grid->field_0 = &Gfx_ViewCoord;
            Gp_GridParams = grid;
        }
        if (list1 != NULL) {
            coord = &Gfx_ViewCoord;
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
            coord = &Gfx_ViewCoord;
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
    Game_Session->field_76 = 0;
    task->state++;
}

void Gp_LinkViewSprts(void)
{
    GameSessionFrom4* sess;
    s32               view;
    DisplayState*     ds;
    GpSprtPrim**      table;
    GpSprtTbl*        tbl;
    GpSprtRec*        recs;
    GpSprtCmd*        rec;
    GpSprtElem*       base;

    sess          = (GameSessionFrom4*)&Game_Session->field_4;
    view          = Gp_GetViewIndex();
    table         = Gp_SprtLists;
    ds            = &Display_State;
    Gp_SprtCursor = table[ds->field_1f];
    tbl           = Gp_SprtTables[sess->field_3 - 1];
    recs          = tbl->field_0[sess->field_2 - 1];
    rec           = recs[(u8)view - 1].field_4;
    base          = recs[(u8)view - 1].field_0;
    if (rec->field_2 == 0) {
        rec++;
    } else {
        ds->field_100 = 0;
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

void Gp_EmitSprts(GpSprtElem* arg0, GpSprtCmd* arg1)
{
    GpSprtCmd*             rec;
    register u32           i asm("s4");
    GpTpageSprt*           dest;
    GpSprtElem*            elem;
    GpSprtElem*            cur;
    register DisplayState* hi asm("v0");
    register DisplayState* ds asm("s7");
    register u32           maskHi asm("s6");
    u32                    mask;
    SPRT*                  sprt;
    u32                    tpage;

    rec            = arg1;
    i              = 0;
    dest           = (GpTpageSprt*)Gpu_PrimCursor;
    elem           = arg0 + rec->field_0;
    Gpu_PrimCursor = (DR_TPAGE*)(dest + rec->field_2);
    if (rec->field_2 != 0) {
        asm("lui %0, %%hi(Display_State)" : "=r"(hi));
        asm("addiu %0, %1, %%lo(Display_State)" : "=r"(ds) : "r"(hi));
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        cur    = elem;
        do {
            sprt = &dest->sprt;
            if ((cur->flags & 1) == 0) {
                *(u32*)&sprt->r0 = *(u32*)&cur->r0;
            }
            tpage = elem->tpage;
            setlen(&dest->tpage, 1);
            setlen(sprt, 4);
            setcode(sprt, 0x64);
            dest->tpage.code[0] = 0xE1000000 | (tpage & 0x9FF);
            MargePrim(dest, sprt);
            sprt->code      |= cur->flags;
            *(u16*)&sprt->u0 = *(u16*)&cur->u0;
            sprt->clut       = cur->clut;
            *(u32*)&sprt->x0 = *(u32*)&cur->x0;
            i++;
            TOUCH_REG(i);
            *(u32*)&sprt->w = *(u32*)&cur->w;
            elem++;
            dest->tpage.tag = (dest->tpage.tag & maskHi) | (*(u_long*)(((((u32)cur->otz << ds->field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt) & mask);
            *(u_long*)(((((u32)cur->otz << ds->field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt) =
                (*(u_long*)(((((u32)cur->otz << ds->field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt) & maskHi) | ((u32)dest & mask);
            dest++;
            cur++;
        } while (i < rec->field_2);
    }
}

void Gp_SetSprtShadeBits(s32 arg0)
{
    GameSessionFrom4*    sess;
    s32                  view;
    register GpSprtPrim* prim asm("a1");
    GpSprtTbl*           tbl;
    GpSprtRec*           recs;
    GpSprtCmd*           rec;
    u8*                  p;
    u32                  i;
    u8                   flags;

    sess          = (GameSessionFrom4*)&Game_Session->field_4;
    view          = Gp_GetViewIndex();
    prim          = Gp_SprtLists[Display_State.field_1f];
    Gp_SprtCursor = prim;
    tbl           = Gp_SprtTables[sess->field_3 - 1];
    recs          = tbl->field_0[sess->field_2 - 1];
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

typedef struct {
    s16  w;
    s16  h;
    s16  x0;
    s16  y0;
    u16  otz;
    u8   u0;
    u8   v0;
    u8   r0;
    u8   g0;
    u8   b0;
    u8   flags;
    byte pad[4];
} GpSprtElemFromW;
STATIC_ASSERT_SIZEOF(GpSprtElemFromW, 0x14);

void Gp_AllocSprtLists(void)
{
    GameSessionFrom4* sess;
    s32               view;
    u32               count;
    register s32      i asm("s3");
    GpSprtTbl*        tbl;
    GpSprtRec*        recs;
    GpSprtCmd*        rec;
    GpSprtElem*       elems;
    GpSprtElem*       elem;
    s32               bufIdx;
    GpTpageSprt*      buf[2];
    GpTpageSprt**     pbuf;
    GpTpageSprt**     p;
    GpTpageSprt**     cursor;
    GpTpageSprt*      dest;
    SPRT*             sprt;
    u32               tpage;

    sess  = (GameSessionFrom4*)&Game_Session->field_4;
    count = 0;
    view  = Gp_GetViewIndex();
    tbl   = Gp_SprtTables[sess->field_3 - 1];
    recs  = tbl->field_0[sess->field_2 - 1];
    rec   = recs[(u8)view - 1].field_4;
    elems = recs[(u8)view - 1].field_0;
    if (rec->field_0 != 0xFFFF) {
        do {
            count += rec->field_2;
            rec++;
        } while (rec->field_0 != 0xFFFF);
    }
    count *= 0x38;
    if (count == 0) {
        Gp_SprtLists[0] = NULL;
        return;
    }
    Gp_SprtLists[0] = Mem_Calloc(count, 1);
    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    count         >>= 1;
    count          += (u32)Gp_SprtLists[0];
    Gp_SprtLists[1] = (GpSprtPrim*)count;
    buf[0]          = (GpTpageSprt*)Gp_SprtLists[0];
    buf[1]          = (GpTpageSprt*)count;
    rec             = recs[(u8)view - 1].field_4;
    if (rec->field_0 != 0xFFFF) {
        pbuf = buf;
        do {
            if (rec->field_5 == 0) {
                bufIdx = 0;
                p      = pbuf;
                do {
                    i    = 0;
                    elem = elems + rec->field_0;
                    if (rec->field_2 != 0) {
                        register GpSprtElemFromW* mid asm("s1");

                        cursor = p;
                        TOUCH_REG(cursor);
                        mid = (GpSprtElemFromW*)&elem->w;
                        TOUCH_REG(mid);
                        do {
                            dest             = *cursor;
                            sprt             = &dest->sprt;
                            *(u32*)&sprt->r0 = 0x8000;
                            setlen(&dest->tpage, 1);
                            tpage = elem->tpage;
                            setlen(&dest->sprt, 4);
                            setcode(&dest->sprt, 0x65);
                            dest->tpage.code[0] = 0xE1000000 | (tpage & 0x9FF);
                            MargePrim(dest, sprt);
                            sprt->code      |= mid->flags;
                            *(u16*)&sprt->u0 = *(u16*)&mid->u0;
                            sprt->clut       = ((u16*)&mid->w)[-1];
                            *(u32*)&sprt->x0 = *(u32*)&mid->x0;
                            TOUCH_REG(i);
                            i++;
                            *(u32*)&sprt->w = *(u32*)&mid->w;
                            elem++;
                            (*cursor)++;
                            mid++;
                        } while (i < rec->field_2);
                    }
                    bufIdx++;
                    p++;
                } while (bufIdx < 2);
            }
            rec++;
        } while (rec->field_0 != 0xFFFF);
    }
}

void Gp_LinkRoomObjects(Task* task)
{
    GameSessionFrom4* sess;
    GpRoomObjRec*     recs;
    GpRoomObjRec*     rec;
    GpGridParams*     grid;
    GpObj4A*          list1;
    register GpObj4A* list2 asm("s1");
    GpObj3A*          list3;
    GpObj4A*          obj;
    GpObj3A*          obj3;
    GsCOORDINATE2*    coord;
    u8                flags;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    Gp_LoadStageView();
    Gp_GridParams = NULL;
    Gp_ClearObj4AList(1);
    Gp_ClearObj4AList(0);
    Gp_ClearObj3AList(0);
    recs = Gp_RoomObjTables[sess->field_3 - 1]->field_0[sess->field_2 - 1];
    if (recs != NULL) {
        rec   = (GpRoomObjRec*)(sess->field_1 * sizeof(GpRoomObjRec) + (s32)recs);
        recs  = rec - 1;
        grid  = rec[-1].field_0;
        list1 = recs->field_4;
        list2 = recs->field_8;
        list3 = recs->field_C;
        if (grid != NULL) {
            grid->field_0 = &Gfx_ViewCoord;
            Gp_GridParams = grid;
        }
        if (list1 != NULL) {
            coord = &Gfx_ViewCoord;
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
            coord = &Gfx_ViewCoord;
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
    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
}

s8 Gp_FindViewIndex(s32 arg0)
{
    s16               idx;
    GameSessionFrom4* sess;
    s16               limit;
    u8*               bytes;

    idx   = 0;
    sess  = (GameSessionFrom4*)&Game_Session->field_4;
    limit = *(s16*)&Gp_ViewCountTables[sess->field_3 - 1]->field_0[sess->field_2 - 1][sess->field_1 - 1];
    bytes = Gp_ViewIndexTables[sess->field_3 - 1]->field_0[sess->field_2 - 1][sess->field_1 - 1];
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

s32 Gp_ViewSprtCmdEmpty(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpSprtTbl**       tbl68;
    s32               i;
    GpViewIndexTbl*   tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;
    u8                idx;
    GpSprtTbl*        tbl2;
    GpSprtRec**       mid2;
    GpSprtRec*        recs;

    session = Game_Session;
    tbl68   = Gp_SprtTables;
    sess    = (GameSessionFrom4*)&session->field_4;
    i       = sess->field_3 - 1;
    tbl68   = &tbl68[i];
    tbl     = Gp_ViewIndexTables[i];
    mid     = tbl->field_0;
    inner   = mid[sess->field_2 - 1];
    bytes   = inner[sess->field_1 - 1];
    idx     = bytes[sess->field_0 - 1];
    tbl2    = *tbl68;
    mid2    = tbl2->field_0;
    recs    = mid2[sess->field_2 - 1];
    return recs[idx - 1].field_4->field_2 == 0;
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD024);

s32 Gp_GetViewIndex(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpViewIndexTbl*   tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;

    session = Game_Session;
    sess    = (GameSessionFrom4*)&session->field_4;
    tbl     = Gp_ViewIndexTables[sess->field_3 - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->field_2 - 1];
    bytes   = inner[sess->field_1 - 1];
    return bytes[sess->field_0 - 1];
}

void* Gp_GetViewSprtExtra(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpViewIndexTbl*   tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;
    u8                idx;
    GpSprtTbl*        tbl2;
    GpSprtRec**       mid2;
    GpSprtRec*        recs;

    session = Game_Session;
    sess    = (GameSessionFrom4*)&session->field_4;
    tbl     = Gp_ViewIndexTables[sess->field_3 - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->field_2 - 1];
    bytes   = inner[sess->field_1 - 1];
    idx     = bytes[sess->field_0 - 1];
    tbl2    = Gp_SprtTables[sess->field_3 - 1];
    mid2    = tbl2->field_0;
    recs    = mid2[sess->field_2 - 1];
    return recs[idx - 1].field_8;
}

void Gp_RoomObjState1(Task* task)
{
    if (task->spawnArg1 != (u8)Game_Session->field_4) {
        Gfx_ViewCoord.flg = 0;
        Gp_UpdateCoord(&Gfx_ViewCoord);
        task->spawnArg1 = (u8)Game_Session->field_4;
    }
    if (Game_Session->field_76 != 0) {
        Gp_LinkRoomObjects(task);
        Game_Session->field_76 = 0;
    }
    func_800AD024();
}

void Gp_LinkSprtCmd(GpSprtElem* arg0, GpSprtCmd* arg1)
{
    register u32  i asm("t0");
    GpSprtPrim*   prim;
    GpSprtElem*   elem;
    DisplayState* ds;
    u_long*       otBase;
    u32           mask;
    u32           maskHi;

    i = 0;
    if (Gp_SprtLists[0] == NULL) {
        return;
    }
    prim = Gp_SprtCursor;
    elem = arg0 + arg1->field_0;
    if (arg1->field_2 != 0) {
        ds     = &Display_State;
        otBase = Gpu_CurrentOt;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            if (arg1->field_4 == 0) {
                prim->tag = (prim->tag & maskHi) | (*(u_long*)(((((u32)elem->otz << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) & mask);
                *(u_long*)(((((u32)elem->otz << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) =
                    (*(u_long*)(((((u32)elem->otz << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) & maskHi) | ((u32)prim & mask);
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
    if (Game_Session->field_64 == 0) {
        funcs.funcs[task->state](task);
    } else {
        Display_State.field_100 = 0;
    }
}

void Gp_AllocSprtListsTask(Task* task)
{
    Gp_AllocSprtLists();
    Task_Kill(task);
}

void func_800AD5B8(Task* task)
{
    TaskFunc funcs[2] = { func_800AD620, func_800AD65C };

    if (Game_Session->field_64 == 0) {
        funcs[task->state](task);
    }
}

void func_800AD620(Task* task)
{
    s32 val;

    val = Gp_ViewSprtCmdEmpty();
    do {
        Display_State.field_100 = val;
    } while (0);
    task->state++;
}

void func_800AD65C(Task* task)
{
    DisplayState* ds;
    s32           val;

    ds = &Display_State;
    if ((ds->field_1e != 2) && (ds->field_104 == 0)) {
        Gp_LinkViewSprts();
    } else {
        val                     = Gp_ViewSprtCmdEmpty();
        Display_State.field_100 = val;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD6BC);

void Gp_SetupDirWarp(void)
{
    Task*             slot7;
    Task*             slot3;
    WipSysConfig*     cfg;
    GameActor*        actor;
    GameSessionFrom4* sess;
    GpWarpRec         rec;
    GpMsg3EE          msg;
    SVECTOR           pos;
    SVECTOR           pos2;
    s32               stage;
    s32               room;
    s16               ret;

    sess  = (GameSessionFrom4*)&Game_Session->field_4;
    stage = sess->field_3;
    room  = sess->field_2;
    slot7 = Game_GetPtrSlot(7);
    slot3 = Game_GetPtrSlot(3);
    cfg   = &Wip_SysConfig;
    actor = ((GpActorWork*)slot3)->actor;

    if (Game_Session->field_1 != 0) {
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
            msg.field_10 = 0;
            msg.field_14 = 0;
            msg.field_12 = (rec.field_0 + 0x800) & 0xFFF;
            if (rec.field_0 == 0x7800 || rec.field_0 == 0x7FFF) {
                pos.vx       = -0x5C1;
                pos.vy       = 0;
                pos.vz       = 0x9C1;
                msg.field_12 = Gp_YawToPosXZ((Task*)Gp_ActorSlots[0], (GpPosXZ*)&pos);
            } else if (rec.field_0 == 0x7FFE) {
                msg.field_12 = actor->field_52;
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
                if (D_80114CF0 != 0 && cfg->field_18 > 0) {
                    SndEvt_EnqueueType6(D_80114CF0, 0, 0);
                }
                return;
            }
            msg.field_10 = 0;
            msg.field_14 = 0;
            msg.field_12 = (rec.field_0 + 0x800) & 0xFFF;
            if (rec.field_0 == 0x7800 || rec.field_0 == 0x7FFF) {
                pos2.vx      = -0x5C1;
                pos2.vy      = 0;
                pos2.vz      = 0x9C1;
                msg.field_12 = Gp_YawToPosXZ((Task*)Gp_ActorSlots[0], (GpPosXZ*)&pos2);
            } else if (rec.field_0 == 0x7FFE) {
                msg.field_12 = actor->field_52;
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

    slot = Game_GetPtrSlot(3);
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
            D_801153F4 = 1;
        }
        Gp_DirPhase++;
    }
}

void Gp_CommitWarp(void)
{
    Task*             slot3;
    Task*             slot7;
    WipSysConfig*     cfg;
    GameSessionFrom4* sess;
    GpWarpRec         rec;
    GpSaveLoc*        loc;
    u8                fade;

    slot3 = Game_GetPtrSlot(3);
    cfg   = &Wip_SysConfig;
    slot7 = Game_GetPtrSlot(7);

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    rec  = Gp_WarpTables[sess->field_3 - 1][sess->field_2 - 1][(Gp_DirNibble >> 4) - 1];

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
        if (cfg->field_18 > 0) {
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

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
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

    slot = Game_GetPtrSlot(3);
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
            D_801153F4 = 1;
            Gp_DirPhase++;
        }
    }
}

void Gp_CommitDirWarp(void)
{
    Task*       slot;
    GpSaveLoc*  loc;
    McSaveData* save;

    slot = Game_GetPtrSlot(7);
    loc  = &Gp_WarpLoc;

    /* first two bytes as one halfword (field_1 cleared) */
    *(u16*)&Gp_WarpLoc = Gp_DirAlt;
    loc->field_2       = Gp_DirAltNibble & 0xF;
    loc->field_4       = 1;
    loc->field_3       = 1;
    loc->field_5       = 0;
    Gp_DispatchMsg(slot, 0x13EE, (s32)loc, (s32)loc);

    save          = &Mc_SaveData;
    save->field_6 = Gp_WarpLoc.field_0;
    save->field_8 = loc->field_2;
    save->field_5 = loc->field_3;
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
    if (Game_Session->field_1 == 0) {
        if (Gp_CapBusy() == 0) {
            if (Gp_DirNibble == 0xFF) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F0, Gp_DirByte, 0);
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
        Game_Session->field_13A = 0;
    }
}

void Gp_RunDirAction(void)
{
    void (*fns[2])(s32, s32) = { D_8017DA78, D_8017EF60 };

    if (Game_Session->field_1 != 0) {
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
    GameSessionFrom4*        sess;
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
    sess  = (GameSessionFrom4*)&Game_Session->field_4;
    if (arg0->field_0 != 0xFF) {
        tables = Gp_AreaTables;
        save   = &Mc_SaveData;
        rec    = arg0;
        idx    = *(volatile u8*)&rec->field_0;
        do {
            tbl         = tables[idx];
            key.field_3 = idx;
            temp        = rec->field_1;
            key.field_1 = 1;
            key.field_2 = temp;
            key.field_0 = sess->field_0;
            mask        = rec->field_3 & 0xF0;
            if (mask == 0) {
                goto set_apply;
            }
            mode = save->field_F;
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

void Gp_ApplyNewGameAreaFlags(void)
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

void Gp_RebuildAreaIdBits(void)
{
    GpAreaKey           key;
    GameSession*        gs;
    GameSessionFrom4*   sess;
    GpAreaRec*          tbl;
    GpAreaObj*          obj;
    GpAreaRec**         tables;
    register GpAreaKey* keyp asm("s4");
    s32                 flag;
    s32                 bits;
    register s32        i asm("s1");
    register s32        one asm("s0");
    register s32        count asm("s5");
    u8                  stage;

    gs          = Game_Session;
    sess        = (GameSessionFrom4*)&gs->field_4;
    stage       = sess->field_3;
    key.field_1 = 1;
    key.field_0 = 2;
    key.field_3 = stage;
    if (gs->field_7 - 1 < 5) {
        count = Gp_AreaIdCounts[sess->field_3 - 1];
        if (count > 0) {
            i      = 1;
            keyp   = &key;
            tables = Gp_AreaTables;
            one    = i;
            do {
                key.field_2 = i;
                tbl         = tables[keyp->field_3];
                flag        = 0;
                if (tbl != NULL) {
                    obj = tbl[keyp->field_2].field_4;
                    if (obj != NULL) {
                        flag = obj->field_1 & 4;
                        flag = flag != 0;
                    } else {
                        MOVE_ZERO(flag);
                    }
                }
                if (flag == one) {
                    if (Gp_GetAreaFlag2(&key) == one) {
                        if (key.field_2 < 0x21U) {
                            goto clear_lo;
                        }
                        goto clear_hi;
                    }
                    if (key.field_2 < 0x21U) {
                        Gp_AreaIdBits[0] |= one << (key.field_2 - 1);
                    } else {
                        bits = Gp_AreaIdBits[1] | (one << (key.field_2 - 0x21));
                        goto store_hi;
                    }
                } else if (key.field_2 < 0x21U) {
                clear_lo:
                    Gp_AreaIdBits[0] &= ~(one << (key.field_2 - 1));
                } else {
                clear_hi:
                    bits = Gp_AreaIdBits[1] & ~(one << (key.field_2 - 0x21));
                store_hi:
                    Gp_AreaIdBits[1] = bits;
                }
                i++;
            } while (count >= i);
        }
    }
}
