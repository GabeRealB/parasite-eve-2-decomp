#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u8             D_801153F1;
extern s32            D_8010CA28;
extern TaskDesc       D_8010CABC;
extern TmdListHead    D_80114B80;
extern TmdListHead    D_80114B88;
extern Task*          D_80114B90;
extern GsCOORDINATE2* D_80114B9C;
extern CVECTOR        D_80114BA4;
extern CVECTOR        D_80114BA8;
extern u8             D_80114BF0[];
extern char           D_80093870[]; // "Item"
extern s32            D_8005ED70;
extern s32            D_8005ED74;
extern GsCOORDINATE2  D_80070F10;
extern s16            D_80114C40;

void func_800A1634(s32 arg0, s32 arg1);
void func_800A4A2C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A7320(s16* arg0);
void func_800A6F38(GpEnemy* arg0, void* arg1);
s32  func_800A7B20(s32 arg0);
void func_8009939C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A82C0(GsCOORDINATE2* arg0, VECTOR* arg1);
void func_800A8864(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void func_800A9730(Task* task);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80097AC0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009850C);

void func_80098F58(GsCOORDINATE2* arg0)
{
    D_80114B9C = arg0;
    func_8009939C(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
}

void func_80098F98(GsCOORDINATE2* arg0, s32 arg1)
{
    if (arg0->sub == NULL) {
        D_80114B9C = arg0;
        func_8009939C(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
        func_800A8864(&D_80070F34, &arg0->workm, &arg0->coord);
    } else {
        func_8009939C(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, arg1);
    }
}

void* func_8009902C(Task* task, TmdSource* src)
{
    TmdListHead* node;
    TmdListHead* last;
    TmdListHead* list;

    node = (TmdListHead*)Tmd_Create(src, 0);
    if (node != NULL) {
        list            = &Tmd_List;
        last            = list->prev;
        node->next      = last->next;
        last->next      = (TmdObject*)node;
        node->prev      = last;
        list->prev      = node;
        task->extra     = node;
        task->spawnType = 1;
    }
    return node;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099098);

void* func_80099170(Task* task, TmdSource* src, s32 flags)
{
    TmdListHead* node;
    TmdListHead* last;
    TmdListHead* list;

    node = (TmdListHead*)Tmd_Create(src, flags);
    if (node != NULL) {
        list            = &Tmd_List;
        last            = list->prev;
        node->next      = last->next;
        last->next      = (TmdObject*)node;
        node->prev      = last;
        list->prev      = node;
        task->extra     = node;
        task->spawnType = 1;
    }
    return node;
}

void func_800991DC(TmdListHead* arg0)
{
    TmdListHead*  next;
    TmdListHead** pp;
    TmdListHead*  prev;

    next = (TmdListHead*)arg0->next;
    if (next == NULL) {
        pp = &Tmd_List.prev;
    } else {
        pp = &next->prev;
    }
    prev       = arg0->prev;
    *pp        = prev;
    prev->next = arg0->next;
}

void func_80099214(TmdObject* arg0)
{
    if (arg0->field_18 != NULL) {
        Mem_Free2(arg0->field_18, 1);
        arg0->field_18 = NULL;
    }
    Mem_Free(arg0);
}

void func_80099258(TmdListHead* arg0)
{
    TmdListHead*  next;
    TmdListHead** pp;
    TmdListHead*  prev;

    next = (TmdListHead*)arg0->next;
    if (next == NULL) {
        pp = &Tmd_ListAlt.prev;
    } else {
        pp = &next->prev;
    }
    prev       = arg0->prev;
    *pp        = prev;
    prev->next = arg0->next;
}

void func_80099290(void* arg0)
{
    Mem_Free(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800992B0);

void func_80099338(void)
{
    Task_CallExit(D_80114B90);
    Tmd_List    = D_80114B80;
    Tmd_ListAlt = D_80114B88;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009939C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009988C);

void func_80099958(void)
{
    func_8009850C(&Gpu_OtBuffers[Display_State.field_1f]);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099994);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099B94);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099D40);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099FF4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A348);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A57C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A804);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AA5C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AC58);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AF90);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009B2F4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009B500);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009BD00);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C024);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C414);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009CED0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D0DC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D388);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D518);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D718);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D900);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DB00);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DCB8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DE48);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E048);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E274);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E4A0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E770);

void func_8009EA50(s32 arg0)
{
    s32 temp;

    if (arg0 <= 0) {
        arg0 = 0;
        temp = 0x80;
    } else {
        if (arg0 >= 0x100) {
            arg0 = 0xFF;
        }
        temp = (0xFF - arg0) >> 1;
    }

    D_80114BA4.r = D_80114BA4.g = D_80114BA4.b = arg0;
    D_80114BA8.r = D_80114BA8.g = D_80114BA8.b = temp;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EAA4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EB84);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EC1C);

u32* func_8009ECC0(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            color = arg2[2];
            setlen(poly, 5);
            *(s32*)&poly->r0 = color;
            setcode(poly, 0x28);
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

u32* func_8009ED28(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            color = arg2[2];
            setlen(poly, 4);
            *(s32*)&poly->r0 = color;
            setcode(poly, 0x20);
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009ED90);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EE28);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EECC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EF64);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EFFC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F0A0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F144);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F1DC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F280);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F360);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F3F8);

u32* func_8009F49C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            color = arg2[2];
            setlen(poly, 5);
            *(s32*)&poly->r0 = color;
            setcode(poly, 0x28);
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F504(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            color = arg2[2];
            setlen(poly, 4);
            *(s32*)&poly->r0 = color;
            setcode(poly, 0x20);
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F56C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F670);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F708);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F824);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F8C8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F970);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FA24);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FB28);

u32* func_8009FC44(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->field_0;
    if (arg0->field_1C-- > 0) {
        stride = arg0->field_18;
        do {
            arg2  += stride;
            prims += 0x24;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = prims;
    return arg2;
}

u32* func_8009FC90(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->field_0;
    if (arg0->field_1C-- > 0) {
        stride = arg0->field_18;
        do {
            arg2  += stride;
            prims += 0x1C;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = prims;
    return arg2;
}

u32* func_8009FCDC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->field_0;
    if (arg0->field_1C-- > 0) {
        stride = arg0->field_18;
        do {
            arg2  += stride;
            prims += 0x14;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = prims;
    return arg2;
}

u32* func_8009FD28(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->field_0;
    if (arg0->field_1C-- > 0) {
        stride = arg0->field_18;
        do {
            arg2  += stride;
            prims += 0x18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = prims;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FD74);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FEDC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0094);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0504);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0718);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A087C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A110C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1558);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1634);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A18BC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1CD0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1F64);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2104);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2BE0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2F60);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A3AF0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A45F0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A4904);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A4A2C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5274);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5574);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A57B0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A63B4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6480);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6A9C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6F38);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A70A4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7320);

u8* func_800A746C(void)
{
    WipSysConfig* p;
    s32           cond;

    p = &Wip_SysConfig;
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        return Mc_SaveData.unknown_850;
    }
    return D_80114BF0;
}

s32 func_800A74C4(void)
{
    WipSysConfig* p;

    p = &Wip_SysConfig;
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
        return 0;
    }
    return p->field_26 == 4;
}

s32 func_800A7508(void)
{
    GpStateF0* p;

    p = &D_801153F0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        return 1;
    }
    return 0;
}

s32 func_800A7550(void)
{
    func_800A1634(1, 0);
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7574);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7600);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A76A4);

void func_800A7744(Task* arg0)
{
    func_800A7320(&arg0->killCountdown);
    arg0->spawnArg1++;
    if (arg0->spawnArg1 == 0x40) {
        if (Game_Session->field_128 == 3) {
            Display_State.field_104 = 1;
        }
        arg0->spawnArg1 = 0;
        arg0->state++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A77B4);

void func_800A7824(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 == 0) {
        func_800A4A2C(0, arg1, arg2, 5);
    }
}

void func_800A784C(void* arg0)
{
    GpLinkNode*  target;
    GpActorWork* work;
    GameActor*   actor;
    GpLinkNode*  node;

    work   = D_80115760[0];
    target = NULL;
    if (work != NULL) {
        actor = work->actor;
        if (actor != NULL) {
            target = actor->field_90C;
        }
        node = D_80115268;
        if (node != NULL) {
            do {
                if (node == target) {
                    if (!(node->field_4 & 1)) {
                        func_800A6F38((GpEnemy*)((u8*)node - OFFSET_OF(GpEnemy, node)), arg0);
                        return;
                    }
                }
                node = node->next;
            } while (node != NULL);
        }
    }
}

s32 func_800A78DC(void)
{
    return D_801153F1 == 0;
}

void func_800A78EC(void)
{
    func_800B065C(func_800A7B20(7) + 0x15);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7918);

void func_800A79F8(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawTitle(obj, D_80093870);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0) {
            obj->field_2E = 6;
        }
    }
}

void func_800A7A64(void)
{
    u8 state;

    state = D_801153F0.field_0;
    if ((state == 1) || (state == 3)) {
        if (Game_Session->field_126 == 0) {
            func_8010A1B0(1, 0xFF);
            func_800FC6C0();
            Display_State.field_12f = 0;
            Display_InitModeObj(&D_8010CABC, 1, 0, 0x102);
        }
    }
}

s32 func_800A7AE4(s32 arg0, s32 arg1)
{
    return (arg0 / 3) * 16 + (arg0 % 3) * 4 + arg1 + 0x300;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7B20);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7BBC);

s32 func_800A7CB0(void)
{
    GpStateF0* p;
    s32        cond;

    p = &D_801153F0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return 0;
    }
    return 0;
}

void func_800A7CF4(u8 arg0)
{
    GpStateF0* p;
    s32        cond;

    p = &D_801153F0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        func_800B065C(arg0);
    }
}

s32 func_800A7D54(void)
{
    GpStateF0* p;
    s32        cond;

    p = &D_801153F0;
    if ((p->field_0 == 1 && p->field_6 != 0) || p->field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return CdCmd_IsIdle() & 0xFFFF;
    }
    return 1;
}

void func_800A7DB8(s32 arg0)
{
    if (!(D_80114C08.field_6 & 1)) {
        D_80114C08.field_E = arg0;
    }
}

void func_800A7DE0(void)
{
    GpStateC08* p;

    CdCmd_EnqueueLoadFile(0, 0, 4);
    p = &D_80114C08;
    if (p->field_A >= 2) {
        p->field_3 = 2;
    }
    p->field_E = 0;
    p->field_A = 0;
    D_80115768 = 0;
    D_801153F4 = 0;
    p->field_7 = 0;
    p->field_8 = 0;
}

void func_800A7E4C(void)
{
    D_8010CA28 = 5;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7E5C);

void func_800A7F24(void)
{
}

s32 func_800A7F2C(s32 arg0)
{
    return arg0 - 0x10;
}

s32 func_800A7F34(s32 arg0)
{
    WipSysConfig* p;
    s32           ret;

    p   = &Wip_SysConfig;
    ret = 1;
    if (p->field_1c >= arg0) {
        p->field_1c -= arg0;
    } else {
        p->field_1c = 0;
        ret         = 0;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7F6C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A82C0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8654);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8724);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8864);

s32 func_800A8A1C(s32 arg0)
{
    return Task_Spawn(0, 0xF, 0, arg0) != NULL;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8A48);

void func_800A8B14(void)
{
    MATRIX*                 m;
    volatile GsCOORDINATE2* c1;
    GsCOORDINATE2*          c2;
    GsCOORDINATE2*          c3;
    s32                     one;

    c1             = &D_80070E90;
    one            = ONE;
    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = one;

    *(volatile s32*)&D_80070E44 = one;
    m                           = &D_80070E44;
    c2                          = (GsCOORDINATE2*)((u8*)m - OFFSET_OF(GsCOORDINATE2, coord));
    *(s32*)&m->m[1][1]          = one;
    m->m[2][2]                  = one;

    c3                 = &D_80070F10;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[2][0] = 0;
    c3->coord.t[0]     = 0;
    c3->coord.t[1]     = 0;
    c3->coord.t[2]     = 0;
    c1->flg            = 0;
    c2->flg            = 0;
    c3->flg            = 0;
}

void func_800A8B6C(void)
{
    GameSessionFrom4* sess;
    GpCb2CTbl*        tbl;
    GpCb2CRec*        recs;
    GpCb2CRec*        rec;
    u8                idx;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    tbl  = D_8010CB2C[sess->field_3 - 1];
    recs = tbl->field_0[sess->field_2 - 1];
    idx  = func_800AD284();
    rec  = (GpCb2CRec*)(idx * sizeof(GpCb2CRec) + (s32)recs);
    Task_Spawn(0, 0xF, 0, (s32)(rec - 1));
    Task_Spawn(0, 0x17, 0, 0);
}

GpCb2CRec* func_800A8C08(GameSessionFrom4* arg0)
{
    GpCb2CTbl* tbl;
    GpCb2CRec* recs;
    u8         idx;

    tbl  = D_8010CB2C[arg0->field_3 - 1];
    recs = tbl->field_0[arg0->field_2 - 1];
    idx  = func_800AD284();
    return &recs[idx - 1];
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8C74);

void func_800A8D5C(void)
{
    VECTOR        vec;
    GsCOORDINATE2 coord;
    s32           one;
    MATRIX*       m;

    vec.vx                      = 0;
    vec.vy                      = 0;
    vec.vz                      = ONE;
    one                         = ONE;
    m                           = &coord.coord;
    coord.sub                   = &D_80070F10;
    *(s32*)&coord.coord         = one;
    *(s32*)&coord.coord.m[0][2] = 0;
    *(s32*)&m->m[1][1]          = one;
    *(s32*)&coord.coord.m[2][0] = 0;
    m->m[2][2]                  = one;
    coord.coord.t[0]            = 0;
    coord.coord.t[1]            = 0;
    coord.coord.t[2]            = 0;
    func_800A82C0(&coord, &vec);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8DC0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8E8C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A9010);

void func_800A91CC(Task* task)
{
    CdCmdQueue*   q;
    s32           i;
    s32           raw;
    s32           target;
    u8            param;
    register s32  type2 asm("v0");
    FsFolderSlot* table;
    FsFolderSlot* slot;

    q = &CdCmd_Queue;
    if (CdCmd_IsIdle() & 0xFFFF) {
        Mem_Set(&q->field_40, 0, 0x10);
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
        CdCmd_SelectMdecBuffer();
        if (D_80114C40 >= 0) {
            task->state++;
            param = *(u8*)&D_80114C40;
            CdCmd_EnqueueReplace(0x61, 0, &param);
            CdCmd_CommitReplace();
            task->killCountdown = 0;
        } else {
            if ((s16)CdCmd_CommitReplace() >= 0) {
                task->state += 2;
            } else {
                task->state = -1;
                func_800A9730(task);
            }
        }
    }
}
