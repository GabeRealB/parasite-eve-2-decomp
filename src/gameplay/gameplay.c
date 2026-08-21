#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u8             D_801153F1;
extern u8             D_8010CA08[]; // "Item obtained!"
extern u8             D_8010CA18[]; // "Bonus item!!"
extern s32            D_8010CA28;
extern GpItemScan     D_8010CA2C;
extern UiObjectDesc   D_8010CA40;
extern UiObjectDesc   D_8010CA78[];
extern UiObjectDesc   D_8010D6D8;
extern UiObjectDesc   D_80185000;
extern TaskDesc       D_8010CABC;
extern TaskDesc       D_8010D1FC;
extern TmdListHead    D_80114B80;
extern s32            D_80114A24;
extern s32            D_80114A34;
extern u8             D_80062734;
extern u16            D_8007A39C;
extern TmdListHead    D_80114B88;
extern Task*          D_80114B90;
extern GsCOORDINATE2* D_80114B9C;
extern CVECTOR        D_80114BA4;
extern CVECTOR        D_80114BA8;
extern u8             D_80114BF0[];
extern TaskFuncTable6 D_80093830;
extern s32            D_80070F60;
extern char           D_80093804[]; // "new_disp_2d ----> NULL\n"
extern CVECTOR        D_8009381C;
extern CVECTOR        D_80093820;
extern CVECTOR        D_80093824;
extern u8             D_80093828[]; // ":"
extern u8             D_8009382C[]; // "'"
extern char           D_80093870[]; // "Item"
extern s32            D_8005ED70;
extern s32            D_8005ED74;
extern GsCOORDINATE2  D_80070F10;
extern s16            D_80114C40;
extern DR_STP         D_80114C50;
extern s32            D_80115724;

#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_nccs_real()  __asm__ volatile("nop; nop; .word 0x4B08041B")
#define gte_nclip_real() __asm__ volatile("nop; nop; .word 0x4B400006")
#define gte_avsz3_real() __asm__ volatile("nop; nop; .word 0x4B58002D")
#define gte_avsz4_real() __asm__ volatile("nop; nop; .word 0x4B68002E")
#define gte_ncct_real()  __asm__ volatile("nop; nop; .word 0x4B18043F")

#define gte_ldsxy3_fifo(p)             \
    __asm__ volatile("lw $14, -8(%0);" \
                     "nop;"            \
                     "mtc2 $14, $15;"  \
                     "lw $14, -4(%0);" \
                     "nop;"            \
                     "mtc2 $14, $15;"  \
                     "lw $14, 0(%0);"  \
                     "nop;"            \
                     "mtc2 $14, $15"   \
                     :                 \
                     : "r"(p)          \
                     : "$14")

#define gte_ldsxy3_fifo_f4(p)           \
    __asm__ volatile("lw $14, -12(%0);" \
                     "nop;"             \
                     "mtc2 $14, $15;"   \
                     "lw $14, -8(%0);"  \
                     "nop;"             \
                     "mtc2 $14, $15;"   \
                     "lw $14, -4(%0);"  \
                     "nop;"             \
                     "mtc2 $14, $15"    \
                     :                  \
                     : "r"(p)           \
                     : "$14")

#define gte_ldsxy3_fifo_gt3(p)          \
    __asm__ volatile("lw $24, -32(%0);" \
                     "nop;"             \
                     "mtc2 $24, $15;"   \
                     "lw $24, -20(%0);" \
                     "nop;"             \
                     "mtc2 $24, $15;"   \
                     "lw $24, -8(%0);"  \
                     "nop;"             \
                     "mtc2 $24, $15"    \
                     :                  \
                     : "r"(p)           \
                     : "$24")

#define gte_ldsxy_fifo0(p)            \
    __asm__ volatile("lw $14, 0(%0);" \
                     "nop;"           \
                     "mtc2 $14, $15"  \
                     :                \
                     : "r"(p)         \
                     : "$14")

#define gte_ldsz0(r0)  __asm__ volatile("mtc2 %0, $16" : : "r"(r0))
#define gte_ldsz1(r0)  __asm__ volatile("mtc2 %0, $17" : : "r"(r0))
#define gte_ldsz2(r0)  __asm__ volatile("mtc2 %0, $18" : : "r"(r0))
#define gte_ldsz3s(r0) __asm__ volatile("mtc2 %0, $19" : : "r"(r0))

#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

#define gte_MulMatrix0_real(r1, r2, r3) \
    {                                   \
        gte_SetRotMatrix(r1);           \
        gte_ldclmv(r2);                 \
        gte_rtir_real();                \
        gte_stclmv(r3);                 \
        gte_ldclmv((char*)(r2) + 2);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 2);    \
        gte_ldclmv((char*)(r2) + 4);    \
        gte_rtir_real();                \
        gte_stclmv((char*)(r3) + 4);    \
    }

void func_800A4904(s32 arg0);
void func_800A4A2C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A5274(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A7824(s32 arg0, s32 arg1, s32 arg2);
void func_800A6A9C(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_8009939C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800A82C0(GsCOORDINATE2* arg0, VECTOR* arg1);
void func_800A9730(Task* task);
void func_807150F8(s32 arg0);
void func_80715198(void);

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

void* func_80099098(Task* task)
{
    GpDisp2d*               node;
    TmdListHead*            last;
    TmdListHead*            list;
    MATRIX*                 m;
    s32                     one;
    register GpDisp2dCoord* coord asm("v1");

    node  = Mem_Calloc(0x60, 0);
    coord = &node->coord;
    if (node != NULL) {
        node->field_C           = 1;
        node->field_8           = coord;
        coord->sub              = &D_80070F10;
        one                     = ONE;
        m                       = &node->coord.mtx;
        *(s32*)&node->coord.mtx = one;
        *(s32*)&m->m[1][1]      = one;
        m->m[2][2]              = one;
        list                    = &Tmd_ListAlt;
        *(s32*)&m->m[0][2]      = 0;
        *(s32*)&m->m[2][0]      = 0;
        coord->mtx.t[2]         = 0;
        coord->mtx.t[1]         = 0;
        coord->mtx.t[0]         = 0;
        coord->rot.vz           = 0;
        coord->rot.vy           = 0;
        coord->rot.vx           = 0;
        coord->flg              = 0;
        last                    = list->prev;
        node->next              = last->next;
        last->next              = (TmdObject*)node;
        node->prev              = last;
        list->prev              = (TmdListHead*)node;
        task->extra             = node;
        task->spawnType         = 2;
    } else {
        printf(D_80093804);
    }
    return node;
}

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

void func_800992B0(void)
{
    D_80114B80       = Tmd_List;
    D_80114B88       = Tmd_ListAlt;
    Tmd_List.next    = NULL;
    Tmd_List.prev    = &Tmd_List;
    Tmd_ListAlt.next = NULL;
    Tmd_ListAlt.prev = &Tmd_ListAlt;
    D_80114B90       = Task_Spawn(0, 0x1A, 0, 0);
}

void func_80099338(void)
{
    Task_CallExit(D_80114B90);
    Tmd_List    = D_80114B80;
    Tmd_ListAlt = D_80114B88;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009939C);

Task* func_8009988C(GsCOORDINATE2* arg0)
{
    Task*          task;
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    u32            i;
    s32            found;
    u32            count;

    task = Task_GetActiveList()->next;
    if (task != NULL) {
        do {
            found = 0;
            switch (task->spawnType) {
                case 1:
                    extra = task->extra;
                    count = extra->field_30;
                    coord = extra->field_8;
                    for (i = 0; i < count; i++) {
                        if (coord == arg0) {
                            found = 1;
                            break;
                        }
                        coord++;
                    }
                    break;
                case 2:
                    extra = task->extra;
                    coord = extra->field_8;
                    if (coord == arg0) {
                        found = 1;
                    }
                    break;
            }
            if (found != 0) {
                break;
            }
            task = task->node.next;
        } while (task != NULL);
    }
    return task;
}

void func_80099958(void)
{
    func_8009850C(&Gpu_OtBuffers[Display_State.field_1f]);
}

u32* func_80099994(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock*  ws;
    register POLY_F4*      poly asm("t0");
    register POLY_F4*      xy asm("a3");
    register s32*          opz asm("t1");
    register DisplayState* ds asm("t5");
    register u32           mask asm("t2");
    register u32           maskHi asm("t4");
    register u32           clipMask asm("t3");
    register s32           hi asm("v0");
    u16*                   rec;
    s32                    sz;
    s32                    idx;
    u8*                    szTable;

    ws   = arg0;
    poly = (POLY_F4*)ws->field_4;
    if (ws->field_1C-- > 0) {
        opz      = &ws->field_28;
        clipMask = 0x80000000;
        asm("lui %0, %%hi(Display_State)" : "=r"(hi));
        asm("addiu %0, %1, %%lo(Display_State)" : "=r"(ds) : "r"(hi));
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        xy     = (POLY_F4*)&poly->x3;
        do {
            rec = (u16*)arg2;
            gte_ldsxy3_fifo_f4(xy);
            gte_nclip_real();
            gte_stopz(opz);
            if (ws->field_28 > 0) {
                goto draw;
            }
            gte_ldsxy_fifo0(xy);
            gte_nclip_real();
            gte_stopz(opz);
            if (ws->field_28 < 0) {
            draw:
                szTable = (u8*)ws->field_10;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldsz0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldsz1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
                        if ((sz & clipMask) == 0) {
                            gte_ldsz2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = *(s32*)(idx + (s32)szTable);
                            if ((sz & clipMask) == 0) {
                                gte_ldsz3s(sz);
                                gte_avsz4_real();
                                gte_stotz(opz);
                                gte_stotz(opz);
                                poly->tag =
                                    (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                                *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                                    (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                            }
                        }
                    }
                }
            }
            xy++;
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_4 = (u8*)poly;
    return arg2;
}

u32* func_80099B94(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock*  ws;
    register POLY_F3*      poly asm("t0");
    register POLY_F3*      xy asm("a3");
    register s32*          opz asm("t2");
    register DisplayState* ds asm("t5");
    register u32           mask asm("t1");
    register u32           maskHi asm("t4");
    register u32           clipMask asm("t3");
    register s32           hi asm("v0");
    u16*                   rec;
    s32                    sz;
    s32                    idx;
    u8*                    szTable;

    ws   = arg0;
    poly = (POLY_F3*)ws->field_4;
    if (ws->field_1C-- > 0) {
        opz      = &ws->field_28;
        clipMask = 0x80000000;
        asm("lui %0, %%hi(Display_State)" : "=r"(hi));
        asm("addiu %0, %1, %%lo(Display_State)" : "=r"(ds) : "r"(hi));
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        xy     = (POLY_F3*)&poly->x2;
        do {
            rec = (u16*)arg2;
            gte_ldsxy3_fifo(xy);
            gte_nclip_real();
            gte_stopz(opz);
            if (ws->field_28 < 0) {
                szTable = (u8*)ws->field_10;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldsz0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldsz1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
                        if ((sz & clipMask) == 0) {
                            gte_ldsz2(sz);
                            gte_avsz3_real();
                            gte_stotz(opz);
                            gte_stotz(opz);
                            poly->tag =
                                (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                            *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                                (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                        }
                    }
                }
            }
            xy++;
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_4 = (u8*)poly;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099D40);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099FF4);

u32* func_8009A348(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    register TmdScratchModelBlock* ws asm("a3");
    register POLY_GT3*             poly asm("t1");
    register POLY_GT3*             xy asm("a1");
    register s32*                  opz asm("t4");
    register u32                   clipMask asm("t5");
    register s32                   len asm("t6");
    register s32                   code asm("t7");
    register DisplayState*         ds asm("t3");
    register u32                   mask asm("t0");
    register u32                   maskHi asm("t2");
    register s32                   hi asm("v0");
    u16*                           rec;
    s32                            sz;
    s32                            idx;
    u8*                            szTable;

    ws   = arg0;
    poly = (POLY_GT3*)ws->field_4;
    if (ws->field_1C-- > 0) {
        opz      = &ws->field_28;
        clipMask = 0x80000000;
        len      = 9;
        code     = 0x34;
        asm("lui %0, %%hi(Display_State)" : "=r"(hi));
        asm("addiu %0, %1, %%lo(Display_State)" : "=r"(ds) : "r"(hi));
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        xy     = poly + 1;
        do {
            rec = (u16*)arg2;
            gte_ldsxy3_fifo_gt3(xy);
            gte_nclip_real();
            gte_stopz(opz);
            if (ws->field_28 > 0) {
                szTable = (u8*)ws->field_10;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldsz1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldsz2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
                        if ((sz & clipMask) == 0) {
                            gte_ldsz3s(sz);
                            gte_avsz3_real();
                            setlen(&xy[-1], len);
                            setcode(&xy[-1], 0x36);
                            gte_stotz(opz);
                            setlen(xy, len);
                            setcode(xy, code);
                            gte_stotz(opz);
                            poly->tag =
                                (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                            *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                                (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                            xy->tag =
                                (xy->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                            *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                                (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)xy & mask);
                        }
                    }
                }
            }
            xy   += 2;
            poly += 2;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_4 = (u8*)poly;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A57C);

u32* func_8009A804(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    s32                   prev;
    s32                   count;
    u32                   idx;
    u16*                  rec;
    CVECTOR               col;
    CVECTOR               col2;
    u8*                   dest;

    ws = arg0;
    __asm__ volatile("" : "+r"(ws));
    col   = D_8009381C;
    col2  = D_8009381C;
    count = ws->field_1C;
    if (count == 0) {
        return arg2;
    }
    prev         = -1;
    ws->field_1C = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->field_8 + (idx & 0xFFF8));
                gte_rtps_real();
                gte_stsz(&ws->field_28);
                gte_stflg(&ws->field_24);
                if (ws->field_24 & 0x80000000) {
                    ws->field_28 |= 0x80000000;
                }
                ws->field_10[*(u16*)arg2 >> 3] = ws->field_28;
            }
            prev = rec[0];
            dest = ws->field_4 + rec[2] + 4;
            gte_stsxy(dest);
            dest = ws->field_4 + rec[3] + 4;
            gte_stsxy(dest);
            gte_stsxy(&ws->field_7C);
            gte_ldv0((u8*)ws->field_C + (rec[1] & 0xFFF8));
            gte_ldrgb(&col2);
            gte_nccs_real();
            gte_strgb(ws->field_4 + rec[2]);
            gte_ldrgb(&col);
            gte_nccs_real();
            gte_strgb(ws->field_4 + rec[3]);
            gte_rtv0_real();
            ws->field_7C = ((s16)ws->field_7C >> 4) + 0x20;
            ws->field_7E = ((s16)ws->field_7E >> 4) + 0x20;
            gte_stsv(&ws->field_74);
            ws->field_7C += (s16)ws->field_74 >> 8;
            ws->field_7E += (s16)ws->field_76 >> 8;
            dest          = ws->field_4 + rec[2];
            dest[8]       = *(u8*)&ws->field_7C;
            dest          = ws->field_4 + rec[2];
            dest[9]       = *(u8*)&ws->field_7E;
            arg2         += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    return arg2;
}

u32* func_8009AA5C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    s32                   prev;
    s32                   count;
    u32                   idx;
    u16*                  rec;
    CVECTOR               col;
    u8*                   dest;

    ws = arg0;
    __asm__ volatile("" : "+r"(ws));
    col = D_80093820;
    gte_ldrgb(&col);
    count = ws->field_1C;
    if (count == 0) {
        return arg2;
    }
    prev         = -1;
    ws->field_1C = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->field_8 + (idx & 0xFFF8));
                gte_rtps_real();
                gte_stsz(&ws->field_28);
                gte_stflg(&ws->field_24);
                if (ws->field_24 & 0x80000000) {
                    ws->field_28 |= 0x80000000;
                }
                ws->field_10[*(u16*)arg2 >> 3] = ws->field_28;
            }
            prev = rec[0];
            dest = ws->field_4 + rec[2];
            gte_stsxy(dest);
            gte_stsxy(&ws->field_7C);
            gte_ldv0((u8*)ws->field_C + (rec[1] & 0xFFF8));
            gte_nccs_real();
            gte_rtv0_real();
            ws->field_7C = ((s16)ws->field_7C >> 4) + 0x20;
            ws->field_7E = ((s16)ws->field_7E >> 4) + 0x20;
            gte_stsv(&ws->field_74);
            ws->field_7C += (s16)ws->field_74 >> 8;
            ws->field_7E += (s16)ws->field_76 >> 8;
            dest          = ws->field_4 + rec[2];
            dest[4]       = *(u8*)&ws->field_7C;
            dest          = ws->field_4 + rec[2];
            dest[5]       = *(u8*)&ws->field_7E;
            arg2         += ws->field_18;
            gte_strgb(ws->field_4 + rec[3]);
        } while (ws->field_1C-- > 0);
    }
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AC58);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AF90);

u32* func_8009B2F4(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    s32                   prev;
    s32                   count;
    u32                   idx;
    u16*                  rec;
    CVECTOR               col;
    CVECTOR               col2;
    u8*                   dest;
    s32                   val;
    s32                   inv;

    ws = arg0;
    __asm__ volatile("" : "+r"(ws));
    col    = D_80093824;
    col2   = D_8009381C;
    val    = ws->field_80->field_2C >> 5;
    inv    = 0x80 - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    count  = ws->field_1C;
    if (count == 0) {
        return arg2;
    }
    prev         = -1;
    ws->field_1C = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->field_8 + (idx & 0xFFF8));
                gte_rtps_real();
                gte_stsz(&ws->field_28);
                gte_stflg(&ws->field_24);
                if (ws->field_24 & 0x80000000) {
                    ws->field_28 |= 0x80000000;
                }
                ws->field_10[*(u16*)arg2 >> 3] = ws->field_28;
            }
            prev = rec[0];
            dest = ws->field_4 + rec[2] + 4;
            gte_stsxy(dest);
            dest = ws->field_4 + rec[3] + 4;
            gte_stsxy(dest);
            gte_stsxy(&ws->field_7C);
            gte_ldv0((u8*)ws->field_C + (rec[1] & 0xFFF8));
            gte_ldrgb(&col);
            gte_nccs_real();
            gte_strgb(ws->field_4 + rec[2]);
            gte_ldrgb(&col2);
            gte_nccs_real();
            gte_strgb(ws->field_4 + rec[3]);
            gte_rtv0_real();
            gte_stsv(&ws->field_74);
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009B500);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009BD00);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C024);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C414);

u32* func_8009CED0(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_GT3*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u16*                  rec;
    u8*                   verts;
    u8*                   norms;

    ws   = arg0;
    poly = (POLY_GT3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        opz    = &ws->field_28;
        ds     = &Display_State;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&ws->field_24);
            if (ws->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_ldrgb(arg2 + 3);
                    gte_stsxy3_gt3(poly);
                    gte_avsz3_real();
                    norms = (u8*)ws->field_C;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ncct_real();
                    gte_strgb3_gt3(poly);
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (ws->field_80->field_C & 2) {
                        setcode(poly, 0x36);
                    }
                    gte_stotz(opz);
                    poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                    *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                        (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D0DC);

u32* func_8009D388(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT3*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        opz    = &ws->field_28;
        ds     = &Display_State;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&ws->field_24);
            if (ws->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3_real();
                    setlen(poly, 7);
                    setcode(poly, 0x25);
                    gte_stotz(opz);
                    poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                    *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                        (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009D518(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    register u32          mask asm("t2");
    u32                   maskHi;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->field_0;
    if (ws->field_1C-- > 0) {
        flg      = &ws->field_24;
        clipMask = 0x80000000;
        opz      = &ws->field_28;
        ds       = &Display_State;
        mask     = 0xFFFFFF;
        maskHi   = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if ((ws->field_24 & clipMask) == 0) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if ((ws->field_24 & clipMask) == 0) {
                    if (ws->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (ws->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
                        setlen(poly, 9);
                        setcode(poly, 0x2D);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                        *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                            (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009D718(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_GT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    register u32          mask asm("t1");
    u32                   maskHi;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_GT4*)ws->field_0;
    if (ws->field_1C-- > 0) {
        flg      = &ws->field_24;
        clipMask = 0x80000000;
        opz      = &ws->field_28;
        ds       = &Display_State;
        mask     = 0xFFFFFF;
        maskHi   = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if ((ws->field_24 & clipMask) == 0) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)ws->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if ((ws->field_24 & clipMask) == 0) {
                    if (ws->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (ws->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
                        gte_stotz(opz);
                        poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                        *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                            (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009D900(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_F4*              poly;
    s32*                  opz;
    DisplayState*         ds;
    register u32          mask asm("t2");
    u32                   maskHi;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_F4*)ws->field_0;
    if (ws->field_1C-- > 0) {
        flg      = &ws->field_24;
        clipMask = 0x80000000;
        opz      = &ws->field_28;
        ds       = &Display_State;
        mask     = 0xFFFFFF;
        maskHi   = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if ((ws->field_24 & clipMask) == 0) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_f4(poly);
                gte_ldv0((u8*)ws->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if ((ws->field_24 & clipMask) == 0) {
                    if (ws->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (ws->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
                        setlen(poly, 5);
                        setcode(poly, 0x28);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                        *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                            (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009DB00(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_F3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_F3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        flg      = &ws->field_24;
        clipMask = 0x80000000;
        opz      = &ws->field_28;
        ds       = &Display_State;
        mask     = 0xFFFFFF;
        maskHi   = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if ((ws->field_24 & clipMask) == 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_stsxy3_f3(poly);
                    gte_stflg(flg);
                    if ((ws->field_24 & clipMask) == 0) {
                        gte_avsz3_real();
                        setlen(poly, 4);
                        setcode(poly, 0x20);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                        *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                            (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009DCB8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT3*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        opz    = &ws->field_28;
        ds     = &Display_State;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&ws->field_24);
            if (ws->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3_real();
                    setlen(poly, 7);
                    setcode(poly, 0x27);
                    gte_stotz(opz);
                    poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                    *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                        (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009DE48(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    register u32          mask asm("t2");
    u32                   maskHi;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->field_0;
    if (ws->field_1C-- > 0) {
        flg      = &ws->field_24;
        clipMask = 0x80000000;
        opz      = &ws->field_28;
        ds       = &Display_State;
        mask     = 0xFFFFFF;
        maskHi   = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(flg);
            if ((ws->field_24 & clipMask) == 0) {
                gte_nclip_real();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->field_8 + (rec[3] & 0xFFF8));
                gte_rtps_real();
                gte_stflg(flg);
                if ((ws->field_24 & clipMask) == 0) {
                    if (ws->field_28 > 0) {
                        goto draw;
                    }
                    gte_nclip_real();
                    gte_stopz(opz);
                    if (ws->field_28 < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4_real();
                        setlen(poly, 9);
                        setcode(poly, 0x2F);
                        gte_stotz(opz);
                        poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                        *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                            (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                    }
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009E048(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_G3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        opz    = &ws->field_28;
        ds     = &Display_State;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&ws->field_24);
            if (ws->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3_real();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->field_C + (rec[3] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->field_C + (rec[4] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->field_C + (rec[5] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x30);
                    gte_stotz(opz);
                    poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                    *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                        (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009E274(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_G3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   mask;
    u32                   maskHi;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->field_0;
    if (ws->field_1C-- > 0) {
        opz    = &ws->field_28;
        ds     = &Display_State;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->field_8;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt_real();
            gte_stflg(&ws->field_24);
            if (ws->field_24 >= 0) {
                gte_nclip_real();
                gte_stopz(opz);
                if (ws->field_28 > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3_real();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->field_C + (rec[3] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->field_C + (rec[4] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->field_C + (rec[5] & 0xFFF8));
                    gte_nccs_real();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x32);
                    gte_stotz(opz);
                    poly->tag = (poly->tag & maskHi) | (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & mask);
                    *(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) =
                        (*(u_long*)(((((u32)ws->field_28 << ds->field_128) >> 2) & 0xFFC) + (s32)ws->field_14) & maskHi) | ((u32)poly & mask);
                }
            }
            poly++;
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    ws->field_0 = (u8*)poly;
    return arg2;
}

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

u32* func_8009EAA4(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    s32                   prev;
    s32                   count;
    u32                   idx;
    u16*                  rec;

    ws    = arg0;
    prev  = -1;
    count = ws->field_1C;
    if (count == 0) {
        return arg2;
    }
    __asm__ volatile("" : "+r"(prev));
    ws->field_1C = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->field_8 + (idx & 0xFFF8));
                gte_rtps_real();
                gte_stsz(&ws->field_28);
                if (ws->field_24 & 0x80000000) {
                    ws->field_28 |= 0x80000000;
                }
                ws->field_10[*(u16*)arg2 >> 3] = ws->field_28;
            }
            prev = rec[0];
            gte_stsxy(ws->field_4 + rec[1]);
            arg2 += ws->field_18;
        } while (ws->field_1C-- > 0);
    }
    return arg2;
}

u32* func_8009EB84(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

u32* func_8009EC1C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            *(u16*)&poly->u3 = ((u16*)&arg2[4])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

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

u32* func_8009ED90(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[3];
            *(s32*)&poly->u1 = arg2[4];
            *(u16*)&poly->u2 = *(u16*)&arg2[5];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009EE28(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[4];
            *(s32*)&poly->u1 = arg2[5];
            *(u16*)&poly->u2 = *(u16*)&arg2[6];
            *(u16*)&poly->u3 = ((u16*)&arg2[6])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009EECC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[4];
            *(s32*)&poly->u1 = arg2[5];
            *(u16*)&poly->u2 = *(u16*)&arg2[6];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009EF64(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[6];
            *(s32*)&poly->u1 = arg2[7];
            *(u16*)&poly->u2 = *(u16*)&arg2[8];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009EFFC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[5];
            *(s32*)&poly->u1 = arg2[6];
            *(u16*)&poly->u2 = *(u16*)&arg2[7];
            *(u16*)&poly->u3 = ((u16*)&arg2[7])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F0A0(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[8];
            *(s32*)&poly->u1 = arg2[9];
            *(u16*)&poly->u2 = *(u16*)&arg2[10];
            *(u16*)&poly->u3 = ((u16*)&arg2[10])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F144(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F1DC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[3];
            *(s32*)&poly->u1 = arg2[4];
            *(u16*)&poly->u2 = *(u16*)&arg2[5];
            *(u16*)&poly->u3 = ((u16*)&arg2[5])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F280(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;
    s32       color;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->r0 = arg2[2];
            *(s32*)&poly->r1 = arg2[3];
            *(s32*)&poly->r2 = arg2[4];
            color            = arg2[5];
            setlen(poly, 12);
            setcode(poly, 0x3E);
            *(s32*)&poly->r3 = color;
            *(s32*)&poly->u0 = arg2[6];
            *(s32*)&poly->u1 = arg2[7];
            *(u16*)&poly->u2 = *(u16*)&arg2[8];
            *(u16*)&poly->u3 = ((u16*)&arg2[8])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F360(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_FT3* poly;

    poly = (POLY_FT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F3F8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_FT4* poly;

    poly = (POLY_FT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            *(u16*)&poly->u3 = ((u16*)&arg2[4])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

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

u32* func_8009F56C(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[3];
            *(s32*)&poly->u1 = arg2[4];
            *(u16*)&poly->u2 = *(u16*)&arg2[5];
            poly->tpage     += (s8)arg0->field_80->field_26;
            tmp              = arg0->field_80->field_27;
            tpage            = poly->tpage;
            tpage           |= 0x20;
            poly->tpage      = tpage;
            poly->clut      += (s8)tmp << 6;
            poly++;
            *(s32*)&poly->u0 = arg2[3];
            *(s32*)&poly->u1 = arg2[4];
            *(u16*)&poly->u2 = *(u16*)&arg2[5];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F670(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            poly++;
            *(s32*)&poly->u0 = arg2[3];
            *(s32*)&poly->u1 = arg2[4];
            *(u16*)&poly->u2 = *(u16*)&arg2[5];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F708(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[4];
            *(s32*)&poly->u1 = arg2[5];
            *(u16*)&poly->u2 = *(u16*)&arg2[6];
            *(u16*)&poly->u3 = ((u16*)&arg2[6])[1];
            poly->tpage     += (s8)arg0->field_80->field_26;
            tmp              = arg0->field_80->field_27;
            tpage            = poly->tpage;
            tpage           |= 0x20;
            poly->tpage      = tpage;
            poly->clut      += (s8)tmp << 6;
            poly++;
            *(s32*)&poly->u0 = arg2[4];
            *(s32*)&poly->u1 = arg2[5];
            *(u16*)&poly->u2 = *(u16*)&arg2[6];
            *(u16*)&poly->u3 = ((u16*)&arg2[6])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F824(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_0;
    if (arg0->field_1C-- > 0) {
        do {
            poly++;
            *(s32*)&poly->u0 = arg2[4];
            *(s32*)&poly->u1 = arg2[5];
            *(u16*)&poly->u2 = *(u16*)&arg2[6];
            *(u16*)&poly->u3 = ((u16*)&arg2[6])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_0 = (u8*)poly;
    return arg2;
}

u32* func_8009F8C8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

u32* func_8009F970(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            *(u16*)&poly->u3 = ((u16*)&arg2[4])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

u32* func_8009FA24(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += (s8)arg0->field_80->field_26;
            tmp              = arg0->field_80->field_27;
            tpage            = poly->tpage;
            tpage           |= 0x20;
            poly->tpage      = tpage;
            poly->clut      += (s8)tmp << 6;
            poly++;
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

u32* func_8009FB28(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)arg0->field_4;
    if (arg0->field_1C-- > 0) {
        do {
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            *(u16*)&poly->u3 = ((u16*)&arg2[4])[1];
            poly->tpage     += (s8)arg0->field_80->field_26;
            tmp              = arg0->field_80->field_27;
            tpage            = poly->tpage;
            tpage           |= 0x20;
            poly->tpage      = tpage;
            poly->clut      += (s8)tmp << 6;
            poly++;
            *(s32*)&poly->u0 = arg2[2];
            *(s32*)&poly->u1 = arg2[3];
            *(u16*)&poly->u2 = *(u16*)&arg2[4];
            *(u16*)&poly->u3 = ((u16*)&arg2[4])[1];
            poly->tpage     += arg0->field_70;
            poly->clut      += arg0->field_72;
            poly++;
            arg2 += arg0->field_18;
        } while (arg0->field_1C-- > 0);
    }
    arg0->field_4 = (u8*)poly;
    return arg2;
}

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

void func_8009FD74(s32 arg0, PadScratch* arg1)
{
    u16                   temp_v0;
    u16                   temp_v1;
    register GpPadReplay* rec asm("a0");
    s32                   offset;

    if (arg0 == 1) {
        func_807150F8(1);
        return;
    }

    offset = (s32)D_80114C38 - (s32)D_8005C374;
    if (Display_State.field_12c == 0x10) {
        offset = (s32)D_80114C38 + 0x7F9FFF00;
    }
    if (offset <= 0x17FDF) {
        if (D_8005EC80 != 0) {
            arg1->buttons = D_80114C02;
            return;
        }
        temp_v1 = D_80114C38->buttons;
        if (temp_v1 != D_80114C02) {
            D_80114C02 = temp_v1;
            D_80114C04 = D_80114C38->duration;
        }
        if (arg1->buttons & 0x800) {
            arg1->buttons        = D_80114C02 | 0x800;
            Wip_SysFlags.field_4 = 1;
        } else {
            arg1->buttons = D_80114C02;
        }
        temp_v0    = D_80114C04 - 1;
        D_80114C04 = temp_v0;
        if (!(temp_v0 & 0xFFFF)) {
            rec        = D_80114C38;
            D_80114C02 = 0xFFFF;
            D_80114C38 = rec + 1;
            asm volatile("");
            if (rec[1].buttons == 0xFFFF) {
                Wip_SysFlags.field_4    = 0;
                Pad_RemapState->field_8 = 0;
            }
        }
    } else {
        Pad_RemapState->field_8 = 0;
    }
}

void func_8009FEDC(Task* task)
{
    GpIdMap30*    rec;
    DisplayState* ds;

    func_800E956C();
    Game_Session->field_5E = 1;
    rec                    = Mem_Calloc(0x30, 0);
    if (rec == NULL) {
        Task_Kill(task);
        return;
    }
    func_800A7574(&rec->extra);
    GameMain_SetFrameTiming(1);
    task->idMap  = (TaskIdMap*)rec;
    rec->field_0 = Mc_SaveData.field_C / 60;
    rec->field_4 = Mc_SaveData.field_C % 60;
    ds           = &Display_State;
    rec->field_8 = ds->field_4;
    func_800B25B0();
    if (ds->field_12c != 0) {
        srand(1);
        ds->field_8           = 0;
        Display_State.field_0 = 0;
        D_80070F60            = 0;
        ds->field_4           = 0;
        ds->field_14          = 0;
        ds->field_c           = 0;
        ds->field_10          = 0;
        if (ds->field_12c == 0x10) {
            D_80114C38 = (GpPadReplay*)0x80600E4C;
        } else {
            D_80114C38 = (GpPadReplay*)((u8*)D_8005C374 + 0xD4C);
        }
        D_80114C02              = 0xFFFF;
        D_80114C04              = 1;
        Pad_RemapState->field_8 = -1;
    } else if (Pad_RemapState->field_9 == 1) {
        func_80715198();
    }
    task->state++;
}

void func_800A0094(Task* task)
{
    TextDrawReq   req;
    u8            buf[0x20];
    GpIdMap30*    rec;
    McSaveData*   save;
    WipSysConfig* cfg;
    GameSession*  session;
    s32           one;
    s32           temp;
    s32           companion;

    rec = (GpIdMap30*)task->idMap;
    cfg = &Wip_SysConfig;
    func_800E956C();

    temp         = Display_State.field_4;
    D_8005ED68  += temp - rec->field_8;
    rec->field_8 = temp;
    if (D_8005ED68 >= 0xE10) {
        McSaveData* p;
        D_8005ED68 -= 0xE10;
        p           = &Mc_SaveData;
        if (p->field_C <= 0xEA5E) {
            p->field_C++;
            rec->field_4++;
            if (rec->field_4 >= 0x3C) {
                rec->field_4 -= 0x3C;
                rec->field_0++;
            }
        } else {
            p->field_C   = 0xEA5F;
            rec->field_0 = 0x3E7;
            rec->field_4 = 0x3B;
        }
    }

    save = &Mc_SaveData;
    one  = 1;
    if (save->field_23 == one) {
        req.x          = -0x96;
        req.y          = 0x64;
        req.otIndex    = 4;
        req.field_8    = 0x502008;
        req.glyphTable = 2;
        req.centerMode = 0;
        req.field_E    = one;
        func_8002E53C(&req, Text_ItoaUnsigned(buf, rec->field_0));
        func_8002E53C(&req, D_80093828);
        func_8002E53C(&req, func_8002F44C(buf, rec->field_4, 2));
        func_8002E53C(&req, D_8009382C);
        func_8002E53C(&req, func_8002F44C(buf, D_8005ED68 / 60, 2));
        Pad_CheckButtons(one, one, 0x100);
    }

    if (Game_Session->field_127 == 0) {
        if (cfg->field_18 > 0) {
            companion = save->field_13;
            if (companion == one) {
                if ((s16)save->field_6C8 <= 0) {
                    goto block_hp;
                }
            }
            if (companion != 3) {
                goto block_normal;
            }
            if ((s16)save->field_6C8 > 0) {
                goto block_normal;
            }
        block_hp:
            if (cfg->field_18 > 0) {
                goto block_companion;
            }
        }

        if (Game_Session->field_1 != 0) {
            cfg->field_18 = 1;
            return;
        }
        D_80114C08.field_3 = 0;
        func_800A7DE0();
        func_800FC6C0();
        session = Game_Session;
        if (session->field_128 != 3) {
            D_80070F60       = D_80070F60 * 5 + 0x71357911;
            session->field_0 = ((u32)D_80070F60 >> 16 & 1) + 1;
            SndEvt_EnqueueType7(0x20000000, 8);
            SndBank_SetEnableFlags(0, 0x20000000);
            CdCmd_EnqueueLoadFile(9, ((u8)Game_Session->field_0 + 0x1D) & 0xFF, 3);
        }

    block_companion: {
        McSaveData* p;
        p = &Mc_SaveData;
        if ((s16)p->field_6C8 <= 0) {
            if (Game_Session->field_1 != 0) {
                p->field_6C8 = 1;
                return;
            }
            D_80114C08.field_3 = 0;
            func_800A7DE0();
            func_800FC6C0();
            companion = p->field_13;
            if (companion == 1) {
                Game_Session->field_128 = companion;
                D_80070F60              = D_80070F60 * 5 + 0x71357911;
                Game_Session->field_0   = ((u32)D_80070F60 >> 16 & 1) + 1;
                SndEvt_EnqueueType7(0x20000000, 8);
                SndBank_SetEnableFlags(0, 0x20000000);
                CdCmd_EnqueueLoadFile(9, ((u8)Game_Session->field_0 + 0x20) & 0xFF, 3);
                companion = p->field_13;
            }
            if (companion == 3) {
                Game_Session->field_128 = 4;
            }
        }
    }
        Display_AcquireRef();
        task->killCountdown  = Game_Session->field_12F;
        Wip_SysFlags.field_1 = 1;
        task->state++;
        return;
    }

block_normal:
    if (Game_Session->field_128 == 0xFF) {
        Display_AcquireRef();
        Game_Session->field_0 = 1;
        task->state++;
    } else {
        func_800A3AF0(&rec->extra);
    }
}

void func_800A0504(Task* arg0)
{
    RECT          rect;
    DisplayState* ds;
    GameSession*  session;
    CdCmdQueue*   queue;
    s32           flag;

    queue = &CdCmd_Queue;
    func_800A7320(&arg0->killCountdown);
    arg0->spawnArg1 += 0xA;
    if (arg0->spawnArg1 < 0x100) {
        return;
    }
    if (Game_Session->field_128 != 3) {
        SetDispMask(0);
    }
    SndEvt_EnqueueType2(0, 8);
    SndEvt_EnqueueType7(0x80000000, 0x78);
    SndEvt_EnqueueType7(0x60010001, 0x78);
    flag            = 0xFF;
    arg0->spawnArg1 = flag;
    Pad_SetCooldown(0);
    Game_ClearPtrSlots();
    ds            = &Display_State;
    ds->field_10b = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Mem_Init();
    CdCmd_ActivatePhase1();
    session          = Game_Session;
    queue->field_20A = 1;
    if (session->field_128 != 3) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        ds->field_100 = 0;
    }
    memset(&Game_Session->field_4, 0, 8);
    Mem_ConfigureAuxHeap(0, 0);
    if (Game_Session->field_128 == flag) {
        D_80068F90         = 0xB000;
        GActiveAuxHeapSize = 0x30000;
        D_80068F88         = (size_t)((u8*)Fs_ImgBuffers - 0x35800);
        GActiveAuxHeap     = (u8*)Fs_ImgBuffers - 0xA800;
    }
    Mem_Init();
    Mem_InitAux();
    if (Game_Session->field_128 != flag) {
        CdCmd_SetupMdecBuffers();
    }
    Task_SpawnFromTable(&D_8010D1FC, 0, 0, 0);
}

void func_800A0718(Task* arg0)
{
    GameSession* session;
    GpEndWork*   work;
    GpSndParam*  pair;

    if (arg0->state == 0) {
        work                = arg0->spawnArg2;
        work->field_4       = 1;
        arg0->killCountdown = 0x1E;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x4300000) {
            arg0->killCountdown = 0x5A;
        }
        SndEvt_EnqueueType6(0xB, 0, 0);
        func_800E8FB0((s32)&D_80114A24, (s32)&D_80114A34);
        func_800AF498();
    } else if (arg0->state == 1) {
        session = Game_Session;
        if (!(session->field_69 & 1)) {
            pair          = (GpSndParam*)&D_8007A39C;
            pair->field_0 = 0;
            pair->field_2 = 0;
            if ((session->field_69 & 4) == 0) {
                Task_SpawnFromTable(&D_80062774, 0, 2, 0);
            } else {
                Task_SpawnFromTable(&D_80062774, 0, 3, 0);
            }
        } else {
            D_80062734 = 0xFF;
        }
    } else {
        goto countdown;
    }
    arg0->state++;
countdown:
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        if (D_80062734 == 0xFF) {
            Task_Kill(arg0);
            Stage_SetEndingFlag();
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A087C);

void func_800A110C(Task* arg0)
{
    register u32           key asm("s2");
    GpEndWork*             work;
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           slot;
    GameSession*           session;
    GpSndParam*            pair;
    GpItemScan*            scan;
    McSaveData*            save;

    if (arg0->state == 0) {
        work = arg0->spawnArg2;
        key  = *(u32*)&Game_Session->field_4;
        key &= 0xFFFF0000;
        Display_InitPrimBufOnce();
        i = 0;
        p = D_80115760;
        do {
            slot = *p;
            if (slot != NULL) {
                slot->actor->field_90C = NULL;
            }
            i++;
            p++;
        } while (i < 2);
        SndEvt_EnqueueType8(0xD);
        func_800B065C((func_800A7B20(7) + 0x15) & 0xFF);
        if (key == 0x1140000) {
            arg0->spawnArg2 = Ui_SpawnFromDesc(&D_80185000, arg0->spawnArg1, 1, 4, NULL);
        } else {
            arg0->spawnArg2 = Ui_SpawnFromDesc(&D_8010CA40, arg0->spawnArg1, 1, 1, NULL);
            if (arg0->spawnArg1 == 0) {
                work->field_4 = 0;
                work->field_0 = 0;
                func_800B5BFC(1, (GpAreaKey*)&Game_Session->field_4);
                Game_Session->field_126 = 1;
                if (key == 0x50B0000 || key == 0x51D0000) {
                    if (Game_Session->field_9 - 1 < 3U) {
                        goto skip_count;
                    }
                }
                save = &Mc_SaveData;
                if (save->field_6CC < 0x270FU) {
                    save->field_6CC++;
                }
            skip_count:
                scan = &D_8010CA2C;
                func_800BAC8C(scan);
                arg0->flags = func_800DB128(scan);
                if (arg0->flags != 0) {
                    Ui_SpawnFromDesc(D_8010CA78, 1, 0, 0x11, arg0->spawnArg2);
                    if (arg0->flags == 2) {
                        Ui_SpawnFromDesc(D_8010CA78 + 1, 2, 0, 0x21, arg0->spawnArg2);
                    }
                }
            } else {
                register s32         hi asm("v0");
                register McSaveData* save2 asm("a0");

                asm("lui %0, %%hi(Mc_SaveData)" : "=r"(hi));
                asm("addiu %0, %1, %%lo(Mc_SaveData)" : "=r"(save2) : "r"(hi));
                arg0->flags = 0;
                if (save2->field_6CE < 0x270FU) {
                    save2->field_6CE++;
                }
            }
        }
        GameMain_SetFrameTiming(0);
        arg0->state++;
    } else if (arg0->state == 1) {
        session = Game_Session;
        if (!(session->field_69 & 2)) {
            session->field_4D = 1;
            pair              = (GpSndParam*)&D_8007A39C;
            pair->field_0     = 0;
            pair->field_2     = 0;
            if (!(Game_Session->field_69 & 8)) {
                Task_SpawnFromTable(&D_80062774, 0, 1, 0);
            } else {
                Task_SpawnFromTable(&D_80062774, 0, 3, 0);
            }
        } else {
            D_80062734 = 0xFF;
        }
        arg0->state++;
    } else if (arg0->state == 2) {
        register s32       hi asm("v0");
        register s32       ff asm("v1");
        register s32       cur asm("v0");
        register UiObject* obj asm("s0");

        asm("lui %0, %%hi(D_80062734)" : "=r"(hi));
        ff = 0xFF;
        asm("lbu %0, %%lo(D_80062734)(%1)" : "=r"(cur) : "r"(hi), "r"(ff));
        obj = arg0->spawnArg2;
        if (cur == ff) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                if (obj->field_2E == 6) {
                    Ui_TeardownTree(obj, obj->owner);
                    if (arg0->flags != 0) {
                        D_80114DDC      = 0x700;
                        arg0->spawnArg2 = Ui_SpawnFromDesc(&D_8010D6D8, 1, 1, 1, NULL);
                        arg0->state++;
                    } else {
                        arg0->killCountdown = 0xA;
                        arg0->state         = 0x10;
                    }
                }
            }
        }
    } else if (arg0->state == 3) {
        register UiObject* obj asm("a0");

        obj = arg0->spawnArg2;
        if ((obj->field_2E == 6) || (obj->field_2E == -1)) {
            Ui_TeardownTree(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 0x10;
        }
    } else if (arg0->state == 0x10) {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            arg0->state = 0x11;
        }
    }

    if (arg0->state >= 0x11) {
        if (D_80062734 == 0xFF) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                GameMain_SetFrameTiming(1);
                SndEvt_EnqueueType9(0xD);
                Task_Kill(arg0);
                Stage_ReleasePrimBuf();
                Stage_SetEndingFlag();
            }
        }
    }
}

u16 func_800A1558(s32 arg0)
{
    WipSysConfig* p;
    s32           cond;
    s32           ret;
    u8*           table;
    s32           idx;
    GpRec16*      recs;
    register s32  off asm("v1");

    recs = D_8011398C;
    idx  = D_80114C08.field_5;
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &Wip_SysConfig;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData.unknown_850;
        } else {
            table = D_80114BF0;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->field_25 & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    off  = arg0 * 2;
    off += (D_80114C08.field_5 * 3 + ret) * 16;
    off  = (s32)recs + off;
    return *(u16*)off;
}

void func_800A1634(s32 arg0, GpIdMapC* arg1)
{
    WipSysConfig* p;
    GpStateF0*    state;
    GpRec8*       rec;
    s32           cond;
    register s32  ret asm("a0");
    u8*           table;
    s32           idx;
    register s32  val1 asm("s1");
    register s32  val2 asm("s2");
    s32           flag;
    s32           temp2;
    s32           temp4;
    u8            kind;
    register s32  off asm("v1");
    register s32  scaled asm("v0");

    idx = D_80114C08.field_B;
    if (arg0 == 1) {
        idx = D_80114C08.field_5;
    }
    if (idx >= 0xC) {
        ret = 1;
    } else {
        p = &Wip_SysConfig;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData.unknown_850;
        } else {
            table = D_80114BF0;
        }
        ret = table[idx];
        if (ret == 0) {
            ret = 1;
        }
        if (p->field_25 & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    off             = idx * 3;
    off             = off * 8;
    off            += (s32)D_80113D38;
    scaled          = ret * 8;
    rec             = (GpRec8*)(off + scaled);
    state           = &D_801153F0;
    temp2           = rec->field_2;
    temp4           = rec->field_4;
    state->field_5  = 0;
    state->field_14 = 0;
    scaled          = (temp2 << 1) + temp2;
    scaled          = (scaled << 3) + temp2;
    val1            = scaled << 2;
    scaled          = (temp4 << 1) + temp4;
    scaled          = (scaled << 3) + temp4;
    val2            = scaled << 2;
    if ((D_801153F0.field_0 == 1 && state->field_6 != 0) || state->field_1 != 0) {
        flag = 1;
    } else {
        flag = 0;
    }
    if (flag != 0) {
        switch (rec->field_0) {
            case 0:
                func_800A45F0(arg0);
                break;
            case 1:
                func_800A7824(arg0, val1, val2);
                if (arg1 != NULL) {
                    arg1->field_16 = 4;
                    arg1->field_18 = val2;
                }
                break;
            case 2:
                func_800A5274(arg0, val1, val2, rec->field_6);
                goto after_23;
            case 3:
                func_800A5574(arg0, val1, val2, rec->field_6);
            after_23:
                if (arg1 != NULL) {
                    kind           = *(u8*)&rec->field_6;
                    arg1->field_18 = val1;
                    arg1->field_16 = kind + 2;
                }
                break;
            case 4:
                func_800A4904(arg0);
                if (arg1 != NULL) {
                    arg1->field_16 = 2;
                    arg1->field_18 = 0x3FFF;
                }
                break;
        }
        if (arg1 != NULL) {
            if (idx >= 0xC) {
                arg1->field_16 = -1;
            }
        }
    } else if (idx == 7) {
        func_800A45F0(arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A18BC);

s32 func_800A1CD0(s32 arg0)
{
    WipSysConfig*          cfg;
    register WipSysConfig* p asm("a1");
    register s32           result asm("t0");
    register s32           ret asm("v1");
    register s32           n asm("a3");
    register GpRec16*      recs asm("v1");
    register s32           off asm("v0");
    GpStateF0*             state;
    GpStateC08*            c08;
    u8*                    table;
    s32                    cond;
    s32                    flag;
    u16                    val;

    cfg    = &Wip_SysConfig;
    result = 0;
    if (arg0 >= 0xC) {
        ret = 1;
    } else {
        p = cfg;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
            cond = 0;
        } else {
            cond = cfg->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData.unknown_850;
        } else {
            table = D_80114BF0;
        }
        ret = table[arg0];
        if (ret == 0) {
            ret = 1;
        }
        if (p->field_25 & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    n = ret;

    state = &D_801153F0;
    if ((D_801153F0.field_0 == 1 && state->field_6 != 0) || state->field_1 != 0) {
        flag = 1;
    } else {
        flag = 0;
    }

    if (flag == 0) {
        recs = D_8011398C;
        off  = (arg0 * 3 + n) * 16;
        asm volatile("" : "+r"(off));
        off += 4;
        asm volatile("" : "+r"(off));
        off += (s32)recs;
        val  = *(u16*)off;
        if (cfg->field_1c < val) {
            result = 1;
        } else if (arg0 != 7) {
            result = 1;
        } else if (cfg->field_1a == cfg->field_18) {
            result = 1;
        }
    } else if (arg0 < 0xC) {
        if (cfg->field_25 & 0x10) {
            result = 1;
        } else {
            if (!(cfg->field_25 & 0x80)) {
                recs = D_8011398C;
                off  = (arg0 * 3 + n) * 16;
                asm volatile("" : "+r"(off));
                off += 4;
                asm volatile("" : "+r"(off));
                off += (s32)recs;
                val  = *(u16*)off;
                if (cfg->field_1c < val) {
                    if (Mc_SaveData.field_5C2 == 0) {
                        result = 1;
                        goto done;
                    }
                }
            }
            if (arg0 == 6) {
                c08 = &D_80114C08;
                if (c08->field_16 != 0) {
                    if ((s8)c08->field_17 != 0) {
                        result = 1;
                        goto done;
                    }
                }
            }
            if (arg0 == 7) {
                if (cfg->field_1a == cfg->field_18) {
                    if (Mc_SaveData.field_5C2 == 0) {
                        result = 1;
                        goto done;
                    }
                }
            }
            if (arg0 == 0xB) {
                if (D_80115724 >= 3) {
                    result = 1;
                    goto done;
                }
            }
            if (cfg->field_25 & 0x80) {
                if (arg0 >= 6) {
                    result = 1;
                } else {
                    recs = D_8011398C;
                    off  = (arg0 * 3 + n) * 16;
                    asm volatile("" : "+r"(off));
                    off += 4;
                    asm volatile("" : "+r"(off));
                    off += (s32)recs;
                    val  = *(u16*)off;
                    if (val * 2 >= cfg->field_18) {
                        result = 1;
                    }
                }
            }
        }
    }

done:
    return result;
}

void func_800A1F64(s32 arg0)
{
    GpStateC08*   p;
    WipSysConfig* cfg;
    s32           cond;
    s32           ret;
    u8*           table;
    register s32  n asm("a1");
    register s32  val asm("a0");
    s32           t;
    s32           tmp;
    s8            temp;
    s32           neg;

    D_80114C08.field_E = 0;
    if (D_80114C08.field_6 & 1) {
        return;
    }
    n                  = (s8)arg0;
    D_80114C08.field_5 = arg0;
    val                = n / 3;
    t                  = (s8)val + 1;
    tmp                = t * 10 + 1;
    asm volatile("");
    t   = (s8)(n - val * 3);
    val = tmp + t;
    t   = (val << 2) + val;
    val = t << 1;
    asm volatile("");
    ret = 1;
    if (n < 0xC) {
        cfg = &Wip_SysConfig;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
            cond = 0;
        } else {
            cond = cfg->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData.unknown_850;
        } else {
            table = D_80114BF0;
        }
        table += n;
        ret    = *table;
        if (ret == 0) {
            ret = 1;
        }
        if (cfg->field_25 & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    val = val + ret;
    p   = &D_80114C08;
    asm volatile("");
    p->field_0 = val;
    neg        = -2;
    asm volatile("" : "+r"(neg));
    p->field_3 = neg;
    temp       = func_800A1558(3);
    p->field_2 = temp;
    if (temp <= 0) {
        p->field_2 = 1;
    }
    p->field_A  = 2;
    D_80115768  = 0;
    D_801153F4  = 0;
    p->field_8  = 1;
    p->field_9  = 0;
    D_80114C34  = 0;
    p->field_6 &= 0xFE;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2104);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2BE0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2F60);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A3AF0);

void func_800A45F0(s32 arg0)
{
    WipSysConfig* cfg;
    GpStateC08*   c08;
    s32           val;

    if (arg0 == 0) {
        D_80114F28 = 1;
        return;
    }

    cfg = &Wip_SysConfig;
    asm volatile("" : "+r"(arg0));
    val = D_80114C08.field_0;
    c08 = &D_80114C08;

    if (val < 0x144) {
        if (val < 0x141) {
            if (val < 0x13A) {
                if (val < 0x137) {
                    return;
                }
                goto case_311;
            }
            return;
        }
        goto case_321;
    }
    if (val < 0x19B) {
        return;
    }
    if (val < 0x19E) {
        goto case_411;
    }
    if (val < 0x1A8) {
        if (val < 0x1A5) {
            return;
        }
        goto case_421;
    }
    return;

case_411: {
    GpItemRec8* rec;
    s32         lo;
    s32         tmp;

    val                 = (u16)(val % 10U);
    rec                 = &D_80113E10[val];
    lo                  = *(u8*)&c08->field_C & 0xF;
    tmp                 = rec->field_6;
    *(u8*)&c08->field_C = lo;
    c08->field_10       = tmp;
    if (lo < 2) {
        *(u8*)&c08->field_C = lo + 1;
    }
    *(u8*)&c08->field_C = *(u8*)&c08->field_C | (val << 4);
    return;
}

case_421: {
    GpItemRec8*  rec;
    s32          lo;
    register s32 temp asm("a0");
    s32          tmp;
    register s32 packed asm("v1");

    c08           = &D_80114C08;
    temp          = D_80114C08.field_0;
    temp          = (u16)(temp % 10U);
    rec           = &D_80113E28[temp];
    lo            = c08->field_D & 0xF;
    tmp           = rec->field_6;
    c08->field_D  = lo;
    c08->field_12 = tmp;
    if (lo < 2) {
        c08->field_D = lo + 1;
    }
    c08->field_D = c08->field_D | (packed = temp << 4);
    return;
}

case_311: {
    GpItemRec8*          rec;
    register GpStateC08* p asm("a1");
    register s32         temp asm("a0");
    register s32         lo asm("v0");
    s32                  tmp;

    p = &D_80114C08;
    asm volatile("" : "+r"(p) : : "v0");
    temp = D_80114C08.field_0;
    temp = (u16)(temp % 10U);
    rec  = &D_80113DC8[temp];
    asm volatile("" : "+r"(rec), "+r"(temp));
    lo          = p->field_F & 0xF;
    tmp         = rec->field_6;
    p->field_F  = lo;
    p->field_14 = tmp;
    if (lo == 0) {
        p->field_F = lo + 1;
    }
    p->field_F |= temp << 4;
    func_8010A1B0(1, 0xFF);
    return;
}

case_321: {
    s32          idx;
    s32          min;
    s32          max;
    register s32 result asm("v1");
    s32          flag;
    s32          t;
    s32          r;
    register s32 hi_part asm("a0");
    GpStateF0*   state;
    GpRec16*     recs;

    recs = D_8011398C;
    idx  = (u16)(val % 3U) + 0x16;
    max  = recs[idx].field[5];
    min  = recs[idx].field[4];
    if (min < max) {
        state = &D_801153F0;
        if ((D_801153F0.field_0 == 1 && state->field_6 != 0) || state->field_1 != 0) {
            flag = 1;
        } else {
            flag = 0;
        }
        if (flag != 0) {
            goto do_random;
        }
    }
    result = min;
    goto add_hp;
do_random:
    t = rand() & 0xFF;
    r = t + 1;
    asm volatile("" ::"r"(t), "r"(r));
    hi_part = max * r;
    r       = min * (0x100 - r);
    {
        register s32 sum asm("v0");
        sum = hi_part + r;
        r   = sum >> 8;
    }
    if (r <= 0) {
        r = 1;
    }
    result = r;
add_hp:
    cfg->field_18 += result;
    if (cfg->field_1a < cfg->field_18) {
        cfg->field_18 = cfg->field_1a;
    }
}
}

void func_800A4904(s32 arg0)
{
    GpLinkNode* node;
    GpEnemy*    enemy;
    GpObj54*    obj54;
    u16         val;
    s32         idx;

    for (node = D_80115268; node != NULL; node = node->next) {
        if ((*(s32*)&node->field_4 & 5) != 1) {
            enemy = (GpEnemy*)((u8*)node - OFFSET_OF(GpEnemy, node));
            obj54 = (GpObj54*)enemy;
            if (arg0 == 0) {
                enemy->field_4E |= 0x80;
            } else {
                val  = D_80114C08.field_0;
                idx  = (val / 100U - 1) * 9;
                idx += ((val % 100U) / 10U - 1) * 3;
                idx += val % 10U;
                idx += 0x28000;
                func_800E1C58(obj54, (void*)idx);
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A4A2C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5274);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5574);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A57B0);

void func_800A63B4(s32 arg0, s32 arg1, s32 arg2)
{
    SPRT_8* p;
    s32     otIdx;
    s32     u;

    otIdx      = 0;
    arg0      -= 6;
    p          = (SPRT_8*)D_80071190;
    arg1      -= 8;
    D_80071190 = (DR_TPAGE*)(p + 1);
    p->x0      = arg0;
    p->y0      = arg1;
    if (arg2 == 1) {
        goto case1;
    }
    if (arg2 >= 2) {
        goto default_case;
    }
    if (arg2 != 0) {
        goto default_case;
    }
    p->u0 = 0xA0;
    p->v0 = 0x88;
    goto after_uv;
case1:
    u = 0xA8;
    goto store;
default_case:
    otIdx = -1;
    u     = 0xA0;
store:
    p->u0 = u;
    p->v0 = 0x80;
after_uv:
    p->clut = 0x3C0D;
    setlen(p, 3);
    setcode(p, 0x77);
    addPrim(Gpu_CurrentOt + otIdx - 2, p);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6480);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6A9C);

void func_800A6F38(GpEnemy* arg0, GpHudTrack* arg1)
{
    void**                 scratch;
    register u8*           head asm("v0");
    register GpHudScratch* block asm("s0");
    s32                    val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head     = head - 0x1C;
    block    = (GpHudScratch*)head;
    *scratch = head;
    if (func_800B9D80(0x100000) != 0) {
        block->field_14 = 0x6A;
        block->field_16 = -0x35;
    } else {
        block->field_14 = 0x6A;
        block->field_16 = -0x64;
    }
    if (arg1->field_0 != arg0) {
        arg1->field_0 = arg0;
        arg1->field_4 = block->field_14;
        arg1->field_4 = block->field_16;
    } else {
        block->field_18   = block->field_14 - arg1->field_4;
        block->field_1A   = block->field_16 - arg1->field_6;
        block->field_18 >>= 3;
        block->field_1A >>= 3;
        block->field_14   = arg1->field_4 + block->field_18;
        block->field_16   = arg1->field_6 + block->field_1A;
    }
    if (arg0->field_50 != NULL) {
        val = arg0->field_50->field_4;
        if (arg0->node.field_4 & 8) {
            val = -1;
        }
        func_800A6A9C(block->field_14 - 8, block->field_16, arg0->field_40, val, 1);
    }
    arg1->field_4           = block->field_14;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    arg1->field_6           = block->field_16;
}

void func_800A70A4(void)
{
    register GsCOORDINATE2*  player asm("s2");
    register u8*             head asm("s3");
    register GpXformScratch* block asm("s0");
    SVECTOR                  tmp;
    register GpLinkXform*    node asm("s1");
    GsCOORDINATE2*           coord;

    node = (GpLinkXform*)D_80115268;
    {
        register Task*         slot asm("v1");
        register GameActorExt* extra asm("v1");
        register void**        scratch asm("v0");
        register u8*           newhead asm("v1");

        slot = Game_GetPtrSlot(3);
        if (slot == NULL) {
            return;
        }
        scratch = (void**)G_SCRATCH_HEAD;
        extra   = (GameActorExt*)slot->extra;
        head    = *scratch;
        player  = (GsCOORDINATE2*)extra->field_8;
        newhead = head - 0x48;
        block   = (GpXformScratch*)newhead;
        asm volatile("" : "+r"(block));
        *scratch = newhead;
        TransposeMatrix(&player->workm, &block->mat);
    }
    if (node != NULL) {
        register SVECTOR* out asm("a1");
        register SVECTOR* tmpp asm("a0");
        out  = (SVECTOR*)(head - 8);
        tmpp = &tmp;
        do {
            if ((node->field_4 & 5) != 1) {
                block->vec.vx = *(u16*)&node->src.vx;
                block->vec.vy = *(u16*)&node->src.vy;
                block->vec.vz = *(u16*)&node->src.vz;
                coord         = node->coord;
                tmp           = block->vec;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(tmpp);
                gte_rtv0_real();
                gte_stsv(out);
                block->vec.vx += *(u16*)&node->coord->workm.t[0];
                block->vec.vy += *(u16*)&node->coord->workm.t[1];
                block->vec.vz += *(u16*)&node->coord->workm.t[2];
                block->vec.vx -= *(u16*)&player->workm.t[0];
                block->vec.vy -= *(u16*)&player->workm.t[1];
                block->vec.vz -= *(u16*)&player->workm.t[2];
                tmp            = block->vec;
                gte_SetRotMatrix(&block->mat);
                gte_ldv0(tmpp);
                gte_rtv0_real();
                gte_stsv(out);
                node->dst.vx = block->vec.vx;
                node->dst.vy = block->vec.vy;
                node->dst.vz = block->vec.vz;
            }
            node = node->next;
        } while (node != NULL);
    }
    {
        register void** p asm("v1");
        register u8*    h asm("v0");
        p  = (void**)G_SCRATCH_HEAD;
        h  = *p;
        *p = h + 0x48;
    }
}

void func_800A7320(s16* arg0)
{
    s16*          dest;
    register s32  three asm("s2");
    GameSession*  sess;
    GameSession*  next;
    WipSysConfig* cfg;
    s8            type;
    u8            mode;

    dest  = arg0;
    cfg   = &Wip_SysConfig;
    mode  = Game_Session->field_128;
    three = 3;
    if (mode == three) {
        return;
    }
    if (mode == 0xFF) {
        return;
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    if (*dest != 0) {
        return;
    }
    sess = Game_Session;
    if (sess->field_12D != 0x7F) {
        sess->field_12D = (u8)sess->field_12D - 1;
        next            = Game_Session;
        if (next->field_12D >= 0) {
            return;
        }
        if (cfg->field_18 <= 0) {
            SndEvt_EnqueueType6((next->field_0 << 16) | 0x70000001, 0, 0);
        } else {
            type = Mc_SaveData.field_13;
            if (type == 1) {
                SndEvt_EnqueueType6(((next->field_0 + 0x31) << 16) | 0x70000001, 0, 0);
            } else if (type == three) {
                SndEvt_EnqueueType7(0x50000000, 1);
                SndEvt_EnqueueType6(0x55170008, 0, 0);
            }
        }
    }
    *dest = 1;
}

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

void func_800A7574(GpIdMapC* arg0)
{
    WipSysConfig* cfg;
    GpStateBE8*   be8;
    GpStateC08*   p;

    cfg                     = &Wip_SysConfig;
    be8                     = &D_80114BE8;
    be8->field_0            = cfg->field_18;
    be8->field_4            = cfg->field_1c;
    arg0->field_16          = -1;
    arg0->field_18          = 0;
    p                       = &D_80114C08;
    p->field_10             = 0;
    p->field_C              = 0;
    p->field_12             = 0;
    p->field_D              = 0;
    p->field_E              = 0;
    p->field_14             = 0;
    p->field_F              = 0;
    p->field_16             = 0;
    p->field_17             = 0;
    p->field_A              = 0;
    Game_Session->field_126 = 0;
    D_8010CA28              = 0;
    Display_State.field_12f = 1;
    p->field_6             &= ~2;
}

void func_800A7600(void)
{
    DisplayState* ds;

    srand(1);
    D_80070F60            = 0;
    ds                    = &Display_State;
    ds->field_8           = 0;
    Display_State.field_0 = 0;
    ds->field_4           = 0;
    ds->field_14          = 0;
    ds->field_c           = 0;
    ds->field_10          = 0;
    if (ds->field_12c == 0x10) {
        D_80114C38 = (GpPadReplay*)0x80600E4C;
    } else {
        D_80114C38 = (GpPadReplay*)((u8*)D_8005C374 + 0xD4C);
    }
    D_80114C02              = 0xFFFF;
    D_80114C04              = 1;
    Pad_RemapState->field_8 = -1;
}

void func_800A76A4(Task* arg0)
{
    GameSession* session;
    GpStateBD8*  p;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        arg0->killCountdown = 0;
        func_800A7320(&arg0->killCountdown);
        session            = Game_Session;
        D_80114C08.field_3 = 0;
        if (session->field_128 != 3) {
            p          = &D_80114BD8;
            p->field_0 = 0;
            p->field_1 = 0;
            p->field_2 = (s8)session->field_12E;
            Task_Spawn(1, 0x31, 0, (s32)p);
        }
        arg0->spawnArg1 = 0;
        arg0->state++;
    }
}

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

void func_800A77B4(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_80093830;
    sp.funcs[arg0->state](arg0);
}

void func_800A7824(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 == 0) {
        func_800A4A2C(0, arg1, arg2, 5);
    }
}

void func_800A784C(GpHudTrack* arg0)
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

void func_800A7918(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (arg0->spawnArg1 == 2) {
        if (arg0->state == 0) {
            Ui_UpdateLayoutSize((UiPanel*)obj, Text_MeasureWidth(D_8010CA18) + 0xA, 0);
            obj->field_C -= 0xF;
            obj->field_E += 9;
            arg0->state++;
        }
        Text_DrawPrompt(obj, obj->field_1C + 6, 7, D_8010CA18, 0x606060, 1, 0);
    } else {
        Text_DrawPrompt(obj, obj->field_1C + 6, 7, D_8010CA08, 0x606060, 1, 0);
    }
}

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

s32 func_800A7B20(s32 arg0)
{
    WipSysConfig* p;
    s32           cond;
    s32           ret;
    u8*           table;

    ret = 1;
    if (arg0 < 0xC) {
        p = &Wip_SysConfig;
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
            cond = 0;
        } else {
            cond = p->field_26 == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData.unknown_850;
        } else {
            table = D_80114BF0;
        }
        ret = table[arg0];
        if (ret == 0) {
            ret = 1;
        }
        if (p->field_25 & 0x80) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    return ret;
}

s32 func_800A7BBC(s32 arg0, s32 arg1)
{
    WipSysConfig* p;
    McSaveData*   save;
    s32           cond;
    u8*           table;

    p = &Wip_SysConfig;
    if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) != 0x1140000) {
        cond = 0;
    } else {
        cond = p->field_26 == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData.unknown_850;
    } else {
        table = D_80114BF0;
    }
    if (arg1 != 0) {
        save = &Mc_SaveData;
        do {
            if (arg1 > 0) {
                do {
                    arg0++;
                    if (arg0 >= 0xC) {
                        arg0 = 0;
                    }
                } while (table[arg0] == 0 && save->field_5C2 == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->field_5C2 == 0);
                arg1++;
            }
        } while (arg1 != 0);
    }
    return arg0;
}

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

s32 func_800A7E5C(s32 arg0)
{
    GpActorWork*  work;
    GameActor*    actor;
    WipSysConfig* p;
    s32           flag;

    flag = 0;
    work = D_80115760[0];
    if (work != NULL) {
        actor = work->actor;
        p     = &Wip_SysConfig;
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (Game_Session->field_13A == 0) {
                    if (p->field_24 == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (arg0 == 0) {
        if (D_80114C08.field_6 & 2) {
            flag = 0;
        }
    }
    if (flag != 0) {
        if (D_8010CA28 <= 0) {
            if (D_801153F1 == 0) {
                return 1;
            }
        }
    }
    return 0;
}

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

void func_800A8654(Task* task)
{
    VECTOR*        vec;
    GsCOORDINATE2* src;
    GsCOORDINATE2* c1;
    GsCOORDINATE2* c2;
    GsCOORDINATE2* c3;
    TmdObject*     extra;
    s32            i;
    s32            j;

    i              = 0;
    c1             = &D_80070E90;
    extra          = task->extra;
    vec            = (VECTOR*)task->idMap;
    src            = extra->field_8;
    c1->coord.t[0] = vec->vx;
    c2             = &D_80070E40;
    c1->coord.t[1] = vec->vy;
    c1->coord.t[2] = vec->vz;

    for (; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            *(s16*)((i * 6 + j * 2) + (s32)c2->coord.m) = src->coord.m[i][j];
        }
    }

    c3             = &D_80070F10;
    c3->coord.t[0] = src->coord.t[0];
    c3->coord.t[1] = src->coord.t[1];
    c3->coord.t[2] = src->coord.t[2];

    D_80070E90.flg = 0;
    D_80070E40.flg = 0;
    D_80070F10.flg = 0;
    Task_Kill(task);
}

void func_800A8724(void)
{
    GameSessionFrom4* sess;
    GpCb2CTbl*        tbl;
    GpCb2CRec*        recs;
    GpCb2CRec*        rec;
    GsCOORDINATE2*    c1;
    MATRIX*           rot;
    VECTOR3*          trans;
    u8                idx;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    tbl  = D_8010CB2C[sess->field_3 - 1];
    recs = tbl->field_0[sess->field_2 - 1];
    idx  = func_800AD284();

    rot   = &D_80070E44;
    trans = &D_80070F28;
    c1    = &D_80070E90;
    rec   = (GpCb2CRec*)(idx * sizeof(GpCb2CRec) + (s32)recs);

    *(GBytes18*)rot = *(GBytes18*)(rec - 1);
    *trans          = *(VECTOR3*)&(rec - 1)->mtx.t;

    rec--;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    Display_State.field_110 = rec->field_20;
    gte_SetGeomScreen(rec->field_20);
    gte_SetGeomOffset(0, 0);

    D_80070E90.flg                                                          = 0;
    ((GsCOORDINATE2*)((u8*)rot - OFFSET_OF(GsCOORDINATE2, coord)))->flg     = 0;
    ((GsCOORDINATE2*)((u8*)trans - OFFSET_OF(GsCOORDINATE2, coord.t)))->flg = 0;
}

void func_800A8864(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2)
{
    register void**           scratch asm("s0");
    register u8*              head asm("t0");
    register MATRIX*          src asm("a3");
    register GpRelMatScratch* tmp asm("a0");
    register short            t4 asm("t4");
    register short            t5 asm("t5");
    register short            t6 asm("t6");
    VECTOR*                   vec;
    VECTOR*                   out;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    src      = arg0;
    tmp      = (GpRelMatScratch*)(head - 0x30);
    *scratch = tmp;
    __asm__ volatile("" : "+r"(tmp), "+r"(src), "+r"(head));

    t4               = src->m[0][0];
    t5               = src->m[1][0];
    t6               = src->m[2][0];
    tmp->mat.m[0][0] = t4;
    tmp->mat.m[0][1] = t5;
    tmp->mat.m[0][2] = t6;

    t4               = src->m[0][1];
    t5               = src->m[1][1];
    t6               = src->m[2][1];
    tmp->mat.m[1][0] = t4;
    tmp->mat.m[1][1] = t5;
    tmp->mat.m[1][2] = t6;

    t4               = src->m[0][2];
    t5               = src->m[1][2];
    t6               = src->m[2][2];
    tmp->mat.m[2][0] = t4;
    tmp->mat.m[2][1] = t5;
    tmp->mat.m[2][2] = t6;

    gte_MulMatrix0_real(&tmp->mat, arg1, arg2);

    tmp->vec.vx = arg1->t[0] - src->t[0];
    tmp->vec.vy = arg1->t[1] - src->t[1];
    tmp->vec.vz = arg1->t[2] - src->t[2];
    vec         = (VECTOR*)(head - 0x10);
    out         = (VECTOR*)arg2->t;
    ApplyMatrixLV(&tmp->mat, vec, out);

    *scratch = (u8*)*scratch + 0x30;
}

s32 func_800A8A1C(s32 arg0)
{
    return Task_Spawn(0, 0xF, 0, arg0) != NULL;
}

void func_800A8A48(GpCb2CRec* arg0)
{
    GsCOORDINATE2* c1;
    MATRIX*        rot;
    VECTOR3*       trans;

    rot   = &D_80070E44;
    trans = &D_80070F28;
    c1    = &D_80070E90;

    *(GBytes18*)rot = *(GBytes18*)arg0;
    *trans          = *(VECTOR3*)&arg0->mtx.t;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    Display_State.field_110 = arg0->field_20;
    gte_SetGeomScreen(arg0->field_20);
    gte_SetGeomOffset(0, 0);

    D_80070E90.flg                                                          = 0;
    ((GsCOORDINATE2*)((u8*)rot - OFFSET_OF(GsCOORDINATE2, coord)))->flg     = 0;
    ((GsCOORDINATE2*)((u8*)trans - OFFSET_OF(GsCOORDINATE2, coord.t)))->flg = 0;
}

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

void func_800A8C74(Task* task)
{
    GsCOORDINATE2* c1;
    MATRIX*        rot;
    VECTOR3*       trans;
    GpCb2CRec*     rec;

    rot   = &D_80070E44;
    trans = &D_80070F28;
    c1    = &D_80070E90;
    rec   = task->spawnArg2;

    *(GBytes18*)rot = *(GBytes18*)rec;
    *trans          = *(VECTOR3*)&rec->mtx.t;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    Display_State.field_110 = rec->field_20;
    gte_SetGeomScreen(rec->field_20);
    gte_SetGeomOffset(0, 0);

    D_80070E90.flg                                                          = 0;
    ((GsCOORDINATE2*)((u8*)rot - OFFSET_OF(GsCOORDINATE2, coord)))->flg     = 0;
    ((GsCOORDINATE2*)((u8*)trans - OFFSET_OF(GsCOORDINATE2, coord.t)))->flg = 0;
    Task_Kill(task);
}

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

void func_800A8DC0(s32 arg0)
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
    if (arg0 == 0) {
        Task_Spawn(0, 0x17, 0, 0);
    }
    if (arg0 == 1) {
        Task_SpawnOnDefaultListA(0, 0x17, 0, 0);
    }
}

void func_800A8E8C(Task* task)
{
    GameSession* sess;
    McSaveData*  save;
    CdCmdQueue*  q;
    s32          loc;

    Game_Session->field_4D = 0;
    if (task->state == 0) {
        task->state = 3;
    }
    save = &Mc_SaveData;
    if (task->spawnArg1 != save->field_4) {
        Game_Session->field_52 = 1;
    }
    sess = Game_Session;
    if (sess->field_52 != 0) {
        q = &CdCmd_Queue;
        if ((q->field_214 == 0) || (q->field_218 == 0)) {
            sess->field_4 = save->field_4;
            Pad_SetCooldown(0);
            func_800A8B6C();
            if (Display_SpawnWithOtSmall(0, 0x1E, 0, 0) != 0) {
                loc                 = (u8)Game_Session->field_4;
                task->killCountdown = 2;
                task->spawnArg1     = loc;
                if (task->state == 3) {
                    task->state = 1;
                }
            }
        }
    }
    if (task->state == 1) {
        Display_AcquireRef();
        task->state += 1;
    }
    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown == 0) {
            Display_ReleaseRef();
            Game_Session->field_4D = 1;
            task->state            = 3;
        }
    }
}

void func_800A9010(Task* task)
{
    DisplayState*     ds;
    CdCmdQueue*       q;
    GameSessionFrom4* sess;
    u8                param1[8];
    u8                param2[8];

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    q    = &CdCmd_Queue;
    if (task->spawnArg1 != 0) {
        Display_State.field_103 = 2;
    }
    ds = &Display_State;
    if (ds->field_114 == ds->frameMode) {
        DrawSync(0);
        SetDrawStp(&D_80114C50, 0);
        DrawPrim(&D_80114C50);
        ds->field_103 = 2;
        if (q->field_214 != 0) {
            Mdec_ResolveStreamBuffer(&Game_Session->field_4);
            task->state = 5;
        } else {
            D_80114C40 = Stream_FindSlot(&Game_Session->field_4, 0, 1);
            if (D_80114C40 >= 0) {
                func_800B62D4();
                q->field_210 = 1;
            } else {
                if (q->field_210 != 0) {
                    func_800B56AC();
                    q->field_210 = 0;
                }
            }
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                CdCmd_ActivatePhase1();
                task->state += 1;
                func_800A954C(task);
            } else {
                param1[3] = sess->field_3;
                param1[2] = sess->field_2;
                param1[0] = func_800AD284();
                param2[0] = 1;
                param2[1] = 0;
                param2[2] = 0;
                param2[3] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                task->state += 2;
                func_800A91CC(task);
            }
        }
    }
}

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
