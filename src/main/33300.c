#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

void Task_AllocIdMap(Task* arg0)
{
    u8         temp_s4;
    u8         temp_s1;
    TaskIdMap* temp_v0;
    u8         temp_a0;
    s32        ret;
    s32        field34;

    temp_s4 = D_80062764[Game_Session->field_7 - 1];
    temp_v0 = Mem_Calloc(8, 0);
    if (temp_v0 != NULL) {
        arg0->field_1C = temp_v0;
        if (D_80062737 != 0) {
            func_8005132C(0, 1);
            D_80062737 = 0;
        }
        temp_a0    = Game_Session->field_7;
        ret        = func_80053BF4(temp_a0, D_8007272D, D_8006276C[temp_a0 - 1]);
        field34    = arg0->field_34;
        D_80062738 = ret;
        D_8007A398 = 0;
        if (field34 == 2) {
            s32      f7;
            GPairU8* p;
            u16      v;
            f7               = Game_Session->field_7;
            D_8007A398       = 0xFF;
            p                = D_80062750[f7 - 1];
            v                = D_80062735;
            D_8007A39A       = 0x12C;
            temp_v0->field_0 = v;
            temp_v0->field_4 = p;
        } else {
            temp_v0->field_4 = D_8006273C[Game_Session->field_7 - 1];
            temp_v0->field_0 =
                D_80062738 + (Game_Session->field_6 * (temp_s4 & 0xFF));
            if ((*((Game_Session->field_6 * (temp_s4 & 0xFF) * 2) +
                   (u8*)temp_v0->field_4) != 0x80) &&
                (D_80062736 != 0)) {
                func_800542D0(0x60010001, 0x1E);
                D_80062736 = 0;
            }
        }
        temp_s1 = *((temp_v0->field_0 * 2) + (u8*)temp_v0->field_4);
        if (temp_s1 == 0xFF) {
            func_8005132C(D_80062739, D_8007A39C);
            D_80062734 = temp_s1;
            Task_Kill(arg0);
            return;
        }
        D_80062734 = 0;
        if (func_80051560(*((temp_v0->field_0 * 2) + (u8*)temp_v0->field_4)) == 1) {
            if ((D_80062739 != 0) && (func_800514F8(D_80062739) != 0)) {
                func_8005132C(D_80062739, (D_8007A39C + 1) & 0xFFFF);
            }
            arg0->field_30 = arg0->field_30 + 1;
            return;
        }
        if (((temp_v0->field_0 * 2) + (u8*)temp_v0->field_4)[1] == 1) {
            func_8005132C(D_80062739, (D_8007A39C + 1) & 0xFFFF);
            goto block_20;
        }
        if (func_800514F8(D_80062739) == 0) {
            arg0->field_30 = arg0->field_30 + 2;
            return;
        }
    }
block_20:
    D_80062734 = 0xFF;
    Task_Kill(arg0);
}

void func_80042DF8(Task* arg0)
{
    u8                param1[8];
    u8                param2[8];
    TaskIdMap*        temp;
    register GPairU8* entry asm("v0");
    s32               field34;
    u8                flag;

    temp = arg0->field_1C;
    if (func_800514F8(D_80062739) == 0) {
        param1[3] = 0;
        param1[2] = 4;
        entry     = (GPairU8*)((temp->field_0 << 1) + (u32)temp->field_4);
        param1[0] = entry->field_0;
        param2[0] = Game_Session->field_74;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        field34 = arg0->field_34;
        if (field34 == 3) {
            arg0->field_30 = arg0->field_30 + 2;
            return;
        }
        entry = (GPairU8*)((temp->field_0 << 1) + (u32)temp->field_4);
        if ((entry->field_1 == 1) && (field34 == 0)) {
            arg0->field_30 = arg0->field_30 + 2;
            return;
        }
        arg0->field_30 = arg0->field_30 + 1;
        return;
    }
    flag = D_8007A398;
    if (flag == 0xFF) {
        D_8007A39A = D_8007A39A - 1;
        if (D_8007A39A == 0x3C) {
            func_8005132C(D_80062739, 1);
        }
        if (D_8007A39A <= 0) {
            D_80062734 = flag;
            Task_Kill(arg0);
        }
    }
}

void func_80042F54(Task* arg0)
{
    TaskIdMap* temp;
    GPairU8*   entry;
    u8         type;

    temp = arg0->field_1C;
    if (func_8001D344() != 0) {
        entry = (GPairU8*)((temp->field_0 << 1) + (u32)temp->field_4);
        type  = entry->field_1;
        if (type != 3) {
            if (type != 2) {
                if (arg0->field_34 == 0) {
                    if (Game_Session->field_4D != 1) {
                        return;
                    }
                }
            }
            func_800512BC(entry->field_0, 0);
            func_800429C8(0);
        }
        D_80062734 = 0xFF;
        D_80062739 = temp->field_4[temp->field_0].field_0;
        Task_Kill(arg0);
    }
}

void func_80043028(s32 arg0)
{
    register GameSession* g asm("v1");
    s32                   idx;
    s32                   product;
    GPairU8*              entry;
    s32                   temp;

    g       = Game_Session;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    temp    = ((D_80062738 + product) & 0xFFFF) * 2;
    entry   = (GPairU8*)(temp + (s32)D_8006273C[idx]);
    if (entry->field_0 != 0xFF) {
        if (entry->field_1 != 3) {
            func_800512BC(entry->field_0, arg0 & 0xFFFF);
            D_80062739 = entry->field_0;
            func_800429C8(0);
        }
    }
}

void func_800430E4(s32 arg0)
{
    register GameSession* g asm("v1");
    s32                   idx;
    s32                   product;
    GPairU8*              entry;
    s32                   temp;

    g       = Game_Session;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    temp    = ((D_80062738 + product) & 0xFFFF) * 2;
    entry   = (GPairU8*)(temp + (s32)D_8006273C[idx]);
    if (entry->field_0 != 0xFF) {
        if (func_800514F8(entry->field_0) != 0) {
            func_8005132C(entry->field_0, (arg0 + 1) & 0xFFFF);
        }
    }
}

void func_80043198(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80013F1C;
    sp.funcs[arg0->field_30](arg0);
}

void func_800431FC(Task* arg0)
{
    if (func_8001D344() != 0) {
        D_80062734 = 0xFF;
        Task_Kill(arg0);
    }
}

void func_8004323C(void)
{
    GameSession* g;
    s32          idx;
    s32          product;
    u8*          base;
    s32          one;

    g       = Game_Session;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    base    = (u8*)D_8006273C[idx];
    if (base[product * 2] == 0x80) {
        if (func_8004ACAC(0x108) == 1) {
            one = 1;
            func_800542D0(0x60010000 | one, 0x1E);
            D_80062736 = 0;
        } else if (D_80062736 == 0) {
            one = 1;
            func_8005414C(0x60010001, 0, 0);
            D_80062736 = one;
        }
    }
}

void Prim_DrawSprt(PrimDrawParams* arg0, u32 arg1, s32 arg2);
void func_80043718(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

s32 TextStream_Draw(TextStream* arg0, u8* arg1, s16* arg2, s32 arg3)
{
    struct {
        /* 0x00 */ s16 field_0;
        /* 0x02 */ s16 field_2;
        /* 0x04 */ s16 field_4;
        /* 0x06 */ s16 field_6;
        /* 0x08 */ s16 field_8;
        /* 0x0A */ s16 field_A;
        /* 0x0C */ u8  field_C;
        /* 0x0D */ u8  field_D;
        /* 0x0E */ u8  field_E;
        /* 0x0F */ u8  pad_F;
        /* 0x10 */ s16 field_10;
        /* 0x12 */ s16 field_12;
    } sp;
    s32 ret;
    s32 i;
    s32 glyphIdx;
    u8  ch;
    s16 tmp6;
    s16 h;
    s32 off;

    ret = 0;
    switch (*arg1) {
        case 0:
            arg0->field_E = 0;
            if (arg0->field_C < 0) {
                i = 0;
                if (*arg0->field_10 != 0xFF) {
                    do {
                        i++;
                        arg0->field_E++;
                    } while (arg0->field_10[i] != 0xFF);
                }
                *arg2 = arg0->field_1A;
            } else {
                *arg2 = arg0->field_C;
            }
            (*arg1)++;
            break;
        case 1:
            sp.field_0  = arg0->field_0;
            sp.field_2  = arg0->field_2;
            sp.field_4  = arg0->field_4;
            tmp6        = arg0->field_6;
            sp.field_C  = 0x80;
            sp.field_D  = 0x80;
            sp.field_E  = 0x80;
            sp.field_10 = 0;
            sp.field_12 = 0x1000;
            sp.field_6  = tmp6;
            if (arg0->field_10[arg0->field_E - 1] != 0xFF) {
                for (i = 0; i < arg0->field_E; i++) {
                    ch = arg0->field_10[i];
                    if (ch == 0xFE) {
                        sp.field_0  = arg0->field_0;
                        sp.field_2 += arg0->field_18;
                    } else if (ch != 0xFF) {
                        glyphIdx = ch & 0x7F;
                        if (((s8)ch >= 0) || (arg3 == 0)) {
                            off        = glyphIdx * 4;
                            sp.field_4 = ((GlyphUvwh*)(off + (s32)arg0->field_14))->u +
                                         (arg0->field_4 & 0x3F);
                            sp.field_6 = ((GlyphUvwh*)(off + (s32)arg0->field_14))->v +
                                         (u8)arg0->field_6;
                            sp.field_8 = ((GlyphUvwh*)(off + (s32)arg0->field_14))->w;
                            h          = ((GlyphUvwh*)(off + (s32)arg0->field_14))->h;
                            sp.field_A = h;
                            if (h != 0) {
                                Prim_DrawSprt((PrimDrawParams*)&sp, arg0->field_8,
                                              arg0->field_A);
                            }
                        }
                        sp.field_0 +=
                            ((GlyphUvwh*)((glyphIdx * 4) + (s32)arg0->field_14))->w;
                    }
                }
                func_80043718(1, arg0->field_4, arg0->field_6, 4);
                *arg2 = *arg2 - 1;
                if (*arg2 < 0) {
                    arg0->field_E = arg0->field_E + 1;
                    if (arg0->field_10[arg0->field_E] == 0xFF) {
                        ret   = -1;
                        *arg2 = arg0->field_1A;
                    } else {
                        *arg2 = arg0->field_C;
                    }
                }
            } else {
                (*arg1)++;
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

void Prim_DrawSprt(PrimDrawParams* arg0, u32 arg1, s32 arg2)
{
    SPRT* p;
    u8    v;

    p          = (SPRT*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    SetSprt(p);
    if (arg0->field_10 == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0   = arg0->field_C;
    p->g0   = arg0->field_D;
    p->b0   = arg0->field_E;
    p->x0   = arg0->field_0;
    p->y0   = arg0->field_2;
    p->u0   = arg0->field_4;
    v       = arg0->field_6;
    p->clut = getClut(arg1, arg2);
    p->v0   = v;
    p->w    = arg0->field_8 - 1;
    p->h    = arg0->field_A - 1;
    AddPrim(D_800710A0 + 4, p);
}

void func_80043718(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    DR_TPAGE* p;

    p          = (DR_TPAGE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    SetDrawTPage(p, 1, 0, GetTPage(0, (s16)arg0, (s16)arg1, (s16)arg2) & 0xFFFF);
    AddPrim(D_800710A0 + arg3, p);
}

s32 func_8004379C(RECT* arg0, u8* arg1, s16* arg2)
{
    PrimDrawParams sp;
    register s32   ret asm("s0");

    ret = 0;
    if (*arg1 != 0) {
        ret = 1;
    } else {
        sp.field_0  = arg0->x;
        sp.field_2  = arg0->y;
        sp.field_8  = arg0->w;
        sp.field_A  = arg0->h;
        sp.field_E  = 0;
        sp.field_D  = 0;
        sp.field_C  = 0;
        sp.field_10 = 1;
        Prim_DrawTile(&sp);
        asm("" : "+r"(ret));
        func_80043718(ret, ret, ret, 5);
        *arg2 = *arg2 - 1;
        if (*arg2 <= 0) {
            ret = 1;
        }
    }
    return ret;
}

void Prim_DrawTile(PrimDrawParams* arg0)
{
    TILE* p;

    p          = (TILE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    SetTile(p);
    if (arg0->field_10 == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0 = arg0->field_C;
    p->g0 = arg0->field_D;
    p->b0 = arg0->field_E;
    p->x0 = arg0->field_0;
    p->y0 = arg0->field_2;
    p->w  = arg0->field_8 - 1;
    p->h  = arg0->field_A - 1;
    AddPrim(D_800710A0 + 5, p);
}
