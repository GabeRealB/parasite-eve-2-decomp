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
extern u8           D_replay_bonus_801192AC;
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

s32 func_replay_bonus_80118B6C(s32 arg0, s32 index);

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

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus_2", func_replay_bonus_80117A08);

void func_replay_bonus_80117DE0(u8 arg0)
{
    char pad[0x10];

    D_replay_bonus_801192AC = 0x7F - (arg0 >> 1);
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus_2", func_replay_bonus_80117E04);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus_2", func_replay_bonus_801183B8);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus_2", func_replay_bonus_80118B6C);

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
