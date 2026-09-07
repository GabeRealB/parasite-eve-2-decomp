#include "common.h"

#include "aya/replay_bonus.h"
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
extern u8           D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern s32          D_replay_bonus_80119288;
extern s32          D_replay_bonus_8011928C;
extern u8           D_replay_bonus_801192AC;
extern GpItemDesc   D_8010DE38[];
extern s32          D_80072A9C;

s32  func_replay_bonus_80118B6C(s32 arg0, s32 index);
void func_replay_bonus_80117194(Task* arg0);

void func_replay_bonus_801158C0(void)
{
    RECT rect;
    s32  height;
    s32  next;
    s32  row;
    s32  xoff;

    rect.w = 0x10;
    row    = D_replay_bonus_8011926E;
    xoff   = row * 0x10;
    SOFT_TOUCH_REG(row);
    rect.x = D_replay_bonus_80119268 + xoff;
    rect.h = D_replay_bonus_80119266;
    rect.y = D_replay_bonus_8011926A;
    LoadImage(&rect, (u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << 5) * (s16)D_replay_bonus_80119266)));
    height                  = D_replay_bonus_80119264;
    D_replay_bonus_8011926C = 0;
    D_replay_bonus_80119270 = D_replay_bonus_80119270 ^ 1;
    next                    = (D_replay_bonus_8011926E = (u16)D_replay_bonus_8011926E + 1);
    next                    = (s16)next;
    if (height < 0) {
        height += 0xF;
    }
    if (next == (height >> 4) - 1) {
        DecDCToutCallback(NULL);
    }
}

void func_replay_bonus_801159A0(Task* arg0)
{
    ReplayBonusStream* stream;
    s32                state;
    s32                width;
    s32                bufSize;
    s32                imgWidth;
    s32                strip;
    s32                next;
    u16                w;
    u16                x;
    u16                h;
    u16                y;
    void*              vlcBuf;
    u_long*            bs;

    state  = arg0->state;
    stream = arg0->spawnArg2;
    switch (state) {
        case 0:
            D_replay_bonus_80119270 = 0;
            w                       = stream->w;
            x                       = stream->x;
            D_replay_bonus_80119264 = w;
            h                       = stream->h;
            D_replay_bonus_80119266 = h;
            D_replay_bonus_80119268 = x;
            y                       = stream->y;
            D_replay_bonus_8011926A = y;
            DecDCTReset(0);
            D_replay_bonus_8011925C = Mem_Malloc((s16)D_replay_bonus_80119266 << 6, 1);
            vlcBuf                  = Mem_Malloc(D_replay_bonus_80119264 * (s16)D_replay_bonus_80119266 * 2, 1);
            bs                      = (u_long*)D_8006C338[stream->fileId].field_4;
            D_replay_bonus_80119260 = vlcBuf;
            bufSize                 = DecDCTBufSize(bs);
            width                   = D_replay_bonus_80119264;
            if (width < 0) {
                width += 0xF;
            }
            DecDCTvlcSize2((bufSize / (width >> 4)) + 2);
            if ((DecDCTvlc2((u_long*)D_8006C338[stream->fileId].field_4, D_replay_bonus_80119260, stream->table) << 0x10) == 0) {
                arg0->state = 2;
                return;
            }
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (DecDCTvlc2(NULL, NULL, stream->table) != 0) {
                return;
            }
            arg0->state = arg0->state + 1;
        case 2:
            DecDCToutCallback(func_replay_bonus_801158C0);
            DecDCTin(D_replay_bonus_80119260, 2);
            DecDCTout((u_long*)D_replay_bonus_8011925C, (s16)D_replay_bonus_80119266 * 8);
            D_replay_bonus_8011926E = 0;
            next                    = arg0->state;
            D_replay_bonus_8011926C = 1;
            arg0->state             = next + 1;
            return;
        case 3:
            if (D_replay_bonus_8011926C == 0) {
                imgWidth = D_replay_bonus_80119264;
                strip    = D_replay_bonus_8011926E;
                if (imgWidth < 0) {
                    imgWidth += 0xF;
                }
                if (strip == (imgWidth >> 4) - 1) {
                    Mem_Free2(D_replay_bonus_8011925C, 1);
                    Mem_Free2(D_replay_bonus_80119260, 1);
                    Task_RequestKill(arg0, 0);
                    return;
                }
                DecDCTout((u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << 5) * (s16)D_replay_bonus_80119266)), (s16)D_replay_bonus_80119266 * 8);
                D_replay_bonus_8011926C = 1;
            }
            break;
    }
}

u16* func_replay_bonus_80115C68(void)
{
    u16* table = Mem_Malloc(0x11000, 1);

    DecDCTvlcBuild(table);
    return table;
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115CA4);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115D60);

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_80115770);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115ED0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801166AC);

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_801157C8);

void func_replay_bonus_80116964(Task* arg0)
{
    UiObject* obj;
    UiObject* spawned;
    Task*     child;
    UiObject* childObj;
    s16       flag;
    u16       copied;
    s32       color;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawTitle((UiPanel*)obj, "WARNING");
    if (arg0->state == 0) {
        obj->field_2C = 0x34;
        Ui_SizeFromText((UiPanel*)obj, D_replay_bonus_8011906C, 0, 0);
        SOFT_BARRIER();
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(3) + 4);
        arg0->state = arg0->state + 1;
    } else if (arg0->state == 1) {
        spawned = func_800CD89C(obj);
        if (spawned != NULL) {
            spawned->owner->spawnArg1 |= 0x10;
            spawned->field_E          += 0x10;
            arg0->state                = arg0->state + 1;
        }
    }
    color = 0x606060;
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, D_replay_bonus_80119014, color, 1, 0);
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        flag     = childObj->field_2E;
        if ((flag == -1) || (flag == 6)) {
            copied        = childObj->field_2C;
            obj->field_2E = 6;
            obj->field_2C = copied;
        }
    }
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116AC0);

void func_replay_bonus_80116D68(Task* arg0)
{
    u8          buf[0x20];
    TextDrawReq req;
    UiObject*   obj;
    s32         xOff;
    s32         bonus;
    s32         color;
    s32         remaining;
    s32         ot;

    obj   = arg0->spawnArg2;
    bonus = D_replay_bonus_80119288;
    Ui_DrawText((UiPanel*)obj, "EXTRA BONUS\0\0\0\0");
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    color          = 0x606060;
    xOff           = obj->field_1C + 2;
    req.x          = obj->baseX + xOff;
    req.y          = obj->baseY;
    ot             = (s16)obj->drawOrder;
    req.glyphTable = 5;
    req.field_8    = color;
    req.centerMode = 0;
    req.field_E    = 1;
    req.otIndex    = ot + 1;
    func_8002E53C(&req, D_replay_bonus_801157C8);
    Text_DrawPrompt(obj, -xOff, 6, Text_ItoaSigned(buf, bonus), color, 3, 2);
    remaining           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = remaining;
    if (obj->status == 1) {
        if (((remaining << 0x10) <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            obj->field_2E = 6;
        }
    }
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116EC0);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117194);

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

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117A08);

void func_replay_bonus_80117DE0(u8 arg0)
{
    char pad[0x10];

    D_replay_bonus_801192AC = 0x7F - (arg0 >> 1);
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80117E04);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_801183B8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80118B6C);

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
                func_replay_bonus_80118B6C(temp, i);
                return;
            }
            count++;
        }
        i++;
    } while (i < 0x32);
}
