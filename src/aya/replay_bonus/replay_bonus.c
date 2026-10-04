#include "replay_bonus_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

void func_replay_bonus_80115ED0(Task* arg0);

static const char   D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern UiObjectDesc D_replay_bonus_801191A8;
extern s32          D_replay_bonus_80119288;

extern s32 D_replay_bonus_80119284;

static void       func_replay_bonus_801158C0(void);
void              func_replay_bonus_801159A0(Task* arg0);
static inline u16 _replayBonusUpgradeCost(s32 slot, s32 from);
static void       func_replay_bonus_80115D60(UiList* list, UiObject* ctx);
static inline s32 _replayBonusTotalBp(UiList* list, UiObject* ctx);
void              func_replay_bonus_801166AC(Task* arg0);
void              func_replay_bonus_80116964(Task* arg0);
static inline s32 _replayBonusShopTier(void);
static inline s32 _replayBonusShopItem(s32 col);
void              func_replay_bonus_80116AC0(Task* arg0);
void              func_replay_bonus_80116D68(Task* arg0);

static void func_replay_bonus_801158C0(void)
{
    RECT rect;
    s32  height;
    s32  next;
    s32  row;
    s32  xoff;

    rect.w = 0x10;
    row    = D_replay_bonus_8011926E;
    xoff   = row * 0x10;
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
    ReplayBonusPictureDecode* picture;
    s32                       state;
    s32                       width;
    s32                       bufSize;
    s32                       imgWidth;
    s32                       strip;
    s32                       next;
    u16                       w;
    u16                       x;
    u16                       h;
    u16                       y;
    void*                     vlcBuf;
    u_long*                   bs;

    state   = arg0->state;
    picture = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            D_replay_bonus_80119270 = 0;
            w                       = picture->width;
            x                       = picture->vramX;
            D_replay_bonus_80119264 = w;
            h                       = picture->height;
            D_replay_bonus_80119266 = h;
            D_replay_bonus_80119268 = x;
            y                       = picture->vramY;
            D_replay_bonus_8011926A = y;
            DecDCTReset(0);
            D_replay_bonus_8011925C = memMalloc((s16)D_replay_bonus_80119266 << 6, true);
            vlcBuf                  = memMalloc(D_replay_bonus_80119264 * (s16)D_replay_bonus_80119266 * 2, true);
            bs                      = D_8006C338[picture->resourceIndex].data;
            D_replay_bonus_80119260 = vlcBuf;
            bufSize                 = DecDCTBufSize(bs);
            width                   = D_replay_bonus_80119264;
            if (width < 0) {
                width += 0xF;
            }
            DecDCTvlcSize2((bufSize / (width >> 4)) + 2);
            if ((DecDCTvlc2(D_8006C338[picture->resourceIndex].data, D_replay_bonus_80119260, picture->vlcTable) << 0x10) == 0) {
                arg0->state = 2;
                return;
            }
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (DecDCTvlc2(NULL, NULL, picture->vlcTable) != 0) {
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
                    memFreeFromHeap(D_replay_bonus_8011925C, true);
                    memFreeFromHeap(D_replay_bonus_80119260, true);
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
    u16* table = memMalloc(STREAM_VLC_TABLE_BYTES, true);

    DecDCTvlcBuild(table);
    return table;
}

/// EXP cost of attachment `slot` one level above `from`.
static inline u16 _replayBonusUpgradeCost(s32 slot, s32 from)
{
    return Gp_IdParamHi.rows[slot * 3 + from + 1].column.expCost;
}

s32 func_replay_bonus_80115CA4(void)
{
    u8* levels;
    s32 spend;
    s32 i;
    s32 j;
    s32 val;

    levels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    spend  = gPlayerStatus.exp;
    i      = 0;
    do {
        if (*levels != 0) {
            for (j = 0; j < *levels; ++j) {
                val = _replayBonusUpgradeCost(i, j);
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
                    val = (val * 4) / 5;
                } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
                    val = (val * 2) / 5;
                }
                spend += val;
            }
        }
        i      += 1;
        levels += 1;
    } while (i < 0xC);
    return spend;
}

static void func_replay_bonus_80115D60(UiList* list, UiObject* ctx)
{
    InventoryItemRow* rec;
    s16*              ids;
    s16*              dest;
    s32               count;
    s32               i;
    s32               found;
    s32               j;
    u16*              p;
    u8                item;
    u8                id;

    rec   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    count = 0;
    ids   = ((s16*)ctx->owner->work);
    dest  = ids;
    i     = count;
    do {
        item = rec->itemId;
        if (item != 0) {
            item += 0x60;
            if ((u8)item >= 0x20U) {
                found = 1;
                id    = rec->itemId;
                p     = D_replay_bonus_8011908C;
                j     = 0;
            loop_4:
                if (*p != id) {
                    j += 1;
                    p += 1;
                    if (j >= 0x4E) {
                        found = 0;
                    } else {
                        goto loop_4;
                    }
                }
                if (found != 0) {
                    count += 1;
                    *dest  = rec->itemId;
                    dest  += 1;
                }
            }
        }
        i   += 1;
        rec += 1;
    } while (i < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows));

    i = 0x101;
    do {
        if (Gp_HasCollectedBit(i) != 0) {
            found = 1;
            p     = D_replay_bonus_8011908C;
            j     = 0;
        loop_13:
            if (*p != i) {
                j += 1;
                p += 1;
                if (j >= 0x4E) {
                    found = 0;
                } else {
                    goto loop_13;
                }
            }
            if (found != 0) {
                ids[count] = i;
                count     += 1;
            }
        }
        i += 1;
    } while (i < 0x200);

    list->visibleRowCount.unsignedValue       = 9;
    list->itemCount                           = count;
    list->firstVisibleItemIndex.unsignedValue = list->itemCount - list->visibleRowCount.unsignedValue;
    if (list->firstVisibleItemIndex.signedValue < 0) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->selectedItemIndex = list->firstVisibleItemIndex.signedValue;
}
static const char D_replay_bonus_80115774[] = "Complete Bonus";
static const char D_replay_bonus_80115784[] = "GET ITEM";
static const char D_replay_bonus_80115790[] = "BONUS BP";
static const char D_replay_bonus_8011579C[] = "TOTAL BP";

static inline s32 _replayBonusTotalBp(UiList* list, UiObject* ctx)
{
    s32           i;
    s32           sum;
    PlayerStatus* cfg;

    cfg = &gPlayerStatus;
    sum = 0;
    for (i = list->firstVisibleItemIndex.signedValue; i < list->itemCount; i++) {
        sum += replayBonusItemBp(((s16*)ctx->owner->work)[i]);
    }
    sum += cfg->bp;
    if (sum > 99999999) {
        sum = 99999999;
    }
    return sum;
}

void func_replay_bonus_80115ED0(Task* arg0)
{
    u8                 buf[0x20];
    TextDrawReq        req;
    TextDrawReq        req2;
    TextDrawReq        req3;
    UiObject*          obj;
    UiList*            list;
    PlayerStatus*      cfg;
    ReplayBonusTotals* totals;
    ShopTier*          p;
    ShopTier*          row;
    McSaveData*        save;
    void*              mem;
    s32                status;
    s32                state;
    s32                n;
    s32                sum;
    s32                idx;
    s32                result;
    u32                spend;
    s32                mask;
    s32                one;
    s32                j;
    s32                remaining;
    s32                xOff;
    s32                yOff;
    s32                ot;
    s32                ot2;
    s32                ot3;
    s32                color;
    s32                exp;
    s32                tmp;
    s32                t;
    s32                acc;
    s32                shop_i;
    s32                bonus_i;
    u8                 nxt;

    list        = &D_replay_bonus_80119130;
    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawText(&(obj)->panel, D_replay_bonus_80115774);
    if (arg0->state == 0) {
        cfg        = &gPlayerStatus;
        mem        = memMalloc(0x258, false);
        arg0->work = mem;
        if (mem == NULL) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            return;
        }
        Gp_ClearPreviewItems();
        D_80067634 = 0;
        func_replay_bonus_80115D60(list, obj);
        Ui_LayoutListPanel(list, &(obj)->panel);
        list->flags                               = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        list->topInset                            = 0xF;
        obj->panel.bounds.unsignedRect.h          = obj->panel.bounds.unsignedRect.h + 0x22;
        arg0->killCountdown                       = 0x3C;
        arg0->state                               = arg0->state + 1;
        list->firstVisibleItemIndex.unsignedValue = 0;
        acc                                       = _replayBonusTotalBp(list, obj);
        totals                                    = &D_replay_bonus_80119274;
        totals->totalBp                           = acc;
        totals->nextBp                            = acc;
        list->firstVisibleItemIndex.unsignedValue = list->itemCount - list->visibleRowCount.unsignedValue;
        tmp                                       = func_replay_bonus_80115CA4();
        exp                                       = cfg->exp;
        D_replay_bonus_80119274.totalExp          = tmp;
        totals->nextExp                           = exp;
        switch (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode) {
            case 3:
                totals->nextExp = exp * 10;
                totals->nextBp  = totals->nextBp * 10;
                break;
            case 2:
                totals->nextExp = exp * 5;
                totals->nextBp  = totals->nextBp * 5;
                break;
            case 1:
                totals->nextExp = exp * 3;
                totals->nextBp  = totals->nextBp * 3;
                break;
        }
        if (D_replay_bonus_80119274.nextBp > 0x98967F) {
            D_replay_bonus_80119274.nextBp = 0x98967F;
        }
        if (D_replay_bonus_80119274.nextExp > 0x98967F) {
            D_replay_bonus_80119274.nextExp = 0x98967F;
        }
        tmp   = func_replay_bonus_80115CA4();
        p     = D_replay_bonus_80118F78;
        spend = tmp;
        idx   = 0;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == 0x1FFF) {
            result = -1;
        } else {
            for (shop_i = 0; shop_i < 0xD; shop_i++, p++) {
                if (p->expCeiling >= spend) {
                    idx = shop_i;
                    break;
                }
            }
            save   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            idx   += save->state.gameMode;
            shop_i = 0;
            if (idx >= 0xD) {
                idx = 0xC;
            }
            one  = 1;
            mask = save->state.shopTiers;
            for (; shop_i < 0xD; shop_i++) {
                if ((mask & (one << idx)) == 0) {
                    break;
                }
                idx += 1;
                if (idx >= 0xD) {
                    idx -= 0xD;
                }
            }
            result = idx;
        }
        D_replay_bonus_80119274.shopTier = result;
        if (result < 0) {
            sum     = 0;
            row     = D_replay_bonus_80118F78;
            bonus_i = sum;
            do {
                j = 0;
                do {
                    sum += Gp_ItemDescs[row->items[j]].price;
                    j++;
                } while (j < 3);
                bonus_i++;
                row++;
            } while (bonus_i < 0xD);
            sum                    += 0x1869F;
            sum                     = sum / 100000;
            sum                    *= 0x186A0;
            D_replay_bonus_80119288 = sum;
        } else {
            D_replay_bonus_80119274.extraBonusBp = 0;
        }
    }

    status                                  = obj->panel.control.word;
    obj->panel.control.word                 = USER_INTERFACE_PANEL_INACTIVE;
    obj->panel.contentBottom.unsignedValue -= 0x13;
    Ui_UpdateListNoAnim(list, obj);
    obj->panel.control.word                 = status;
    obj->panel.contentBottom.unsignedValue += 0x13;

    state = arg0->state;
    if (state == 1) {
        remaining           = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = remaining;
        if ((remaining << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
        }
    } else if (state == 2) {
        n = list->itemCount;
        if (list->visibleRowCount.signedValue < n) {
            if (list->scrollPixelsRemaining <= 0) {
                nxt                                       = list->firstVisibleItemIndex.unsignedValue - 1;
                list->firstVisibleItemIndex.unsignedValue = nxt;
                if ((s8)nxt < 0) {
                    list->firstVisibleItemIndex.unsignedValue = 0;
                    arg0->killCountdown                       = 0xBC;
                    arg0->state                               = arg0->state + 1;
                } else {
                    SndEvt_EnqueueType6(SOUND_MENU_CURSOR, 0, 0);
                    list->scrollDirection       = USER_INTERFACE_LIST_STEP_PREVIOUS;
                    list->scrollPixelsRemaining = (s8)(u8)list->rowHeight;
                }
                list->selectedItemIndex = list->firstVisibleItemIndex.signedValue;
            }
            list->scrollPixelsRemaining = (u16)list->scrollPixelsRemaining - 1;
        } else {
            arg0->killCountdown = 0xBC;
            arg0->state         = arg0->state + 1;
        }
    } else {
        remaining           = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = remaining;
        if ((remaining << 0x10) <= 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }

    yOff = obj->panel.contentTop.signedValue + 0xC;
    xOff = obj->panel.contentLeft.signedValue + 2;
    uiDrawHorizontalSeparator(&(obj)->panel, xOff, obj->panel.contentRight.signedValue - 2, yOff);
    color = 0x606060;

    req.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req.y          = obj->panel.contentOriginY.unsignedValue - 4;
    req.y         += yOff;
    ot             = obj->panel.otIndex.signedValue;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    req.otIndex    = ot + 1;
    Text_DrawString(&req, D_replay_bonus_80115784);

    req2.x          = obj->panel.contentOriginX.unsignedValue - xOff;
    req2.y          = obj->panel.contentOriginY.unsignedValue - 4;
    req2.y         += yOff;
    ot2             = obj->panel.otIndex.signedValue;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_RIGHT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    req2.otIndex    = ot2 + 1;
    Text_DrawString(&req2, D_replay_bonus_80115790);

    t    = obj->panel.contentBottom.signedValue;
    yOff = t - 1;
    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue + 2, obj->panel.contentRight.signedValue - 2, t - 0x10);

    req3.x          = obj->panel.contentOriginX.unsignedValue + 0x70 + xOff;
    req3.y          = obj->panel.contentOriginY.unsignedValue - 6;
    req3.y         += yOff;
    ot3             = obj->panel.otIndex.signedValue;
    req3.colorRgb   = color;
    req3.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req3.alignment  = TEXT_ALIGNMENT_RIGHT;
    req3.drawMode   = TEXT_DRAW_OUTLINED;
    req3.otIndex    = ot3 + 1;
    Text_DrawString(&req3, D_replay_bonus_8011579C);

    sum = _replayBonusTotalBp(list, obj);
    Text_DrawPrompt(obj, -xOff, yOff, Text_ItoaUnsigned(buf, (u32)sum), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

static const char D_replay_bonus_801157A8[] = "Balance";
static const char D_replay_bonus_801157B0[] = "NEXT REPLAY BONUS";
static const char D_replay_bonus_801157C4[] = "EXP";

void func_replay_bonus_801166AC(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    UiObject*     obj;
    UiObject*     childObj;
    Task*         child;
    PlayerStatus* cfg;
    s32           xOff;
    s32           negX;
    s32           color;
    s32           value;
    s32           remaining;
    s32           ot;
    s32           ot2;
    s32           flag;

    cfg = &gPlayerStatus;
    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, D_replay_bonus_801157A8);
    } else {
        Ui_DrawText(&(obj)->panel, D_replay_bonus_801157B0);
    }
    color          = 0x606060;
    xOff           = obj->panel.contentLeft.signedValue + 2;
    req.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req.y          = obj->panel.contentOriginY.unsignedValue - 8;
    ot             = obj->panel.otIndex.signedValue;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    req.otIndex    = ot + 1;
    Text_DrawString(&req, D_replay_bonus_801157C4);
    value = cfg->exp;
    if (arg0->spawnArg1.value == 1) {
        value = D_replay_bonus_8011927C;
    }
    negX = -xOff;
    Text_DrawPrompt(obj, negX, -2, Text_ItoaSigned(buf, value), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    req2.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req2.y          = obj->panel.contentOriginY.unsignedValue + 0xB;
    ot2             = obj->panel.otIndex.signedValue;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_LEFT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    req2.otIndex    = ot2 + 1;
    Text_DrawString(&req2, D_replay_bonus_801157C8);
    value = D_replay_bonus_80119274.totalBp;
    if (arg0->spawnArg1.value == 1) {
        value = D_replay_bonus_80119274.nextBp;
    }
    Text_DrawPrompt(obj, negX, 0x11, Text_ItoaSigned(buf, value), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    if (arg0->state == 1) {
        remaining           = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = remaining;
        if ((remaining << 0x10) <= 0) {
            if (arg0->spawnArg1.value == 0) {
                Ui_SpawnFromDesc(&D_replay_bonus_801191A8, 1, 1, 1, obj);
                obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                arg0->state             = arg0->state + 1;
            } else {
                obj->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->result;
        if (flag == USER_INTERFACE_RESULT_CONFIRM) {
            obj->result = flag;
        }
    }
}

static const char D_replay_bonus_801157C8[] = "BP";

void func_replay_bonus_80116964(Task* arg0)
{
    UiObject* obj;
    UiObject* spawned;
    Task*     child;
    UiObject* childObj;
    s16       flag;
    u16       copied;
    s32       color;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, "WARNING");
    if (arg0->state == 0) {
        obj->resultValue = 0x34;
        Ui_SizeFromText(&(obj)->panel, D_replay_bonus_8011906C, 0, 0);
        Ui_UpdateLayoutSize(&(obj)->panel, 0, Ui_Scale15(3) + 4);
        arg0->state = arg0->state + 1;
    } else if (arg0->state == 1) {
        spawned = func_800CD89C(obj);
        if (spawned != NULL) {
            spawned->owner->spawnArg1.value      |= 0x10;
            spawned->panel.bounds.unsignedRect.y += 0x10;
            arg0->state                           = arg0->state + 1;
        }
    }
    color = 0x606060;
    Text_DrawMultiLine(obj, obj->panel.contentLeft.signedValue + 2, obj->panel.contentTop.signedValue + 0xF, D_replay_bonus_80119014, color, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        flag     = childObj->result;
        if ((flag == USER_INTERFACE_RESULT_CANCEL) || (flag == USER_INTERFACE_RESULT_CONFIRM)) {
            copied           = childObj->resultValue;
            obj->result      = USER_INTERFACE_RESULT_CONFIRM;
            obj->resultValue = copied;
        }
    }
}

/// Shop tier offered for the current spend and game mode, skipping tiers
/// already bought; -1 once every tier has been bought.
static inline s32 _replayBonusShopTier(void)
{
    ShopTier*   p;
    u32         spend;
    s32         idx;
    s32         i;
    McSaveData* save;
    s32         mask;
    s32         one;

    spend = func_replay_bonus_80115CA4();
    p     = D_replay_bonus_80118F78;
    idx   = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == 0x1FFF) {
        return -1;
    }
    for (i = 0; i < 0xD; i++, p++) {
        if (p->expCeiling >= spend) {
            idx = i;
            break;
        }
    }
    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    idx += save->state.gameMode;
    i    = 0;
    if (idx >= 0xD) {
        idx = 0xC;
    }
    one  = 1;
    mask = save->state.shopTiers;
    for (; i < 0xD; i++) {
        if ((mask & (one << idx)) == 0) {
            break;
        }
        idx += 1;
        if (idx >= 0xD) {
            idx -= 0xD;
        }
    }
    return idx;
}

/// Item in column `col` of the shop tier currently on offer, or 0 when none is.
static inline s32 _replayBonusShopItem(s32 col)
{
    s32 tier;

    tier = _replayBonusShopTier();
    if (tier < 0) {
        return 0;
    }
    return D_replay_bonus_80118F78[tier].items[col];
}

void func_replay_bonus_80116AC0(Task* arg0)
{
    s32       remaining;
    s32       dt;
    UiObject* obj;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (arg0->state == 0) {
        if (D_replay_bonus_80119284 < 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            return;
        }
        arg0->extraState.value = arg0->spawnArg1.value;
        arg0->killCountdown    = 0xBC;
        Gp_SetPreviewItem(_replayBonusShopItem(arg0->extraState.value), 0);
        arg0->spawnArg1.value = _replayBonusShopItem(arg0->extraState.value) + 0x20000;
    }
    func_800C5F70(arg0);
    dt                  = gDisplayState.frameTicks;
    remaining           = (u16)arg0->killCountdown - dt;
    arg0->killCountdown = remaining;
    if ((remaining << 0x10) <= 0) {
        obj->result         = USER_INTERFACE_RESULT_CONFIRM;
        arg0->killCountdown = 0x7FFF;
    }
}

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

    obj   = arg0->spawnArg2.pointer;
    bonus = D_replay_bonus_80119288;
    Ui_DrawText(&(obj)->panel, "EXTRA BONUS\0\0\0\0");
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    color          = 0x606060;
    xOff           = obj->panel.contentLeft.signedValue + 2;
    req.x          = obj->panel.contentOriginX.unsignedValue + xOff;
    req.y          = obj->panel.contentOriginY.unsignedValue;
    ot             = obj->panel.otIndex.signedValue;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.colorRgb   = color;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    req.otIndex    = ot + 1;
    Text_DrawString(&req, D_replay_bonus_801157C8);
    Text_DrawPrompt(obj, -xOff, 6, Text_ItoaSigned(buf, bonus), color, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    remaining           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = remaining;
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (((remaining << 0x10) <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void func_replay_bonus_80116EC0(void)
{
    McSaveData    copy;
    McSaveData*   dst;
    McSaveData*   save;
    PlayerStatus* cfg;
    u8*           p;
    s32           i;
    s32           j;
    s32           sum;
    s32           shift;
    s32           exp;

    cfg  = &gPlayerStatus;
    copy = gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    Mc_InitBufferSlots();
    dst                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    dst->state.clearCount   = copy.state.clearCount;
    dst->state.vibration    = copy.state.vibration;
    dst->state.demoScene    = copy.state.demoScene;
    dst->state.moveMode     = copy.state.moveMode;
    dst->state.buttonLayout = copy.state.buttonLayout;
    dst->state.musicVolume  = copy.state.musicVolume;
    dst->state.cursorMode   = copy.state.cursorMode;
    dst->state.soundMode    = copy.state.soundMode;
    i                       = 0;
    do {
        dst->state.itemSeenBits[i] = copy.state.itemSeenBits[i];
        i                         += 1;
    } while (i < 0x60);
    i = 0;
    do {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[i] = copy.state.attachUseCounts[i];
        i                                                          += 1;
    } while (i < 0x12);
    i = 0;
    do {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[i] = copy.state.weaponUseCounts[i];
        i                                                          += 1;
    } while (i < 0x20);

    save                   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.saveCount  = 0xFF;
    save->state.maxExp     = copy.state.maxExp;
    save->state.maxBp      = copy.state.maxBp;
    save->state.shopTiers  = copy.state.shopTiers;
    save->state.shopStock  = copy.state.shopStock;
    save->state.replayRank = copy.state.replayRank;
    save->state.clearCount++;
    if (save->state.clearCount >= 100) {
        save->state.clearCount = 99;
    }
    if (save->state.maxExp < D_replay_bonus_80119274.totalExp) {
        save->state.maxExp = D_replay_bonus_80119274.totalExp;
    }
    if (save->state.maxBp < D_replay_bonus_80119274.totalBp) {
        save->state.maxBp = D_replay_bonus_80119274.totalBp;
    }
    sum = D_replay_bonus_80119274.nextBp + D_replay_bonus_80119274.extraBonusBp;
    if (sum > 0x98967F) {
        sum = 0x98967F;
    }
    cfg->bp               = sum;
    save->state.savePoint = 0xF;
    exp                   = D_replay_bonus_80119274.nextExp;
    cfg->exp              = exp;
    save->state.playerExp = exp;
    save->state.playerBp  = cfg->bp;
    if (copy.state.gameMode >= 2) {
        save->state.replayRank = 2;
    } else if (save->state.replayRank <= 0) {
        if (D_replay_bonus_80119274.totalExp > 0x10D88) {
            save->state.replayRank = 1;
        }
    }
    p = copy.state.attachLevels;
    j = 0;
    do {
        shift = j * 2;
        if ((s32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock >> shift) & 3) < *p) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock &= ~(3 << shift);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock |= *p << shift;
        }
        j += 1;
        p += 1;
    } while (j < 0xC);
    if (D_replay_bonus_80119284 >= 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers |= 1 << D_replay_bonus_80119284;
    }
}
