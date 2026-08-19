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

void func_800A9730(Task* task);
s32  func_800A9E44(void);
s32  func_800AA120(void);
void func_800AA548(s32 arg0);
void func_800AE7AC(void);
void func_800AD024(void);
void func_800AD620(Task* task);
void func_800AD65C(Task* task);
void D_8017DA78(s32 arg0, s32 arg1);
void D_8017EF60(s32 arg0, s32 arg1);
void func_80724748(GameSessionFrom4* arg0);
void func_80724E2C(void);

extern TaskDesc       D_80183824[];
extern TaskFuncTable6 D_800938B4;
extern TaskFuncTable3 D_80093918;
extern TaskFuncTable8 D_80093924;
extern TaskFuncTable3 D_80093944;
extern u16            D_80114CD2;
extern u16            D_80114CD4;
extern u16            D_80114CD6;
extern u8             D_80114CD8;
extern u8             D_80114CD9;
extern u8             D_80114CDA;
extern u8             D_80114CDB;
extern u8             D_80114CDC;
extern u8             D_80114CDD;
extern s32            D_80114CF0;
extern s16            D_80114CF4;
extern u16            D_80114CF6;
extern u8             D_80114CF8;

void func_800A9310(void)
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
            slot      = func_800BAFE0(item + 0x7F);
            asm volatile("" : "+r"(slot));
            attach = slot->field_2;
            if (attach != 0 && attach != 0xFF) {
                temp = attach;
                asm volatile("" : "+r"(temp));
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

void func_800A954C(Task* task)
{
    GameSessionFrom4* sess;
    u8                param1[8];
    u8                param2[8];

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = sess->field_3;
        param1[2] = sess->field_2;
        param1[0] = func_800AD284();
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        task->state++;
    }
}

void func_800A95E0(Task* task)
{
    if (CdCmd_Queue.field_1FA != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        func_800A9730(task);
    }
}

void func_800A9630(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = -2;
        func_800A9730(task);
    }
}

void func_800A966C(Task* task)
{
    if (CdCmd_Queue.field_1FE == 0xFF) {
        task->state = -1;
        func_800A9730(task);
    }
}

void func_800A96A0(void)
{
    s32           raw;
    s32           i;
    s32           target;
    register s32  type2 asm("v0");
    FsFolderSlot* table;
    FsFolderSlot* slot;

    raw    = func_800AD284();
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

void func_800A9730(Task* task)
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

void func_800A97DC(Task* task)
{
    TaskFuncTable6 sp;

    sp = D_800938B4;
    Pad_SetCooldown(0);
    if (task->state < 0) {
        func_800A9730(task);
    } else {
        sp.funcs[task->state](task);
    }
}

void func_800A987C(void)
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
    func_800A8DC0(2);
    Game_Session->field_4D = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

void func_800A990C(s32 arg0)
{
    Task* slot;

    slot                  = Game_GetPtrSlot(1);
    Mc_SaveData.field_4   = arg0;
    Game_Session->field_4 = arg0;
    slot->spawnArg1       = (u8)arg0;
    Pad_SetCooldown(0);
    func_800A8DC0(1);
    Display_State.field_100 = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void func_800A9980(Task* task)
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

void func_800A99E0(Task* task)
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
    func_800ACAA8();
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

void func_800A9A40(s32 arg0)
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

    raw    = func_800AD284();
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
    param1[0] = func_800AD284();
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

void func_800A9B3C(s32 arg0)
{
    u8     param1[8];
    u8     param2[8];
    GpTbl5 table;

    table = D_800938CC;
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

void func_800A9BE4(void)
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
    func_800A9310();
}

void func_800A9C50(void)
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

void func_800A9CBC(s32 arg0, s32 arg1)
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

void func_800A9DF0(Task* task)
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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9E44);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA120);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AA548);

void func_800AAA68(Task* arg0)
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

void func_800AABB0(Task* task)
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

            func_800A9B3C(0);
            func_800A9BE4();
            sess            = Game_Session;
            sess->field_11C = save->field_22;
            sess->field_11E = Wip_SysConfig.field_26;
        }
        func_800A78EC();
        task->state++;
    }
    color = 8;
    ds    = &Display_State;
    asm("lui %0, %%hi(CdCmd_Queue)" : "=r"(qhi) : "r"(color), "r"(ds));
    buf  = ds->field_114;
    tile = &D_80114C80[buf];
    asm("" : : "r"(qhi), "r"(tile));
    dr     = &D_80114CA0[buf];
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

void func_800AADDC(Task* task)
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
    tile   = &D_80114C80[buf];
    dr     = &D_80114CA0[buf];
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
            func_800A9C50();
            Game_Session->field_78 = Game_Session->field_7;
        }
        task->state++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AAF70);

void func_800AB1C8(Task* task)
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
    tile = &D_80114C80[buf];
    asm("" : : "r"(qhi), "r"(tile));
    dr     = &D_80114CA0[buf];
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
        flag = func_800ABA4C();
        if ((u8)flag) {
            Game_Session->field_124 = flag;
            save                    = &Mc_SaveData;
            func_800A9CBC((u8)save->field_13, (u8)save->field_5C7);
        }
        task->state++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB3A8);

void func_800AB5F4(Task* task)
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
    tile = &D_80114C80[buf];
    asm("" : : "r"(qhi), "r"(tile));
    dr     = &D_80114CA0[buf];
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
                    D_80114C60 = 0;
                    D_80114C74++;
                }
                done = 0;
                break;
            case 2:
                done = func_800A9E44() & 0xFFFF;
                if (done) {
                    done = 1;
                    break;
                }
            default:
                done = 0;
                break;
        }

        if (done & 0xFFFF) {
            func_800E0294();
            Tmd_InitLists();
            ds2 = &Display_State;
            func_8009850C(&Gpu_OtBuffers[ds2->field_1f]);
            task->state++;
            if (Mc_SaveData.field_5C3 != 0) {
                ds2->dispEnv[1].isinter = 1;
                ds2->dispEnv[0].isinter = 1;
            }
        }
    }
}

void func_800AB828(Task* task)
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
    tile  = &D_80114C80[buf];
    dr    = &D_80114CA0[buf];
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

void func_800AB980(GameSessionFrom4* arg0)
{
    McSaveData*  save;
    GpFlagBank** banks;
    GpFlagBank*  bank;

    banks = D_80060A30;
    save  = &Mc_SaveData;
    if ((save->field_10 & 1) == 0) {
        save->field_10 = 1;
        func_800E4080();
        func_800AE7AC();
        save->field_6CA = 0x64;
        save->field_6C8 = 0x64;
        func_800B8014();
    }
    if ((((s8)save->field_10 >> arg0->field_3) & 1) == 0) {
        bank             = banks[arg0->field_3];
        bank->field_4[0] = 0;
        bank->field_4[1] = 0;
        func_800BAB64(arg0->field_3);
        if (Display_State.field_112 != 0) {
            func_80724748(arg0);
        }
    }
}

s32 func_800ABA4C(void)
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

void func_800ABCC8(void)
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

void func_800ABE68(GpActorArg* arg0, u16* arg1)
{
    McSaveData* save;
    s32         field;

    save  = &Mc_SaveData;
    field = save->field_13;
    if (field != 0) {
        if (field == 2) {
            func_8010BAC8(arg0, save->field_13, GameFlag_GetNibble(0x4B), arg1);
        } else {
            func_8010BAC8(arg0, field, 0, arg1);
        }
    }
}

void func_800ABEF8(s32 arg0)
{
    GpFlagBank* bank;

    bank             = D_80060A30[arg0];
    bank->field_4[0] = 0;
    bank->field_4[1] = 0;
}

void func_800ABF1C(GpAreaKey* arg0)
{
    McSaveData* save;
    GpFlagBank* bank;
    s32         which;
    s32         bit;
    s32         mask;
    s32         flags;

    bank = D_80060A30[arg0->field_3];
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
        func_800B5D44(arg0);
    }
}

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

void func_800AC008(Task* task)
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

void func_800AC058(Task* task)
{
    SndBank_SetEnableFlags(0, 0x40000000);
    if (Game_Session->field_0 != 0) {
        Task_Kill(task);
        return;
    }
    if ((task->spawnArg1 & 0x10) == 0) {
        if (D_801153F0.field_0 == 2) {
            D_801153F0.field_0 = 3;
        }
        func_800A7A64();
    }
    task->state++;
}

void func_800AC0F0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_80093918;
    Pad_SetCooldown(0);
    *(volatile u8*)&D_801153F4 = 1;
    sp.funcs[((volatile Task*)task)->state](task);
}

void func_800AC164(Task* task)
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

void func_800AC25C(Task* task)
{
    TaskFuncTable8 sp;
    DisplayState*  ds;

    sp = D_80093924;
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

void func_800AC344(Task* task)
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

s32 func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
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

void func_800AC4D8(Task* task)
{
    GameSessionFrom4* sess;
    GpCb7CRec*        recs;
    GpCb7CRec*        rec;
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
    recs = D_8010CB7C[sess->field_3 - 1]->field_0[sess->field_2 - 1];
    if (recs != NULL) {
        rec   = (GpCb7CRec*)(sess->field_1 * sizeof(GpCb7CRec) + (s32)recs);
        recs  = rec - 1;
        grid  = rec[-1].field_0;
        list1 = recs->field_4;
        list2 = recs->field_8;
        list3 = recs->field_C;
        if (grid != NULL) {
            grid->field_0 = &D_80070F10;
            D_80115448    = grid;
        }
        if (list1 != NULL) {
            coord = &D_80070F10;
            obj   = list1;
            do {
                obj->field_8 = coord;
                func_800E1688(1, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                asm volatile("" : "+r"(obj));
                obj++;
            } while (!(flags & 0x80));
        }
        if (list2 != NULL) {
            coord = &D_80070F10;
            obj   = list2;
            do {
                obj->field_8 = coord;
                func_800E1688(0, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                asm volatile("" : "+r"(obj));
                obj++;
            } while (!(flags & 0x80));
        }
        if (list3 != NULL) {
            obj3 = list3;
            do {
                func_800E17B4(0, obj3);
                flags          = obj3->field_3A | 0x40;
                obj3->field_3A = flags;
                asm volatile("" : "+r"(obj3));
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

void func_800AC688(void)
{
    GameSessionFrom4* sess;
    s32               view;
    DisplayState*     ds;
    GpPrim1C**        table;
    GpCb68Tbl*        tbl;
    GpCb68Rec*        recs;
    GpCb68Obj*        rec;
    GpCb68Elem*       base;

    sess       = (GameSessionFrom4*)&Game_Session->field_4;
    view       = func_800AD284();
    table      = D_8010CAE8;
    ds         = &Display_State;
    D_80114CC8 = table[ds->field_1f];
    tbl        = D_8010CB68[sess->field_3 - 1];
    recs       = tbl->field_0[sess->field_2 - 1];
    rec        = recs[(u8)view - 1].field_4;
    base       = recs[(u8)view - 1].field_0;
    if (rec->field_2 == 0) {
        rec++;
    } else {
        ds->field_100 = 0;
    }
    if (rec->field_0 != 0xFFFF) {
        do {
            if (rec->field_5 == 0) {
                func_800AD410(base, rec);
            }
            rec++;
        } while (rec->field_0 != 0xFFFF);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC790);

void func_800AC960(s32 arg0)
{
    GameSessionFrom4*  sess;
    s32                view;
    register GpPrim1C* prim asm("a1");
    GpCb68Tbl*         tbl;
    GpCb68Rec*         recs;
    GpCb68Obj*         rec;
    u8*                p;
    u32                i;
    u8                 flags;

    sess       = (GameSessionFrom4*)&Game_Session->field_4;
    view       = func_800AD284();
    prim       = D_8010CAE8[Display_State.field_1f];
    D_80114CC8 = prim;
    tbl        = D_8010CB68[sess->field_3 - 1];
    recs       = tbl->field_0[sess->field_2 - 1];
    rec        = recs[(u8)view - 1].field_4;
    if (rec->field_0 != 0xFFFF) {
        do {
            if (rec->field_5 == 0) {
                if (D_8010CAE8[0] != NULL) {
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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACAA8);

void func_800ACD2C(Task* task)
{
    GameSessionFrom4* sess;
    GpCb7CRec*        recs;
    GpCb7CRec*        rec;
    GpGridParams*     grid;
    GpObj4A*          list1;
    register GpObj4A* list2 asm("s1");
    GpObj3A*          list3;
    GpObj4A*          obj;
    GpObj3A*          obj3;
    GsCOORDINATE2*    coord;
    u8                flags;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    func_800A8724();
    D_80115448 = NULL;
    func_800E1758(1);
    func_800E1758(0);
    func_800E1884(0);
    recs = D_8010CB7C[sess->field_3 - 1]->field_0[sess->field_2 - 1];
    if (recs != NULL) {
        rec   = (GpCb7CRec*)(sess->field_1 * sizeof(GpCb7CRec) + (s32)recs);
        recs  = rec - 1;
        grid  = rec[-1].field_0;
        list1 = recs->field_4;
        list2 = recs->field_8;
        list3 = recs->field_C;
        if (grid != NULL) {
            grid->field_0 = &D_80070F10;
            D_80115448    = grid;
        }
        if (list1 != NULL) {
            coord = &D_80070F10;
            obj   = list1;
            do {
                obj->field_8 = coord;
                func_800E1688(1, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                asm volatile("" : "+r"(obj));
                obj++;
            } while (!(flags & 0x80));
        }
        if (list2 != NULL) {
            coord = &D_80070F10;
            obj   = list2;
            do {
                obj->field_8 = coord;
                func_800E1688(0, obj);
                flags         = obj->field_4A | 0x40;
                obj->field_4A = flags;
                asm volatile("" : "+r"(obj));
                obj++;
            } while (!(flags & 0x80));
        }
        if (list3 != NULL) {
            obj3 = list3;
            do {
                func_800E17B4(0, obj3);
                flags          = obj3->field_3A | 0x40;
                obj3->field_3A = flags;
                asm volatile("" : "+r"(obj3));
                obj3++;
            } while (!(flags & 0x80));
        }
    }
    D_80070F10.flg = 0;
    func_80098F58(&D_80070F10);
}

s8 func_800ACEBC(s32 arg0)
{
    s16               idx;
    GameSessionFrom4* sess;
    s16               limit;
    u8*               bytes;

    idx   = 0;
    sess  = (GameSessionFrom4*)&Game_Session->field_4;
    limit = *(s16*)&D_8010CB40[sess->field_3 - 1]->field_0[sess->field_2 - 1][sess->field_1 - 1];
    bytes = D_8010CB54[sess->field_3 - 1]->field_0[sess->field_2 - 1][sess->field_1 - 1];
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

s32 func_800ACF8C(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpCb68Tbl**       tbl68;
    s32               i;
    GpCb54Tbl*        tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;
    u8                idx;
    GpCb68Tbl*        tbl2;
    GpCb68Rec**       mid2;
    GpCb68Rec*        recs;

    session = Game_Session;
    tbl68   = D_8010CB68;
    sess    = (GameSessionFrom4*)&session->field_4;
    i       = sess->field_3 - 1;
    tbl68   = &tbl68[i];
    tbl     = D_8010CB54[i];
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

s32 func_800AD284(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpCb54Tbl*        tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;

    session = Game_Session;
    sess    = (GameSessionFrom4*)&session->field_4;
    tbl     = D_8010CB54[sess->field_3 - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->field_2 - 1];
    bytes   = inner[sess->field_1 - 1];
    return bytes[sess->field_0 - 1];
}

void* func_800AD2E8(void)
{
    GameSession*      session;
    GameSessionFrom4* sess;
    GpCb54Tbl*        tbl;
    u8***             mid;
    u8**              inner;
    u8*               bytes;
    u8                idx;
    GpCb68Tbl*        tbl2;
    GpCb68Rec**       mid2;
    GpCb68Rec*        recs;

    session = Game_Session;
    sess    = (GameSessionFrom4*)&session->field_4;
    tbl     = D_8010CB54[sess->field_3 - 1];
    mid     = tbl->field_0;
    inner   = mid[sess->field_2 - 1];
    bytes   = inner[sess->field_1 - 1];
    idx     = bytes[sess->field_0 - 1];
    tbl2    = D_8010CB68[sess->field_3 - 1];
    mid2    = tbl2->field_0;
    recs    = mid2[sess->field_2 - 1];
    return recs[idx - 1].field_8;
}

void func_800AD378(Task* task)
{
    if (task->spawnArg1 != (u8)Game_Session->field_4) {
        D_80070F10.flg = 0;
        func_80098F58(&D_80070F10);
        task->spawnArg1 = (u8)Game_Session->field_4;
    }
    if (Game_Session->field_76 != 0) {
        func_800ACD2C(task);
        Game_Session->field_76 = 0;
    }
    func_800AD024();
}

void func_800AD410(GpCb68Elem* arg0, GpCb68Obj* arg1)
{
    register u32  i asm("t0");
    GpPrim1C*     prim;
    GpCb68Elem*   elem;
    DisplayState* ds;
    u_long*       otBase;
    u32           mask;
    u32           maskHi;

    i = 0;
    if (D_8010CAE8[0] == NULL) {
        return;
    }
    prim = D_80114CC8;
    elem = arg0 + arg1->field_0;
    if (arg1->field_2 != 0) {
        ds     = &Display_State;
        otBase = Gpu_CurrentOt;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            if (arg1->field_4 == 0) {
                prim->tag = (prim->tag & maskHi) | (*(u_long*)(((((u32)elem->field_C << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) & mask);
                *(u_long*)(((((u32)elem->field_C << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) =
                    (*(u_long*)(((((u32)elem->field_C << ds->field_128) >> 2) & 0xFFC) + (s32)otBase) & maskHi) | ((u32)prim & mask);
            }
            prim++;
            i++;
            elem++;
        } while (i < arg1->field_2);
    }
    D_80114CC8 = prim;
}

void func_800AD50C(Task* task)
{
    TaskFuncTable3 funcs;

    funcs = D_80093944;
    if (Game_Session->field_64 == 0) {
        funcs.funcs[task->state](task);
    } else {
        Display_State.field_100 = 0;
    }
}

void func_800AD58C(Task* task)
{
    func_800ACAA8();
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

    val = func_800ACF8C();
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
        func_800AC688();
    } else {
        val                     = func_800ACF8C();
        Display_State.field_100 = val;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD6BC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ADA04);

void func_800ADE74(void)
{
    void* slot;
    u8    fade;

    slot = Game_GetPtrSlot(3);
    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
        D_80114CF6 += 0x1E;
        if ((s16)D_80114CF6 >= 0x100) {
            D_80114CF6 = 0xFF;
        }
    }
    if (func_800AC464(slot, 0x3F0, 0, 0) == 0) {
        if (D_80114CF4 != 0) {
            D_801153F4 = 1;
        }
        D_80114CD6++;
    }
}

void func_800ADF3C(void)
{
    Task*             slot3;
    Task*             slot7;
    WipSysConfig*     cfg;
    GameSessionFrom4* sess;
    GpCb90Rec         rec;
    GpSaveLoc*        loc;
    u8                fade;

    slot3 = Game_GetPtrSlot(3);
    cfg   = &Wip_SysConfig;
    slot7 = Game_GetPtrSlot(7);

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    rec  = D_8010CB90[sess->field_3 - 1][sess->field_2 - 1][(D_80114CD9 >> 4) - 1];

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
        D_80114CF6 += 0x1E;
        if ((s16)D_80114CF6 >= 0x100) {
            D_80114CF6 = 0xFF;
        }
    }

    loc                = &D_80114CE8;
    loc->field_4       = 1;
    loc->field_3       = 1;
    loc->field_5       = 0;
    *(u16*)&D_80114CE8 = D_80114CD8;
    loc->field_2       = D_80114CD9 & 0xF;
    loc->field_6       = rec.field_36;
    func_800AC464(slot7, 0x13EE, (s32)loc, (s32)loc);

    if (D_80114CF0 != 0) {
        if (cfg->field_18 > 0) {
            SndEvt_EnqueueType6(D_80114CF0, 0, 0);
        }
    }

    if (D_80114CF4 == 0) {
        func_800AC464(slot3, 0x3F1, 0, 0);
        D_80114CF8    = 0;
        D_80114CD9    = 0;
        D_80114CD8    = 0;
        D_80114CD2    = 0;
        cfg->field_24 = 0;
    } else {
        D_80114CD6++;
    }
}

void func_800AE150(void)
{
    u8 fade;

    if (*(s16*)&D_80114CF6 != 0) {
        fade = *(u8*)&D_80114CF6;
        Fade_DrawOverlay(fade, fade, fade, 2);
        D_80114CF6 += 0x1E;
        if ((s16)D_80114CF6 >= 0x100) {
            D_80114CF6 = 0xFF;
        }
    }
    if (D_80114CF0 == 0 || SndVoice_HasActiveId(D_80114CF0) == 0) {
        D_80114CD6++;
    }
}

void func_800AE1F0(void)
{
    Task*      slot;
    GameActor* actor;
    u8         flags;
    s32        facing;
    u8*        row;

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    flags = D_80114CD8;
    if (flags & 0x80) {
        facing = actor->field_82;
        if (D_80114CD2 & 0x100) {
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
    if (func_800AC464(slot, 0x3F0, 0, 0) == 0) {
        func_800AC464(slot, 0x3F1, 0, 0);
        D_80114CF8 = 0;
        D_80114CD9 = 0;
        D_80114CD8 = 0;
        D_80114CD2 = 0;
        D_80114CDB = 0;
        D_80114CDA = 0;
        D_80114CD4 = 0;
        D_80114CDD = 0;
    } else if (func_800E1BF0(&D_80114CD4, &D_80114CDA, &D_80114CDB)) {
        if ((u8)D_80114CD4 == 0) {
            D_801153F4 = 1;
            D_80114CD6++;
        }
    }
}

void func_800AE36C(void)
{
    Task*       slot;
    GpSaveLoc*  loc;
    McSaveData* save;

    slot = Game_GetPtrSlot(7);
    loc  = &D_80114CE8;

    /* first two bytes as one halfword (field_1 cleared) */
    *(u16*)&D_80114CE8 = D_80114CDA;
    loc->field_2       = D_80114CDB & 0xF;
    loc->field_4       = 1;
    loc->field_3       = 1;
    loc->field_5       = 0;
    func_800AC464(slot, 0x13EE, (s32)loc, (s32)loc);

    save          = &Mc_SaveData;
    save->field_6 = D_80114CE8.field_0;
    save->field_8 = loc->field_2;
    save->field_5 = loc->field_3;
    Task_Spawn(0, 0x11, 0, 0);

    D_80114CDB = 0;
    D_80114CDA = 0;
    D_80114CF8 = 0;
    D_80114CD9 = 0;
    D_80114CD8 = 0;
    D_80114CD2 = 0;
    D_80114CD4 = 0;
    D_80114CDD = 0;
}

void func_800AE45C(void)
{
    if (Game_Session->field_1 == 0) {
        if (func_800E6CE0() == 0) {
            if (D_80114CD9 == 0xFF) {
                func_800AC464(Game_GetPtrSlot(7), 0x13F0, D_80114CD8, 0);
            } else {
                func_800E3D8C(D_80114CD8, D_80114CD9);
            }
        }
    }
    D_80114CF8 = 0;
    D_80114CD9 = 0;
    D_80114CD8 = 0;
    D_80114CD2 = 0;
    D_80114CDB = 0;
    D_80114CDA = 0;
    D_80114CD4 = 0;
    if (D_80114CDC == 0) {
        Game_Session->field_13A = 0;
    }
}

void func_800AE53C(void)
{
    void (*fns[2])(s32, s32) = { D_8017DA78, D_8017EF60 };

    if (Game_Session->field_1 != 0) {
        D_80114CF8 = 0;
        D_80114CD9 = 0;
        D_80114CD8 = 0;
        D_80114CD2 = 0;
        D_80114CDB = 0;
        D_80114CDA = 0;
        D_80114CD4 = 0;
    } else {
        fns[(D_80114CD2 >> 8) & 0x7F](D_80114CD8, D_80114CD9);
        D_80114CF8 = 0;
        D_80114CD9 = 0;
        D_80114CD8 = 0;
        D_80114CD2 = 0;
        D_80114CDB = 0;
        D_80114CDA = 0;
        D_80114CD4 = 0;
    }
}

void func_800AE62C(GpAreaApplyRec* arg0)
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
        tables = D_8010CBCC;
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
                func_800B5B30(&key, rec->field_2, 1);
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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE7AC);

void func_800AE9B0(void)
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
        count = D_8010CAF0[sess->field_3 - 1];
        if (count > 0) {
            i      = 1;
            keyp   = &key;
            tables = D_8010CBCC;
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
                        asm volatile("" : "=r"(flag) : "0"(0));
                    }
                }
                if (flag == one) {
                    if (func_800B59A8(&key) == one) {
                        if (key.field_2 < 0x21U) {
                            goto clear_lo;
                        }
                        goto clear_hi;
                    }
                    if (key.field_2 < 0x21U) {
                        D_80114D00[0] |= one << (key.field_2 - 1);
                    } else {
                        bits = D_80114D00[1] | (one << (key.field_2 - 0x21));
                        goto store_hi;
                    }
                } else if (key.field_2 < 0x21U) {
                clear_lo:
                    D_80114D00[0] &= ~(one << (key.field_2 - 1));
                } else {
                clear_hi:
                    bits = D_80114D00[1] & ~(one << (key.field_2 - 0x21));
                store_hi:
                    D_80114D00[1] = bits;
                }
                i++;
            } while (count >= i);
        }
    }
}
