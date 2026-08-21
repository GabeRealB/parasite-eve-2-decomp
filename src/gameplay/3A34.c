#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#define gte_rtv0_real()   __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()   __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtirtr_real() __asm__ volatile("nop; nop; .word 0x4A498012")
#define gte_gpf12_real()  __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_gpl12_real()  __asm__ volatile("nop; nop; .word 0x4BA8003E")

void func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);

extern s32 D_80070F60;

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D5B14);

s32 func_800D6170(s32 arg0, GpItemRec* arg1)
{
    WipSysConfig* cfg;
    GpItemScan*   scan;
    s32           ret;
    s32           val;

    ret = 1;
    cfg = &Wip_SysConfig;
    if (arg0 != 0) {
        if ((u32)(arg0 - 0x80) < 0x20U) {
            ret = 0;
        } else if ((u32)(arg0 - 0xA0) < 0x20U) {
            scan = &Mc_SaveData.field_5BC;
            val  = arg1->field_2 - func_800BAFF4(scan, arg0);
            if (val > 0) {
                if (func_800B715C(scan, cfg->field_21 + 0x7F, arg0, 0) == 0) {
                    ret = 0;
                }
            }
        } else if ((u32)(arg0 - 1) < 0x41U) {
            switch (arg0) {
                case 1:
                case 2:
                case 3:
                    if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 4:
                    if (D_80114C08.field_16 == 0) {
                        ret = 0;
                    }
                    break;
                case 8:
                    if ((s8)D_80114C08.field_17 == 0) {
                        ret = 0;
                    }
                    break;
                case 5:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    } else if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 6:
                case 7:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    }
                    break;
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x41:
                    ret = 0;
                    break;
                case 0x3D:
                    if (cfg->field_1c < cfg->field_1e) {
                        ret = 0;
                    } else if (cfg->field_18 < cfg->field_1a) {
                        ret = 0;
                    }
                    break;
                case 0x3E:
                    if (func_800B9D80(0x140) == 0) {
                        ret = 0;
                    }
                    break;
            }
        }
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6334);

/* After Armor/Attachments from func_800D6334 so overlay .rodata stays packed. */
const char D_80097454[] = {
    'W',
    'e',
    'a',
    'p',
    'o',
    'n',
    '\0',
    0x60,
};

s32 func_800D68C4(s32 arg0)
{
    s32 val;

    val = D_8010F88C;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    D_8010F88C = 0;
    return func_800B715C(&Mc_SaveData.field_5BC, arg0, val, -1);
}

GpItemRec* func_800D6910(s32 arg0)
{
    GpItemScan* scan;
    GpItemRec*  table;
    s32         i;
    s32         count;
    GpItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.field_5BC;
    table = func_800BB500(scan);
    i     = 0;
    table = &table[scan->field_0];
    count = scan->field_1;
    for (; i < count; i++) {
        if (table->field_0 == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

GpItemRec* func_800D6994(s32 arg0)
{
    GpItemScan* scan;
    GpItemRec*  table;
    s32         i;
    s32         count;
    GpItemRec*  rec;

    rec   = NULL;
    scan  = &Mc_SaveData.field_5BC;
    table = func_800BB500(scan);
    i     = 0;
    table = &table[scan->field_0];
    count = scan->field_1;
    for (; i < count; i++) {
        if ((s8)table->field_1 == arg0 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

GpItemRec* func_800D6A24(s32 arg0, GpItemScan* arg1)
{
    GpItemRec* table;
    s32        i;
    s32        count;
    GpItemRec* rec;

    rec   = NULL;
    table = func_800BB500(arg1);
    i     = 0;
    table = &table[arg1->field_0];
    count = arg1->field_1;
    for (; i < count; i++) {
        if (table->field_0 == arg0) {
            rec = table;
        }
        table++;
    }
    return rec;
}

void func_800D6AA4(Task* arg0)
{
    UiPanel* panel;
    s32      x;
    s32      y;

    panel            = arg0->spawnArg2;
    panel->field_C.y = 0x1C - Display_State.vramYOffset;
    Ui_InsetLayout(panel, NULL, NULL, 0);
    x = (s16)panel->field_1C;
    y = (s16)panel->field_18;
    func_800C2140(panel, x + 2, y + 0xF, 1);
    Ui_DrawText(panel, D_80097454);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6B20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6E5C);

s32 func_800D70E4(GpObj44* arg0, VECTOR3* arg1)
{
    register void**         scratch asm("a3");
    register u8*            head;
    register GpAttnScratch* tmp asm("a0");
    register GpAttnScratch* block asm("t0");
    register s32            result asm("a1");
    register s32            lum;
    register GpObj44*       obj asm("t1");
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    s32                     sq;
    u16                     scale;
    u8*                     ptr;

    obj                                     = arg0;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    vx                                      = obj->field_38.vx;
    head                                    = *scratch;
    vx                                     -= arg1->vx;
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = (obj->field_38.vy - arg1->vy) >> 1;
    block->vec.vz                           = (obj->field_38.vz - arg1->vz) >> 1;
    sq                                      = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->distSq                           = sq;
    sq                                      = obj->field_5C;
    lum                                     = sq * sq;
    sq                                      = lum >> 2;
    lum                                     = block->distSq;
    result                                  = 0;
    block->outerSq                          = sq;
    *scratch                                = block;
    tooFar                                  = (u32)sq < (u32)lum;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (obj->field_58 * obj->field_58) >> 2;
        r              = obj->field_50;
        g              = obj->field_52;
        b              = obj->field_54;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            asm volatile("" : "+r"(temp));
            lum = inner;
            asm volatile("" : "+r"(lum));
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    obj->field_4A         = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x20;
    return result;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D72D0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D759C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D78A4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D7A9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8684);

void func_800D8C0C(GpEnemy* arg0, MATRIX* arg1, s32 arg2)
{
    s32 i;
    s32 val;

    if (arg2 == 1) {
        goto case1;
    } else if (arg2 < 2) {
        goto def;
    } else if (arg2 == 2) {
        goto case2;
    } else if (arg2 == 3) {
        goto case3;
    } else {
        goto def;
    }

case1: {
    s32 t;
    for (i = 0; i < 3; i++) {
        t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
        arg1->m[0][i] = t * 4;
        arg1->m[1][i] = t * 2;
        arg1->m[2][i] = t;
    }
}
    return;

case3:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x180;
    arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = 0x100;
    arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x100;
    return;

case2:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = 0;
    arg1->m[0][1] = 0;
    arg1->m[0][2] = 0;
    arg1->m[1][0] = 0;
    arg1->m[1][1] = 0;
    arg1->m[1][2] = 0;
    arg1->m[2][0] = 0;
    arg1->m[2][1] = 0;
    arg1->m[2][2] = 0;
    return;

def:
    if ((arg0->field_4E & 0x80) && (arg0->field_4B == 0)) {
    flicker:
        val = rsin(Display_State.field_14 << 6) + 0x1800;
        if ((Display_State.field_14 & 1) == 0) {
            val >>= 1;
        }
        arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x200;
        arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = val;
        arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x200;
        arg0->field_4E                               &= 0x7F;
    } else if (arg0->field_4C & 0xC) {
        s32 t;
        for (i = 0; i < 3; i++) {
            t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
            arg1->m[0][i] = t * 3;
            arg1->m[1][i] = t;
            arg1->m[2][i] = t * 3;
        }
    }
}

void func_800D8EA0(GpEnemy* arg0, VECTOR* arg1)
{
    GameActorExt*   extra;
    MATRIX*         colorMtx;
    s32             mode;
    u8*             head;
    GpColorScratch* block;
    SVECTOR*        col0;
    SVECTOR*        col1;
    GpMtxCol*       src;
    GpMtxCol*       dst;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = (GameActorExt*)arg0->task->extra;
    colorMtx = extra->field_20;
    mode     = arg0->field_4E & 3;
    if ((!(extra->field_C & 0x80) && (extra->field_18 != NULL)) || (Game_Session->field_65 != 1)) {
        {
            register void**          scratch asm("v1");
            register GpColorScratch* tmp asm("v0");

            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            tmp      = (GpColorScratch*)(head - 0x30);
            block    = tmp;
            *scratch = tmp;
        }
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->field_4F <= 0) {
            func_800D8C0C(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            func_800D8C0C(arg0, colorMtx, mode);
            func_800D8C0C(arg0, &block->mtx, (arg0->field_4E >> 2) & 3);
            i    = 0;
            col0 = (SVECTOR*)(head - 0x10);
            col1 = (SVECTOR*)(head - 8);
            src  = (GpMtxCol*)colorMtx;
            w0   = (s8)arg0->field_4F << 8;
            dst  = (GpMtxCol*)block;
            w1   = 0x1000 - w0;
            do {
                block->col0.vx = src->x;
                asm volatile("" : "+r"(src));
                block->col0.vy = src->y;
                asm volatile("" : "+r"(src));
                block->col0.vz = src->z;
                block->col1.vx = dst->x;
                asm volatile("" : "+r"(dst));
                block->col1.vy = dst->y;
                asm volatile("" : "+r"(dst));
                block->col1.vz = dst->z;
                gte_lddp(w1);
                gte_ldsv(col0);
                gte_gpf12_real();
                gte_lddp(w0);
                gte_ldsv(col1);
                gte_gpl12_real();
                gte_stsv(col0);
                src->x = block->col0.vx;
                dst    = (GpMtxCol*)&dst->_0;
                src->y = block->col0.vy;
                i++;
                src->z = block->col0.vz;
                src    = (GpMtxCol*)&src->_0;
            } while (i < 3);
            if (D_801153F4 == 0) {
                arg0->field_4F--;
            }
        }
        *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
    }
}

void func_800D9138(GpObj44* arg0)
{
    register void**         scratch asm("a1");
    register u8*            head asm("a0");
    register GpAttnScratch* tmp asm("v1");
    register GpAttnScratch* block asm("a2");
    register s32            result asm("t0");
    register s32            lum asm("v1");
    s32                     tooFar;
    s32                     r;
    s32                     g;
    s32                     b;
    s32                     dist;
    s32                     inner;
    s32                     vx;
    u16                     scale;
    u8*                     ptr;

    result                                  = 0;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    vx                                      = arg0->field_18.vx;
    head                                    = *scratch;
    vx                                    >>= 1;
    tmp                                     = (GpAttnScratch*)(head - 0x20);
    ((GpAttnScratch*)(head - 0x20))->vec.vx = vx;
    block                                   = tmp;
    block->vec.vy                           = arg0->field_18.vy >> 1;
    block->vec.vz                           = arg0->field_18.vz >> 1;
    block->distSq                           = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq                          = (arg0->field_5C * arg0->field_5C) >> 2;
    tooFar                                  = (u32)block->outerSq < (u32)block->distSq;
    *scratch                                = block;
    block->scale                            = 0;
    if (!tooFar) {
        block->innerSq = (arg0->field_58 * arg0->field_58) >> 2;
        r              = arg0->field_50;
        g              = arg0->field_52;
        b              = arg0->field_54;
        block->scale   = 0x1000;
        lum            = (r * 8 + g * 6 + b * 2) >> 8;
        dist           = block->distSq;
        inner          = block->innerSq;
        result         = lum + 0xF00;
        if ((u32)inner < (u32)dist) {
            s32 temp;

            temp = block->outerSq;
            asm volatile("" : "+r"(temp));
            lum = inner;
            asm volatile("" : "+r"(lum));
            block->outerSq = temp - inner;
            block->distSq -= lum;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                lum          = block->scale * result;
                result       = (u32)lum >> 12;
            }
        }
    }
    scale                 = block->scale;
    ptr                   = *(u8**)G_SCRATCH_HEAD;
    arg0->field_38.vx     = result;
    arg0->field_4A        = scale;
    *(u8**)G_SCRATCH_HEAD = ptr + 0x20;
}

void func_800D930C(GpObj4C* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->field_4E;
    arg1 &= 3;
    if ((val & 3) != arg1) {
        arg0->field_4E = (val & 0xF0) | ((val & 3) << 2) | arg1;
        arg0->field_4F = 0x10;
    }
}

s32 func_800D9340(GpObj38* arg0)
{
    s32 val;

    val = arg0->field_24.t[2] - Display_State.field_110;
    if (val >= 0x7FFF) {
        val = 0x7FFF;
    }
    if (val < -0x7FFF) {
        val = -0x7FFF;
    }
    return val >> 8;
}

s32 func_800D937C(GpObj38* arg0)
{
    void**        scratch;
    u8*           head;
    GpPanScratch* block;
    SVECTOR*      vec;
    s32           ret;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpPanScratch*)(head - 0x18);
    *scratch = block;
    vec      = &block->vec;
    gte_SetRotMatrix(&arg0->field_24);
    gte_SetTransMatrix(&arg0->field_24);
    block->vec.vz = 0;
    block->vec.vy = 0;
    block->vec.vx = 0;
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpPanScratch*)(head - 0x18))->sx);
    gte_stdp(&((GpPanScratch*)(head - 0x18))->p);
    gte_stflg(&((GpPanScratch*)(head - 0x18))->flag);
    gte_stszotz(&((GpPanScratch*)(head - 0x18))->otz);
    if (block->flag >= 0) {
        if (block->sx >= 0xA0) {
            block->sx = 0x9F;
        }
        if (block->sx < -0x9F) {
            block->sx = -0xA0;
        }
        ret = -block->sx / 10;
    } else {
        ret = 0;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    return -ret;
}

void func_800D94B8(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        D_80114F18 = 0;
        return;
    }
    D_80114F18 = 1;
    D_80114F20 = *arg0;
}

void func_800D9504(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        D_80115250 = 0;
        return;
    }
    D_80115250 = 1;
    D_80115258 = *arg0;
}

void func_800D9550(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->field_20;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

GpCbA4Vec* func_800D957C(GameSessionFrom4* arg0)
{
    GpCbA4Rec** mid;
    GpCbA4Rec*  rec;
    GpCbA4Vec*  result;
    GpCbA4Vec*  table;

    mid = D_8010CBA4[arg0->field_3 - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    result = (GpCbA4Vec*)&D_8010F9E4;
    if (rec != NULL) {
        table = rec->field_4;
        if (table != NULL) {
            if (table->field_0 >= arg0->field_0) {
                result = &table[arg0->field_0];
            }
        }
    }
    return result;
}

s32 func_800D9618(void)
{
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 8; i++) {
        if (D_80114F30[i].field_0 != 0) {
            count++;
        }
    }
    return count;
}

s32 func_800D9654(GameSessionFrom4* arg0)
{
    GpCbA4Rec** mid;
    GpCbA4Rec*  rec;
    s32         result;

    result = 0;
    mid    = D_8010CBA4[arg0->field_3 - 1];
    rec    = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    if (rec != NULL) {
        result = rec->field_0;
    }
    return result;
}

void func_800D96C8(Task* arg0)
{
    TaskFunc funcs[2] = { func_800D9D18, func_800D8684 };

    funcs[arg0->state](arg0);
}

s32 func_800D9718(GpObj44* arg0)
{
    s16 val;

    val = arg0->field_44;
    if (val != 0 && (u8)Game_Session->field_4 != val) {
        return 0;
    }
    arg0->field_4A = 0x1000;
    return ((arg0->field_50 * 8 + arg0->field_52 * 6 + arg0->field_54 * 2) >> 8) + 0xF00;
}

s32 func_800D9788(GpObj38* arg0)
{
    return arg0->field_24.t[0];
}

void func_800D9794(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("a2");

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpLightScratch*)(head - 0x1C);
    dir      = (SVECTOR*)(head - 0xC);
    *scratch = block;
    dirMtx   = arg3->field_1C;
    colorMtx = arg3->field_20;
    Gfx_NormalizeLightDir((VECTOR*)((GpObj38*)arg1)->field_24.t, dir);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void func_800D98C4(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("t0");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpLightScratch*)(head - 0x1C);
    dir          = (SVECTOR*)(head - 0xC);
    dirMtx       = arg3->field_1C;
    colorMtx     = arg3->field_20;
    block->in.vx = arg2->vx - ((GpObj38*)arg1)->field_24.t[0];
    block->in.vy = arg2->vy - ((GpObj38*)arg1)->field_24.t[1];
    *scratch     = block;
    block->in.vz = arg2->vz - ((GpObj38*)arg1)->field_24.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void func_800D9A30(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3)
{
    void**                   scratch;
    u8*                      head;
    register GpLightScratch* block asm("s0");
    SVECTOR*                 dir;
    MATRIX*                  dirMtx;
    MATRIX*                  colorMtx;
    register s32             val asm("v0");
    register s32             scale asm("t0");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpLightScratch*)(head - 0x1C);
    dir          = (SVECTOR*)(head - 0xC);
    dirMtx       = arg3->field_1C;
    colorMtx     = arg3->field_20;
    block->in.vx = arg2->vx - ((GpObj38*)arg1)->field_24.t[0];
    block->in.vy = arg2->vy - ((GpObj38*)arg1)->field_24.t[1];
    *scratch     = block;
    block->in.vz = arg2->vz - ((GpObj38*)arg1)->field_24.t[2];
    Gfx_NormalizeLightDir(&block->in, dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    val          = arg1->field_4A;
    scale        = val;
    block->scale = val;
    gte_lddp(scale);
    gte_ldsv(&arg1->field_50);
    gte_gpf12_real();
    gte_stsv(dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    *scratch = (u8*)*scratch + 0x1C;
}

void func_800D9B9C(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpRec12* rec;
    GpRec12* next;

    if (arg1 <= 0) {
        return;
    }

    rec = (GpRec12*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (rec->field_4 < arg1) {
        if (arg4 < 3) {
            rec[1] = *rec;
        }
        if (arg4 > 0) {
            func_800D9B9C(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->field_4 = arg1;
            arg0->field_0 = arg2;
            arg0->field_8 = arg3;
        }
    } else if (arg4 < 3) {
        next           = rec + 1;
        next->field_4  = arg1;
        rec[1].field_0 = arg2;
        next->field_8  = arg3;
    }
}

void func_800D9C3C(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

GpCbA4Rec* func_800D9C64(GameSessionFrom4* arg0)
{
    GpCbA4Rec** mid;
    GpCbA4Rec*  rec;

    mid = D_8010CBA4[arg0->field_3 - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->field_2 - 1];
        if (rec != NULL) {
            rec = &rec[arg0->field_1 - 1];
        }
    }
    return rec;
}

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

void func_800D9CE8(GBytes8* arg0)
{
    *arg0 = D_8010F9E4;
}

void func_800D9D18(Task* arg0)
{
    GpActorWork*     slot;
    GameActorExt*    extra;
    GameActor*       actor;
    register s32     result asm("v1");
    s32              i;
    register MATRIX* mtxA asm("a2");
    register MATRIX* mtxB asm("a1");
    register s32     addr asm("v0");

    slot  = Game_GetPtrSlot(3);
    extra = slot->extra;
    if (slot != NULL) {
        result = func_800D9654((GameSessionFrom4*)&Game_Session->field_4);
        i      = 0;
        if (result == 0) {
            Task_Kill(arg0);
            return;
        }
        addr = (s32)&D_80114E98;
        __asm__ volatile("" : "+r"(addr));
        mtxA = (MATRIX*)addr;
        addr = (s32)&D_80114EB8;
        __asm__ volatile("" : "+r"(addr));
        mtxB            = (MATRIX*)addr;
        arg0->spawnArg2 = (void*)result;
        extra->field_1C = mtxA;
        extra->field_20 = mtxB;
        actor           = slot->actor;
        D_80114F18      = 0;
        D_80115250      = 0;
        D_80114F28      = 0;
        do {
            extra           = (&actor->field_920)[i]->extra;
            extra->field_1C = mtxA;
            extra->field_20 = mtxB;
            i++;
        } while (i < 2);
        arg0->state++;
        func_800D8684(arg0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9DFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA2A0);

void func_800DA6E8(void* arg0, s32 arg1)
{
    GpSlot70* found;
    s32       i;
    GpSlot70* p;

    found = NULL;
    i     = 0;
    p     = D_80115270;
loop:
    if (p->field_0 == arg0) {
        if (arg1 >= 0) {
            if (p->field_4 >= 0) {
                found = p;
                goto done;
            }
            p++;
        } else if (p->field_4 < 0) {
            found = p;
            goto done;
        } else {
            p++;
        }
    } else {
        p++;
    }
    i++;
    if (i < 0x20) {
        goto loop;
    }
done:
    if (found == NULL) {
        i = 0;
        p = D_80115270;
    loop2:
        if (p->field_0 == NULL) {
            found          = p;
            p->field_0     = arg0;
            found->field_4 = 0;
        } else {
            i++;
            p++;
            if (i < 0x20) {
                goto loop2;
            }
        }
        if (found != NULL) {
            goto update;
        }
    } else {
    update:
        found->field_6  = 0x14;
        found->field_4 += arg1;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA7B8);

void func_800DAB38(GpLinkNode* node)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GameActor*             actor;
    GpLinkNode**           list;

    i = 0;
    p = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->actor;
            if (actor->field_90C == node) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);

    if (node->field_6 == 1) {
        list = &D_80115268;
        if (D_80115268 != node) {
            do {
                if (*list == NULL) {
                    goto done;
                }
                list = &(*list)->next;
            } while (*list != node);
        }
        if (*list != NULL) {
            *list = node->next;
        }
    done:
        node->field_6 = 0;
        node->field_5 = 0;
    }
}

void func_800DABEC(GpLinkNode* node)
{
    GpLinkNode** p;
    register s32 val asm("v0");

    if (node->field_6 == 0) {
        p = &D_80115268;
        if (D_80115268 != NULL) {
            do {
                p = &(*p)->next;
            } while (*p != NULL);
        }
        *p            = node;
        val           = node->field_4;
        node->next    = NULL;
        node->field_5 = 0;
        node->field_6 = 1;
    } else {
        val = node->field_4;
    }
    node->field_4 = val & 0xFE;
}

s32 func_800DAC54(GpLinkNode* arg0)
{
    s32                    mask;
    s32                    i;
    s32                    one;
    GpActorWork* volatile* p;
    GpActorWork*           work;

    mask = 0;
    i    = mask;
    one  = 1;
    p    = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            if (work->actor->field_90C == arg0) {
                mask |= one << i;
            }
        }
        i++;
        p++;
    } while (i < 2);
    return mask;
}

void func_800DACAC(GpLinkNode* arg0)
{
    GpActorWork* work;
    GameActor*   actor;
    GpLinkNode*  node;
    u8           val;

    work = D_80115760[0];
    if (work != NULL) {
        actor = work->actor;
        node  = actor->field_90C;
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg0;
    }
    val           = arg0->field_4;
    arg0->field_5 = 1;
    arg0->field_4 = val & 0xFE;
}

void func_800DACF8(GpLinkNode* arg0)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GameActor*             actor;
    u8                     val;

    i = 0;
    p = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->actor;
            if (actor->field_90C == arg0) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);
    val           = arg0->field_4;
    arg0->field_5 = 0;
    arg0->field_4 = val | 1;
}

void* func_800DAD54(GpActorWork* arg0)
{
    VECTOR3 pos;

    return func_800DA2A0(arg0, &pos, 0);
}

void* func_800DAD78(GpActorWork* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, p, flag);
}

void* func_800DADE4(GpActorWork* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, pos, flag);
}

/* After D_8009745C from func_800D8684 so overlay .rodata stays packed. */
const char D_80097460[] = {
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    '#',
    'g',
    'e',
    't',
    '_',
    'l',
    'o',
    'c',
    'k',
    '_',
    'p',
    'o',
    's',
    ' ',
    '-',
    '-',
    '-',
    '>',
    ' ',
    'N',
    'U',
    'L',
    'L',
    '!',
    '!',
    '!',
    '\n',
    '\0',
    0x8C,
    0x16,
};

void func_800DAE50(GpLockPos* arg0, VECTOR3* out)
{
    GsCOORDINATE2* world;
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    MATRIX*        mat;

    if (arg0 == NULL) {
        printf(D_80097460);
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return;
    }

    coord = arg0->coord;
    world = &D_80070F10;
    if (coord == world) {
        out->vx = arg0->pos.vx;
        out->vy = arg0->pos.vy;
        out->vz = arg0->pos.vz;
        return;
    }

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x28;
    func_80098F58(coord);
    mat = (MATRIX*)(head - 0x20);
    func_800A8864(&world->workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    gte_SetTransMatrix(mat);
    gte_ldlvl(&arg0->pos);
    gte_rtirtr_real();
    gte_stlvl(out);
    *scratch = (u8*)*scratch + 0x28;
}

void func_800DAF98(void)
{
    s32       i;
    GpSlot70* p;

    p = D_80115270;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void func_800DAFD0(void)
{
    D_80115268 = NULL;
    func_800DAF98();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

s32 func_800DB004(GpPerspSrc* arg0, s32* sxy)
{
    void**          scratch;
    u8*             head;
    GpPerspScratch* block;
    s32             ret;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (GpPerspScratch*)(head - 0x14);
    block->vec.vx = arg0->field_C;
    block->vec.vy = arg0->field_10;
    *scratch      = block;
    block->vec.vz = arg0->field_14;
    __asm__ volatile("" ::: "memory");
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_SetTransMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(sxy);
    gte_stdp(&((GpPerspScratch*)(head - 0x14))->p);
    gte_stflg(&((GpPerspScratch*)(head - 0x14))->flag);
    gte_stszotz(&((GpPerspScratch*)(head - 0x14))->otz);
    ret      = block->otz;
    *scratch = (u8*)*scratch + 0x14;
    return ret;
}

void func_800DB0D8(void)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GpLinkNode*            node;

    i = 0;
    p = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            node = work->actor->field_90C;
            if (node != NULL) {
                node->field_5 = 0;
            }
        }
        i++;
        p++;
    } while (i < 2);
}

s32 func_800DB128(GpItemScan* arg0)
{
    GameSessionFrom4* loc;
    GpGiveRec*        rec;
    s32               key;
    s32               ret;
    s32               i;
    u16               item;
    s8                mode;
    u8                stage;
    u8                area;
    u8                sub;

    ret   = 0;
    loc   = (GameSessionFrom4*)&Game_Session->field_4;
    stage = loc->field_3;
    area  = loc->field_2;
    sub   = loc->field_5;
    key   = (stage << 24) | (area << 16) | (sub << 8);
    mode  = Mc_SaveData.field_F;
    if ((mode == 0) || (mode == 2)) {
        rec = D_8010F9F4[stage];
    } else {
        rec = D_8010FA0C[stage];
    }
    if (rec->field_0 != -1) {
        do {
            if (rec->field_0 == key) {
                for (i = 0; i < 4; i++) {
                    item = rec->items[i];
                    if (item != 0) {
                        if ((i != 3) || (func_800B9D80(0x80000) != 0)) {
                            if (func_800B7420(item) == 0) {
                                ret = 1;
                                if (i == 3) {
                                    ret = 2;
                                }
                                func_800BAD08(arg0, item, -1);
                            }
                        }
                    }
                }
                return ret;
            }
            rec++;
        } while (rec->field_0 != -1);
    }
    return ret;
}

s32 func_800DB28C(GpActorWork* arg0, GpImgRec* arg1, RECT* arg2)
{
    s32        ret;
    TmdObject* extra;
    s32        x;

    extra = (TmdObject*)arg0->extra;
    ret   = 0;
    if (arg1 != NULL) {
        arg1->rect.x = ((s8)extra->field_24 << 6) + (x = (arg2->x + 1) / 2 + 0x180);
        arg1->rect.y = arg2->y + 0x100;
        arg1->rect.w = arg2->w;
        arg1->rect.h = arg2->h;
        func_800DB31C(arg1);
    } else {
        ret = 1;
    }
    return ret;
}

void func_800DB31C(GpImgRec* arg0)
{
    void**         scratch;
    void*          head;
    register void* temp asm("v0");
    RECT*          dest;
    s32            done;
    register s32   max asm("s4");

    done     = 0;
    scratch  = (void**)G_SCRATCH_HEAD;
    max      = 0xFF;
    head     = *scratch;
    temp     = (u8*)head - 8;
    dest     = temp;
    *scratch = dest;

    do {
        if (arg0->field_0 == 0) {
            dest->x = arg0->rect.x;
            dest->y = arg0->rect.y;
            dest->w = arg0->rect.w;
            dest->h = arg0->rect.h;
            LoadImage(dest, arg0->data);
        } else {
            done = 1;
        }
        arg0++;
    } while (done == 0);

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
    asm("" ::"r"(max));
}

void func_800DB3FC(void)
{
    GpStateF0*  p;
    McSaveData* save;
    u8          val;

    p                  = &D_801153F0;
    D_801153F0.field_0 = 0;
    p->field_1         = 0;
    p->field_2         = 0;
    p->field_3         = 0;
    p->field_4         = 0;
    p->field_5         = 0;
    p->field_6         = 0;
    p->field_8         = 0;
    p->field_C         = 0;
    p->field_10        = 0;
    p->field_14        = 0;
    p->field_18        = 0;
    p->field_19        = 0;
    p->field_1A        = 0;
    p->field_1B        = 0;
    p->field_1C        = 0;
    p->field_1D        = 0;
    p->field_1E        = 0;
    p->field_1F        = 0;
    p->field_20        = 0;
    p->field_21        = 0;
    p->field_22        = 0;
    p->field_23        = 0;
    p->field_24        = 0;
    p->field_25        = 0;
    p->field_26        = 0;
    p->field_27        = 0;
    p->field_28        = 0;
    p->field_29        = 0;
    p->field_2A        = 0;
    if (func_800A74C4() == 1) {
        p->field_2B = 0;
    } else {
        save        = &Mc_SaveData;
        val         = (u8)save->field_F;
        p->field_2B = val;
        if (val == 0) {
            if (save->field_E != 0) {
                p->field_2B = 4;
            }
        }
    }
}

void func_800DB4E0(s32 arg0)
{
    if (D_801153F0.field_0 == 0) {
        D_801153F0.field_0 = 1;
    }
}

void func_800DB500(s32 arg0)
{
    if (arg0 != 0) {
        D_801153F0.field_2 |= 1 << (arg0 - 1);
    }
}

void func_800DB530(s32 arg0)
{
    D_801153F3 = arg0;
}

void func_800DB53C(void)
{
    D_801153F0.field_6++;
}

void func_800DB558(GpObj20E* arg0)
{
    GpStateF0*  p;
    GpStateF0*  q;
    GpPairSrcE* rec;

    p = &D_801153F0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            D_801153F0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
        rec = arg0->field_20->field_50;
        if (rec != NULL) {
            q            = &D_801153F0;
            q->field_8  += rec->field_6;
            q->field_C  += rec->field_8;
            q->field_10 += rec->field_A;
        }
    }
}

void func_800DB630(void)
{
    GpStateF0* p;

    p = &D_801153F0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            D_801153F0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            p->field_8         = 0;
            p->field_C         = 0;
            p->field_10        = 0;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void func_800DB6B4(void)
{
    GpStateF0* p;

    p = &D_801153F0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            D_801153F0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void func_800DB72C(void)
{
    if (Game_GetPtrSlot(3) != NULL) {
        func_8010154C();
        func_800E0540(D_80115570);
        func_800E0540(D_80115574);
        func_800E0540(D_80115578);
        func_800E0540(D_8011557C);
        func_800E0540(D_80115580);
        func_800E0540(D_8011558C);
        func_800E0540(D_80115590);
        func_800E0414(D_80115570, D_80115578);
        func_800E0414(D_80115570, D_8011557C);
        func_800E0414(D_80115570, D_80115580);
        func_800E0414(D_80115570, D_80115590);
        func_800DB900(D_80115570);
        func_800E0414(D_80115574, D_80115578);
        func_800E0414(D_80115574, D_80115580);
        func_800E0414(D_80115574, D_80115588);
        func_800E0414(D_80115578, D_80115580);
        func_800E0414(D_80115578, D_80115590);
        func_800DB900(D_80115578);
        func_800E0414(D_8011557C, D_80115580);
        func_800E0414(D_80115580, D_80115590);
        if (D_80115424 != 0) {
            func_800E0B08();
        }
        func_800E0608(D_80115570, 0x9007, 0x9004);
        if (Game_Session->field_12C == 0) {
            func_800E06AC(D_80115570, 0xA007, 0xA004);
        }
    }
}

void func_800DB900(GpObj* node)
{
    GpObj*     other;
    GpU16Pair* rec;
    s32        rowOff;
    s32        temp;
    u16        flags;
    u16        handler;
    u16        swap;
    u8         kind;
    u8         otherKind;

    for (; node != NULL; node = node->next) {
        flags = node->flags;
        other = node->next;
        if (flags & 0x8000) {
            kind = (node->flags & 7) - 1;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->field_2;
                        handler   = rec->field_0;
                        if (swap == 0) {
                            D_8010FA38[handler](node, other, handler);
                        } else {
                            D_8010FA38[handler](other, node, handler);
                        }
                    }
                }
            }
        }
    }
}

void func_800DBA20(GpObj* arg0, GpObj* arg1, GpSphereScratch* arg2)
{
    register s32      a3v asm("a3");
    register GpRec18* slot asm("t1");
    register GpRec18* otable asm("t0");
    register GpRec18* otherSlot asm("v1");
    unsigned int      recFlags;
    u16               f0;
    s32               key;

    if (arg1->field_18 == 0) {
        return;
    }

    a3v = 0;
    switch (arg0->flags & 7) {
        case 0:
            break;
        case 1:
            a3v = (s32)arg0->field_C;
            break;
        case 2:
            a3v = (s32)((GpObj*)arg0->field_C)->field_C;
            break;
        case 3:
            a3v = (s32)((GpActorD4Rec*)arg0->field_C)->field_14;
            break;
        empty_or: /* between case 3 and 4 so the empty-slot trampoline matches */
        {
            register s32 tmp asm("v0");
            tmp           = a3v & 0xF0;
            slot->field_0 = recFlags | (tmp + 1);
            goto fill;
        }
        case 4:
            a3v = (s32)((GpObj*)arg0->field_C)->field_8;
            break;
    }
    slot = (GpRec18*)a3v;
    if (slot == NULL) {
        return;
    }

    a3v = arg0->flags;
    if (a3v & 0x800) {
        recFlags = slot->field_0;
        if (recFlags & 1) {
            {
                register s32 cmp asm("v0");
                cmp = 0x100000;
                a3v = 0xFFFF0000;
                if ((slot->field_4 & a3v) != cmp) {
                    a3v    = (((s32)slot->field_12 << 16) & a3v) | (u16)slot->field_10;
                    otable = NULL;
                    switch (((GpObj*)a3v)->flags & 7) {
                        case 0:
                            break;
                        case 1:
                            otable = ((GpObj*)a3v)->field_C;
                            break;
                        case 2:
                            otable = ((GpObj*)((GpObj*)a3v)->field_C)->field_C;
                            break;
                        case 3:
                            otable = ((GpActorD4Rec*)((GpObj*)a3v)->field_C)->field_14;
                            break;
                        case 4:
                            otable = (GpRec18*)((GpObj*)((GpObj*)a3v)->field_C)->field_8;
                            break;
                    }
                    otherSlot = otable;
                    if (otherSlot == NULL) {
                        return;
                    }
                    key = arg0->field_18;
                loop:
                    if (otherSlot->field_4 != key) {
                        if (otherSlot->field_0 & 2) {
                            return;
                        }
                        otherSlot++;
                        goto loop;
                    }
                    f0                  = otherSlot->field_0;
                    otherSlot->field_4  = 0;
                    otherSlot->field_2  = 0;
                    otherSlot->field_8  = 0;
                    otherSlot->field_A  = 0;
                    otherSlot->field_C  = 0;
                    otherSlot->field_10 = 0;
                    otherSlot->field_12 = 0;
                    otherSlot->field_14 = 0;
                    otherSlot->field_0  = f0 & 0xFFFE;
                }
            }
            recFlags      = slot->field_0;
            recFlags      = recFlags | ((arg0->flags & 0xF0) + 1);
            slot->field_0 = recFlags;
            goto fill;
        } else {
            goto empty_or;
        }
    } else {
        while (1) {
            recFlags = slot->field_0;
            if (!(recFlags & 1)) {
                break;
            }
            if (recFlags & 2) {
                return;
            }
            slot++;
        }
        slot->field_0 = recFlags | ((arg0->flags & 0xF0) + 1);
    }

fill:
    slot->field_4              = arg1->field_18;
    slot->field_2              = (u16)arg2->rsum;
    *(SVECTOR*)&slot->field_8  = arg2->src;
    *(SVECTOR*)&slot->field_10 = arg2->extra;
}

s32 func_800DBCAC(GpObj* arg0, GpObj* arg1)
{
    void**           scratch;
    u8*              head;
    GpSphereScratch* block;
    register s32     dx asm("v0");
    register s32     a asm("a0");
    register s32     b asm("v1");
    register s32     c asm("a1");
    register s32     ret asm("s5");
    register s32     t0 asm("a2");
    s32              dz;
    s32              rsum;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (GpSphereScratch*)(head - 0x48);
    *scratch = block;
    func_800E08CC(arg0, (VECTOR3*)(head - 0x34));
    func_800E08CC(arg1, (VECTOR3*)(head - 0x24));

    dx              = block->pos0.vx;
    a               = block->pos1.vx;
    b               = block->pos0.vy;
    c               = block->pos1.vy;
    dx             -= a;
    a               = block->pos0.vz;
    b              -= c;
    block->delta.vy = b;
    b               = block->pos1.vz;
    ret             = 0;
    block->delta.vx = dx;
    __asm__ volatile("" ::: "memory");
    if (dx < 0) {
        dx = -dx;
    }
    dz              = a - b;
    block->delta.vz = dz;
    if ((dx > 0x7FFF) || (ABS(dz) > 0x7FFF)) {
        *scratch = (u8*)*scratch + 0x48;
        return 0;
    }

    __asm__ volatile("" ::: "memory");
    dx            = block->delta.vx;
    t0            = dx * dx;
    dx            = block->delta.vy;
    c             = dx * dx;
    dx            = block->delta.vz;
    a             = dx * dx;
    rsum          = (u16)arg0->field_1C + (u16)arg1->field_1C;
    block->rsum32 = rsum;
    dx            = t0 + c + a;
    if (dx < (b = rsum * rsum)) {
        s32 rad;

        a                             = (s32)arg0;
        c                             = (s32)arg1;
        dx                            = (u16)block->pos1.vx;
        t0                            = (s32)block;
        ((SVECTOR*)(head - 0x48))->vx = dx;
        dx                            = (u16)block->pos1.vy;
        b                             = (u16)block->pos1.vz;
        rad                           = (u16)block->rsum32;
        ret                           = 1;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->src.vy                 = dx;
        block->src.vz                 = b;
        block->rsum                   = rad;
        func_800DBA20((GpObj*)a, (GpObj*)c, (GpSphereScratch*)t0);

        ((SVECTOR*)(head - 0x48))->vx = (s16)block->pos0.vx;
        block->src.vy                 = (s16)block->pos0.vy;
        block->src.vz                 = (s16)block->pos0.vz;
        block->extra.vx               = 0;
        block->extra.vy               = 0;
        block->extra.vz               = 0;
        block->rsum                   = (s16)block->rsum32;
        func_800DBA20(arg1, arg0, block);
    }

    *scratch = (u8*)*scratch + 0x48;
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBE7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DC528);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DCB80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD324);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD940);

void func_800DDC2C(GpObj* arg0)
{
    register GpObj*         obj asm("a0");
    register SVECTOR*       dir asm("a1");
    register void**         scratch asm("a2");
    register u8*            head asm("s0");
    register GpEdgeScratch* block asm("s1");
    GsCOORDINATE2*          coord;
    s32                     prod;
    s32                     x;

    obj     = arg0;
    dir     = (SVECTOR*)obj->field_C;
    prod    = dir->vx * (u16)obj->field_1C;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register s32 tmp asm("v0");

        tmp              = (s32)head - 0x50;
        x                = (u16)obj->field_10;
        block            = (GpEdgeScratch*)tmp;
        block->src[0].vy = 0;
        x               += prod >> 12;
        block->src[0].vx = x;
    }
    block->src[0].vz = (u16)obj->field_14 + ((dir->vz * (u16)obj->field_1C) >> 12);
    prod             = dir->vx * (u16)obj->field_1C;
    x                = (u16)obj->field_10;
    block->src[1].vy = 0;
    x               += (-prod) >> 12;
    block->src[1].vx = x;
    head            -= 0x20;
    block->src[1].vz = (u16)obj->field_14 + ((-(dir->vz * (u16)obj->field_1C)) >> 12);
    coord            = (GsCOORDINATE2*)obj->field_8;
    *scratch         = block;
    func_800A8864(&D_80070F34, &coord->workm, (MATRIX*)head);
    gte_SetRotMatrix((MATRIX*)head);
    {
        register VECTOR*       out asm("a2");
        register s32           off asm("a3");
        register s32           i asm("t0");
        register s32           val asm("v0");
        register s32           t asm("v1");
        register GpGridParams* p asm("a1");
        register s32           y asm("a0");
        register s32           hi asm("t1");

        i = 0;
        asm volatile("lui %0, %%hi(D_80115448)" : "=r"(hi) : "r"(i) : "memory");
        out = block->pos;
        off = 0x20;
        do {
            gte_ldv0((SVECTOR*)((u8*)block + off));
            gte_rtv0_real();
            gte_stlvnl(out);
            off += 8;
            i++;
            val = out->vx;
            asm volatile("" : "+r"(val));
            asm("lw %0, %%lo(D_80115448)(%2)\n\tlw %1, 68(%3)" : "=r"(p), "=r"(t) : "r"(hi), "r"(block));
            y       = p->field_14;
            out->vy = 0;
            out->vx = val + t + y;
            y       = p->field_18;
            out->vz = out->vz + block->mat.t[2] + y;
            out++;
        } while (i < 2);
        func_800DE2C0(block->pos, 0);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x50;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDDF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE150);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE2C0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE7CC);

void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1)
{
    void**                 scratch;
    u8*                    head;
    GpGridPairScratch*     block;
    VECTOR*                out;
    register GpGridParams* p asm("a2");

    scratch      = (void**)G_SCRATCH_HEAD;
    head         = *scratch;
    block        = (GpGridPairScratch*)(head - 0x40);
    block->in.vx = arg0->vx;
    block->in.vy = arg0->vy;
    block->in.vz = arg0->vz;
    out          = (VECTOR*)(head - 0x30);
    *scratch     = block;
    ApplyTransposeMatrixLV(&D_80115448->field_0->workm, &block->in, out);
    p              = D_80115448;
    block->pos0.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos0.vy = 0;
    block->pos0.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    block->in.vx   = arg1->vx;
    block->in.vy   = arg1->vy;
    block->in.vz   = arg1->vz;
    ApplyTransposeMatrixLV(&p->field_0->workm, &block->in, out);
    p              = D_80115448;
    block->pos1.vx = (s16)((u16)block->out.vx + (u16)p->field_14 - (u16)p->field_0->coord.t[0]);
    block->pos1.vy = 0;
    block->pos1.vz = (s16)((u16)block->out.vz + (u16)p->field_18 - (u16)p->field_0->coord.t[2]);
    func_800DE2C0((VECTOR*)(head - 0x20), 0);
    *scratch = (u8*)*scratch + 0x40;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEC80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEF80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DF6AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DFCCC);

void func_800E0294(void)
{
    D_80115570 = NULL;
    D_80115574 = NULL;
    D_80115578 = NULL;
    D_8011557C = NULL;
    D_80115580 = NULL;
    D_80115584 = NULL;
    D_80115588 = NULL;
    D_8011558C = NULL;
    D_80115590 = NULL;
    D_80115448 = 0;
    D_8011556C = NULL;
    D_80115554 = NULL;
    D_80115550 = NULL;
    D_80115424 = 0;
}

s32 func_800E0308(SVECTOR* arg0, SVECTOR* arg1)
{
    void**           scratch;
    register u8*     head asm("v0");
    register VECTOR* vec asm("s1");
    GpObj3A*         node;
    s32              ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    vec                          = (VECTOR*)head;
    __asm__ volatile("" : "+r"(vec) : "r"(head));
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

void func_800E0414(GpObj* a, GpObj* b)
{
    GpObj*     other;
    GpU16Pair* rec;
    s32        rowOff;
    s32        temp;
    u16        flags;
    u16        handler;
    u16        swap;
    u8         kind;
    u8         otherKind;

    for (; a != NULL; a = a->next) {
        flags = a->flags;
        if (flags & 0x8000) {
            kind  = (a->flags & 7) - 1;
            other = b;
            if (other != NULL) {
                rowOff = kind << 4;
                for (; other != NULL; other = other->next) {
                    if (other->flags & 0x8000) {
                        otherKind = (other->flags & 7) - 1;
                        temp      = (otherKind << 2) + rowOff;
                        rec       = &D_8010FA4C[0][0] + (temp >> 2);
                        swap      = rec->field_2;
                        handler   = rec->field_0;
                        if (swap == 0) {
                            D_8010FA38[handler](a, other, handler);
                        } else {
                            D_8010FA38[handler](other, a, handler);
                        }
                    }
                }
            }
        }
    }
}

void func_800E0540(GpObj* node)
{
    u16 flags;

    if (D_80115448 != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        func_800DC528(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        func_800DCB80(node);
                        break;
                }
            }
        }
    }
}

void func_800E0608(GpObj* node, s32 mask, s32 match)
{
    GpObj4C* other;

    other = D_8011556C;
    for (; node != NULL; node = node->next) {
        if ((node->flags & mask) == (u16)match) {
            for (; other != NULL; other = other->next) {
                if (other->field_4A & 0x40) {
                    func_800DEF80(node, other);
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E06AC);

s32 func_800E076C(void)
{
    return 0;
}

void func_800E0774(VECTOR3* arg0, SVECTOR3* arg1)
{
    void**        scratch;
    u8*           head;
    VECTOR*       vec;
    GpGridParams* p;
    s32           val;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vec = *scratch = (VECTOR*)(head - 0x10);
    ApplyTransposeMatrixLV(&D_80115448->field_0->workm, (VECTOR*)arg0, vec);
    p   = D_80115448;
    val = ((VECTOR*)(head - 0x10))->vx + p->field_14 - p->field_0->coord.t[0];
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = D_80115448;
    arg1->vy = 0;
    val      = vec->vz + p->field_18 - p->field_0->coord.t[2];
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_800E08CC(GpObj* arg0, VECTOR3* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR3* vec;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x30);
    *scratch = vec;
    __asm__ volatile("" ::: "memory");
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    gte_ldv0(&arg0->field_10);
    gte_rtv0_real();
    gte_stlvnl(vec);
    arg1->vx = ((GsCOORDINATE2*)arg0->field_8)->workm.t[0] + ((VECTOR3*)(head - 0x30))->vx;
    arg1->vy = ((GsCOORDINATE2*)arg0->field_8)->workm.t[1] + vec->vy;
    arg1->vz = ((GsCOORDINATE2*)arg0->field_8)->workm.t[2] + vec->vz;
    *scratch = (u8*)*scratch + 0x30;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0994);

void func_800E0B08(void)
{
    GpObj4C* node;

    for (node = D_8011556C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
        }
    }
}

void func_800E0B48(VECTOR3* arg0, SVECTOR3* arg1)
{
    s32           val;
    GpGridParams* p;

    p   = D_80115448;
    val = arg0->vx + p->field_14;
    if (val >= 0) {
        arg1->vx = val / p->field_20;
    } else {
        arg1->vx = -1;
    }
    p        = D_80115448;
    arg1->vy = 0;
    val      = arg0->vz + p->field_18;
    if (val >= 0) {
        arg1->vz = val / p->field_20;
    } else {
        arg1->vz = -1;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0C10);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0FEC);

s32 func_800E1380(GpObj* arg0, s32 arg1)
{
    register void** scratch asm("v0");
    register s32    hi asm("v0");
    u8*             head;
    GpNearScratch*  block;
    GpActorD4Rec*   rec;
    GpRec18*        slot;
    s32             minDist;
    s32             index;
    s32             best;
    s32             dx;
    s32             dy;
    s32             dz;
    s32             dist;

    minDist = -1;
    index   = 0;
    best    = index;
    asm("lui %0, 0x1F80" : "=r"(hi) : "r"(best));
    asm("ori %0, %1, 0x3FC" : "=r"(scratch) : "r"(hi));
    rec      = (GpActorD4Rec*)arg0->field_C;
    head     = *scratch;
    slot     = rec->field_14;
    *scratch = (void*)(head - 0x28);
    __asm__ volatile("" ::: "memory");
    block = (GpNearScratch*)(head - 0x28);
    gte_SetRotMatrix(&((GsCOORDINATE2*)arg0->field_8)->workm);
    block->local.vx = (u16)rec->field_8 + (u16)arg0->field_10;
    block->local.vy = (u16)rec->field_A + (u16)arg0->field_12;
    block->local.vz = (u16)rec->field_C + (u16)arg0->field_14;
    gte_ldv0((SVECTOR*)(head - 8));
    gte_rtv0_real();
    gte_stlvnl(block);
    block->world.vx = ((VECTOR3*)(head - 0x28))->vx + ((GsCOORDINATE2*)arg0->field_8)->workm.t[0];
    block->world.vy = block->vec.vy + ((GsCOORDINATE2*)arg0->field_8)->workm.t[1];
    block->world.vz = block->vec.vz + ((GsCOORDINATE2*)arg0->field_8)->workm.t[2];

    for (;;) {
        if ((slot->field_0 & 1) && ((slot->field_4 & 0xFFFF0000) == arg1)) {
            dx            = slot->field_8 - block->world.vx;
            block->vec.vx = dx;
            dy            = slot->field_A - block->world.vy;
            block->vec.vy = dy;
            dz            = slot->field_C - block->world.vz;
            block->vec.vz = dz;
            dist          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            if ((u32)dist < (u32)minDist) {
                minDist = dist;
                best    = index + 1;
            }
        }
        if (slot->field_0 & 2) {
            break;
        }
        slot++;
        index++;
    }

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
    return best;
}

void func_800E15AC(s32 arg0, GpObj* arg1)
{
    u16    flags;
    GpObj* head;
    GpObj* node;
    GpObj* temp;

    head  = D_8010FA8C[arg0];
    flags = arg1->flags;
    if (!(flags & 0x8)) {
        if ((flags & 0x7) < 5) {
            arg1->flags = flags | 0x8;
            temp        = head->next;
            if (temp != NULL) {
                node = temp;
                while (node->next != NULL) {
                    node = node->next;
                }
                node->next = arg1;
                arg1->prev = node;
            } else {
                head->next = arg1;
                arg1->prev = head;
            }
            arg1->next = NULL;
        }
    }
}

void func_800E1638(GpObj* node)
{
    u16    flags;
    GpObj* next;
    GpObj* prev;

    flags = node->flags;
    if (flags & 0x8) {
        next        = node->next;
        node->flags = flags & 0x7;
        prev        = node->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = node->prev;
            node->next = NULL;
        } else {
            prev->next = NULL;
        }
        node->prev = NULL;
    }
}

void func_800E1688(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* temp;

    head  = D_8010FAB0[arg0];
    flags = arg1->field_4A;
    if (!(flags & 0x20)) {
        arg1->field_4A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void func_800E1708(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* next;
    GpObj4A* prev;

    flags = arg1->field_4A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_4A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void func_800E1758(s32 arg0)
{
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* next;
    GpObj4A* temp;
    s32      flags;
    s32      mask;

    head = D_8010FAB0[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_4A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_4A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void func_800E17B4(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* temp;

    head  = D_8010FAB8[arg0];
    flags = arg1->field_3A;
    if (!(flags & 0x20)) {
        arg1->field_3A = flags | 0x20;
        temp           = head->next;
        if (temp != NULL) {
            node = temp;
            while (node->next != NULL) {
                node = node->next;
            }
            node->next = arg1;
            arg1->prev = node;
        } else {
            head->next = arg1;
            arg1->prev = head;
        }
        arg1->next = NULL;
    }
}

void func_800E1834(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* next;
    GpObj3A* prev;

    flags = arg1->field_3A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_3A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void func_800E1884(s32 arg0)
{
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* next;
    GpObj3A* temp;
    s32      flags;
    s32      mask;

    head = D_8010FAB8[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_3A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_3A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void func_800E18E0(GpRec18* arg0, s32 arg1, s32 arg2)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].field_0 = 2;
}

void func_800E192C(void)
{
    s32          i;
    GameSession* session;
    GpCbB8Rec**  recs;

    for (i = 7; i >= 0; i--) {
        D_80115428[i] = 0;
    }

    session = Game_Session;
    recs    = D_8010CBB8[session->field_7 - 1][session->field_6 - 1];
    for (i = 0; i < 8; i++) {
        D_80115428[i] = recs[i]->field_3;
    }
}

s32 func_800E19B8(GpRec18* arg0, s32 arg1)
{
    s32 result;
    s32 index;

    result = 0;
    for (index = 1;; index++) {
        if (arg0->field_0 & 1) {
            if (arg1 == 0) {
                return 1;
            }
            if (arg0->field_4 == arg1) {
                result = index;
            }
        }
        if ((arg0++)->field_0 & 2) {
            break;
        }
    }
    return result;
}

s32 func_800E1A1C(GpRec18* arg0, s32 arg1)
{
    s32 count;

    count = 0;
    do {
        if ((arg0->field_0 & 1) && ((arg0->field_4 & 0xFFFF0000) == arg1)) {
            count += 1;
        }
    } while (!((arg0++)->field_0 & 2));
    return count;
}

void func_800E1A6C(GpRec18* arg0)
{
    for (;;) {
        if (arg0->field_0 & 1) {
            arg0->field_0 &= 2;
            arg0->field_2  = 0;
            arg0->field_4  = 0;
            arg0->field_8  = 0;
            arg0->field_A  = 0;
            arg0->field_C  = 0;
            arg0->field_10 = 0;
            arg0->field_12 = 0;
            arg0->field_14 = 0;
        }
        if (arg0->field_0 & 2) {
            break;
        }
        arg0++;
    }
}

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

s32 func_800E1B24(s32 arg0)
{
    s32 mask[2];
    s32 val;
    s32 tmp;
    s32 ret;

    val     = 1 << arg0;
    mask[0] = val;
    tmp     = (u8)val << 12;
    if (tmp != 0) {
        ret = cln(tmp) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

void func_800E1B80(void)
{
    GpObj4C* node;

    for (node = D_80115554; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
            if ((u8)Game_Session->field_4 == node->field_48) {
                Mc_SaveData.field_4 = node->field_49;
            }
        }
    }
}

s32 func_800E1BF0(u16* arg0, u8* arg1, u8* arg2)
{
    GpObj4C* node;

    for (node = D_8011556C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            D_80115424 = 1;
            *arg0      = node->field_46;
            *arg1      = node->field_48;
            *arg2      = node->field_49;
            return 1;
        }
    }
    return 0;
}

void func_800E1C58(GpObj54* arg0, void* arg1)
{
    GpSlot18*  slot;
    GpSlot18*  temp;
    s32        one;
    GpStateF0* p;

    temp = arg0->field_54;
    if (temp != NULL) {
        slot = temp;
        one  = 1;
        while (1) {
            if ((*(s32*)&slot->field_0 & 3) != one) {
                break;
            }
            slot++;
        }
        slot->field_4  = arg1;
        slot->field_2  = 0;
        slot->field_8  = 0;
        slot->field_A  = 0;
        slot->field_C  = 0;
        slot->field_10 = 0;
        slot->field_12 = 0;
        slot->field_14 = 0;
        slot->field_0 |= 1;
        p              = &D_801153F0;
        p->field_5++;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1FEC);

s32 func_800E2438(s32 arg0, s32 arg1, s32* arg2, s32 arg3)
{
    s32                ret;
    s32                lo;
    s32                extra;
    s8                 rem;
    u16*               scaleTbl;
    register s32       hp asm("v0");
    register GpDmgRow* table asm("a1");
    register u16*      cols asm("a0");
    register s32       val asm("a1");
    register s32       addr asm("v0");
    register s32       scale asm("v0");
    register s32       div asm("v0");
    register u32       mag asm("v0");

    ret = 0;
    if ((arg0 & 0xFFFF0000) != 0x40000) {
        return ret;
    }

    lo = arg0 & 0xFFF;
    if (arg2 != NULL) {
        *arg2 = ((u32)arg0 >> 12) & 0xF;
    }

    if (arg3 == 0) {
        register s32 col asm("v1");
        register s32 row asm("a0");

        hp    = Wip_SysConfig.field_18;
        table = D_80113EF0;
        cols  = D_80113F54;
        addr  = (s32)&cols[hp / 10];
        asm("lui %0, %%hi(D_8011541B)" : "=r"(row) : "r"(addr));
        col = *(u16*)addr;
        asm("lbu %0, %%lo(D_8011541B)(%1)" : "=r"(row) : "r"(row), "r"(col));
        col <<= 1;
        asm volatile("");
        col  += row * 20;
        col  += (s32)table;
        extra = D_80114C08.field_C;
        col   = ((GpDmgSlot*)col)->field_A;
        val   = col << 8;
        if (extra != 0) {
            scaleTbl = D_80113CFC;
            div      = extra / 16;
            col      = (div - 1) * 2;
            rem      = extra % 16;
            scale    = scaleTbl[col + rem];
            col      = val * scale;
            asm volatile("" : "+r"(col));
            mag = 0x51EB851F;
            asm volatile("multu %0, %1" : : "r"(col), "r"(mag));
            asm volatile("mfhi %0" : "=r"(col));
            val = (u32)col >> 5;
        }
    } else {
        register s32 col asm("v1");
        register s32 row asm("a0");

        hp    = (s16)Mc_SaveData.field_6C8;
        table = D_80113EF0;
        cols  = D_80113F54;
        addr  = (s32)&cols[hp / 10];
        asm("lui %0, %%hi(D_8011541B)" : "=r"(row) : "r"(addr));
        col = *(u16*)addr;
        asm("lbu %0, %%lo(D_8011541B)(%1)" : "=r"(row) : "r"(row), "r"(col));
        col <<= 1;
        asm volatile("");
        col  += row * 20;
        col  += (s32)table;
        scale = ((GpDmgSlot*)col)->field_0;
        val   = scale << 8;
    }

    {
        register s32 hi asm("v1");
        mag = 0x51EB851F;
        asm volatile("multu %0, %1" : : "r"(val), "r"(mag));
        asm volatile("mfhi %0" : "=r"(hi));
        val = (u32)hi >> 5;
        hi  = lo * val;
        ret = (u32)hi >> 8;
    }
    if (ret == 0) {
        if (lo != 0) {
            ret = 1;
        }
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E25F8);

void func_800E2A24(GpObj5D* arg0, s32 arg1)
{
    u16 raw;
    s32 kind;
    s32 val;
    s32 limit;
    s32 rand;

    if ((arg1 & 0x8000) == 0) {
        raw = D_80113390[arg1 & 0x7F].field_4;
        asm volatile("" : "+r"(raw));
        kind = raw;
    } else {
        raw = D_8011398C[arg1 & 0x7F].field[5];
        asm volatile("" : "+r"(raw));
        kind = raw;
    }

    switch (kind) {
        case 0:
            break;
        case 1:
            arg0->field_4C |= 1;
            break;
        case 2:
            arg0->field_58  = 0;
            arg0->field_5B  = 0;
            arg0->field_4C |= 2;
            if ((arg1 & 0x8000) == 0) {
                arg0->field_5D = 0;
                return;
            }
            if ((arg1 & 0x3F) == 0x31) {
                arg0->field_5D = 0;
                return;
            }
            arg0->field_5D = D_80114C08.field_0 % 10U;
            break;
        case 3:
            val        = arg0->field_50->field_D;
            limit      = (val << 12) / 100;
            D_80070F60 = D_80070F60 * 5 + 0x71357911;
            rand       = (u32)D_80070F60 >> 16 & 0xFFF;
            if (rand < limit) {
                arg0->field_5A  = 0;
                arg0->field_4C |= 4;
                D_80070F60      = D_80070F60 * 5 + 0x71357911;
                arg0->field_59  = ((u32)D_80070F60 >> 16 & 0xF) + 0x53;
                if ((arg1 & 0x8000) == 0) {
                    arg0->field_5C = 0;
                    return;
                }
                arg0->field_5C = D_80114C08.field_0 % 10U;
            }
            break;
    }
}

s32 func_800E2BF8(GpObj50* arg0, s32 arg1)
{
    GpU16Pair* pairs;
    s32        ret;

    if (arg0->field_50 == NULL) {
        return 0;
    }
    pairs = arg0->field_50->field_0;
    ret   = pairs[arg1].field_0 & 0xFFF;
    ret  |= (pairs[arg1].field_2 & 0xF) << 12;
    ret  |= 0x40000;
    return ret;
}

s32 func_800E2C40(GpU16Pair* arg0, s32 arg1)
{
    s32 ret;

    if (arg0 == NULL) {
        return 0;
    }
    ret  = arg0[arg1].field_0 & 0xFFF;
    ret |= (arg0[arg1].field_2 & 0xF) << 12;
    ret |= 0x40000;
    return ret;
}

void func_800E2C78(GpObj40* arg0, s32 arg1, s32 arg2)
{
    s32 val;

    if ((u32)((arg1 & 0x7F) - 0x19) < 3U) {
        val = arg0->field_40;
        if ((u32)val < (u32)arg2) {
            D_801153F0.field_14 += val;
            return;
        }
        D_801153F0.field_14 += arg2;
    }
}

s32 func_800E2CD4(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = D_80114028[(u16)arg0].field_0;
            break;
        case 1:
            ret = D_80114054[(u16)arg0].field_0;
            break;
    }
    return ret;
}

s32 func_800E2D3C(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_4;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field[5];
    }
    return ret;
}

s32 func_800E2D90(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_6;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field[6];
    }
    return ret;
}

void func_800E2DE4(GpObj5C* arg0, s32 arg1)
{
    s32 val;
    s32 limit;
    s32 rand;

    val        = arg0->field_50->field_D;
    limit      = (val << 12) / 100;
    D_80070F60 = D_80070F60 * 5 + 0x71357911;
    rand       = (u32)D_80070F60 >> 16 & 0xFFF;
    if (rand < limit) {
        arg0->field_5A  = 0;
        arg0->field_4C |= 4;
        D_80070F60      = D_80070F60 * 5 + 0x71357911;
        arg0->field_59  = ((u32)D_80070F60 >> 16 & 0xF) + 0x53;
        if ((arg1 & 0x8000) == 0) {
            arg0->field_5C = 0;
            return;
        }
        arg0->field_5C = D_80114C08.field_0 % 10U;
    }
}

s32 func_800E2EC4(GpObj5C* arg0)
{
    s32 ret;
    s32 val;
    s32 scale;

    ret = 0;
    arg0->field_59--;
    if (arg0->field_59 == 0) {
        arg0->field_5A++;
        D_80070F60     = D_80070F60 * 5 + 0x71357911;
        arg0->field_59 = ((u32)D_80070F60 >> 16 & 0xF) + 0x53;
        val            = arg0->field_50->field_4;
        scale          = D_80113D38[arg0->field_5C];
        ret            = (val * scale) / 100;
        if (ret == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 func_800E2F7C(GpObj5C* arg0)
{
    s32          val;
    s32          ret;
    register s32 scale asm("v1");

    ret = 0;
    val = arg0->field_50->field_E;
    if (!(arg0->field_4C & 4)) {
        return 1;
    }
    if (val == 0) {
        return 0;
    }
    scale = D_80113D28[arg0->field_5C];
    if (arg0->field_5A >= (val * scale) / 100) {
        ret = 1;
    }
    return ret;
}

void func_800E3008(GpObj4C* arg0)
{
    arg0->field_4C |= 1;
}

void func_800E301C(GpObj5D* arg0, s32 arg1)
{
    arg0->field_58  = 0;
    arg0->field_5B  = 0;
    arg0->field_4C |= 2;
    if ((arg1 & 0x8000) == 0) {
        arg0->field_5D = 0;
        return;
    }
    if ((arg1 & 0x3F) == 0x31) {
        arg0->field_5D = 0;
        return;
    }
    arg0->field_5D = D_80114C08.field_0 % 10U;
}

s32 func_800E3084(GpObj5D* arg0)
{
    s32 ret;
    s32 limit;
    s32 val;
    s32 scale;

    ret = 0;
    val = arg0->field_50->field_C;
    if (val == 0) {
        return ret;
    }
    scale = D_80113D30[arg0->field_5D];
    limit = (val * scale) / 100;
    if (arg0->field_58 < limit) {
        arg0->field_5B++;
        if (arg0->field_5B >= 0x1F) {
            arg0->field_58++;
            if (arg0->field_58 >= limit) {
                D_80070F60     = D_80070F60 * 5 + 0x71357911;
                arg0->field_5B = (u32)D_80070F60 >> 16 & 0x3F;
            } else {
                arg0->field_5B = 0;
            }
        }
    } else {
        arg0->field_5B--;
        if (arg0->field_5B == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 func_800E3194(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_8;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field[7];
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E31E8);

void func_800E337C(Task* arg0)
{
    s32 flags;
    s32 bit0;
    s32 mode;
    s32 flag;

    flag  = 1;
    flags = arg0->spawnArg1;
    switch (arg0->state) {
        case 0:
            bit0 = flags & 1;
            if (bit0 != 0) {
                func_800E3BBC(0);
                D_801153F4 = flag;
            }
            if (flags & 2) {
                func_800E3B80(0);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            func_800E34D8((s32)arg0->spawnArg2, mode);
            arg0->state++;
            break;
        case 1:
            if (func_800E6CE0() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                func_800E3BBC(1);
                D_801153F4 = 0;
            }
            if (flags & 2) {
                func_800E3B80(1);
            }
            if (D_80115598 != 0) {
                func_800AC464(Game_GetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            Task_Kill(arg0);
            break;
    }
}
