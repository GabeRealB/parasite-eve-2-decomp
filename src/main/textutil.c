#include "common.h"

#include <psyq/rand.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/wipsys.h"

s32 Text_ParseLine(u8** arg0, u8* arg1)
{
    s32 ret;
    u8* src;
    u8* p;
    u8  c;
    u8  next;

    ret = 0;
    do {
        src = *arg0;
        c   = *src;
        if (c == 0x5C) {
            *arg0 = src + 1;
            switch (src[1]) {
                case 'Z':
                case 'z':
                    *arg1++ = 0;
                    ret     = -1;
                    (*arg0)++;
                    break;
                case 'N':
                case 'n':
                    *arg1++ = 0;
                    ret     = 1;
                    *arg0  += ret;
                    break;
                case 0x5C:
                    *arg1 = **arg0;
                    arg1 += 1;
                    (*arg0)++;
                    break;
                default:
                    *arg1++ = 0x5C;
                    *arg1   = **arg0;
                    arg1   += 1;
                    (*arg0)++;
                    break;
            }
        } else if (c == 0) {
            *arg1++ = 0;
            ret     = -1;
            (*arg0)++;
        } else if (c == 0xA) {
            *arg1++ = 0;
            ret     = 1;
            *arg0  += ret;
        } else if (c == 0xD) {
            *arg1 = 0;
            p     = *arg0;
            *arg0 = p + 1;
            next  = p[1];
            arg1 += 1;
            if (next == 0xA) {
                arg1 += 1;
                *arg0 = p + 2;
            }
            ret = 1;
        } else if (((u8)(c + 0x7F) < 0x1FU) || ((u8)(c + 0x20) < 0x1DU)) {
            *arg1 = c;
            p     = *arg0;
            *arg0 = p + 1;
            arg1 += 1;
            *arg1 = p[1];
            arg1 += 1;
            (*arg0)++;
        } else {
            *arg1 = c;
            arg1 += 1;
            (*arg0)++;
        }
    } while (ret == 0);
    return ret;
}

s32 Text_DrawMultiLine(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    u8                    sp10[0x40];
    TextDrawReq           sp50[2];
    u8*                   cur;
    s32                   temp;
    register UiObject*    obj asm("s3");
    register s32          x asm("s2");
    register s32          y asm("s1");
    register TextDrawReq* p asm("s0");
    register u8*          buf asm("s4");
    register s32          ret asm("s5");
    register s32          four asm("s6");
    register s32          a6 asm("s7");
    s32                   a5;

    a5   = arg5;
    a6   = arg6;
    obj  = arg0;
    x    = arg1;
    y    = arg2;
    p    = sp50;
    buf  = sp10;
    four = 4;
    cur  = arg3;

    do {
        ret = Text_ParseLine(&cur, sp10);
        if (obj != NULL) {
            if (obj->mode != 5) {
                sp50[0].x          = obj->baseX + x;
                sp50[0].y          = (obj->baseY + y) - 3;
                temp               = (s16)obj->drawOrder;
                sp50[0].field_8    = arg4;
                sp50[0].otIndex    = temp + 1;
                p->glyphTable      = 4;
                sp50[0].centerMode = a6;
                sp50[0].field_E    = a5;
                func_8002E53C(p, buf);
            }
        } else {
            sp50[1].x          = x;
            sp50[1].y          = y;
            p[1].otIndex       = four;
            sp50[1].field_8    = arg4;
            p[1].glyphTable    = four;
            sp50[1].centerMode = a6;
            sp50[1].field_E    = a5;
            func_8002E53C(&sp50[1], buf);
        }
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return 0;
}

s32 Text_MeasureWidth(u8* arg0)
{
    TextDrawReq sp10;

    sp10.glyphTable = 4;
    sp10.x          = 0;
    sp10.y          = 0;
    sp10.otIndex    = 0;
    sp10.field_8    = 0;
    sp10.centerMode = 2;
    sp10.field_E    = 0;
    Text_MeasureAndCenter(&sp10, arg0);
    return -sp10.x;
}

s32 Text_MeasureMultiLine(u8* arg0)
{
    u8           sp10[0x40];
    TextDrawReq  sp50;
    s32          maxWidth;
    s32          height;
    TextDrawReq* p;
    u8*          buf;
    u8*          cur;
    s32          ret;
    s32          tmp;
    s8           c;

    maxWidth = 0;
    height   = maxWidth;
    p        = &sp50;
    cur      = arg0;
    buf      = sp10;

    do {
        ret = Text_ParseLine(&cur, sp10);

        c             = 4;
        sp50.x        = 0;
        sp50.y        = 0;
        sp50.otIndex  = 0;
        sp50.field_8  = 0;
        tmp           = c;
        p->glyphTable = tmp;
        c             = 2;
        p->centerMode = c;
        sp50.field_E  = 0;
        Text_MeasureAndCenter(p, buf);

        if (maxWidth < -sp50.x) {
            do {
            } while (0);
            maxWidth = -sp50.x;
        }
        height += 0xF;
        cur     = buf;
    } while (ret != -1);

    return (height << 16) | maxWidth;
}

s32 Text_DrawPrompt(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    TextDrawReq sp10;
    TextDrawReq sp20;
    s32         temp;

    if (arg0 != NULL) {
        if (arg0->mode == 5) {
            return 0;
        }
    } else {
        sp20.x          = arg1;
        sp20.y          = arg2;
        sp20.otIndex    = 4;
        sp20.field_8    = arg4;
        sp20.glyphTable = 4;
        sp20.centerMode = arg6;
        sp20.field_E    = arg5;
        func_8002E53C(&sp20, arg3);
        return arg1;
    }
    sp10.x          = arg0->baseX + arg1;
    sp10.y          = (arg0->baseY + arg2) - 3;
    temp            = (s16)arg0->drawOrder;
    sp10.field_8    = arg4;
    sp10.glyphTable = 4;
    sp10.centerMode = arg6;
    sp10.field_E    = arg5;
    sp10.otIndex    = temp + 1;
    func_8002E53C(&sp10, arg3);
    return sp10.x - (s16)arg0->baseX;
}

void Text_DrawPromptCompat(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6)
{
    Text_DrawPrompt(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

s32 Text_DrawMultiLineScroll(UiObject* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6,
                             s32 arg7, s32 arg8)
{
    u8                    sp10[0x40];
    TextDrawReq           sp50[2];
    u8*                   cur;
    s32                   temp;
    s32                   four;
    register UiObject*    obj asm("s4");
    register s32          x asm("s3");
    register s32          y asm("s1");
    register s32          result asm("s7");
    register s32          rem asm("s2");
    register TextDrawReq* p asm("s0");
    register u8*          buf asm("s5");
    register s32          ret asm("s6");
    register u8*          a0tmp asm("a0");
    register s32          a1tmp asm("a1");

    x      = arg1;
    y      = arg2;
    result = 1;
    rem    = arg7;
    a1tmp  = arg8;
    a0tmp  = arg3;
    cur    = a0tmp;

    if ((a1tmp & 0xF) != 0) {
        rem += result;
        y   -= a1tmp & 0xF;
    }
    a1tmp >>= 4;
    if (a1tmp != 0) {
        cur = Text_SkipLines(a0tmp, a1tmp);
    }
    obj  = arg0;
    p    = sp50;
    buf  = sp10;
    four = 4;

    do {
        ret = Text_ParseLine(&cur, sp10);
        if (obj != NULL) {
            if (obj->mode != 5) {
                sp50[0].x          = obj->baseX + x;
                sp50[0].y          = (obj->baseY + y) - 3;
                temp               = (s16)obj->drawOrder;
                sp50[0].field_8    = arg4;
                sp50[0].otIndex    = temp + 1;
                p->glyphTable      = 4;
                sp50[0].centerMode = (u8)arg6;
                sp50[0].field_E    = (u8)arg5;
                func_8002E53C(p, buf);
            }
        } else {
            sp50[1].x          = x;
            sp50[1].y          = y;
            p[1].otIndex       = four;
            sp50[1].field_8    = arg4;
            p[1].glyphTable    = four;
            sp50[1].centerMode = (u8)arg6;
            sp50[1].field_E    = (u8)arg5;
            func_8002E53C(&sp50[1], buf);
        }
        rem -= 1;
        if (rem <= 0) {
            result = 0;
            break;
        }
        x  = arg1;
        y += 0xF;
    } while (ret != -1);

    return result;
}

void Text_LoadClutImages(void)
{
    RECT rect;

    rect.x = 0x100;
    rect.y = 0xF3;
    rect.w = 0x40;
    rect.h = 1;
    LoadImage(&rect, D_80060910);

    rect.x = 0x3D0;
    rect.y = 0x1FF;
    rect.w = 0x30;
    rect.h = 1;
    LoadImage(&rect, D_800609B0);
}

void Mc_BuildFileName(u8* arg0, s32 arg1)
{
    s32 i;

    i = 0;
    do {
        *arg0 = D_80060DC8[i];
        i++;
        arg0++;
    } while (i < 0xC);

    *arg0   = D_80060E08[arg1];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    arg0[1] = 0;
}

void Mc_InitDualBankBuffers(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];
    McSaveData* p;
    s32         one;
    s32         two;
    s32         idx;

    Mem_Set(&Wip_SysConfig, 0, 0x40);
    Mem_Set(Wip_SysConfig.field_40, 0xFF, 0x40);
    Mem_Set(D_80073980, 0, 0x100);
    Mem_Set(&D_80073980[0x100], 0xFF, 0x100);

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    do {
        b = D_800734C8;
        Mem_Set(b, 0, 0xB0);
        c = D_80073628;
        Mem_Set(c, 0, 0x24);
        d = D_80073670;
        Mem_Set(d, 0, 0xE4);
        e = D_80073838;
        Mem_Set(e, 0, 0xA4);
        Mem_Set(a + 1, 0xFF, 0x6C);
        Mem_Set(b + 1, 0xFF, 0xB0);
        Mem_Set(c + 1, 0xFF, 0x24);
        Mem_Set(d + 1, 0xFF, 0xE4);
        Mem_Set(e + 1, 0xFF, 0xA4);
        p = &Mc_SaveData;
    } while (0);

    one          = 1;
    p->field_6   = 0x14;
    two          = 2;
    p->field_7   = one;
    p->field_4   = one;
    p->field_5   = one;
    p->field_8   = 7;
    p->field_9   = one;
    p->field_5C5 = two;
    p->field_22  = one;
    Mc_InitSaveSlotDefaults();
    idx                            = p->field_22 - 1;
    (&Wip_SysConfig)[idx].field_21 = two;
}

void Mc_InitBufferSlots(void)
{
    McBufferSlot*             base;
    register McBufferSlot*    slot asm("t1");
    McBufferSlot*             end;
    register McChecksumBlock* block asm("t0");
    register s32              size asm("a3");
    register u8*              fptr asm("a0");
    register s16              sum asm("a2");
    register u32              fi asm("v1");
    register u32              i asm("a1");
    register u32              count asm("a0");
    register s32              fill asm("t2");
    register volatile u8*     cptr asm("v1");
    s32                       tmp;
    s32                       cond;

    fill = -1;
    base = Mc_BufferSlots;
    slot = base + 1;
    end  = base + 9;
    do {
        size = slot->field_4;
        fptr = (u8*)slot->field_0;
        cond = size;
        fi   = 0;
        if (cond != 0) {
            do {
                *fptr = 0;
                fi   += 1;
                fptr += 1;
            } while (fi < (u32)size);
        }
        cond = size;
        fi   = 0;
        if (cond != 0) {
            do {
                *fptr = fill;
                fi   += 1;
                fptr += 1;
            } while (fi < (u32)size);
        }
        sum   = 0;
        block = slot->field_0;
        asm("");
        i     = 0;
        count = size - 4;
        cptr  = block->field_4;
        if (count != 0) {
            do {
                i    += 1;
                tmp   = (s8)*cptr;
                sum   = sum + tmp;
                cptr += 1;
            } while (i < count);
        }
        slot          += 1;
        block->field_2 = ~sum;
        block->field_0 = sum;
    } while ((u32)slot < (u32)end);

    Display_State.field_10e = 1;
    Mc_InitDualBankBuffers();

    Mc_SaveData.field_21  = 0;
    Mc_SaveData.field_1a8 = 0;
    Mc_SaveData.field_1aa = 0;
    Mc_SaveData.field_1ab = 0;
    Mc_SaveData.field_1a9 = 0;
    Mc_SaveData.field_25  = 0;
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
}
