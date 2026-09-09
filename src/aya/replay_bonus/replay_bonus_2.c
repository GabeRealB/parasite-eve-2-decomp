#include "common.h"

#include "aya/replay_bonus.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/wipsys.h"
#include "psyq/libpress.h"
#include "psyq/strings.h"
extern u8           D_replay_bonus_801157A8[];
extern u8           D_replay_bonus_801157B0[];
extern u8           D_replay_bonus_801157C4[];
extern u8           D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern UiObjectDesc D_replay_bonus_801191A8;
extern s32          D_replay_bonus_80119288;
extern s32          D_replay_bonus_8011928C;
extern GpItemDesc   D_8010DE38[];
extern s32          D_80072A9C;

extern UiObjectDesc D_800611E4;
extern u8           D_80071086;
extern UiObjectDesc D_replay_bonus_80119170;
extern UiObjectDesc D_replay_bonus_8011918C;
extern UiObjectDesc D_replay_bonus_801191C4;
extern UiObjectDesc D_replay_bonus_801191E0;
extern UiObjectDesc D_replay_bonus_801191FC;
extern s32          D_replay_bonus_80119284;
extern s8           D_80071068;

void func_replay_bonus_80116EC0(void);

void func_replay_bonus_80117194(Task* arg0)
{
    UiObject* obj;
    Task*     owner;
    s32       copied;
    s16       flag;

    obj  = arg0->spawnArg2;
    flag = obj->field_2E;
    if ((flag == -1) || (flag == 6)) {
        owner  = obj->owner;
        copied = obj->field_2C;
        Ui_TeardownTree(obj, owner);
        switch (arg0->state) {
            case 2:
                arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_8011918C, 0, 1, 1, NULL);
                break;
            case 3:
                if (D_replay_bonus_80119284 < 0) {
                    arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_801191FC, 0, 1, 1, NULL);
                    arg0->state     = arg0->state + 2;
                } else {
                    arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_801191C4, 0, 1, 1, NULL);
                }
                break;
            case 4:
                arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_801191E0, 1, 1, 1, NULL);
                break;
            case 5:
                arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_801191C4, 2, 1, 1, NULL);
                break;
            case 6:
                GameMain_SetFrameTiming(0);
                func_replay_bonus_80116EC0();
                D_80071086      = 0xFF;
                arg0->spawnArg2 = Ui_SpawnFromDesc(&D_800611E4, 0, 1, 1, NULL);
                break;
            case 7:
                if (copied == 0x33) {
                    arg0->spawnArg2 = ((UiObject * (*)(UiObject*, s32, s32, s32)) Gp_SpawnItemPrompt)(NULL, 0x11, 0, 1);
                    arg0->state     = arg0->state + 1;
                } else {
                    arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_80119170, 0, 1, 2, NULL);
                }
                break;
            case 8:
                if (copied == 0x33) {
                    arg0->spawnArg2 = ((UiObject * (*)(UiObject*, s32, s32, s32)) Gp_SpawnItemPrompt)(NULL, 0xF, 0, 1);
                } else {
                    arg0->spawnArg2 = Ui_SpawnFromDesc(&D_800611E4, 1, 1, 1, NULL);
                    arg0->state     = arg0->state - 2;
                }
                break;
        }
        arg0->state = arg0->state + 1;
        return;
    }
    arg0->killCountdown = 0x10;
}

s32 func_replay_bonus_801173A8(void)
{
    ReplayBonusShopTier* p;
    u32                  spend;
    s32                  idx;
    s32                  i;
    McSaveData*          save;
    s32                  mask;
    s32                  one;

    spend = func_replay_bonus_80115CA4();
    p     = D_replay_bonus_80118F78;
    idx   = 0;
    if (D_80072A9C == 0x1FFF) {
        return -1;
    }
    i = 0;
    do {
    loop:
        if (!(p->spendThreshold < spend)) {
            idx = i;
            break;
        }
        i++;
        p++;
        if (i < 0xD) {
            goto loop;
        }
    } while (0);

    save = &Mc_SaveData;
    idx += save->field_F;
    i    = 0;
    if (idx >= 0xD) {
        idx = 0xC;
    }
    one  = 1;
    mask = save->field_934;
    do {
        if ((mask & (one << idx)) == 0) {
            return idx;
        }
        idx += 1;
        if (idx >= 0xD) {
            idx -= 0xD;
        }
        i += 1;
    } while (i < 0xD);
    return idx;
}

s16 func_replay_bonus_80117484(s32 arg0, s32 arg1)
{
    ReplayBonusShopTier* p;
    u32                  spend;
    s32                  idx;
    s32                  i;
    McSaveData*          save;
    s32                  mask;
    s32                  one;
    s32                  result;

    spend = func_replay_bonus_80115CA4();
    p     = D_replay_bonus_80118F78;
    idx   = 0;
    if (D_80072A9C == 0x1FFF) {
        result = -1;
    } else {
        i = 0;
        do {
        loop:
            if (!(p->spendThreshold < spend)) {
                idx = i;
                break;
            }
            i++;
            p++;
            if (i < 0xD) {
                goto loop;
            }
        } while (0);

        save = &Mc_SaveData;
        idx += save->field_F;
        i    = 0;
        if (idx >= 0xD) {
            idx = 0xC;
        }
        one  = 1;
        mask = save->field_934;
        do {
        loop2:
            if ((mask & (one << idx)) == 0) {
                break;
            }
            idx += 1;
            if (idx >= 0xD) {
                idx -= 0xD;
            }
            i += 1;
            if (i < 0xD) {
                goto loop2;
            }
        } while (0);
        result = idx;
    }
    if (result < 0) {
        return 0;
    }
    return D_replay_bonus_80118F78[result].items[arg1];
}

s32 func_replay_bonus_80117598(s32 arg0)
{
    u16* p;
    s32  i;

    p = D_replay_bonus_8011908C;
    i = 0;
    do {
        i++;
        if (*p != arg0) {
            p++;
        } else {
            return 1;
        }
    } while (i < 0x4E);
    return 0;
}

s16 func_replay_bonus_801175D0(UiList* list, ReplayBonusCtx* ctx, s32 index)
{
    s16* p = ctx->itemList->itemIds + index;

    return *p;
}

s32 func_replay_bonus_801175F0(UiList* list, ReplayBonusCtx* ctx)
{
    WipSysConfig* cfg;
    s32           i;
    s32           n;
    s32           sum;
    s32           limit;
    s16*          p;
    s32           item;
    s32           idx;
    s32           lo;
    s32           hi;
    s32           ptr;
    s32           price;
    s32           ids;
    s32           off;

    cfg = &Wip_SysConfig;
    i   = *(s8*)&list->field_9;
    n   = list->field_4;
    sum = 0;
    if (i < n) {
        lo    = (s32)Gp_ItemDescs;
        hi    = (s32)D_8010DE38;
        limit = n;
        ids   = (s32)ctx->itemList->itemIds;
        off   = i * 2;
        p     = (s16*)(off + ids);
        do {
            item = *p;
            idx  = item;
            if (item < 0x100) {
                SOFT_TOUCH_REG(idx);
                ptr = (item * 8) + lo;
            } else {
                ptr = ((idx - 0x100) * 8) + hi;
            }
            price = *(u16*)ptr;
            SOFT_TOUCH_REG(price);
            sum += price >> 1;
            i++;
            p++;
        } while (i < limit);
    }
    sum += cfg->field_C;
    if (sum > 0x05F5E0FF) {
        sum = 0x05F5E0FF;
    }
    return sum;
}

void func_replay_bonus_801176A8(DialogPrompt* prompt, UiObject* obj)
{
    u8   buf[0x20];
    s32  item;
    s32  idx;
    s16* p;
    s32  table;
    s32  off;
    s32  price;

    p = ((ReplayBonusItemList*)obj->owner)->itemIds + prompt->field_8;
    SOFT_BARRIER();
    item = *p;
    idx  = item;
    Gp_SetItemSeenBit(item, 1);
    Gp_DrawItemLabel(obj, prompt->field_18, prompt->field_1A, item, 0x606060, 0);
    if (item < 0x100) {
        SOFT_TOUCH_REG(idx);
        table = (s32)Gp_ItemDescs;
        SOFT_TOUCH_REG(table);
        off = item * 8;
    } else {
        table = (s32)D_8010DE38;
        SOFT_TOUCH_REG(table);
        off = (idx - 0x100) * 8;
    }
    price = *(u16*)(off + table);
    SOFT_TOUCH_REG(price);
    Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, Text_ItoaSigned(buf, price >> 1), 0x606060, 3, 2);
}

s32 func_replay_bonus_801177A0(void)
{
    ReplayBonusShopTier* tier;
    s16*                 p;
    s32                  j;
    s32                  sum;
    s32                  i;

    sum  = 0;
    tier = D_replay_bonus_80118F78;
    i    = sum;
    do {
        j = 0;
        p = (s16*)tier;
        do {
            sum += Gp_ItemDescs[p[2]].price;
            p++;
            j++;
        } while (j < 3);
        i++;
        tier++;
    } while (i < 0xD);
    sum += 0x1869F;
    sum  = sum / 100000;
    return sum * 0x186A0;
}

void func_replay_bonus_80117848(Task* arg0)
{
    u16 timer = arg0->killCountdown + 1;

    arg0->killCountdown = timer;
    if ((s16)timer >= 0x78) {
        Game_Session->field_2 = 1;
        GameMain_SetFrameTiming(0);
        CdCmd_EnqueueLoadFile(1, 0x3E, 3);
        arg0->state = arg0->state + 1;
    }
}

void func_replay_bonus_801178C0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Text_LoadClutImages();
        arg0->spawnArg2 = Ui_SpawnFromDesc(&D_replay_bonus_80119154, 0, 1, 1, NULL);
        arg0->state     = (s32)(arg0->state + 1);
    }
}

void func_replay_bonus_80117924(Task* arg0)
{
    u16 remaining = arg0->killCountdown - 1;

    arg0->killCountdown = remaining;
    if ((s16)remaining < 0) {
        Display_State.field_11e = 0;
        Game_Session->field_2   = 0;
        Task_CallExit(arg0);
        Display_State.field_11e = 1;
    }
}

void func_replay_bonus_8011797C(Task* arg0)
{
    TaskFunc states[11] = {
        func_replay_bonus_80117848,
        func_replay_bonus_801178C0,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117924,
    };

    states[arg0->state](arg0);
}

void func_replay_bonus_80117A08(Task* arg0)
{
    RECT               rect;
    s32                temp_a1;
    s32                temp_v1_2;
    s32                temp_v1_3;
    u16*               temp_v0;
    u16                temp_v0_2;
    u16                temp_v0_3;
    u32                temp_v1;
    ReplayBonusStfHdr* hdr;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            D_replay_bonus_80119226        = 0;
            D_replay_bonus_80119227        = 0;
            D_replay_bonus_801192AC        = 0;
            D_replay_bonus_801192BC        = Mem_Calloc(0x10U, false);
            temp_v0                        = func_replay_bonus_80115C68();
            D_replay_bonus_80119228        = NULL;
            D_replay_bonus_80119225        = 0;
            D_replay_bonus_801192BC->table = temp_v0;
            func_replay_bonus_80118F00(0);
            GameMain_SetFrameTiming(0);
            SetDispMask(1);
            Display_SetMode(0x1141);
            rect.x = 0x280;
            rect.w = 0xF0;
            rect.y = 0;
            rect.h = 0xB0;
            ClearImage(&rect, 0, 0, 0);
            rect.y = 0x100;
            ClearImage(&rect, 0, 0, 0);
            D_replay_bonus_801192A4 = -0x1E0;
            D_80071068              = 0;
            D_replay_bonus_801192B0 = 0;
            arg0->killCountdown     = D_replay_bonus_80119294->hold0 * 6;
            CdCmd_StartOverlay(0U, 1U, 0xBU);
            CdCmd_EnqueueOverlay82();
            goto advance;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                CdCmd_EnqueueOverlay81();
                goto advance;
            }
            return;
        case 2:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            temp_v0_2           = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0_2;
            if ((temp_v0_2 << 0x10) <= 0) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 1, 0xB4, 0);
                arg0->state = 0xA;
            }
            if (Pad_CheckFlag800() != 0) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0x1E, 0);
                CdCmd_CancelReplaceAndActivate();
                arg0->state         = 0xB;
                arg0->killCountdown = 0x1E;
                return;
            }
            break;
        case 10:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            hdr                     = D_replay_bonus_80119294;
            temp_v1_2               = D_replay_bonus_801192A8 + hdr->speed;
            temp_a1                 = D_replay_bonus_801192A4 + (temp_v1_2 >> 8);
            D_replay_bonus_801192A8 = temp_v1_2;
            D_replay_bonus_801192A4 = temp_a1;
            D_replay_bonus_801192A8 = temp_v1_2 & 0xFF;
            temp_v1_3               = D_replay_bonus_80119298[D_replay_bonus_801192A0 - 1].y - 0x1E0;
            if (temp_v1_3 < temp_a1) {
                D_replay_bonus_801192A4 = temp_v1_3;
                arg0->state            += 1;
                arg0->killCountdown     = hdr->hold1 * 6;
            }
            if (Pad_CheckFlag800() != 0) {
                CdCmd_CancelReplaceAndActivate();
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0x1E, 0);
                arg0->killCountdown = 0x1E;
                arg0->state        += 1;
                return;
            }
            break;
        case 11:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            temp_v0_3           = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0_3;
            if ((s16)temp_v0_3 == 0xB4) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0xB4, 0);
            }
            if ((s16)arg0->killCountdown <= 0) {
                arg0->state = 0x14;
                return;
            }
            break;
        case 20:
            if (D_replay_bonus_80119225 != 1) {
                SetDispMask(0);
                goto advance;
            }
            break;
        case 21:
            Gp_RestoreStreamRng();
            Mem_Free(D_replay_bonus_801192BC);
            Display_SetMode(0x1010);
            goto advance;
        case 24:
            func_800B2968();
        case 22:
        case 23:
        advance:
            arg0->state += 1;
            return;
        case 25:
            SetDispMask(1);
            Task_Kill(arg0);
            break;
    }
}

void func_replay_bonus_80117DE0(u8 arg0)
{
    char pad[0x10];

    D_replay_bonus_801192AC = 0x7F - (arg0 >> 1);
}

void func_replay_bonus_80117E04(void)
{
    s32                          start;
    s32                          i;
    s32                          end;
    s32                          count;
    s32                          n;
    s32                          scrollY;
    s32                          visEnd;
    s32                          y;
    s32                          rgb;
    s32                          code;
    u8                           wait;
    volatile ReplayBonusStfLine* line;
    LINE_F2*                     lf2;
    SPRT*                        sprt;
    DR_TPAGE*                    tpage;

    start                   = 0;
    i                       = start;
    count                   = D_replay_bonus_801192A0;
    D_replay_bonus_801192B4 = 0;
    D_replay_bonus_801192C0 =
        Gpu_PrimHeapBase + Gpu_PrimHeapSize + (D_replay_bonus_80119224 << 16);
    D_replay_bonus_80119224 ^= 1;
    end                      = count - 1;

    if (count > 0) {
        n       = count;
        line    = D_replay_bonus_80119298;
        scrollY = D_replay_bonus_801192A4;
        visEnd  = scrollY + 0x1E0;
        do {
            if (line->y < scrollY) {
                start = 0;
                if (i != 0) {
                    start = i - 1;
                }
            }
            if (visEnd < line->y) {
                end = i;
                break;
            }
            i++;
            line++;
        } while (i < n);
        i = start;
    }

    for (; i < end + 1; i++) {
        lf2                      = (LINE_F2*)(D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
        D_replay_bonus_801192B4 += 0x10;
        setLineF2(lf2);
        y       = D_replay_bonus_80119298[i].y - (D_replay_bonus_801192A4 & 0xFFFFFE) - 0xF0;
        lf2->x0 = -0x140;
        lf2->x1 = 0x140;
        lf2->r0 = 0x40;
        lf2->g0 = 0x80;
        lf2->b0 = 0x40;
        lf2->y0 = y;
        lf2->y1 = y;
        func_replay_bonus_801183B8(y, D_replay_bonus_80119298[i].cmds);
    }

    tpage          = Gpu_PrimCursor;
    Gpu_PrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, 0);
    addPrim(Gpu_CurrentOt + 10, tpage);

    if (D_replay_bonus_80119225 != 2) {
        sprt           = (SPRT*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
        setSprt(sprt);
        sprt->x0 = D_replay_bonus_801192B8 - 0x140;
        sprt->y0 = -0x8C;
        sprt->w  = 0xF0;
        sprt->h  = 0xB0;
        sprt->r0 = D_replay_bonus_801192AC;
        sprt->g0 = D_replay_bonus_801192AC;
        sprt->b0 = D_replay_bonus_801192AC;
        sprt->u0 = 0;
        sprt->v0 = 0;
        addPrim(Gpu_CurrentOt + 11, sprt);

        tpage          = Gpu_PrimCursor;
        Gpu_PrimCursor = tpage + 1;
        setDrawTPage(tpage, 0, 1, getTPage(2, 0, 0x280, D_replay_bonus_80119226 << 8));
        addPrim(Gpu_CurrentOt + 11, tpage);
        return;
    }

    code           = 0x64;
    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    setlen(sprt, 4);
    setcode(sprt, code);
    rgb      = (D_replay_bonus_801192AC * (0x78 - D_replay_bonus_80119227)) / 120;
    sprt->x0 = D_replay_bonus_801192B8 - 0x140;
    sprt->y0 = -0x8C;
    sprt->w  = 0xF0;
    sprt->h  = 0xB0;
    sprt->u0 = 0;
    sprt->v0 = 0;
    setSemiTrans(sprt, 1);
    sprt->r0 = rgb;
    sprt->g0 = rgb;
    sprt->b0 = rgb;
    addPrim(Gpu_CurrentOt + 11, sprt);

    tpage          = Gpu_PrimCursor;
    Gpu_PrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, getTPage(2, 1, 0x280, D_replay_bonus_80119226 << 8));
    addPrim(Gpu_CurrentOt + 11, tpage);

    sprt           = (SPRT*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
    setlen(sprt, 4);
    setcode(sprt, code);
    rgb      = (D_replay_bonus_801192AC * D_replay_bonus_80119227) / 120;
    sprt->x0 = D_replay_bonus_801192B8 - 0x140;
    sprt->y0 = -0x8C;
    sprt->w  = 0xF0;
    sprt->h  = 0xB0;
    sprt->u0 = 0;
    sprt->v0 = 0;
    sprt->r0 = rgb;
    sprt->g0 = rgb;
    sprt->b0 = rgb;
    addPrim(Gpu_CurrentOt + 11, sprt);

    tpage          = Gpu_PrimCursor;
    Gpu_PrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, getTPage(2, 0, 0x280, (D_replay_bonus_80119226 ^ 1) << 8));
    addPrim(Gpu_CurrentOt + 11, tpage);

    wait                    = D_replay_bonus_80119227 - 1;
    D_replay_bonus_80119227 = wait;
    if (!(wait & 0xFF)) {
        D_replay_bonus_80119225 = 0;
    }
}

void func_replay_bonus_801183B8(s32 y, ReplayBonusStfCmd* cmds)
{
    s32                  tpageId;
    s32                  col;
    s32                  align;
    s16                  i;
    s16                  j;
    s32                  width;
    s16                  x;
    s16                  x1;
    s16                  y0;
    s32                  glyphFlag;
    s16                  shift;
    s16                  piece;
    s32                  gu;
    s16                  gv;
    u8                   idx;
    s32                  tpageX;
    s32                  clut;
    s32                  clutY;
    s32                  gh;
    s32                  flags;
    u16                  page;
    u8                   pageFlags;
    ReplayBonusStfGlyph* glyph;
    ReplayBonusStfSpr*   spr;
    POLY_FT4*            p;
    SPRT*                sprt;
    DR_TPAGE*            dr;
    u_long*              glyphOt;
    u_long               glyphTag;
    u_long               drawTag;

    tpageId = 7;
    col     = 0;
    align   = col;
    i       = col;
    if (cmds->op != 0xFF) {
        do {
            switch (cmds[i].op) {
                case 0:
                    width = 0;
                    j     = 0;
                    while (cmds[i + j].op == 0) {
                        width += D_replay_bonus_80119290[cmds[i + j].arg].w;
                        j++;
                    }
                    x = 0;
                    switch ((u8)align) {
                        case 0:
                            x = D_replay_bonus_80119294->align[col].left - ((s16)width / 2);
                            break;
                        case 1:
                            x = D_replay_bonus_80119294->align[col].center;
                            break;
                        case 2:
                            x = D_replay_bonus_80119294->align[col].right - width;
                            break;
                    }
                    x -= 0x140;
                    j  = 0;
                    while (cmds[i + j].op == 0) {
                        glyph                    = &D_replay_bonus_80119290[cmds[i + j].arg];
                        width                    = glyph->w;
                        gh                       = glyph->h;
                        gu                       = glyph->u;
                        gv                       = glyph->v;
                        p                        = (POLY_FT4*)((u8*)D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
                        D_replay_bonus_801192B4 += 0x28;
                        setPolyFT4(p);
                        y0        = gh & 0x7F;
                        p->u0     = gu;
                        p->v0     = gv;
                        p->u1     = width + gu;
                        p->v1     = gv;
                        p->u2     = gu;
                        p->v2     = gv + y0;
                        p->u3     = width + gu;
                        p->v3     = gv + y0;
                        p->r0     = ((CVECTOR*)&D_replay_bonus_801192AC)->r;
                        p->x0     = x;
                        p->x2     = x;
                        p->y2     = y;
                        p->y3     = y;
                        p->g0     = ((CVECTOR*)&D_replay_bonus_801192AC)->r;
                        p->b0     = D_replay_bonus_801192AC;
                        y0        = y - y0;
                        x1        = x + width;
                        p->y0     = y0;
                        p->x1     = x1;
                        p->y1     = y0;
                        p->x3     = x1;
                        glyphFlag = (u32)gh >> 7;
                        p->clut   = (((tpageId / 4) + 0x1FE) << 6) | ((tpageId & 3) | 0x38);
                        p->tpage  = 0x1E;
                        if (glyphFlag) {
                            p->tpage = 0xF;
                        }
                        x = x1;
                        setaddr(p, getaddr(Gpu_CurrentOt + 10));
                        glyphOt  = Gpu_CurrentOt + 10;
                        glyphTag = (*glyphOt & 0xFF000000) | ((u_long)p & 0xFFFFFF);
                        SOFT_USE_REG(gv);
                        *glyphOt = glyphTag;
                        j++;
                    }
                    i += j - 1;
                default:
                case 9:
                    break;
                case 3:
                    tpageId = cmds[i].arg;
                    break;
                case 4:
                    align = 0;
                    col   = cmds[i].arg;
                    break;
                case 5:
                    align = 1;
                    col   = cmds[i].arg;
                    break;
                case 6:
                    align = 2;
                    col   = cmds[i].arg;
                    break;
                case 7:
                    idx = cmds[i].arg;
                    x   = 0;
                    if (idx != 0xFF) {
                        cmds[i].arg = 0xFF;
                        switch ((u8)align) {
                            case 0:
                                x = D_replay_bonus_80119294->align[col].left - 0x78;
                                break;
                            case 1:
                                x = D_replay_bonus_80119294->align[col].center;
                                break;
                            case 2:
                                x = D_replay_bonus_80119294->align[col].right - 0xF0;
                                break;
                        }
                        D_replay_bonus_801192B8 = x;
                        Task_SpawnFromTable(&D_replay_bonus_8011922C, 3, idx + 1, 0);
                    }
                    break;
                case 8:
                    idx   = cmds[i].arg;
                    width = D_replay_bonus_8011929C[idx].w;
                    x     = 0;
                    switch ((u8)align) {
                        case 0:
                            x = D_replay_bonus_80119294->align[col].left - ((s16)width / 2);
                            break;
                        case 1:
                            x = D_replay_bonus_80119294->align[col].center;
                            break;
                        case 2:
                            x = D_replay_bonus_80119294->align[col].right - width;
                            break;
                    }
                    shift  = 2;
                    spr    = (ReplayBonusStfSpr*)(idx * sizeof(*spr) + (size_t)D_replay_bonus_8011929C);
                    tpageX = spr->tpageX;
                    gu     = spr->u;
                    gv     = spr->v;
                    gh     = spr->h;
                    clutY  = spr->clutY << 6;
                    clut   = clutY | ((spr->clutX >> 4) & 0x3F);
                    flags  = spr->flags;
                    switch (flags) {
                        case 1:
                            shift = 1;
                            break;
                        case 2:
                            shift = 0;
                            break;
                    }
                    while ((s16)width > 0) {
                        piece = width;
                        if (gu + (s16)width >= 0x101) {
                            piece = 0x100 - gu;
                            width = width - piece;
                            gu    = 0;
                        } else {
                            width = 0;
                        }
                        sprt           = (SPRT*)Gpu_PrimCursor;
                        Gpu_PrimCursor = (DR_TPAGE*)(sprt + 1);
                        setlen(sprt, 4);
                        setcode(sprt, 0x64);
                        sprt->r0   = D_replay_bonus_801192AC;
                        sprt->x0   = x - 0x140;
                        sprt->g0   = D_replay_bonus_801192AC;
                        sprt->b0   = ((CVECTOR*)&D_replay_bonus_801192AC)->r;
                        sprt->y0   = y - gh;
                        sprt->w    = piece;
                        sprt->h    = gh;
                        sprt->clut = clut;
                        sprt->u0   = gu;
                        sprt->v0   = gv;
                        setaddr(sprt, getaddr(Gpu_CurrentOt + 10));
                        dr             = (DR_TPAGE*)Gpu_PrimCursor;
                        Gpu_PrimCursor = dr + 1;
                        setaddr(Gpu_CurrentOt + 10, sprt);
                        setlen(dr, 1);
                        pageFlags   = D_replay_bonus_8011929C[idx].flags;
                        page        = (u32)(tpageX & 0x3FF) >> 6;
                        dr->code[0] = ((pageFlags & 3) << 7) | (s16)((s32)((D_replay_bonus_8011929C[idx].tpageBits & 0x100) << 16) >> 20) | page | ((s16)(D_replay_bonus_8011929C[idx].tpageBits & 0x200) * 4) | 0xE1000200;
                        tpageX     += 0x100 >> shift;
                        x           = x + piece;
                        drawTag     = (dr->tag & 0xFF000000) | (getaddr(Gpu_CurrentOt + 10) & 0xFFFFFF);
                        SOFT_USE_REG2(piece, gh);
                        dr->tag = drawTag;
                        setaddr(Gpu_CurrentOt + 10, dr);
                    }
                    break;
            }
            i++;
        } while (cmds[i].op != 0xFF);
    }
}

s32 func_replay_bonus_80118B6C(ReplayBonusStfFile* file, s32 index)
{
    ReplayBonusStfLine*  rec;
    ReplayBonusStfLine** slot;
    ReplayBonusStfTable* table;
    s32                  count;
    s32                  i;
    s32                  val;

    if (strncmp(file->magic, "STF", 3) != 0) {
        return 0;
    }

    if (file->field_C > 0) {
        i               = 0;
        slot            = &D_replay_bonus_80119298;
        file->field_C  += (s32)file;
        file->field_8  += (s32)file;
        file->field_10 += (s32)file;
        file->field_14 += (s32)file;
        SCHED_BARRIER();
        table = (ReplayBonusStfTable*)file->field_10;
        TOUCH_REG_USE(table, i);
        *slot                   = (ReplayBonusStfLine*)(file->field_10 + 4);
        count                   = table->count;
        D_replay_bonus_801192A0 = count;
        if (count > 0) {
            do {
                rec                     = D_replay_bonus_80119298;
                i                      += 1;
                val                     = (s32)rec->cmds;
                D_replay_bonus_80119298 = rec + 1;
                rec->cmds               = (ReplayBonusStfCmd*)(val + (s32)file);
            } while (i < D_replay_bonus_801192A0);
        }
    }

    D_replay_bonus_80119290 = (ReplayBonusStfGlyph*)file->field_C;
    D_replay_bonus_80119294 = (ReplayBonusStfHdr*)file->field_8;
    D_replay_bonus_80119298 = (ReplayBonusStfLine*)((ReplayBonusStfTable*)file->field_10 + 1);
    D_replay_bonus_8011929C = (ReplayBonusStfSpr*)file->field_14;
    return 1;
}

void func_replay_bonus_80118C64(Task* arg0)
{
    s32                poll;
    s32                temp_v1;
    ReplayBonusStream* stream;
    Task*              t;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            if ((u32)(D_replay_bonus_80119225 - 1) < 2U) {
                Task_Kill(arg0);
                break;
            }
            stream                  = D_replay_bonus_801192BC;
            stream->fileId          = (u16)arg0->spawnArg1;
            stream->x               = 0x280;
            stream->y               = (D_replay_bonus_80119226 ^ 1) << 8;
            stream->w               = 0xF0;
            stream->h               = 0xB0;
            D_replay_bonus_80119228 = Task_SpawnFromTable(&D_replay_bonus_80118F6C, 0, 0, (s32)stream);
            D_replay_bonus_80119225 = 1;
            arg0->state            += 1;
            break;
        case 1:
            if (Task_PollKill(D_replay_bonus_80119228, &poll) != 0) {
                t                        = arg0;
                D_replay_bonus_80119227  = 0x78;
                D_replay_bonus_80119226 ^= 1;
                D_replay_bonus_80119225  = 2;
                Task_Kill(t);
            }
            break;
    }
}

void func_replay_bonus_80118D7C(Task* arg0)
{
    s32 temp_v1;
    u16 temp_v0;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            arg0->killCountdown = (u16)arg0->spawnArg1;
            arg0->state        += 1;
            break;
        case 1:
            temp_v0             = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) <= 0) {
                Task_Kill(arg0);
            }
            break;
    }
    func_replay_bonus_80117DE0(((s32)(arg0->killCountdown * 0xFF) / (s32)arg0->spawnArg1) & 0xFF);
}

void func_replay_bonus_80118E3C(Task* arg0)
{
    s32 temp_v1;
    u16 temp_v0;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            arg0->killCountdown = 0;
            arg0->state        += 1;
            break;
        case 1:
            temp_v0             = arg0->killCountdown + 1;
            arg0->killCountdown = temp_v0;
            if ((s16)temp_v0 >= arg0->spawnArg1) {
                Task_Kill(arg0);
            }
            break;
    }
    func_replay_bonus_80117DE0(((s32)(arg0->killCountdown * 0xFF) / (s32)arg0->spawnArg1) & 0xFF);
}

void func_replay_bonus_80118F00(s32 arg0)
{
    FsFolderSlot* slot;
    s32           count;
    s32           i;
    s32           type;
    s32           temp;

    count = 0;
    i     = count;
    type  = 3;
    do {
        slot = &D_8006C338[i];
        if (slot->field_0 == type) {
            if (count == arg0) {
                temp                    = slot->field_4;
                D_replay_bonus_8011928C = temp;
                func_replay_bonus_80118B6C((ReplayBonusStfFile*)temp, i);
                return;
            }
            count++;
        }
        i++;
    } while (i < 0x32);
}
