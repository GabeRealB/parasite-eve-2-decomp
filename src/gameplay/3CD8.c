#include "common.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>
#include <psyq/strings.h>

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
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32            Gp_LcgState;
extern u16            D_8007A396;
extern TaskFuncTable3 D_800974C8;
extern char           Gp_StrCapMagic[];
extern char           Gp_StrEvsFmt[];
extern TaskFuncTable3 D_8009752C;
extern TaskFuncTable3 D_80097538;
extern TaskFuncTable3 D_8009762C;
extern TaskFuncTable5 D_80097638;
extern TaskFuncTable5 D_8009764C;
extern TaskFuncTable3 D_80097678;
extern TaskDesc       Gp_EvtSpawnTable[];
extern TaskDesc       D_8010FB4C[];
extern GpRec14        Gp_WeaponMsgRec;
extern s32            Gp_CapCaretGrey;
extern s32            Gp_CapCaretDir;
extern s32            D_8010FB90[];
extern s32            D_8010FBE0;
extern s32            D_8010FBE4;
extern s32            D_8010FBE8;
extern u8             Gp_BtnMap0[];
extern u8             Gp_BtnMap1[];
extern u8             Gp_BtnMap2[];
extern u8             Gp_BtnMap2Alt[];
extern u16            Gp_WeaponIdBase[];
extern u16            Gp_AllyIdBase[];
extern GpEvt12*       Gp_CapTable;
extern s16            D_801155AC;
extern u16            D_801155AE;
extern s16            D_801155B0;
extern s16            D_801155B2;
extern s16            D_801155B4;
extern s16            D_801155B6;
extern u8             D_801155B8;
extern u8             D_801155BA;
extern u8             D_801155BB;
extern s16            D_801155BC;
extern s16            D_801155C0;
extern u8             D_80115670;
extern Task*          Gp_CapTask;
extern s16            D_80115678;
extern s16            D_8011567A;
extern GlyphUvwh*     Gp_CapGlyphs;
extern u8             D_80115680;
extern u8             D_80115688;
extern u8             D_80115648;
extern s16            D_8011564A;
extern u16            Gp_CapCaretX;
extern u16            Gp_CapCaretY;
extern s16            D_80115654;
extern s16            D_80115656;
extern u8             Gp_CapCaretDelay;
extern u8             D_80115659;
extern u8             D_8011565A;
extern u16            D_8011565C;
extern s32            D_80115660;
extern s16            D_80115664;
extern s16            D_80115666;
extern s16            Gp_CapEventKey;
extern s16            D_8011566A;
extern u8             D_8011566C;
extern u8             D_8011566E;
extern u8             D_8011566F;
extern s32            Gp_CapFile;
extern u8             D_80115690;
extern s16            D_80115698;
extern s16            D_8011569A;
extern u8             D_8011569C;
extern s32*           Gp_CapCmds;
extern u8             D_801156A4;
extern s32            D_801156A8;
extern s8             D_801156B0;
extern s8             D_801156B1;
extern s32            D_801156B4;
extern Task*          D_801156B8;
extern s16            D_801156BC;
extern u16            D_801156C0;
extern u16            D_801156C2;
extern u16            D_801156C4;
extern u16            D_801156C6;
extern u8             D_801156C8;
extern u8             D_801156C9;
extern u8             D_801156CA;
extern u8             D_801156CB;
extern u8             D_801156CC;
extern u8             D_801156CD;
extern u8             D_801156CE;
extern s32            D_801156D0;
extern s32            D_801156EC;
extern u8             D_801156F0;
extern GpOverlayIds*  D_801156F4;
extern u8             D_801156F8;
extern u8             D_801156F9;
extern u8             D_801153F4;
extern u8             Gp_PadScriptHalt;
extern u8             Gp_PadHoldHalt;
extern u8             Gp_PadLerpHalt;
extern u8             D_80115708;
extern u8             D_80115709;
extern u16            D_8011570A;
extern s16            D_8011570C;
extern s16            D_8011570E;
extern s16            D_80115710;
extern s16            D_80115712;
extern u8             D_80115714;
extern s16            D_80115716;
extern s16            D_80115718;

void func_800E44A0(Task* arg0);
void func_80724120(void);
void func_80724324(void);
void func_807244CC(char* arg0);
void func_8072455C(s16 arg0, s32 arg1);
void func_807245B8(void);
void func_80724714(void);
void Gp_CapExit(Task* arg0);
s32  Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2);
s32  Gp_AbortCap(void);
void Gp_LoadCapFile(s32 arg0);
void Gp_ApplyCapEvtFlags(void);
s32  Gp_FindCapEvt(s32 arg0);
s32  Gp_LookupSlot4(s32 arg0);
void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
void Gp_StepScriptA(Task* task);
void Gp_StepScriptB(Task* task);
void Gp_SpawnPadHold(s16 arg0);
void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3);

void Gp_SetNibbleIf(s32 arg0, s32 arg1)
{
    if (arg0 != 0) {
        GameFlag_SetNibble(arg0, arg1);
    }
}

void Gp_RunCapCmd1(s32 arg0)
{
    Gp_RunCapCmd(arg0, 1);
}

void Gp_MsgPlayer3F3(s32 arg0)
{
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F3, arg0, 0);
}

void Gp_MsgPlayerWeapon(s32 arg0)
{
    GpRec14 sp;

    if (arg0 == 0) {
        sp         = Gp_WeaponMsgRec;
        sp.field_0 = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&sp, 0);
    } else {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
    }
}

void Gp_MsgSlot4Chain(s32 arg0, s32 arg1)
{
    s32 out;

    arg0 = (arg0 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    if (out != 0) {
        Gp_DispatchMsg((Task*)out, 0x7D5, arg1, 0);
    }
}

void Gp_PlayerWeaponId(s32* arg0)
{
    *arg0 = Gp_WeaponIdBase[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
}

void Gp_AllyAnimId(s32* arg0)
{
    *arg0 = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
}

void Gp_FillPlayerHpMp(void)
{
    WipSysConfig* p;

    p           = &Wip_SysConfig;
    p->field_18 = p->field_1a;
    p->field_1c = p->field_1e;
}

void Gp_FillAllyHp(void)
{
    Mc_SaveData.field_6C8 = Mc_SaveData.field_6CA;
}

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1)
{
    if (Gp_CapBusy() == 0) {
        Task_SpawnFromTable(Gp_EvtSpawnTable, 0, arg1, arg0);
    }
}

void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    SndEvt_EnqueueType6(arg0, (s8)arg1, (s8)arg2);
}

s32 Gp_PackStageSndId(s32 arg0)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    return arg0;
}

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= Game_Session->field_7 << 24;
    }
    SndEvt_EnqueueType7(arg0, arg1 & 0xFFFF);
}

void Gp_MsgAlly3F3(s32 arg0)
{
    Task* slot;

    slot = Game_GetPtrSlot(0xA);
    if (slot != NULL) {
        Gp_DispatchMsg(slot, 0x3F3, arg0, 0);
    }
}

void Gp_MsgAllyWeapon(s32 arg0)
{
    Task*   slot;
    GpRec14 sp;

    slot = Game_GetPtrSlot(0xA);
    if (slot != NULL) {
        if (arg0 == 0) {
            sp         = Gp_WeaponMsgRec;
            sp.field_0 = Gp_AllyIdBase[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
            Gp_DispatchMsg(slot, 0x3E8, (s32)&sp, 0);
        } else {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
        }
    }
}

void func_800E3FAC(s32 arg0, s32 arg1)
{
    D_80073980[arg0 / 2 + 4] = arg1;
}

s32 func_800E3FCC(s32 arg0)
{
    return D_80073980[arg0 / 2 + 4];
}

s32 func_800E3FF0(s32 arg0, s32 arg1, GBytes8* arg2, GBytes8* arg3)
{
    *arg3 = *arg2;
    return 1;
}

s32 func_800E4018(void)
{
    return 0;
}

void func_800E4020(void)
{
}

void func_800E4028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800974C8;
    sp.funcs[arg0->state](arg0);
}

void Gp_ClearAllFlagNibbles(void)
{
    s32 i;

    for (i = 0; i < 0x1F8; i++) {
        GameFlag_SetNibble(i, 0);
    }
}

void Gp_SpawnEvt1(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(Gp_EvtSpawnTable, 1, arg0, arg1);
}

s32 Gp_RelocCapFile(GpCapFile* file)
{
    s32            i;
    s32            count;
    s32            flag;
    GpEvt12*       rec;
    s32*           ptr;
    GpCapEvtTable* evts;
    GpCapPtrTable* ptrs;

    if (strncmp(file->magic, Gp_StrCapMagic, 3) != 0) {
        return 0;
    }

    i = 0;
    if (file->field_8 > 0) {
        file->field_8  += (s32)file;
        file->field_C  += (s32)file;
        file->field_10 += (s32)file;
        evts            = (GpCapEvtTable*)file->field_C;
        rec             = (GpEvt12*)(evts + 1);
        count           = evts->count;
        if (count > 0) {
            flag = -1;
            do {
                if (rec->field_8 != flag) {
                    rec->field_8 += (s32)file;
                } else {
                    rec++;
                }
                i++;
                rec++;
            } while (i < count);
        }
        ptrs  = (GpCapPtrTable*)file->field_10;
        i     = 0;
        count = ptrs->count;
        ptr   = (s32*)(ptrs + 1);
        if (count > 0) {
            do {
                if (*ptr != 0) {
                    *ptr += (s32)file;
                }
                i++;
                ptr++;
            } while (i < count);
        }
    }

    Gp_CapGlyphs = (GlyphUvwh*)file->field_8;
    Gp_CapCmds   = (s32*)((GpCapPtrTable*)file->field_10 + 1);
    return 1;
}

s32 Gp_StartCap(s32 arg0, s16 arg1, s16 arg2)
{
    register s16 mode asm("s3");
    CdCmdQueue*  queue;
    TaskDesc*    desc;

    mode  = arg1;
    queue = &CdCmd_Queue;
    if (arg0 == 0) {
        return 0;
    }

    Gp_CapEventKey = arg2;
    Gp_CapTable    = (GpEvt12*)arg0;
    D_801155AC     = 0;
    D_801155AE     = 1;
    Gp_CapTable    = (GpEvt12*)arg0;
    D_801155B0     = 0;
    D_801155B2     = 0x30;
    D_801155B4     = 0xC0;
    D_801155B8     = 7;
    D_801155B2     = 0x140;
    D_80115664     = 0;
    D_8011569A     = 0;
    D_80115698     = 0;
    D_8011567A     = 0;
    D_801155C0     = 0;
    D_801156A8     = 0;
    D_801155BC     = 0;
    D_8011566E     = 0;
    D_8011566F     = 0;
    D_801155BA     = 0;
    D_801155BB     = 0;
    D_80115648     = 0;
    D_8011566A     = 0;
    D_8011565A     = 0;
    D_80115688     = 0;
    D_80115690     = 0;
    D_80115680     = 1;
    D_80115659     = 0xF;
    D_801155AE     = 1;
    D_8011566C     = Mc_SaveData.field_4;
    D_8011565C     = queue->field_22A;
    if (Display_State.field_112 != 0) {
        func_807245B8();
        D_8011564A = -1;
    }

    D_801155AE = Gp_FindCapEvt((s16)D_801155AE);
    if (Gp_CapTable[(s16)D_801155AE].field_8 == -1) {
        Gp_CapTable = 0;
        return 0;
    }

    Gp_ApplyCapEvtFlags();
    D_801155B4 = Gp_CapTextTopY((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
    D_801155B6 = Gp_CapTextHeight((u16*)Gp_CapTable[(s16)D_801155AE].field_8);
    D_80115666 = mode;
    D_80115660 = 0;
    if (mode != 0) {
        D_80115666 = mode;
        desc       = Task_GetDesc(2, 7);
        Gp_CapTask = (Task*)Display_InitModeObj(desc, 0, 0, 0);
        if (D_80115666 != 3) {
            return 0;
        }
        Task_SpawnFromTable(D_8010FB4C, 0, 0, 0);
        D_80115666 = 1;
    } else {
        Gp_CapTask = Task_Spawn(2, 7, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E44A0);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E5578);

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E62C0);

void Gp_CapExit(Task* arg0)
{
    CdCmdQueue* queue;
    char        buf[0x20];

    queue = &CdCmd_Queue;
    if (D_80115666 == 2) {
        Gp_DispatchMsg(Game_GetPtrSlot(5), 0xBB8, 0, 0);
    }
    if (D_80115666 != 0) {
        if (Mc_SaveData.field_4 == D_8011566C) {
            Stage_SetEndingFlag();
        } else {
            queue->field_22A = D_8011565C;
            Display_BeginMode7(D_8011566C);
        }
        goto block_11;
    }
    if (D_80115690 == 0) {
        D_801153F4 = 0;
    }
    if (Game_Session->field_1 == 0) {
        Game_Session->field_68 = 0;
        Mc_SaveData.field_4    = D_8011566C;
        Gp_MsgPlayer3F3(1);
        Gp_MsgAlly3F3(1);
        if (Display_State.field_112 != 0) {
            func_8072455C(D_8011564A, D_8011566C);
            goto block_11;
        }
    } else {
    block_11:
        if (Display_State.field_112 != 0 && D_801156F4 != 0) {
            sprintf(
                buf, Gp_StrEvsFmt, D_801156F4->field_0, D_801156F4->field_2,
                D_801156F4->field_4);
            func_807244CC(buf);
        }
    }
    Gp_CapTable = 0;
    D_8011565A  = 0;
    D_801156A4  = 0;
    Task_Kill(arg0);
}

void Gp_DrawCapCaret(void)
{
    POLY_G3*     p;
    s32          color;
    u16          x;
    u16          y;
    register u32 mask asm("a3");
    u32          mask_hi;
    u_long*      ot;

    if (Gp_CapCaretDelay != 0) {
        Gp_CapCaretDelay--;
        return;
    }

    p          = (POLY_G3*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    setPolyG3(p);

    color = (Gp_CapCaretGrey << 7) / 15;
    setRGB0(p, color, color, color);

    color = (Gp_CapCaretGrey * 0xC0) / 15;
    setRGB1(p, color, color, color);
    setRGB2(p, color, color, color);

    x     = Gp_CapCaretX;
    y     = Gp_CapCaretY;
    p->x0 = x + 3;
    p->y0 = y - Display_State.vramYOffset;
    p->x1 = (mask = 0xFF0000, x);
    asm("" : : "m"(p->y0));
    p->y1 = ((s8) * (volatile u8*)&Display_State.vramYOffset + 7) * -1 + y;
    p->x2 = x + 7;
    asm volatile("" : "+r"(mask) : "m"(p->y1));
    mask |= 0xFFFF;
    p->y2 = ((s8) * (volatile u8*)&Display_State.vramYOffset + 7) * -1 + y;

    ot      = Gpu_CurrentOt;
    mask_hi = 0xFF000000;
    p->tag  = (p->tag & mask_hi) | (ot[2] & mask);
    ot[2]   = (ot[2] & mask_hi) | ((u32)p & mask);

    if (Gp_CapCaretDir == 0) {
        Gp_CapCaretGrey++;
        if (Gp_CapCaretGrey >= 0xF) {
            Gp_CapCaretDir = 1;
        }
    } else {
        Gp_CapCaretGrey--;
        if (Gp_CapCaretGrey < 9) {
            Gp_CapCaretDir = 0;
        }
    }
}

s16 Gp_CapCenterX(u16* arg0)
{
    register s32        lineW asm("t0");
    register s32        maxW asm("t1");
    register s32        i asm("a3");
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW   = 0;
    maxW    = lineW;
    i       = lineW;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        table = Gp_CapGlyphs;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((lineW << 16) > (maxW << 16)) {
                    maxW = lineW;
                }
                lineW = 0;
                goto do_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            asm volatile("" : "+r"(masked));
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                asm volatile("" : "+r"(v0tmp));
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
                asm volatile("" : "+r"(v0tmp));
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)maxW;
    return (0x140 - width) / 2 - 5;
}

s16 Gp_CapCenterXLine(u16* arg0, s32 arg1)
{
    register s32        lineW asm("t1");
    register s32        selectedW asm("t3");
    register s32        i asm("t0");
    register s32        lineIndex asm("t2");
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW     = 0;
    selectedW = lineW;
    i         = lineW;
    lineIndex = lineW;
    code      = arg0[0];
    shifted   = code << 16;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        table = Gp_CapGlyphs;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((s16)lineIndex == arg1) {
                    selectedW = lineW;
                }
                lineW = 0;
                v0tmp = i + 1;
                i     = v0tmp;
                lineIndex++;
                goto after_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            asm volatile("" : "+r"(masked));
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                asm volatile("" : "+r"(v0tmp));
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
            after_inc:
                asm volatile("" : "+r"(v0tmp));
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)selectedW;
    return (0x140 - width) / 2 - 5;
}

s16 Gp_CapTextHeight(u16* arg0)
{
    register s32        lineH asm("a3");
    register s32        total asm("t0");
    register s32        i asm("t1");
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;
    s32                 newline;
    s32                 skip;

    lineH   = 0;
    total   = lineH;
    i       = lineH;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        newline = -2;
        skip    = -3;
        table   = Gp_CapGlyphs;
        do {
            if (shifted >> 16 == newline) {
                if (lineH == 0) {
                    lineH = 2;
                }
                total += lineH;
                lineH  = 0;
            } else if (shifted >> 16 != skip) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        asm volatile("" : "+r"(v0tmp));
                        lineH = v0tmp + 2;
                    }
                }
            }
            v0tmp   = i + 1;
            code    = arg0[(s16)v0tmp];
            i       = v0tmp;
            shifted = code << 16;
        } while (shifted >> 16 != -1);
    }
    if ((s16)total == 2) {
        total = 0;
    }
    return total;
}

s16 Gp_CapTextTopY(u16* arg0)
{
    register s32        lineH asm("a3");
    register s32        total asm("t0");
    register s32        i asm("t1");
    register s32        seenBreak asm("t2");
    u16                 code;
    s32                 shifted;
    register s32        next asm("v1");
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");

    lineH     = 0;
    total     = lineH;
    i         = lineH;
    code      = arg0[0];
    shifted   = code << 16;
    seenBreak = lineH;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        do {
            v0tmp = seenBreak;
            if (shifted >> 16 == -2) {
                if (v0tmp != 0) {
                    if (lineH == 0) {
                        lineH = 2;
                    }
                    total += lineH;
                } else {
                    seenBreak = 1;
                }
                lineH = 0;
            } else if (shifted >> 16 != -3) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)Gp_CapGlyphs);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        asm volatile("" : "+r"(v0tmp));
                        lineH = v0tmp + 2;
                    }
                }
            }
            next    = i + 1;
            code    = arg0[(s16)next];
            i       = next;
            shifted = code << 16;
            v0tmp   = -1;
        } while (shifted >> 16 != v0tmp);
    }
    return (s16)(0xD0 - total);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E6BB8);

s32 Gp_StartCapSlot(s16 arg0, s16 arg1, s16 arg2)
{
    s32 entry;

    if (Gp_CapTable != 0) {
        return 0;
    }

    entry = Gp_CapCmds[arg0];
    if (entry == 0) {
        return 1;
    }
    return (s16)Gp_StartCap(entry, arg1, arg2);
}

s32 Gp_CapBusy(void)
{
    return Gp_CapTable != 0;
}

s32 Gp_AbortCap(void)
{
    if (Gp_CapTable != 0) {
        if (Gp_CapTask != NULL) {
            Gp_CapExit(Gp_CapTask);
            return 0;
        }
        return -1;
    }
    return -1;
}

s32 Gp_GetCapEventKey(void)
{
    return Gp_CapEventKey;
}

void func_800E6D4C(s16 arg0, s16 arg1)
{
    D_80115654 = arg0;
    D_80115656 = arg1;
}

void Gp_LoadCapFile(s32 arg0)
{
    s32           i;
    s32           count;
    register s32  type3 asm("v0");
    s32*          out;
    FsFolderSlot* slot;
    DisplayState* ds;

    count = 0;
    i     = count;
    ds    = &Display_State;
    for (; i < 50; i++) {
        out   = &Gp_CapFile;
        slot  = &D_8006C338[i];
        type3 = 3;
        if (slot->field_0 == type3) {
            if (count == arg0) {
                if (ds->field_112 != 0) {
                    func_80724714();
                }
                Gp_CapFile = slot->field_4;
                Gp_RelocCapFile((GpCapFile*)Gp_CapFile);
                break;
            }
            count++;
        }
    }
}

void Gp_ResetCap(void)
{
    Gp_CapTable = 0;
    D_801156A8  = 0;
    D_8011565A  = 0;
    func_800E6D4C(0x180, 0);
    Gp_CapFile = 0;
    Gp_LoadCapFile(0);
    D_8011569C = 0;
}

void func_800E6E44(s32 arg0)
{
    D_80115660 = arg0;
}

void Gp_ApplyCapEvtFlags(void)
{
    GpEvt12* p;
    u8       field4;
    s32      base;
    s32      idx;

    idx        = *(s16*)&D_801155AE;
    base       = (s32)Gp_CapTable;
    p          = (GpEvt12*)(idx * sizeof(GpEvt12) + base);
    field4     = p->field_4;
    D_80115670 = field4;
    if (p->field_7 != 0) {
        D_80115670 = field4 & 0xFE;
    }
    D_80115678 = p->field_7;
}

s32 Gp_FindCapEvt(s32 arg0)
{
    s32      flag;
    s32      id;
    s32      base;
    GpEvt12* p;

    flag = -1;
    id   = Gp_CapEventKey;
    base = (s32)Gp_CapTable;
    p    = (GpEvt12*)(arg0 * sizeof(GpEvt12) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_800E6EF4(Task* task)
{
    if (task->state > 0) {
        if (Gp_CapTable != 0 && D_8011565A == 0) {
            Gp_CapTable = 0;
        }
        Task_Kill(task);
    }
    task->state++;
}

void Gp_DelayedMsgTask(Task* task)
{
    register s32 val asm("s0");
    GpSpawnArg*  arg;
    s32          mode;
    Task*        slot;

    switch (task->state) {
        case 0:
            arg                 = (GpSpawnArg*)&task->spawnArg1;
            task->killCountdown = arg->field_1;
            task->state++;
            break;
        case 1:
            if (task->killCountdown == 0) {
                arg  = (GpSpawnArg*)&task->spawnArg1;
                mode = arg->field_2;
                val  = arg->field_0;
                if (mode == 0) {
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x401, val, 0);
                } else if (mode == 1) {
                    slot = Game_GetPtrSlot(0xA);
                    if (slot != NULL) {
                        Gp_DispatchMsg(slot, 0x401, val, 0);
                    }
                } else {
                    slot = (Task*)Gp_LookupSlot4(mode - 2);
                    if (slot != NULL) {
                        Gp_DispatchMsg(slot, 0x7E0, val, 0);
                    }
                }
                Task_Kill(task);
            }
            task->killCountdown--;
            break;
    }
}

void func_800E704C(void)
{
    D_801155AE++;
    D_801155AE       = Gp_FindCapEvt((s16)D_801155AE);
    D_80115648       = 0;
    Gp_CapCaretDelay = 0x1E;
    D_80115659       = 0xF;
    Gp_ApplyCapEvtFlags();
}

void func_800E70AC(Task* task)
{
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                if (D_80115666 == 2) {
                    Gp_DispatchMsg(Game_GetPtrSlot(5), 0xBB8, 1, 0);
                }
                task->state++;
                break;
        }
        func_800E44A0(task);
    }
}

void Gp_EndWaitTask(Task* task)
{
    GpEndWait* flag;

    flag = task->spawnArg2;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x2C, 0, (s32)flag);
            task->state++;
            break;
        case 1:
            if (flag->field_2 != 0) {
                Stage_SetEndingFlag();
                Task_Kill(task);
            }
            break;
    }
}

void func_800E71B0(Task* task)
{
    void* mem;

    mem = Mem_Calloc(4, 0);
    if (mem == NULL) {
        Task_Kill(task);
        return;
    }
    Gp_ResetCap();
    D_801156B8     = NULL;
    task->field_24 = D_8010FB90;
    Game_SetPtrSlot(task, 6);
    task->idMap = mem;
    D_801156B0  = 0;
    task->state++;
}

void func_800E7240(void)
{
    if (Display_State.field_112 != 0) {
        func_80724120();
        func_80724324();
    }
    if (Gp_CapFile != 0) {
        Gp_RelocCapFile((GpCapFile*)Gp_CapFile);
    }
    if (Gp_CapBusy() != 0 && D_801156B0 != 0) {
        D_801156BC++;
        if ((D_801156A4 & 0x20) == 0) {
            if (D_801156BC >= 0x1E) {
                D_801156A4 |= 0x20;
                D_801156B0  = 0;
            }
        }
    }
}

s32 Gp_StartCapAndClear(s32 arg0, s32 arg1, s16 arg2)
{
    Gp_StartCapSlot(arg2, 0, 0);
    D_801156B0 = 0;
    return 0;
}

s32 func_800E731C(void)
{
    D_8011569A = 0;
    D_80115698 = 0;
    return 0;
}

s32 Gp_AbortCapClear(void)
{
    D_801156B0 = 0;
    return Gp_AbortCap();
}

s32 func_800E7358(void)
{
    return Gp_CapBusy();
}

s32 func_800E7378(void)
{
    if (Mc_SaveData.field_23 == 9) {
        if (D_801156B8 != NULL) {
            return 0;
        }
        D_801156B8 = Task_Spawn(9, 8, 0, 0);
    } else {
        Game_Session->field_68 = 1;
    }
    return 0;
}

s32 func_800E73E8(void)
{
    Task* task;

    if (Mc_SaveData.field_23 == 9) {
        task = D_801156B8;
        if (task != NULL) {
            task->spawnArg1 = 1;
            D_801156B8      = NULL;
            return 0;
        }
    } else {
        Game_Session->field_68 = 0;
    }
    return 0;
}

s32 func_800E7434(void)
{
    if (Mc_SaveData.field_23 == 9) {
        if (D_801156B8 == NULL) {
            return 0;
        }
        Task_Kill(D_801156B8);
        D_801156B8 = NULL;
    } else {
        Game_Session->field_68 = 0;
    }
    return 0;
}

s32 func_800E7498(s32 arg0, s32 arg1, GpOverlayIds* arg2)
{
    if (arg2 != NULL) {
        CdCmd_StartOverlay(arg2->field_0, arg2->field_2, arg2->field_4);
    }
    D_801156B4 = 1;
    D_801156B1 = arg2 != NULL;
    return 0;
}

s32 func_800E74EC(s32 arg0, s32 arg1, s32 arg2)
{
    if (Game_Session->field_5F == 0) {
        if (D_801156B1 != 0) {
            func_8001D580();
            D_801156B0 = 1;
            D_801156BC = 0;
        } else {
            D_801156B0 = 1;
            if (arg2 == 2) {
                D_801156BC = 0x1E;
            } else {
                D_801156BC = 0;
            }
        }
    }
    return 0;
}

void func_800E7570(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_8009752C;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E75C8);

void Gp_VolFadeTask(Task* arg0)
{
    GpVolFade* fade;
    s32        volume;

    fade = arg0->spawnArg2;
    switch (arg0->state) {
        case 0:
            if (fade->field_2 == 0) {
                Snd_ApplyVolumeTable(fade->field_0);
                Task_Kill(arg0);
                D_8010FBE4 = 0;
            } else {
                D_801156C2 = 0;
                D_801156C0 = D_8007A396;
            }
            arg0->state++;
            break;
        case 1:
            D_801156C2++;
            volume = (D_801156C0 * (fade->field_2 - D_801156C2) + fade->field_0 * D_801156C2) / fade->field_2;
            Snd_ApplyVolumeTable(volume & 0xFFFF);
            if (D_801156C2 == fade->field_2) {
                Task_Kill(arg0);
                D_8010FBE4 = 0;
            }
            break;
    }
}

void Gp_SndFadeTask(Task* arg0)
{
    GpSndFade* fade;
    s32        volume;

    fade = arg0->spawnArg2;
    switch (arg0->state) {
        case 0:
            if (fade->field_8 == 0) {
                SndEvt_EnqueueTypeA(fade->field_0, 0, (s8)fade->field_6);
                fade->field_4 = fade->field_6;
                Task_Kill(arg0);
                D_8010FBE8 = 0;
            } else {
                D_801156C6 = 0;
                D_801156C4 = fade->field_4;
            }
            arg0->state++;
            break;
        case 1:
            D_801156C6++;
            volume = (D_801156C4 * (fade->field_8 - D_801156C6) + fade->field_6 * D_801156C6) / fade->field_8;
            SndEvt_EnqueueTypeA(fade->field_0, 0, (s8)volume);
            fade->field_4 = volume;
            if (D_801156C6 == fade->field_8) {
                Task_Kill(arg0);
                D_8010FBE8 = 0;
            }
            break;
    }
}

void func_800E8614(s32 arg0, s32 arg1)
{
    func_800E8634(arg0, arg1, 0);
}

void func_800E8634(s32 arg0, s32 arg1, s32 arg2)
{
    Game_Session->field_1  = 1;
    Game_Session->field_5F = 0;
    D_8010FBE0             = 0;
    D_8010FBE4             = 0;
    D_801156D0             = arg2;
    D_801156C9             = 0;
    D_801156CC             = 0;
    D_801156F0             = 5;
    D_801156CD             = 0;
    D_801156CE             = 0;
    D_801156F8             = Mc_SaveData.field_4;
    D_801156EC             = Wip_SysConfig.field_21;
    SndEvt_EnqueueType7(0xFF0D, 1);
    Task_Spawn(9, 7, arg1, arg0);
}

s32 Gp_LookupSlot4(s32 arg0)
{
    s32 out;

    arg0 = (arg0 << 12) | (Game_Session->field_7 << 8) | Game_Session->field_6;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7D0, arg0, (s32)&out);
    return out;
}

void Gp_ScriptInit(Task* arg0)
{
    GpState34*   mem;
    GpScriptCmd* script;

    mem = Mem_Calloc(0x34, 0);
    if (mem == NULL) {
        Task_Kill(arg0);
        return;
    }
    D_801156F9 = 0;
    D_801156F4 = 0;
    Display_AcquireRef();
    script       = arg0->spawnArg2;
    D_801156A4   = 0;
    arg0->idMap  = (TaskIdMap*)mem;
    mem->field_4 = 0;
    D_801156C8   = 0;
    mem->field_0 = script;
    D_801156CA   = 0;
    if (arg0->spawnArg1 == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
    }
    D_801156CB    = 1;
    mem->field_2C = 0;
    mem->field_30 = 0;
    mem->field_28 = 0;
    D_8011569C    = 0;
    arg0->state++;
}

void func_800E8830(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_80097538;
    sp.funcs[arg0->state](arg0);
}

void func_800E8888(Task* arg0)
{
    s16 tmp;

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
            arg0->spawnArg1     = -1;
            arg0->state++;
            break;
        case 1:
            arg0->killCountdown = (u16)arg0->killCountdown - (u16)arg0->spawnArg1;
            if (arg0->killCountdown >= 9) {
                arg0->killCountdown = 8;
            }
            tmp = arg0->killCountdown;
            if (tmp < 0) {
                Game_Session->field_139 = 0;
                Task_Kill(arg0);
            } else {
                Game_Session->field_139 = tmp * 2;
            }
            break;
    }
}

void Gp_ShakeTask(Task* arg0)
{
    register s32 tmp asm("v0");
    register s32 hi asm("v1");
    s32          packed;
    s32          lo;
    s32          scaled;
    s32          val;

    packed = (s32)arg0->spawnArg2;
    lo     = packed & 0xFF;

    switch (arg0->state) {
        case 0:
            arg0->spawnArg1 = -lo;
            arg0->state++;
            break;
        case 1:
            if (lo < arg0->spawnArg1) {
                Display_ClampField126(0);
                Task_Kill(arg0);
            } else {
                tmp         = ABS(arg0->spawnArg1);
                hi          = lo - tmp;
                tmp         = packed >> 8;
                scaled      = hi * tmp;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                hi          = (u32)Gp_LcgState >> 16;
                hi          = scaled * hi;
                hi          = hi / lo;
                val         = hi >> 16;
                if (arg0->spawnArg1 & 1) {
                    val = ABS(val);
                    Display_ClampField126(val);
                } else {
                    tmp = ABS(val);
                    val = -tmp;
                    Display_ClampField126(val);
                }
                arg0->spawnArg1++;
            }
            break;
    }
}

void Gp_StepScriptA(Task* task)
{
    GpState34*   state;
    GpScriptCmd* table;
    GpScriptRec* recs;
    u16          cmd;
    s32          opcode;
    u8           tmp;

    state          = (GpState34*)task->idMap;
    table          = state->field_0;
    recs           = state->field_4;
    cmd            = table[state->field_E].field_0;
    opcode         = cmd & 0xFF;
    state->field_A = cmd;

    if (opcode != 0) {
        if (opcode == 1) {
            state->field_12 = cmd >> 8;
            state->field_10 = recs[state->field_12].field_2;
            Gp_SpawnPadHold(state->field_10);
            state->field_E++;
        } else if (opcode == 2) {
            state->field_10 = cmd >> 8;
            state->field_E++;
        } else if (opcode == 3) {
            tmp = state->field_14;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_14 = tmp;
                state->field_E++;
            } else {
                tmp--;
                state->field_14 = tmp;
                state->field_E++;
            }
        } else if (opcode == 4) {
            if (state->field_14 == 0) {
                state->field_E++;
            } else {
                state->field_E = table[state->field_E].field_0 >> 8;
            }
            Gp_StepScriptA(task);
        }
    }
}

void Gp_StepScriptB(Task* task)
{
    GpState34*   state;
    GpScriptCmd* table;
    GpScriptRec* recs;
    u16          cmd;
    s32          opcode;
    u8           tmp;

    state          = (GpState34*)task->idMap;
    table          = state->field_0;
    recs           = state->field_4;
    cmd            = table[state->field_F].field_2;
    opcode         = cmd & 0xFF;
    state->field_C = cmd;

    if (opcode != 0) {
        if (opcode == 1) {
            state->field_13 = cmd >> 8;
            state->field_11 = recs[state->field_13].field_2;
            Gp_SpawnPadLerpScaled(state->field_11, recs[state->field_13].field_0, recs[state->field_13].field_1, state->field_8);
            state->field_F++;
        } else if (opcode == 2) {
            state->field_11 = cmd >> 8;
            state->field_F++;
        } else if (opcode == 3) {
            tmp = state->field_15;
            if (tmp == 0) {
                tmp             = cmd >> 8;
                state->field_15 = tmp;
                state->field_F++;
            } else {
                tmp--;
                state->field_15 = tmp;
                state->field_F++;
            }
        } else if (opcode == 4) {
            if (state->field_15 == 0) {
                state->field_F++;
            } else {
                state->field_F = table[state->field_F].field_2 >> 8;
            }
            Gp_StepScriptB(task);
        }
    }
}

void Gp_SpawnPadHold(s16 arg0)
{
    if (arg0 != 0) {
        Task_Spawn(2, 0xB, arg0, 0);
    }
}

void Gp_SpawnPadLerp(s16 arg0, u8 arg1, u8 arg2)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;

    if (arg0 != 0) {
        mem = Mem_Calloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                Mem_Free(mem);
            } else {
                end                 = (arg2 & 0xFF) << 8;
                start               = (arg1 & 0xFF) << 8;
                task->idMap         = (TaskIdMap*)mem;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

void Gp_SpawnPadLerpScaled(s16 arg0, u8 arg1, u8 arg2, s16 arg3)
{
    Task*      task;
    GpState0C* mem;
    s32        start;
    s32        end;
    s16        scale;
    s32        temp;

    if (arg0 != 0) {
        mem = Mem_Calloc(0xC, 0);
        if (mem != NULL) {
            task = Task_Spawn(2, 0xC, 0, 0);
            if (task == NULL) {
                Mem_Free(mem);
            } else {
                task->idMap = (TaskIdMap*)mem;
                temp        = arg3 >> 3;
                if (temp == 0) {
                    scale = 1;
                } else {
                    scale = temp;
                }
                end                 = (arg2 & 0xFF) / scale;
                start               = (arg1 & 0xFF) / scale;
                end               <<= 8;
                start             <<= 8;
                mem->field_8        = arg0;
                mem->field_4.as_s32 = start;
                mem->field_0        = (end - start) / arg0;
            }
        }
    }
}

void Gp_HaltPadScripts(void)
{
    Gp_PadScriptHalt        = 1;
    Gp_PadHoldHalt          = 1;
    Gp_PadLerpHalt          = 1;
    Game_Session->field_13B = 0;
    Pad_ClearEvents(0);
}

Task* Gp_SpawnScript18(s32 arg0, s32 arg1)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = 0;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

void Gp_KickScriptAB(Task* task)
{
    Gp_StepScriptA(task);
    Gp_StepScriptB(task);
    task->state++;
}

void Gp_DispatchScript18(Task* task)
{
    TaskFuncTable5 tableA;
    TaskFuncTable5 tableB;
    GpState18*     state;

    state  = (GpState18*)task->idMap;
    tableA = D_80097638;
    tableB = D_8009764C;
    tableA.funcs[state->field_A](task);
    tableB.funcs[state->field_C](task);
    if (state->field_A == 0 && state->field_C == 0) {
        task->state++;
    }
}

void Gp_ClearPadHalt(void)
{
    Gp_PadScriptHalt = 0;
    Gp_PadHoldHalt   = 0;
    Gp_PadLerpHalt   = 0;
}

Task* Gp_SpawnScript18Ex(s32 arg0, s32 arg1, s32 arg2)
{
    Task*      task;
    GpState18* mem;

    mem = Mem_Calloc(0x18, 0);
    if (mem != NULL) {
        task = Task_Spawn(2, 0xD, 0, 0);
        if (task != NULL) {
            task->idMap  = (TaskIdMap*)mem;
            mem->field_8 = arg2;
            mem->field_0 = arg0;
            mem->field_4 = arg1;
            return task;
        }
        Mem_Free(mem);
    }
    return NULL;
}

void Gp_Script18Task(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_8009762C;
    if (D_801153F4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (Gp_PadScriptHalt != 0) {
            arg0->state = 2;
        }
        sp.funcs[arg0->state](arg0);
    }
}

void func_800E92BC(void)
{
}

void Gp_TickScriptADelay(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_10 == 0) {
        Gp_StepScriptA(task);
    }
}

void func_800E9308(Task* task)
{
    Gp_StepScriptA(task);
}

void func_800E9328(Task* task)
{
    Gp_StepScriptA(task);
}

void func_800E9348(void)
{
}

void Gp_TickScriptBDelay(Task* task)
{
    GpState34* state;

    state = (GpState34*)task->idMap;
    if (--state->field_11 == 0) {
        Gp_StepScriptB(task);
    }
}

void func_800E9394(Task* task)
{
    Gp_StepScriptB(task);
}

void func_800E93B4(Task* task)
{
    Gp_StepScriptB(task);
}

void Gp_PadHoldTask(Task* task)
{
    if (D_801153F4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (task->spawnArg1 != 0 && Gp_PadHoldHalt == 0) {
            task->spawnArg1--;
            Pad_PostEvent(0, 0, 1, 1);
            Game_Session->field_13B |= 1;
        } else {
            Game_Session->field_13B &= ~1;
            Task_Kill(task);
        }
    }
}

void Gp_PadLerpTask(Task* task)
{
    GpState0C* state;

    state = (GpState0C*)task->idMap;
    if (D_801153F4 == 0 || (Game_Session->field_13B & 0x80)) {
        if (state->field_8 != 0 && Gp_PadLerpHalt == 0) {
            state->field_8--;
            Pad_PostEvent(0, 1, state->field_4.bytes.as_u8, 1);
            state->field_4.as_s32   += state->field_0;
            Game_Session->field_13B |= 2;
        } else {
            Game_Session->field_13B &= ~2;
            Task_Kill(task);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3CD8", func_800E956C);

u16 Gp_RemapButtons(GameActor* actor, u16 mask)
{
    u16 result;
    s32 i;

    result = 0;
    switch (Mc_SaveData.field_1a8) {
        case 0:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap0[i];
                }
            }
            break;
        case 1:
            for (i = 0; i < 0x10; i++) {
                if ((mask >> i) & 1) {
                    result |= 1 << Gp_BtnMap1[i];
                }
            }
            break;
        case 2:
            if (actor->field_954 == 0 && actor->field_956 >= 2) {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2Alt[i];
                    }
                }
            } else {
                for (i = 0; i < 0x10; i++) {
                    if ((mask >> i) & 1) {
                        result |= 1 << Gp_BtnMap2[i];
                    }
                }
            }
            break;
    }
    return result;
}

void func_800E9BDC(u8 arg0, s32 arg1)
{
    switch (arg0) {
        case 1:
        case 5:
            D_80115714  = 0;
            D_8011570A |= arg1;
            break;
        case 3:
            D_8011570A |= arg1;
            D_80115714  = 1;
            break;
        case 0:
        case 2:
            D_80115714  = 0;
            D_8011570A &= ~arg1;
        case 4:
            break;
    }
}

void func_800E9C6C(void)
{
    D_80115712 = 0;
    D_8011570A = 0;
    D_8011570C = 0;
    D_8011570E = 0;
    D_80115710 = 0;
    D_80115708 = 0;
    D_80115709 = 0;
    D_80115714 = 0;
    D_80115716 = 8;
    D_80115718 = 4;
}
