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

/// Hold duration in nominal 60-Hz ticks and the replay panels' text colour.
enum {
    REPLAY_BONUS_PANEL_HOLD_TICKS = 188,
    REPLAY_BONUS_PANEL_TEXT_COLOR = 0x606060,
};

static void       _replayBonusUploadPictureStrip(void);
static inline s32 _replayBonusUpgradeCost(s32 abilitySlot, s32 previousLevel);
static void       _replayBonusBuildItemList(UiList* list, UiObject* object);
static inline s32 _replayBonusTotalBp(UiList* list, UiObject* ctx);
static inline s32 _replayBonusShopTier(void);
static inline s32 _replayBonusShopItem(s32 itemColumn);

/// Uploads the completed 16-bit credits-picture strip and advances its buffer.
static void _replayBonusUploadPictureStrip(void)
{
    enum { REPLAY_BONUS_STRIP_PIXEL_BYTES_SHIFT = 5,
           REPLAY_BONUS_STRIP_WIDTH_SHIFT       = 4 };
    RECT rect;
    s32  imageWidth;
    s32  nextStrip;
    s32  stripIndex;
    s32  stripXOffset;

    rect.w       = FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    stripIndex   = D_replay_bonus_8011926E;
    stripXOffset = stripIndex * FILE_SYSTEM_IMAGE_STRIP_WIDTH;
    rect.x       = D_replay_bonus_80119268 + stripXOffset;
    rect.h       = D_replay_bonus_80119266;
    rect.y       = D_replay_bonus_8011926A;
    LoadImage(&rect, (u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << REPLAY_BONUS_STRIP_PIXEL_BYTES_SHIFT) * (s16)D_replay_bonus_80119266)));
    // The callback releases the strip before the task queues the other buffer.
    imageWidth              = D_replay_bonus_80119264;
    D_replay_bonus_8011926C = 0;
    D_replay_bonus_80119270 = D_replay_bonus_80119270 ^ 1;
    nextStrip               = (D_replay_bonus_8011926E = (u16)D_replay_bonus_8011926E + 1);
    nextStrip               = (s16)nextStrip;
    if (imageWidth < 0) {
        imageWidth += FILE_SYSTEM_IMAGE_STRIP_WIDTH - 1;
    }
    if (nextStrip == (imageWidth >> REPLAY_BONUS_STRIP_WIDTH_SHIFT) - 1) {
        DecDCToutCallback(NULL);
    }
}

/// Feeds the expanded credits picture to MDEC and requests its first strip.
static inline void _replayBonusStartPictureDecode(Task* task)
{
    enum { REPLAY_BONUS_STRIP_WORDS_PER_ROW = FILE_SYSTEM_IMAGE_STRIP_WIDTH / 2 };
    s32 nextState;

    DecDCToutCallback(_replayBonusUploadPictureStrip);
    DecDCTin(D_replay_bonus_80119260, MDEC_IMAGE_MODE_RGB16_MASK_BIT);
    DecDCTout((u_long*)D_replay_bonus_8011925C, (s16)D_replay_bonus_80119266 * REPLAY_BONUS_STRIP_WORDS_PER_ROW);
    D_replay_bonus_8011926E = 0;
    nextState               = task->state;
    D_replay_bonus_8011926C = 1;
    task->state             = nextState + 1;
}

void replayBonusDecodePictureTask(Task* task)
{
    enum {
        REPLAY_BONUS_DECODE_INITIALIZE        = 0,
        REPLAY_BONUS_DECODE_VLC               = 1,
        REPLAY_BONUS_DECODE_START_MDEC        = 2,
        REPLAY_BONUS_DECODE_STRIPS            = 3,
        REPLAY_BONUS_STRIP_WIDTH_SHIFT        = 4,
        REPLAY_BONUS_STRIP_PIXEL_BYTES_SHIFT  = 5,
        REPLAY_BONUS_DOUBLE_STRIP_BYTES_SHIFT = 6,
        REPLAY_BONUS_STRIP_WORDS_PER_ROW      = FILE_SYSTEM_IMAGE_STRIP_WIDTH / 2,
        REPLAY_BONUS_PICTURE_PIXEL_BYTES      = sizeof(u16),
    };
    ReplayBonusPictureDecode* picture;
    s32                       decodeState;
    s32                       vlcImageWidth;
    s32                       decodedWordCount;
    s32                       outputImageWidth;
    s32                       completedStrips;
    u16                       pictureWidth;
    u16                       vramX;
    u16                       pictureHeight;
    u16                       vramY;
    u_long*                   decodedBitstream;
    u_long*                   bitstream;

    decodeState = task->state;
    picture     = task->spawnArg2.pointer;
    switch (decodeState) {
        case REPLAY_BONUS_DECODE_INITIALIZE:
            // Expand VLC in slices, then feed one picture to MDEC.
            D_replay_bonus_80119270 = 0;
            pictureWidth            = picture->width;
            vramX                   = picture->vramX;
            D_replay_bonus_80119264 = pictureWidth;
            pictureHeight           = picture->height;
            D_replay_bonus_80119266 = pictureHeight;
            D_replay_bonus_80119268 = vramX;
            vramY                   = picture->vramY;
            D_replay_bonus_8011926A = vramY;
            DecDCTReset(0);
            D_replay_bonus_8011925C = memMalloc((s16)D_replay_bonus_80119266 << REPLAY_BONUS_DOUBLE_STRIP_BYTES_SHIFT, true);
            decodedBitstream        = memMalloc(D_replay_bonus_80119264 * (s16)D_replay_bonus_80119266 * REPLAY_BONUS_PICTURE_PIXEL_BYTES, true);
            bitstream               = D_8006C338[picture->resourceIndex].data;
            D_replay_bonus_80119260 = decodedBitstream;
            decodedWordCount        = DecDCTBufSize(bitstream);
            vlcImageWidth           = D_replay_bonus_80119264;
            if (vlcImageWidth < 0) {
                vlcImageWidth += FILE_SYSTEM_IMAGE_STRIP_WIDTH - 1;
            }
            DecDCTvlcSize2((decodedWordCount / (vlcImageWidth >> REPLAY_BONUS_STRIP_WIDTH_SHIFT)) + 2);
            if ((DecDCTvlc2(D_8006C338[picture->resourceIndex].data, D_replay_bonus_80119260, picture->vlcTable) << 0x10) == 0) {
                task->state = REPLAY_BONUS_DECODE_START_MDEC;
                return;
            }
            task->state = task->state + 1;
            return;
        case REPLAY_BONUS_DECODE_VLC:
            if (DecDCTvlc2(NULL, NULL, picture->vlcTable) != 0) {
                return;
            }
            task->state = task->state + 1;
        case REPLAY_BONUS_DECODE_START_MDEC:
            _replayBonusStartPictureDecode(task);
            return;
        case REPLAY_BONUS_DECODE_STRIPS:
            // Output sizes count 32-bit words; buffer offsets count bytes.
            if (D_replay_bonus_8011926C == 0) {
                outputImageWidth = D_replay_bonus_80119264;
                completedStrips  = D_replay_bonus_8011926E;
                if (outputImageWidth < 0) {
                    outputImageWidth += FILE_SYSTEM_IMAGE_STRIP_WIDTH - 1;
                }
                // The retained stop count leaves the final 16-pixel column untouched.
                if (completedStrips == (outputImageWidth >> REPLAY_BONUS_STRIP_WIDTH_SHIFT) - 1) {
                    memFreeFromHeap(D_replay_bonus_8011925C, true);
                    memFreeFromHeap(D_replay_bonus_80119260, true);
                    taskRequestKill(task, 0);
                    return;
                }
                DecDCTout((u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << REPLAY_BONUS_STRIP_PIXEL_BYTES_SHIFT) * (s16)D_replay_bonus_80119266)), (s16)D_replay_bonus_80119266 * REPLAY_BONUS_STRIP_WORDS_PER_ROW);
                D_replay_bonus_8011926C = 1;
            }
            break;
    }
}

u16* replayBonusCreatePictureVlcTable(void)
{
    u16* table = memMalloc(sizeof(DECDCTTAB), true);

    DecDCTvlcBuild(table);
    return table;
}

/// Discounted EXP cost of learning the next level of a wheel ability.
///
/// `abilitySlot` is 0..11 and `previousLevel` is 0..2. Positive game modes
/// pay 4/5 of the base cost; a cleared normal game pays 2/5. Each level
/// rounds down separately, using the completed run's mode and clear count.
static inline s32 _replayBonusUpgradeCost(s32 abilitySlot, s32 previousLevel)
{
    s32 expCost = Gp_IdParamHi.rows[abilitySlot * ATTACHMENT_AREA_LEVEL_COUNT + previousLevel + 1].column.expCost;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode > 0) {
        expCost = (expCost * 4) / 5;
    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
        expCost = (expCost * 2) / 5;
    }
    return expCost;
}

s32 replayBonusGetTotalExp(void)
{
    const u8* learnedLevels;
    s32       totalExp;
    s32       abilitySlot;
    s32       previousLevel;
    s32       upgradeExpCost;

    learnedLevels = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    totalExp      = gPlayerStatus.exp;
    abilitySlot   = 0;
    do {
        if (*learnedLevels != 0) {
            for (previousLevel = 0; previousLevel < *learnedLevels; ++previousLevel) {
                upgradeExpCost = _replayBonusUpgradeCost(abilitySlot, previousLevel);
                totalExp      += upgradeExpCost;
            }
        }
        abilitySlot   += 1;
        learnedLevels += 1;
    } while (abilitySlot < ATTACHMENT_SPELL_COUNT);
    return totalExp;
}

/// Fills the Complete Bonus list with non-consumable rows and collected ids.
///
/// The UI owner supplies writable `s16` item-id storage. The saved table's
/// 256 rows and the whitelist's eight collectible ids emit at most 264 ids,
/// fitting its 300-element allocation. Navigation still requires at most 127
/// emitted rows for the signed-byte UI indices; legal-run limits are unproven.
/// Starts the list on its last nine rows, clamping an empty or short list to 0.
static void _replayBonusBuildItemList(UiList* list, UiObject* object)
{
    enum {
        REPLAY_BONUS_ITEM_WHITELIST_COUNT = 78,
        REPLAY_BONUS_COLLECTION_ID_LIMIT  = 0x200,
        REPLAY_BONUS_LIST_VISIBLE_ROWS    = 9,
    };
    /// Tests the fixed whitelist and writes a 0/1 membership result.
    ///
    /// Arguments are side-effect-free scalar/lvalue identifiers, evaluated
    /// repeatedly; result, cursor and index must be distinct. `retryLabel` must
    /// be unique in the function. The scan excludes the terminator and keeps
    /// the two scan labels separate. Requires this function's whitelist-count enum.
#define REPLAY_BONUS_FIND_ITEM_IN_WHITELIST(itemId, result, cursor, index, retryLabel) \
    {                                                                                  \
        (result) = 1;                                                                  \
        (cursor) = D_replay_bonus_8011908C;                                            \
        (index)  = 0;                                                                  \
    retryLabel:                                                                        \
        if (*(cursor) != (itemId)) {                                                   \
            (index)  += 1;                                                             \
            (cursor) += 1;                                                             \
            if ((index) >= REPLAY_BONUS_ITEM_WHITELIST_COUNT) {                        \
                (result) = 0;                                                          \
            } else {                                                                   \
                goto retryLabel;                                                       \
            }                                                                          \
        }                                                                              \
    }

    const InventoryItemRow* savedRow;
    s16*                    itemIds;
    s16*                    nextItemId;
    s32                     itemCount;
    s32                     itemIndex;
    s32                     listed;
    s32                     whitelistIndex;
    const u16*              whitelistItem;
    u8                      consumableIndex;
    u8                      itemId;

    // Each eligible saved row contributes once, independent of quantity.
    savedRow   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
    itemCount  = 0;
    itemIds    = object->owner->work;
    nextItemId = itemIds;
    itemIndex  = itemCount;
    do {
        consumableIndex = savedRow->itemId;
        if (consumableIndex != INVENTORY_ITEM_NONE) {
            consumableIndex -= INVENTORY_CONSUMABLE_ITEM_FIRST;
            if ((u8)consumableIndex >= (u32)INVENTORY_CONSUMABLE_ITEM_COUNT) {
                itemId = savedRow->itemId;
                REPLAY_BONUS_FIND_ITEM_IN_WHITELIST(itemId, listed, whitelistItem, whitelistIndex, scanSavedItem);
                if (listed != 0) {
                    itemCount  += 1;
                    *nextItemId = savedRow->itemId;
                    nextItemId += 1;
                }
            }
        }
        itemIndex += 1;
        savedRow  += 1;
    } while (itemIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows));

    // Collection ids contribute once per whitelisted id, after saved rows.
    itemIndex = INVENTORY_COLLECTION_ID_PARTHENON_KEY;
    do {
        if (inventoryHasCollectedBit(itemIndex) != 0) {
            REPLAY_BONUS_FIND_ITEM_IN_WHITELIST(itemIndex, listed, whitelistItem, whitelistIndex, scanCollectedItem);
            if (listed != 0) {
                itemIds[itemCount] = itemIndex;
                itemCount         += 1;
            }
        }
        itemIndex += 1;
    } while (itemIndex < REPLAY_BONUS_COLLECTION_ID_LIMIT);

    list->visibleRowCount.unsignedValue       = REPLAY_BONUS_LIST_VISIBLE_ROWS;
    list->itemCount                           = itemCount;
    list->firstVisibleItemIndex.unsignedValue = list->itemCount - list->visibleRowCount.unsignedValue;
    if (list->firstVisibleItemIndex.signedValue < 0) {
        list->firstVisibleItemIndex.unsignedValue = 0;
    }
    list->selectedItemIndex = list->firstVisibleItemIndex.signedValue;
#undef REPLAY_BONUS_FIND_ITEM_IN_WHITELIST
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
    uiDrawPanelLabel(&(obj)->panel, D_replay_bonus_80115774);
    if (arg0->state == 0) {
        cfg        = &gPlayerStatus;
        mem        = memMalloc(0x258, false);
        arg0->work = mem;
        if (mem == NULL) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
            return;
        }
        itemMenuClearPreviewItems();
        D_80067634 = 0;
        _replayBonusBuildItemList(list, obj);
        uiFitPanelToList(list, &(obj)->panel);
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
        tmp                                       = replayBonusGetTotalExp();
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
        tmp   = replayBonusGetTotalExp();
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
            sum                                 += 0x1869F;
            sum                                  = sum / 100000;
            sum                                 *= 0x186A0;
            D_replay_bonus_80119274.extraBonusBp = sum;
        } else {
            D_replay_bonus_80119274.extraBonusBp = 0;
        }
    }

    status                                  = obj->panel.control.word;
    obj->panel.control.word                 = USER_INTERFACE_PANEL_INACTIVE;
    obj->panel.contentBottom.unsignedValue -= 0x13;
    uiUpdateList(list, &obj->panel);
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
                    sndEvtRequestScriptStart(SOUND_MENU_CURSOR, 0, 0);
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
    textDrawString(&req, D_replay_bonus_80115784);

    req2.x          = obj->panel.contentOriginX.unsignedValue - xOff;
    req2.y          = obj->panel.contentOriginY.unsignedValue - 4;
    req2.y         += yOff;
    ot2             = obj->panel.otIndex.signedValue;
    req2.colorRgb   = color;
    req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req2.alignment  = TEXT_ALIGNMENT_RIGHT;
    req2.drawMode   = TEXT_DRAW_OUTLINED;
    req2.otIndex    = ot2 + 1;
    textDrawString(&req2, D_replay_bonus_80115790);

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
    textDrawString(&req3, D_replay_bonus_8011579C);

    sum = _replayBonusTotalBp(list, obj);
    textDrawUiLine(obj, -xOff, yOff, textItoaUnsigned(buf, (u32)sum), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    if ((obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

static const char D_replay_bonus_801157A8[] = "Balance";
static const char D_replay_bonus_801157B0[] = "NEXT REPLAY BONUS";
static const char D_replay_bonus_801157C4[] = "EXP";

void replayBonusBalancePanelTask(Task* task)
{
    enum {
        REPLAY_BONUS_BALANCE_CURRENT    = 0,
        REPLAY_BONUS_BALANCE_NEXT       = 1,
        REPLAY_BONUS_BALANCE_INITIALIZE = 0,
        REPLAY_BONUS_BALANCE_HOLD       = 1,
    };

    /// Builds and draws a small outlined EXP/BP label using the caller's request.
    ///
    /// Arguments are side-effect-free lvalues/scalars, evaluated repeatedly.
    /// The request, object and OT index must refer to distinct objects. Pixel
    /// coordinates are relative to the panel origin; preserves request order.
#define REPLAY_BONUS_DRAW_BALANCE_LABEL(request, object, xOffset, yOffset, color, index, caption) \
    {                                                                                             \
        (request).x          = (object)->panel.contentOriginX.unsignedValue + (xOffset);          \
        (request).y          = (object)->panel.contentOriginY.unsignedValue + (yOffset);          \
        (index)              = (object)->panel.otIndex.signedValue;                               \
        (request).colorRgb   = (color);                                                           \
        (request).glyphTable = TEXT_GLYPH_TABLE_SMALL;                                            \
        (request).alignment  = TEXT_ALIGNMENT_LEFT;                                               \
        (request).drawMode   = TEXT_DRAW_OUTLINED;                                                \
        (request).otIndex    = (index) + 1;                                                       \
        textDrawString(&(request), (caption));                                                    \
    }

    u8                  numberText[0x20];
    TextDrawReq         expLabel;
    TextDrawReq         bpLabel;
    UiObject*           object;
    UiObject*           childObject;
    Task*               childTask;
    const PlayerStatus* playerStatus;
    s32                 leftInset;
    s32                 rightInset;
    s32                 textColor;
    s32                 balance;
    s32                 holdTicksLeft;
    s32                 expOtIndex;
    s32                 bpOtIndex;
    s32                 childResult;

    playerStatus = &gPlayerStatus;
    object       = task->spawnArg2.pointer;
    if (task->state == REPLAY_BONUS_BALANCE_INITIALIZE) {
        task->killCountdown = REPLAY_BONUS_PANEL_HOLD_TICKS;
        task->state         = task->state + 1;
    }
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->spawnArg1.value == REPLAY_BONUS_BALANCE_CURRENT) {
        uiDrawPanelLabel(&(object)->panel, D_replay_bonus_801157A8);
    } else {
        uiDrawPanelLabel(&(object)->panel, D_replay_bonus_801157B0);
    }
    textColor = REPLAY_BONUS_PANEL_TEXT_COLOR;
    leftInset = object->panel.contentLeft.signedValue + 2;
    REPLAY_BONUS_DRAW_BALANCE_LABEL(expLabel, object, leftInset, -8, textColor, expOtIndex, D_replay_bonus_801157C4);
    balance = playerStatus->exp;
    if (task->spawnArg1.value == REPLAY_BONUS_BALANCE_NEXT) {
        balance = D_replay_bonus_80119274.nextExp;
    }
    rightInset = -leftInset;
    textDrawUiLine(object, rightInset, -2, textItoaSigned(numberText, balance), textColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    REPLAY_BONUS_DRAW_BALANCE_LABEL(bpLabel, object, leftInset, 0xB, textColor, bpOtIndex, D_replay_bonus_801157C8);
    balance = D_replay_bonus_80119274.totalBp;
    if (task->spawnArg1.value == REPLAY_BONUS_BALANCE_NEXT) {
        balance = D_replay_bonus_80119274.nextBp;
    }
    textDrawUiLine(object, rightInset, 0x11, textItoaSigned(numberText, balance), textColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    // The current-balance hold opens the next-balance panel as a child.
    if (task->state == REPLAY_BONUS_BALANCE_HOLD) {
        holdTicksLeft       = (u16)task->killCountdown - 1;
        task->killCountdown = holdTicksLeft;
        if ((holdTicksLeft << 0x10) <= 0) {
            if (task->spawnArg1.value == REPLAY_BONUS_BALANCE_CURRENT) {
                uiSpawnObject(&D_replay_bonus_801191A8, REPLAY_BONUS_BALANCE_NEXT, 1, 1, object);
                object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                task->state                = task->state + 1;
            } else {
                object->result = USER_INTERFACE_RESULT_CONFIRM;
            }
        }
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if (childResult == USER_INTERFACE_RESULT_CONFIRM) {
            object->result = childResult;
        }
    }
#undef REPLAY_BONUS_DRAW_BALANCE_LABEL
}

static const char D_replay_bonus_801157C8[] = "BP";

void replayBonusQuitWarningTask(Task* task)
{
    enum {
        REPLAY_BONUS_QUIT_SIZE_PANEL = 0,
        REPLAY_BONUS_QUIT_OPEN_MENU  = 1,
    };
    UiObject* object;
    UiObject* yesNoMenu;
    Task*     childTask;
    UiObject* childObject;
    s16       childResult;
    u16       selectedCommand;
    s32       textColor;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(object)->panel, "WARNING");
    if (task->state == REPLAY_BONUS_QUIT_SIZE_PANEL) {
        object->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
        uiSizePanelForText(&(object)->panel, D_replay_bonus_8011906C, 0, 0);
        uiSetPanelContentSize(&(object)->panel, 0, uiGetTextRowsHeight(3) + 4);
        task->state = task->state + 1;
    } else if (task->state == REPLAY_BONUS_QUIT_OPEN_MENU) {
        yesNoMenu = itemMenuSpawnYesNoMenuDefaultNo(object);
        if (yesNoMenu != NULL) {
            yesNoMenu->owner->spawnArg1.value      |= ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND;
            yesNoMenu->panel.bounds.unsignedRect.y += 0x10;
            task->state                             = task->state + 1;
        }
    }
    textColor = REPLAY_BONUS_PANEL_TEXT_COLOR;
    textDrawUiLines(object, object->panel.contentLeft.signedValue + 2, object->panel.contentTop.signedValue + 0xF, D_replay_bonus_80119014, textColor, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    // The controller needs the Yes/No command even when the child cancels.
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        if ((childResult == USER_INTERFACE_RESULT_CANCEL) || (childResult == USER_INTERFACE_RESULT_CONFIRM)) {
            selectedCommand     = childObject->resultValue;
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = selectedCommand;
        }
    }
}

/// Shop tier unlocked by this clear's total EXP and game mode.
///
/// Begins at the first inclusive EXP ceiling, adds the mode (0..3) and caps
/// at the top tier, then wraps past unlocked tiers. Returns -1 if all thirteen
/// tiers are unlocked. Reads the completed run without modifying its unlocks.
static inline s32 _replayBonusShopTier(void)
{
    const ShopTier*   tierRow;
    u32               totalExp;
    s32               tierIndex;
    s32               tiersChecked;
    const McSaveData* saveData;
    s32               unlockedTiers;
    s32               tierBit;

    totalExp  = replayBonusGetTotalExp();
    tierRow   = D_replay_bonus_80118F78;
    tierIndex = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == SHOP_TIER_ALL_MASK) {
        return -1;
    }
    // Start at the EXP ceiling, then apply the mode boost.
    for (tiersChecked = 0; tiersChecked < SHOP_TIER_COUNT; tiersChecked++, tierRow++) {
        if (tierRow->expCeiling >= totalExp) {
            tierIndex = tiersChecked;
            break;
        }
    }
    saveData     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    tierIndex   += saveData->state.gameMode;
    tiersChecked = 0;
    if (tierIndex >= SHOP_TIER_COUNT) {
        tierIndex = SHOP_TIER_COUNT - 1;
    }
    tierBit       = 1;
    unlockedTiers = saveData->state.shopTiers;
    // Search cyclically for the first tier not already unlocked.
    for (; tiersChecked < SHOP_TIER_COUNT; tiersChecked++) {
        if ((unlockedTiers & (tierBit << tierIndex)) == 0) {
            break;
        }
        tierIndex += 1;
        if (tierIndex >= SHOP_TIER_COUNT) {
            tierIndex -= SHOP_TIER_COUNT;
        }
    }
    return tierIndex;
}

/// Item id in column `itemColumn` (0..2) of the tier unlocked by this clear.
///
/// Returns `INVENTORY_ITEM_NONE` when every tier is already unlocked.
static inline s32 _replayBonusShopItem(s32 itemColumn)
{
    s32 tierIndex;

    tierIndex = _replayBonusShopTier();
    if (tierIndex < 0) {
        return INVENTORY_ITEM_NONE;
    }
    return D_replay_bonus_80118F78[tierIndex].items[itemColumn];
}

void replayBonusUnlockedItemPanelTask(Task* task)
{
    enum { REPLAY_BONUS_ITEM_CONFIRMED_HOLD_TICKS = 0x7FFF };
    s32       holdTicksLeft;
    s32       frameTicks;
    UiObject* object;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        if (D_replay_bonus_80119274.shopTier < 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
            return;
        }
        // The information task takes over the state and first spawn argument.
        task->extraState.value = task->spawnArg1.value;
        task->killCountdown    = REPLAY_BONUS_PANEL_HOLD_TICKS;
        itemMenuSetPreviewItem(_replayBonusShopItem(task->extraState.value), CD_COMMAND_DISPLAY_LOAD_MENU);
        task->spawnArg1.value = _replayBonusShopItem(task->extraState.value) + ITEM_MENU_INFO_NEXT_REPLAY;
    }
    itemMenuInfoTask(task);
    frameTicks          = gDisplayState.frameTicks;
    holdTicksLeft       = (u16)task->killCountdown - frameTicks;
    task->killCountdown = holdTicksLeft;
    if ((holdTicksLeft << 0x10) <= 0) {
        object->result      = USER_INTERFACE_RESULT_CONFIRM;
        task->killCountdown = REPLAY_BONUS_ITEM_CONFIRMED_HOLD_TICKS;
    }
}

void replayBonusExtraBpPanelTask(Task* task)
{
    u8          numberText[0x20];
    TextDrawReq bpLabel;
    UiObject*   object;
    s32         leftInset;
    s32         extraBonusBp;
    s32         textColor;
    s32         holdTicksLeft;
    s32         otIndex;

    object       = task->spawnArg2.pointer;
    extraBonusBp = D_replay_bonus_80119274.extraBonusBp;
    uiDrawPanelLabel(&(object)->panel, "EXTRA BONUS\0\0\0\0");
    if (task->state == 0) {
        task->killCountdown = REPLAY_BONUS_PANEL_HOLD_TICKS;
        task->state         = task->state + 1;
    }
    textColor          = REPLAY_BONUS_PANEL_TEXT_COLOR;
    leftInset          = object->panel.contentLeft.signedValue + 2;
    bpLabel.x          = object->panel.contentOriginX.unsignedValue + leftInset;
    bpLabel.y          = object->panel.contentOriginY.unsignedValue;
    otIndex            = object->panel.otIndex.signedValue;
    bpLabel.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    bpLabel.colorRgb   = textColor;
    bpLabel.alignment  = TEXT_ALIGNMENT_LEFT;
    bpLabel.drawMode   = TEXT_DRAW_OUTLINED;
    bpLabel.otIndex    = otIndex + 1;
    textDrawString(&bpLabel, D_replay_bonus_801157C8);
    textDrawUiLine(object, -leftInset, 6, textItoaSigned(numberText, extraBonusBp), textColor, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    holdTicksLeft       = (u16)task->killCountdown - 1;
    task->killCountdown = holdTicksLeft;
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (((holdTicksLeft << 0x10) <= 0) || (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void replayBonusPrepareClearedSave(void)
{
    enum {
        MEMORY_CARD_SAVE_COUNT_NEW             = 0xFF,
        MEMORY_CARD_SAVE_POINT_OPENING         = 15,
        REPLAY_BONUS_MAX_CLEAR_COUNT           = 99,
        REPLAY_BONUS_MAX_STARTING_BALANCE      = 9999999,
        REPLAY_BONUS_FIRST_RANK_EXP_CEILING    = 69000,
        REPLAY_BONUS_HIGH_RANK_MIN_GAME_MODE   = 2,
        REPLAY_BONUS_RANK_FIRST                = 1,
        REPLAY_BONUS_RANK_HIGH                 = 2,
        REPLAY_BONUS_CARRIED_ABILITY_USE_COUNT = 18,
        REPLAY_BONUS_SHOP_STOCK_LEVEL_BITS     = 2,
        REPLAY_BONUS_SHOP_STOCK_LEVEL_MASK     = 3,
    };
    /// Retains the highest learned level in one packed two-bit shop slot.
    ///
    /// `slot` is 0..11 and `level` is 0..3; both must be side-effect-free.
    /// `shift` is a separate writable s32 lvalue; `level` must not alias stock.
    /// Arguments are evaluated
    /// repeatedly. The live save owns the stock, which is updated in place.
#define REPLAY_BONUS_RETAIN_SHOP_ABILITY_LEVEL(slot, level, shift)                                                                   \
    {                                                                                                                                \
        (shift) = (slot) * REPLAY_BONUS_SHOP_STOCK_LEVEL_BITS;                                                                       \
        if ((s32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock >> (shift)) & REPLAY_BONUS_SHOP_STOCK_LEVEL_MASK) < (level)) { \
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock &= ~(REPLAY_BONUS_SHOP_STOCK_LEVEL_MASK << (shift));                  \
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopStock |= (level) << (shift);                                                \
        }                                                                                                                            \
    }

    McSaveData    completedSave;
    McSaveData*   newSave;
    McSaveData*   saveData;
    PlayerStatus* playerStatus;
    const u8*     learnedLevel;
    s32           historyIndex;
    s32           abilitySlot;
    s32           nextBp;
    s32           stockLevelShift;
    s32           nextExp;

    // Reset run progress, then restore persistent options and history.
    playerStatus  = &gPlayerStatus;
    completedSave = gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    mcResetSaveData();
    newSave                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    newSave->state.clearCount   = completedSave.state.clearCount;
    newSave->state.vibration    = completedSave.state.vibration;
    newSave->state.demoScene    = completedSave.state.demoScene;
    newSave->state.moveMode     = completedSave.state.moveMode;
    newSave->state.buttonLayout = completedSave.state.buttonLayout;
    newSave->state.musicVolume  = completedSave.state.musicVolume;
    newSave->state.cursorMode   = completedSave.state.cursorMode;
    newSave->state.soundMode    = completedSave.state.soundMode;
    historyIndex                = 0;
    do {
        newSave->state.itemSeenBits[historyIndex] = completedSave.state.itemSeenBits[historyIndex];
        historyIndex                             += 1;
    } while (historyIndex < ARRAY_SIZE(completedSave.state.itemSeenBits));
    // Carry eighteen ability-use entries; the final halfword stays reset.
    historyIndex = 0;
    do {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[historyIndex] = completedSave.state.attachUseCounts[historyIndex];
        historyIndex                                                          += 1;
    } while (historyIndex < REPLAY_BONUS_CARRIED_ABILITY_USE_COUNT);
    historyIndex = 0;
    do {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[historyIndex] = completedSave.state.weaponUseCounts[historyIndex];
        historyIndex                                                          += 1;
    } while (historyIndex < ARRAY_SIZE(completedSave.state.weaponUseCounts));

    saveData                   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    saveData->state.saveCount  = MEMORY_CARD_SAVE_COUNT_NEW;
    saveData->state.maxExp     = completedSave.state.maxExp;
    saveData->state.maxBp      = completedSave.state.maxBp;
    saveData->state.shopTiers  = completedSave.state.shopTiers;
    saveData->state.shopStock  = completedSave.state.shopStock;
    saveData->state.replayRank = completedSave.state.replayRank;
    saveData->state.clearCount++;
    if (saveData->state.clearCount >= REPLAY_BONUS_MAX_CLEAR_COUNT + 1) {
        saveData->state.clearCount = REPLAY_BONUS_MAX_CLEAR_COUNT;
    }
    if (saveData->state.maxExp < D_replay_bonus_80119274.totalExp) {
        saveData->state.maxExp = D_replay_bonus_80119274.totalExp;
    }
    if (saveData->state.maxBp < D_replay_bonus_80119274.totalBp) {
        saveData->state.maxBp = D_replay_bonus_80119274.totalBp;
    }
    // Apply the replay awards to both resident and saved currency.
    nextBp = D_replay_bonus_80119274.nextBp + D_replay_bonus_80119274.extraBonusBp;
    if (nextBp > REPLAY_BONUS_MAX_STARTING_BALANCE) {
        nextBp = REPLAY_BONUS_MAX_STARTING_BALANCE;
    }
    playerStatus->bp          = nextBp;
    saveData->state.savePoint = MEMORY_CARD_SAVE_POINT_OPENING;
    nextExp                   = D_replay_bonus_80119274.nextExp;
    playerStatus->exp         = nextExp;
    saveData->state.playerExp = nextExp;
    saveData->state.playerBp  = playerStatus->bp;
    if (completedSave.state.gameMode >= REPLAY_BONUS_HIGH_RANK_MIN_GAME_MODE) {
        saveData->state.replayRank = REPLAY_BONUS_RANK_HIGH;
    } else if (saveData->state.replayRank <= 0) {
        if (D_replay_bonus_80119274.totalExp > REPLAY_BONUS_FIRST_RANK_EXP_CEILING) {
            saveData->state.replayRank = REPLAY_BONUS_RANK_FIRST;
        }
    }
    // Shop ability stock retains the highest level learned on any clear.
    learnedLevel = completedSave.state.attachLevels;
    abilitySlot  = 0;
    do {
        REPLAY_BONUS_RETAIN_SHOP_ABILITY_LEVEL(abilitySlot, *learnedLevel, stockLevelShift);
        abilitySlot  += 1;
        learnedLevel += 1;
    } while (abilitySlot < ATTACHMENT_SPELL_COUNT);
    if (D_replay_bonus_80119274.shopTier >= 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers |= 1 << D_replay_bonus_80119274.shopTier;
    }
#undef REPLAY_BONUS_RETAIN_SHOP_ABILITY_LEVEL
}
