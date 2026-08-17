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
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "main/wipsys.h"

void func_800A9310(void);
void func_800A9730(Task* task);
void func_800AA548(s32 arg0);
void func_800AE7AC(void);
void func_800ACD2C(Task* task);
void func_800AD024(void);
void func_800AD410(void* arg0, GpCb68Obj* arg1);
void func_800AD620(Task* task);
void func_800AD65C(Task* task);
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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9310);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800A9CBC);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AABB0);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AADDC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AAF70);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB1C8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB3A8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB5F4);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AB828);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABA4C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ABCC8);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC4D8);

void func_800AC688(void)
{
    GameSessionFrom4* sess;
    s32               view;
    DisplayState*     ds;
    void**            table;
    GpCb68Tbl*        tbl;
    GpCb68Rec*        recs;
    GpCb68Obj*        rec;
    void*             base;

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AC960);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACAA8);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ACD2C);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AD410);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800ADF3C);

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

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE53C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE62C);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE7AC);

INCLUDE_ASM("gameplay/nonmatchings/D4", func_800AE9B0);
