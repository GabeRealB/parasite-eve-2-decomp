#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "replay_bonus_private.h"

#include "gameplay/ending.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

extern u8           D_replay_bonus_801157A8[];
extern u8           D_replay_bonus_801157B0[];
extern u8           D_replay_bonus_801157C4[];
extern u8           D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern UiObjectDesc D_replay_bonus_801191A8;

extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_replay_bonus_80119170;
extern UiObjectDesc D_replay_bonus_8011918C;
extern UiObjectDesc D_replay_bonus_801191C4;
extern UiObjectDesc D_replay_bonus_801191E0;
extern UiObjectDesc D_replay_bonus_801191FC;

/// Where a credits picture is decoded to and how large it is drawn.
///
/// Every picture has this size. Two VRAM rows at this column hold the picture
/// on screen and the one being decoded, so that one can fade into the other.
enum {
    REPLAY_BONUS_PICTURE_VRAM_X = 640, // Left edge of both picture buffers in VRAM
    REPLAY_BONUS_PICTURE_WIDTH  = 240, // Picture width in pixels
    REPLAY_BONUS_PICTURE_HEIGHT = 176, // Picture height in pixels
};

/// Credits RGB modulation, byte fade amounts and worker state values.
enum {
    REPLAY_BONUS_CREDITS_MAX_BRIGHTNESS = 127,
    REPLAY_BONUS_CREDITS_FADE_MAX       = 255,
    REPLAY_BONUS_CREDITS_FADE_SHIFT     = 1,
    REPLAY_BONUS_FADE_INITIALIZE        = 0,
    REPLAY_BONUS_FADE_RUN               = 1,
};

/// Shared credits drawing coordinates, text ordering bucket and picture lifecycle.
enum {
    REPLAY_BONUS_CREDITS_HALF_WIDTH        = 320,
    REPLAY_BONUS_CREDITS_TEXT_OT_INDEX     = 10,
    REPLAY_BONUS_PICTURE_IDLE              = 0,
    REPLAY_BONUS_PICTURE_DECODING          = 1,
    REPLAY_BONUS_PICTURE_CROSS_FADING      = 2,
    REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES = 120,
    REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT      = 8,
};

static void _replayBonusDrawCreditsFrame(void);
static void _replayBonusDrawCreditsRow(s32 bottomY, ReplayBonusStfCommand* commands);
static void _replayBonusSelectCreditsResource(s32 dataResourceIndex);

static void _replayBonusStartCreditsPictureTask(Task* task);

/// Advances the replay-award panels and the cleared-save confirmation sequence.
///
/// States 2..9 borrow the live UI object in spawn argument 2. A panel result
/// closes its tree before the next panel opens. Completed saving skips the quit
/// warning; declining to quit retries saving. Keeps the final restart delayed
/// by sixteen callback ticks while any panel is still open.
static void _replayBonusAdvanceAwardScreen(Task* task)
{
    enum {
        REPLAY_BONUS_SCREEN_COMPLETE_ITEMS   = 2,
        REPLAY_BONUS_SCREEN_BALANCE          = 3,
        REPLAY_BONUS_SCREEN_FIRST_SHOP_ITEM  = 4,
        REPLAY_BONUS_SCREEN_SECOND_SHOP_ITEM = 5,
        REPLAY_BONUS_SCREEN_LAST_AWARD       = 6,
        REPLAY_BONUS_SCREEN_SAVE             = 7,
        REPLAY_BONUS_SCREEN_QUIT_WARNING     = 8,
        REPLAY_BONUS_RESTART_DELAY_TICKS     = 16,
    };
    UiObject* object;
    Task*     panelTask;
    s32       commandResult;
    s16       uiResult;

    object   = task->spawnArg2.pointer;
    uiResult = object->result;
    if ((uiResult == USER_INTERFACE_RESULT_CANCEL) || (uiResult == USER_INTERFACE_RESULT_CONFIRM)) {
        // Capture the child command before its closing lifecycle resumes.
        panelTask     = object->owner;
        commandResult = object->resultValue;
        uiStartTreeClosing(object, panelTask);
        switch (task->state) {
            case REPLAY_BONUS_SCREEN_COMPLETE_ITEMS:
                task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_8011918C, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_BALANCE:
                if (D_replay_bonus_80119274.shopTier < 0) {
                    task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_801191FC, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                    task->state             = task->state + 2;
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_801191C4, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                }
                break;
            case REPLAY_BONUS_SCREEN_FIRST_SHOP_ITEM:
                task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_801191E0, 1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_SECOND_SHOP_ITEM:
                task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_801191C4, 2, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_LAST_AWARD:
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                replayBonusPrepareClearedSave();
                gDisplayState.gameMode  = DISPLAY_GAME_MODAL;
                task->spawnArg2.pointer = uiSpawnObject(&D_800611E4, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_SAVE:
                if (commandResult == USER_INTERFACE_LIST_COMMAND_YES) {
                    task->spawnArg2.pointer = itemMenuSpawnNotice(NULL, ITEM_MENU_NOTICE_SAVE_COMPLETE, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                    task->state             = task->state + 1;
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_80119170, 0, USER_INTERFACE_PANEL_ACTIVE, 2, NULL);
                }
                break;
            case REPLAY_BONUS_SCREEN_QUIT_WARNING:
                if (commandResult == USER_INTERFACE_LIST_COMMAND_YES) {
                    task->spawnArg2.pointer = itemMenuSpawnNotice(NULL, ITEM_MENU_NOTICE_SAVE_CANCELLED, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&D_800611E4, 1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                    task->state             = task->state - 2;
                }
                break;
        }
        task->state = task->state + 1;
        return;
    }
    task->killCountdown = REPLAY_BONUS_RESTART_DELAY_TICKS;
}

/// Selects the shop tier unlocked by this clear, or -1 if all are unlocked.
///
/// Starts at the first inclusive EXP ceiling, adds saved mode (0..3), caps at
/// the last tier and searches cyclically past unlocked tiers. Reads the live
/// save and catalogue without changing their unlocks. An EXP value beyond
/// every ceiling retains the initial tier zero before the mode boost.
static s32 _replayBonusSelectShopTier(void)
{
    return _replayBonusResolveShopTier();
}

/// Returns one item of the shop tier unlocked by this clear.
///
/// `itemColumn` is 0..2; the first argument is unused. Returns
/// `INVENTORY_ITEM_NONE` when every tier is unlocked. Reads the completed run's
/// EXP and saved unlocks using the same policy as `_replayBonusSelectShopTier`.
static s16 _replayBonusGetUnlockedShopItem(s32 unusedArgument, s32 itemColumn)
{
    s32 selectedTier = _replayBonusResolveShopTier();

    if (selectedTier < 0) {
        return INVENTORY_ITEM_NONE;
    }
    return D_replay_bonus_80118F78[selectedTier].items[itemColumn];
}

/// Returns 1 when an item id belongs to the Complete Bonus whitelist, otherwise 0.
///
/// Checks the 78 stored ids, excluding the trailing terminator. This tests
/// eligibility alone, without checking possession, quantity or identification.
static s32 _replayBonusIsListedItem(s32 itemId)
{
    enum { REPLAY_BONUS_ITEM_WHITELIST_COUNT = 78 };
    const u16* whitelistItem;
    s32        whitelistIndex;

    whitelistItem  = D_replay_bonus_8011908C;
    whitelistIndex = 0;
    do {
        whitelistIndex++;
        if (*whitelistItem != itemId) {
            whitelistItem++;
        } else {
            return 1;
        }
    } while (whitelistIndex < REPLAY_BONUS_ITEM_WHITELIST_COUNT);
    return 0;
}

/// Returns the signed item id at `rowIndex` in the Complete Bonus list.
///
/// The first argument is unused. The object's live owner borrows an allocated
/// `s16` item-id array; the caller supplies a nonnegative index below its item
/// count. Performs no bounds check and does not take ownership of the storage.
static s16 _replayBonusGetListItemId(const UiList* unusedList, const UiObject* object, s32 rowIndex)
{
    const s16* itemIds = object->owner->work;
    const s16* itemId  = &itemIds[rowIndex];

    return *itemId;
}

/// Returns the BP balance plus credits for the displayed list suffix.
///
/// Sums from the first visible item through the list's end and caps at eight
/// decimal digits. The owner's live work is a borrowed `s16` item-id array;
/// the row range must fit it and each id must meet `_replayBonusItemBp`'s
/// catalogue bounds. Neither inventory nor the BP balance is changed.
static s32 _replayBonusGetListTotalBp(const UiList* list, const UiObject* object)
{
    enum { REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP = 99999999 };
    s32                 itemIndex;
    s32                 totalBp;
    const PlayerStatus* playerStatus;

    playerStatus = &gPlayerStatus;
    totalBp      = 0;
    for (itemIndex = list->firstVisibleItemIndex.signedValue; itemIndex < list->itemCount; itemIndex++) {
        const s16* itemIds = object->owner->work;

        totalBp += _replayBonusItemBp(itemIds[itemIndex]);
    }
    totalBp += playerStatus->bp;
    if (totalBp > REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP) {
        totalBp = REPLAY_BONUS_MAX_DISPLAYED_TOTAL_BP;
    }
    return totalBp;
}

/// Draws a Complete Bonus item's name and half-price BP credit, identifying it.
///
/// Requires a live list and UI object and a whitelisted catalogue id. Row
/// coordinates are panel-relative pixels: the name uses the left X, and the
/// BP text is right-aligned at its mirrored X. Identification persists in
/// the live save; drawing does not consume the item or grant BP.
static inline void _replayBonusDrawItemRow(const UiList* list, const UiObject* object, s32 itemId)
{
    enum { REPLAY_BONUS_ITEM_ROW_TEXT_COLOR         = 0x606060,
           REPLAY_BONUS_ITEM_ROW_NO_ATTACHMENT_MARK = 0 };
    u8 bpText[0x20];

    itemSetIdentified(itemId, true);
    itemMenuDrawItemRow(object, list->rowTextX.signedValue, list->rowTextY.signedValue, itemId, REPLAY_BONUS_ITEM_ROW_TEXT_COLOR, REPLAY_BONUS_ITEM_ROW_NO_ATTACHMENT_MARK);
    textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, textItoaSigned(bpText, _replayBonusItemBp(itemId)), REPLAY_BONUS_ITEM_ROW_TEXT_COLOR, TEXT_DRAW_TRANSLUCENT_OUTLINED,
                   TEXT_ALIGNMENT_RIGHT);
}

/// Draws and identifies the Complete Bonus list's current item.
///
/// A `UiListRowCallback`: the current index must address the owner's borrowed
/// `s16` item-id array. Draws its name and BP credit at the supplied row position.
static void _replayBonusDrawCurrentItemRow(UiList* list, UiObject* object)
{
    const s16* itemIds;
    const s16* itemId;

    itemIds = object->owner->work;
    itemId  = &itemIds[list->currentItemIndex];
    _replayBonusDrawItemRow(list, object, *itemId);
}

/// Totals full purchase prices of every shop-tier entry, rounded up to 100000 BP.
///
/// Counts all three items of every tier, including any repeated catalogue id.
/// Requires the gameplay item catalogue to remain loaded; grants no currency.
static s32 _replayBonusGetShopCatalogueBp(void)
{
    enum { REPLAY_BONUS_SHOP_BP_ROUNDING_UNIT = 100000 };
    const ShopTier* tier;
    s32             itemColumn;
    s32             totalBp;
    s32             tierIndex;

    totalBp   = 0;
    tier      = D_replay_bonus_80118F78;
    tierIndex = totalBp;
    do {
        itemColumn = 0;
        do {
            totalBp += Gp_ItemDescs[tier->items[itemColumn]].price;
            itemColumn++;
        } while (itemColumn < ARRAY_SIZE(tier->items));
        tierIndex++;
        tier++;
    } while (tierIndex < SHOP_TIER_COUNT);
    totalBp += REPLAY_BONUS_SHOP_BP_ROUNDING_UNIT - 1;
    totalBp  = totalBp / REPLAY_BONUS_SHOP_BP_ROUNDING_UNIT;
    return totalBp * REPLAY_BONUS_SHOP_BP_ROUNDING_UNIT;
}

/// Waits 120 callback ticks, then queues the replay-award UI resource.
///
/// Starts with a zero countdown. Opens session UI, selects one-vblank timing
/// and advances to the resource-load wait state after queuing file 20162.
static void _replayBonusWaitToLoadAwardUi(Task* task)
{
    enum { REPLAY_BONUS_AWARD_UI_WAIT_TICKS    = 120,
           REPLAY_BONUS_AWARD_UI_FILE_HUNDREDS = 1,
           REPLAY_BONUS_AWARD_UI_FILE_INDEX    = 62 };
    u16 elapsedTicks = task->killCountdown + 1;

    task->killCountdown = elapsedTicks;
    if ((s16)elapsedTicks >= REPLAY_BONUS_AWARD_UI_WAIT_TICKS) {
        gGameSession->uiOpen = true;
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        cdCmdEnqueueDisplayResource(REPLAY_BONUS_AWARD_UI_FILE_HUNDREDS, REPLAY_BONUS_AWARD_UI_FILE_INDEX, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
        task->state = task->state + 1;
    }
}

/// Opens the Complete Bonus panel once the queued award UI load is idle.
///
/// Receives the award controller in state 1 after its resource request.
/// Uploads text palettes before opening an active, unparented panel with a
/// one-tick opening delay. Stores a borrowed pointer to the panel's task-owned
/// UI object in spawn argument 2 and advances to state 2; subsequent panel
/// dispatch requires a successful allocation. The controller does not free it.
static void _replayBonusWaitForAwardUi(Task* task)
{
    enum { REPLAY_BONUS_PANEL_OPEN_DELAY_TICKS = 1 };

    if (cdCmdIsIdle() != 0) {
        textUploadPalettes();
        task->spawnArg2.pointer = uiSpawnObject(&D_replay_bonus_80119154, 0, USER_INTERFACE_PANEL_ACTIVE, REPLAY_BONUS_PANEL_OPEN_DELAY_TICKS, NULL);
        task->state             = task->state + 1;
    }
}

/// Ends the award controller after its countdown and requests a game restart.
///
/// The countdown is tested as a signed halfword after decrement, so zero
/// gets one last callback. Closes session UI and calls the task's exit handler.
/// The resident restart then resets both heaps, retiring surviving credits
/// allocations, including the pictures' auxiliary-heap VLC table.
static void _replayBonusRestartAfterAwards(Task* task)
{
    u16 remaining = task->killCountdown - 1;

    task->killCountdown = remaining;
    if ((s16)remaining < 0) {
        gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
        gGameSession->uiOpen   = false;
        // Finish the controller before the resident loop resets task and heap storage.
        taskCallExit(task);
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }
}

/// Presents replay awards, confirms the cleared save and restarts the game.
///
/// Starts as a zeroed bodyless task. State 0 delays the UI resource request
/// for 120 callbacks; state 1 waits for it and opens Complete Bonus. States
/// 2..9 borrow the live panel in spawn argument 2 and advance through the
/// balances, three unlocked items (or extra BP), save prompt, quit warning
/// and final notice. State 10 delays the restart until the countdown is
/// negative. Dispatch requires state 0..10 and performs no bounds check.
/// Requires the replay package, gameplay catalogue and live save to remain live, with
/// one award sequence using the shared totals. Panel tasks own their UI objects;
/// the controller requests their closing before replacing its borrowed pointer.
static void _replayBonusAwardScreenTask(Task* task)
{
    const TaskFuncTable11 stateHandlers = { {
        _replayBonusWaitToLoadAwardUi,
        _replayBonusWaitForAwardUi,
        _replayBonusAdvanceAwardScreen, // 2 Complete Bonus item list
        _replayBonusAdvanceAwardScreen, // 3 Balance and NEXT REPLAY BONUS
        _replayBonusAdvanceAwardScreen, // 4 First unlocked item
        _replayBonusAdvanceAwardScreen, // 5 Second unlocked item
        _replayBonusAdvanceAwardScreen, // 6 Third unlocked item or extra BP
        _replayBonusAdvanceAwardScreen, // 7 Cleared-save prompt
        _replayBonusAdvanceAwardScreen, // 8 Quit warning
        _replayBonusAdvanceAwardScreen, // 9 Save completion or cancellation notice
        _replayBonusRestartAfterAwards,
    } };

    stateHandlers.funcs[task->state](task);
}

/// Clears both credits picture buffers to black in off-screen VRAM.
///
/// Each RGB16 buffer is 240x176 pixels, at VRAM (640, 0) or (640, 256).
/// Call before drawing or decoding pictures to provide a black initial image
/// and keep the final 16-pixel column black where decoding leaves it unwritten.
/// Submits both fills without waiting for GPU completion.
static inline void _replayBonusClearCreditsPictureBuffers(void)
{
    RECT pictureRegion;

    setRECT(&pictureRegion, REPLAY_BONUS_PICTURE_VRAM_X, 0, REPLAY_BONUS_PICTURE_WIDTH, REPLAY_BONUS_PICTURE_HEIGHT);
    ClearImage(&pictureRegion, 0, 0, 0);
    pictureRegion.y = 1 << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT;
    ClearImage(&pictureRegion, 0, 0, 0);
}

/// Plays the ending credits and hands presentation to the replay-award screen.
///
/// Starts as a bodyless task in state zero; spawn arguments are unused. Requires
/// the ending memory layout, a writable STF document with at least one row and
/// its picture resources, and successful picture-record/VLC allocations. Only
/// one credits controller may use the shared drawing and decoder state.
/// Holds before scrolling, advances at the document's 8.8 pixels-per-frame
/// speed, then holds the last row. Holds use six callbacks per STF timing unit.
/// Start in the initial hold or scroll requests a 30-tick fade. Normal fades
/// last 180 ticks; the final fade starts when the countdown decrements to 180.
/// Drawing runs once per callback while holding or scrolling.
/// Waits for picture decoding to end before freeing the primary-heap request.
/// The auxiliary-heap VLC table survives awards until the game restart
/// reconfigures image memory and rebuilds the heaps. Restores the normal display
/// over successive callbacks before releasing this task.
static void _replayBonusCreditsTask(Task* task)
{
    enum {
        REPLAY_BONUS_CREDITS_INITIALIZE             = 0,
        REPLAY_BONUS_CREDITS_WAIT_AUDIO             = 1,
        REPLAY_BONUS_CREDITS_START_HOLD             = 2,
        REPLAY_BONUS_CREDITS_SCROLL                 = 10,
        REPLAY_BONUS_CREDITS_END_HOLD               = 11,
        REPLAY_BONUS_CREDITS_WAIT_PICTURE           = 20,
        REPLAY_BONUS_CREDITS_RESTORE_DISPLAY        = 21,
        REPLAY_BONUS_CREDITS_WAIT_DISPLAY_FIRST     = 22,
        REPLAY_BONUS_CREDITS_WAIT_DISPLAY_SECOND    = 23,
        REPLAY_BONUS_CREDITS_START_AWARDS           = 24,
        REPLAY_BONUS_CREDITS_FINISH                 = 25,
        REPLAY_BONUS_CREDITS_DISPLAY_SETUP          = 0x1141, // Interlaced 640x480, 16-bit colour
        REPLAY_BONUS_CREDITS_VIEWPORT_HEIGHT_PIXELS = 480,
        REPLAY_BONUS_CREDITS_FADE_IN_TASK_INDEX     = 1,
        REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX    = 2,
        REPLAY_BONUS_CREDITS_FADE_TICKS             = 180,
        REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS        = 30,
        REPLAY_BONUS_CREDITS_SCROLL_FRACTION_BITS   = 8,
        REPLAY_BONUS_CREDITS_SCROLL_FRACTION_MASK   = 0xFF,
        REPLAY_BONUS_CREDITS_AUDIO_GROUP            = 0,
        REPLAY_BONUS_CREDITS_AUDIO_STREAM_ID        = 1,
        REPLAY_BONUS_CREDITS_AUDIO_SUB_ID           = 11,
    };

    s32                   nextScrollY;
    s32                   scrollFixedPoint;
    s32                   lastScrollY;
    u16*                  vlcTable;
    u16                   startHoldTicksLeft;
    u16                   endHoldTicksLeft;
    ReplayBonusStfParams* creditsParams;

    switch (task->state) {
        case REPLAY_BONUS_CREDITS_INITIALIZE:
            // Keep both picture pages black until the first decode and fade-in.
            D_replay_bonus_80119226           = 0;
            D_replay_bonus_80119227           = 0;
            D_replay_bonus_801192AC           = 0;
            D_replay_bonus_801192BC           = memCalloc(sizeof(ReplayBonusPictureDecode), false);
            vlcTable                          = replayBonusCreatePictureVlcTable();
            D_replay_bonus_80119228           = NULL;
            D_replay_bonus_80119225           = REPLAY_BONUS_PICTURE_IDLE;
            D_replay_bonus_801192BC->vlcTable = vlcTable;
            _replayBonusSelectCreditsResource(0);
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            SetDispMask(true);
            displayConfigureFramebuffers(REPLAY_BONUS_CREDITS_DISPLAY_SETUP);
            _replayBonusClearCreditsPictureBuffers();
            D_replay_bonus_801192A4                 = -REPLAY_BONUS_CREDITS_VIEWPORT_HEIGHT_PIXELS;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            D_replay_bonus_801192B0                 = 0;
            task->killCountdown                     = D_replay_bonus_80119294->startHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
            cdCmdSelectScene(REPLAY_BONUS_CREDITS_AUDIO_GROUP, REPLAY_BONUS_CREDITS_AUDIO_STREAM_ID, REPLAY_BONUS_CREDITS_AUDIO_SUB_ID);
            cdCmdEnqueueSceneAudioStart();
            task->state += 1;
            return;
        case REPLAY_BONUS_CREDITS_WAIT_AUDIO:
            if (cdCmdIsIdle()) {
                cdCmdEnqueueScenePlayback();
                task->state += 1;
                return;
            }
            return;
        case REPLAY_BONUS_CREDITS_START_HOLD:
            D_replay_bonus_801192B0 += 1;
            _replayBonusDrawCreditsFrame();
            startHoldTicksLeft  = task->killCountdown - 1;
            task->killCountdown = startHoldTicksLeft;
            if ((s16)startHoldTicksLeft <= 0) {
                taskSpawnFromTable(D_replay_bonus_8011922C, REPLAY_BONUS_CREDITS_FADE_IN_TASK_INDEX, REPLAY_BONUS_CREDITS_FADE_TICKS, 0);
                task->state = REPLAY_BONUS_CREDITS_SCROLL;
            }
            if (padIsStartPressed() != 0) {
                taskSpawnFromTable(D_replay_bonus_8011922C, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS, 0);
                cdCmdCancelScene();
                task->state         = REPLAY_BONUS_CREDITS_END_HOLD;
                task->killCountdown = REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_SCROLL:
            D_replay_bonus_801192B0 += 1;
            _replayBonusDrawCreditsFrame();
            // Carry whole pixels into document Y, retaining the fractional byte.
            creditsParams           = D_replay_bonus_80119294;
            scrollFixedPoint        = D_replay_bonus_801192A8 + creditsParams->scrollSpeed;
            nextScrollY             = D_replay_bonus_801192A4 + (scrollFixedPoint >> REPLAY_BONUS_CREDITS_SCROLL_FRACTION_BITS);
            D_replay_bonus_801192A8 = scrollFixedPoint;
            D_replay_bonus_801192A4 = nextScrollY;
            D_replay_bonus_801192A8 = scrollFixedPoint & REPLAY_BONUS_CREDITS_SCROLL_FRACTION_MASK;
            lastScrollY             = D_replay_bonus_80119298[D_replay_bonus_801192A0 - 1].y - REPLAY_BONUS_CREDITS_VIEWPORT_HEIGHT_PIXELS;
            if (lastScrollY < nextScrollY) {
                D_replay_bonus_801192A4 = lastScrollY;
                task->state            += 1;
                task->killCountdown     = creditsParams->endHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
            }
            if (padIsStartPressed() != 0) {
                cdCmdCancelScene();
                taskSpawnFromTable(D_replay_bonus_8011922C, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS, 0);
                task->killCountdown = REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS;
                task->state        += 1;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_END_HOLD:
            D_replay_bonus_801192B0 += 1;
            _replayBonusDrawCreditsFrame();
            endHoldTicksLeft    = task->killCountdown - 1;
            task->killCountdown = endHoldTicksLeft;
            if ((s16)endHoldTicksLeft == REPLAY_BONUS_CREDITS_FADE_TICKS) {
                taskSpawnFromTable(D_replay_bonus_8011922C, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_FADE_TICKS, 0);
            }
            if (task->killCountdown <= 0) {
                task->state = REPLAY_BONUS_CREDITS_WAIT_PICTURE;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_WAIT_PICTURE:
            // The request record remains borrowed until the picture worker finishes.
            if (D_replay_bonus_80119225 != REPLAY_BONUS_PICTURE_DECODING) {
                SetDispMask(false);
                task->state += 1;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_RESTORE_DISPLAY:
            streamFinishScene();
            // The separate VLC allocation is retired by the post-awards restart.
            memFree(D_replay_bonus_801192BC);
            displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT);
            task->state += 1;
            return;
        case REPLAY_BONUS_CREDITS_START_AWARDS:
            endingSpawnReplayAwardScreen();
            // Fall through to the same delayed display handoff as the preceding states.
        case REPLAY_BONUS_CREDITS_WAIT_DISPLAY_FIRST:
        case REPLAY_BONUS_CREDITS_WAIT_DISPLAY_SECOND:
            task->state += 1;
            return;
        case REPLAY_BONUS_CREDITS_FINISH:
            SetDispMask(true);
            taskKill(task);
            break;
    }
}

/// Converts a credits fade amount into shared text and picture brightness.
///
/// `fadeAmount` is 0..255: zero gives RGB modulation 127 and 255 gives black.
/// Integer halving makes adjacent fade amounts share a brightness level.
static void _replayBonusSetCreditsFade(u8 fadeAmount)
{
    // Unused storage retains the leaf routine's sixteen-byte stack frame.
    char unusedStackBytes[0x10];

    D_replay_bonus_801192AC = REPLAY_BONUS_CREDITS_MAX_BRIGHTNESS - (fadeAmount >> REPLAY_BONUS_CREDITS_FADE_SHIFT);
}

/// Selects the next private credits primitive buffer and resets its byte cursor.
///
/// The selector must be 0 or 1. Uses one of two 64-KiB arenas immediately after
/// the normal primitive arena, then toggles the selector for the next call.
/// The ending memory layout reserves both arenas before the auxiliary heap.
/// Packets in the selected arena must have finished drawing before reuse;
/// subsequent row-guide and glyph packets must fit its 64-KiB byte extent.
/// This only selects existing storage and does not allocate or clear packets.
static inline void _replayBonusSelectCreditsPrimitiveBuffer(void)
{
    enum { REPLAY_BONUS_CREDITS_PRIMITIVE_BUFFER_BYTES = 0x10000 };
    D_replay_bonus_801192B4  = 0;
    D_replay_bonus_801192C0  = Gpu_PrimHeapBase + Gpu_PrimHeapSize + D_replay_bonus_80119224 * REPLAY_BONUS_CREDITS_PRIMITIVE_BUFFER_BYTES;
    D_replay_bonus_80119224 ^= 1;
}

/// Draws the visible STF credits rows and advances any picture cross-fade.
///
/// Requires relocated, writable credits tables in increasing document Y order,
/// the normal primitive arena and two additional 64-KiB glyph buffers after it.
/// Each selected row consumes a line packet plus one quad per glyph; the caller
/// must keep that total within a buffer. Coordinates use the centred 640x480
/// display: scroll bits 1..23 are retained and GPU Y narrows to a halfword.
/// Includes up to two rows above the viewport and the first beyond its bottom, so
/// picture commands can start before their row appears. Call once per frame;
/// another picture may be requested after 120 cross-fade draws.
static void _replayBonusDrawCreditsFrame(void)
{
    enum {
        REPLAY_BONUS_CREDITS_SCREEN_HEIGHT    = 480,
        REPLAY_BONUS_CREDITS_HALF_HEIGHT      = 240,
        REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX = 11,
        REPLAY_BONUS_CREDITS_SCROLL_Y_MASK    = 0xFFFFFE,
        REPLAY_BONUS_PICTURE_SCREEN_TOP       = -140,
        REPLAY_BONUS_PICTURE_TEXTURE_16_BIT   = 2,
        REPLAY_BONUS_PICTURE_BLEND_ADDITIVE   = 1,
    };
    s32                                firstLine;
    s32                                lineIndex;
    s32                                lastLine;
    s32                                lineCount;
    s32                                scanLineCount;
    s32                                scrollY;
    s32                                visibleBottomY;
    s32                                bottomY;
    s32                                pictureBrightness;
    u8                                 crossFadeFramesLeft;
    const volatile ReplayBonusStfLine* scanLine; // Retains the two ordered Y reads in the image
    LINE_F2*                           rowGuide;
    SPRT*                              pictureSprite;
    DR_TPAGE*                          drawPage;

    // Alternate private glyph buffers while the preceding frame is drawn.
    firstLine = 0;
    lineIndex = firstLine;
    lineCount = D_replay_bonus_801192A0;
    _replayBonusSelectCreditsPrimitiveBuffer();
    lastLine = lineCount - 1;

    // Retain up to two rows above the viewport and the first below its bottom edge.
    if (lineCount > 0) {
        scanLineCount  = lineCount;
        scanLine       = D_replay_bonus_80119298;
        scrollY        = D_replay_bonus_801192A4;
        visibleBottomY = scrollY + REPLAY_BONUS_CREDITS_SCREEN_HEIGHT;
        do {
            if (scanLine->y < scrollY) {
                firstLine = 0;
                if (lineIndex != 0) {
                    firstLine = lineIndex - 1;
                }
            }
            if (visibleBottomY < scanLine->y) {
                lastLine = lineIndex;
                break;
            }
            lineIndex++;
            scanLine++;
        } while (lineIndex < scanLineCount);
        lineIndex = firstLine;
    }

    for (; lineIndex < lastLine + 1; lineIndex++) {
        rowGuide                 = (LINE_F2*)(D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
        D_replay_bonus_801192B4 += sizeof(*rowGuide);
        // Retained row-guide packets consume space but are never linked for drawing.
        setLineF2(rowGuide);
        bottomY      = D_replay_bonus_80119298[lineIndex].y - (D_replay_bonus_801192A4 & REPLAY_BONUS_CREDITS_SCROLL_Y_MASK) - REPLAY_BONUS_CREDITS_HALF_HEIGHT;
        rowGuide->x0 = -REPLAY_BONUS_CREDITS_HALF_WIDTH;
        rowGuide->x1 = REPLAY_BONUS_CREDITS_HALF_WIDTH;
        rowGuide->r0 = 0x40;
        rowGuide->g0 = 0x80;
        rowGuide->b0 = 0x40;
        rowGuide->y0 = bottomY;
        rowGuide->y1 = bottomY;
        _replayBonusDrawCreditsRow(bottomY, D_replay_bonus_80119298[lineIndex].cmds.pointer);
    }

    drawPage       = gGpuPrimCursor;
    gGpuPrimCursor = drawPage + 1;
    setDrawTPage(drawPage, 0, 1, 0);
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, drawPage);

    if (D_replay_bonus_80119225 != REPLAY_BONUS_PICTURE_CROSS_FADING) {
        pictureSprite  = gGpuPrimCursor;
        gGpuPrimCursor = pictureSprite + 1;
        setSprt(pictureSprite);
        pictureSprite->x0 = D_replay_bonus_801192B8 - REPLAY_BONUS_CREDITS_HALF_WIDTH;
        pictureSprite->y0 = REPLAY_BONUS_PICTURE_SCREEN_TOP;
        pictureSprite->w  = REPLAY_BONUS_PICTURE_WIDTH;
        pictureSprite->h  = REPLAY_BONUS_PICTURE_HEIGHT;
        pictureSprite->r0 = D_replay_bonus_801192AC;
        pictureSprite->g0 = D_replay_bonus_801192AC;
        pictureSprite->b0 = D_replay_bonus_801192AC;
        pictureSprite->u0 = 0;
        pictureSprite->v0 = 0;
        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, pictureSprite);

        drawPage       = gGpuPrimCursor;
        gGpuPrimCursor = drawPage + 1;
        setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, 0, REPLAY_BONUS_PICTURE_VRAM_X, D_replay_bonus_80119226 << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);
        return;
    }

    // Blend the newly decoded page over the outgoing page for 120 frames.
    pictureSprite  = gGpuPrimCursor;
    gGpuPrimCursor = pictureSprite + 1;
    setSprt(pictureSprite);
    pictureBrightness = (D_replay_bonus_801192AC * (REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES - D_replay_bonus_80119227)) / REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
    pictureSprite->x0 = D_replay_bonus_801192B8 - REPLAY_BONUS_CREDITS_HALF_WIDTH;
    pictureSprite->y0 = REPLAY_BONUS_PICTURE_SCREEN_TOP;
    pictureSprite->w  = REPLAY_BONUS_PICTURE_WIDTH;
    pictureSprite->h  = REPLAY_BONUS_PICTURE_HEIGHT;
    pictureSprite->u0 = 0;
    pictureSprite->v0 = 0;
    setSemiTrans(pictureSprite, 1);
    pictureSprite->r0 = pictureBrightness;
    pictureSprite->g0 = pictureBrightness;
    pictureSprite->b0 = pictureBrightness;
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, pictureSprite);

    drawPage       = gGpuPrimCursor;
    gGpuPrimCursor = drawPage + 1;
    setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, REPLAY_BONUS_PICTURE_BLEND_ADDITIVE, REPLAY_BONUS_PICTURE_VRAM_X, D_replay_bonus_80119226 << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);

    pictureSprite  = gGpuPrimCursor;
    gGpuPrimCursor = pictureSprite + 1;
    setSprt(pictureSprite);
    pictureBrightness = (D_replay_bonus_801192AC * D_replay_bonus_80119227) / REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
    pictureSprite->x0 = D_replay_bonus_801192B8 - REPLAY_BONUS_CREDITS_HALF_WIDTH;
    pictureSprite->y0 = REPLAY_BONUS_PICTURE_SCREEN_TOP;
    pictureSprite->w  = REPLAY_BONUS_PICTURE_WIDTH;
    pictureSprite->h  = REPLAY_BONUS_PICTURE_HEIGHT;
    pictureSprite->u0 = 0;
    pictureSprite->v0 = 0;
    pictureSprite->r0 = pictureBrightness;
    pictureSprite->g0 = pictureBrightness;
    pictureSprite->b0 = pictureBrightness;
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, pictureSprite);

    drawPage       = gGpuPrimCursor;
    gGpuPrimCursor = drawPage + 1;
    setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, 0, REPLAY_BONUS_PICTURE_VRAM_X, (D_replay_bonus_80119226 ^ 1) << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);

    crossFadeFramesLeft     = D_replay_bonus_80119227 - 1;
    D_replay_bonus_80119227 = crossFadeFramesLeft;
    if (!(crossFadeFramesLeft & 0xFF)) {
        D_replay_bonus_80119225 = REPLAY_BONUS_PICTURE_IDLE;
    }
}

/// Interprets and draws one writable STF row at its bottom edge in screen pixels.
///
/// `commands` is terminated by `REPLAY_BONUS_STF_COMMAND_END` and remains
/// writable: picture operands are marked after their first request. Glyph,
/// column (0..7), sprite and picture-resource indices must address live tables;
/// palette indices are 0..7. Command cursors and run widths must fit signed
/// halfwords; `bottomY` narrows to a halfword in each GPU packet. Glyph quads
/// consume the current private credits buffer; image sprites and draw-page
/// packets consume the normal primitive arena.
static void _replayBonusDrawCreditsRow(s32 bottomY, ReplayBonusStfCommand* commands)
{
    enum {
        REPLAY_BONUS_CREDITS_PICTURE_TASK_INDEX = 3,
        REPLAY_BONUS_STF_ANCHOR_CENTER          = 0,
        REPLAY_BONUS_STF_ANCHOR_LEFT            = 1,
        REPLAY_BONUS_STF_ANCHOR_RIGHT           = 2,
        REPLAY_BONUS_STF_DEFAULT_PALETTE        = 7,
        REPLAY_BONUS_STF_PALETTES_PER_ROW       = 4,
        REPLAY_BONUS_STF_CLUT_FIRST_Y           = 510,
        REPLAY_BONUS_STF_CLUT_FIRST_X           = 896,
        REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS    = 256,
        REPLAY_BONUS_STF_PIXEL_MODE_8_BIT       = 1,
        REPLAY_BONUS_STF_PIXEL_MODE_16_BIT      = 2,
        REPLAY_BONUS_STF_PALETTE_COLORS         = 16,
        REPLAY_BONUS_STF_FONT_PAGE0_X           = 896,
        REPLAY_BONUS_STF_FONT_PAGE0_Y           = 256,
        REPLAY_BONUS_STF_FONT_PAGE1_X           = 960,
        REPLAY_BONUS_STF_FONT_PAGE1_Y           = 0,
        REPLAY_BONUS_STF_VRAM_X_MASK            = 0x3FF,
        REPLAY_BONUS_STF_VRAM_PAGE_X_SHIFT      = 6,
    };
    /// Selects a run's left edge from the current relocated layout columns.
    ///
    /// Scalar arguments must be side-effect-free identifiers or constants;
    /// `resultX` is a distinct s16 lvalue initialized to zero. Width narrows
    /// to s16 for centring, and the assignment narrows the selected edge.
    /// Uses this function's anchor constants and the live STF parameter table.
#define REPLAY_BONUS_SELECT_CREDITS_ANCHOR(column, mode, width, resultX)                         \
    switch ((u8)(mode)) {                                                                        \
        case REPLAY_BONUS_STF_ANCHOR_CENTER:                                                     \
            (resultX) = D_replay_bonus_80119294->columns[(column)].centerX - ((s16)(width) / 2); \
            break;                                                                               \
        case REPLAY_BONUS_STF_ANCHOR_LEFT:                                                       \
            (resultX) = D_replay_bonus_80119294->columns[(column)].leftX;                        \
            break;                                                                               \
        case REPLAY_BONUS_STF_ANCHOR_RIGHT:                                                      \
            (resultX) = D_replay_bonus_80119294->columns[(column)].rightX - (width);             \
            break;                                                                               \
    }

    s32                        paletteIndex;
    s32                        columnIndex;
    s32                        anchorMode;
    s16                        commandIndex;
    s16                        glyphOffset;
    s32                        drawWidth;
    s16                        penX;
    s16                        rightX;
    s16                        glyphHeight;
    s16                        topY;
    s32                        alternateFontPage;
    s16                        pixelsPerVramWordShift;
    s16                        pieceWidth;
    s32                        textureU;
    s16                        textureV;
    u8                         imageIndex;
    s32                        texturePageX;
    s32                        clutId;
    s32                        heightValue; // Packed glyph height/page or plain sprite height
    s32                        pixelMode;
    u16                        texturePageColumn;
    u8                         drawPixelMode;
    const ReplayBonusStfGlyph* glyph;
    POLY_FT4*                  glyphQuad;
    SPRT*                      imageSprite;
    DR_TPAGE*                  drawPage;

    paletteIndex = REPLAY_BONUS_STF_DEFAULT_PALETTE;
    columnIndex  = 0;
    anchorMode   = columnIndex;
    commandIndex = columnIndex;
    if (commands->op != REPLAY_BONUS_STF_COMMAND_END) {
        do {
            switch (commands[commandIndex].op) {
                case REPLAY_BONUS_STF_COMMAND_GLYPH:
                    // Align the complete glyph run before advancing by individual cell widths.
                    drawWidth   = 0;
                    glyphOffset = 0;
                    while (commands[commandIndex + glyphOffset].op == REPLAY_BONUS_STF_COMMAND_GLYPH) {
                        drawWidth += D_replay_bonus_80119290[commands[commandIndex + glyphOffset].arg].width;
                        glyphOffset++;
                    }
                    penX = 0;
                    REPLAY_BONUS_SELECT_CREDITS_ANCHOR(columnIndex, anchorMode, drawWidth, penX);
                    penX       -= REPLAY_BONUS_CREDITS_HALF_WIDTH;
                    glyphOffset = 0;
                    while (commands[commandIndex + glyphOffset].op == REPLAY_BONUS_STF_COMMAND_GLYPH) {
                        glyph                    = &D_replay_bonus_80119290[commands[commandIndex + glyphOffset].arg];
                        drawWidth                = glyph->width;
                        heightValue              = glyph->heightAndPage;
                        textureU                 = glyph->u;
                        textureV                 = glyph->v;
                        glyphQuad                = (POLY_FT4*)(D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
                        D_replay_bonus_801192B4 += sizeof(*glyphQuad);
                        setPolyFT4(glyphQuad);
                        glyphHeight       = heightValue & REPLAY_BONUS_STF_GLYPH_HEIGHT_MASK;
                        glyphQuad->u0     = textureU;
                        glyphQuad->v0     = textureV;
                        glyphQuad->u1     = drawWidth + textureU;
                        glyphQuad->v1     = textureV;
                        glyphQuad->u2     = textureU;
                        glyphQuad->v2     = textureV + glyphHeight;
                        glyphQuad->u3     = drawWidth + textureU;
                        glyphQuad->v3     = textureV + glyphHeight;
                        glyphQuad->r0     = D_replay_bonus_801192AC;
                        glyphQuad->x0     = penX;
                        glyphQuad->x2     = penX;
                        glyphQuad->y2     = bottomY;
                        glyphQuad->y3     = bottomY;
                        glyphQuad->g0     = D_replay_bonus_801192AC;
                        glyphQuad->b0     = D_replay_bonus_801192AC;
                        topY              = bottomY - glyphHeight;
                        rightX            = penX + drawWidth;
                        glyphQuad->y0     = topY;
                        glyphQuad->x1     = rightX;
                        glyphQuad->y1     = topY;
                        glyphQuad->x3     = rightX;
                        alternateFontPage = (u32)heightValue >> REPLAY_BONUS_STF_GLYPH_PAGE_SHIFT;
                        glyphQuad->clut   = getClut(REPLAY_BONUS_STF_CLUT_FIRST_X + (paletteIndex & (REPLAY_BONUS_STF_PALETTES_PER_ROW - 1)) * REPLAY_BONUS_STF_PALETTE_COLORS, REPLAY_BONUS_STF_CLUT_FIRST_Y + paletteIndex / REPLAY_BONUS_STF_PALETTES_PER_ROW);
                        glyphQuad->tpage  = getTPage(0, 0, REPLAY_BONUS_STF_FONT_PAGE0_X, REPLAY_BONUS_STF_FONT_PAGE0_Y);
                        if (alternateFontPage) {
                            glyphQuad->tpage = getTPage(0, 0, REPLAY_BONUS_STF_FONT_PAGE1_X, REPLAY_BONUS_STF_FONT_PAGE1_Y);
                        }
                        penX = rightX;
                        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, glyphQuad);
                        glyphOffset++;
                    }
                    commandIndex += glyphOffset - 1;
                default:
                case 9:
                    break;
                case REPLAY_BONUS_STF_COMMAND_PALETTE:
                    paletteIndex = commands[commandIndex].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_CENTER:
                    anchorMode  = REPLAY_BONUS_STF_ANCHOR_CENTER;
                    columnIndex = commands[commandIndex].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_LEFT:
                    anchorMode  = REPLAY_BONUS_STF_ANCHOR_LEFT;
                    columnIndex = commands[commandIndex].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_RIGHT:
                    anchorMode  = REPLAY_BONUS_STF_ANCHOR_RIGHT;
                    columnIndex = commands[commandIndex].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_PICTURE:
                    imageIndex = commands[commandIndex].arg;
                    penX       = 0;
                    if (imageIndex != REPLAY_BONUS_STF_PICTURE_STARTED) {
                        // Mark before spawning: visible rows are interpreted again next frame.
                        commands[commandIndex].arg = REPLAY_BONUS_STF_PICTURE_STARTED;
                        REPLAY_BONUS_SELECT_CREDITS_ANCHOR(columnIndex, anchorMode, REPLAY_BONUS_PICTURE_WIDTH, penX);
                        D_replay_bonus_801192B8 = penX;
                        taskSpawnFromTable(D_replay_bonus_8011922C, REPLAY_BONUS_CREDITS_PICTURE_TASK_INDEX, imageIndex + 1, 0);
                    }
                    break;
                case REPLAY_BONUS_STF_COMMAND_SPRITE:
                    imageIndex = commands[commandIndex].arg;
                    drawWidth  = D_replay_bonus_8011929C[imageIndex].width;
                    penX       = 0;
                    REPLAY_BONUS_SELECT_CREDITS_ANCHOR(columnIndex, anchorMode, drawWidth, penX);
                    pixelsPerVramWordShift = 2;
                    texturePageX           = D_replay_bonus_8011929C[imageIndex].tpageX;
                    textureU               = D_replay_bonus_8011929C[imageIndex].u;
                    // The chained V load retains the matching allocation of shared height storage.
                    textureV = heightValue = D_replay_bonus_8011929C[imageIndex].v;
                    heightValue            = D_replay_bonus_8011929C[imageIndex].height;
                    clutId                 = getClut(D_replay_bonus_8011929C[imageIndex].clutX, D_replay_bonus_8011929C[imageIndex].clutY);
                    pixelMode              = D_replay_bonus_8011929C[imageIndex].pixelMode;
                    switch (pixelMode) {
                        case REPLAY_BONUS_STF_PIXEL_MODE_8_BIT:
                            pixelsPerVramWordShift = 1;
                            break;
                        case REPLAY_BONUS_STF_PIXEL_MODE_16_BIT:
                            pixelsPerVramWordShift = 0;
                            break;
                    }
                    // Emit one sprite and draw-page packet for each texture-page piece.
                    while ((s16)drawWidth > 0) {
                        pieceWidth = drawWidth;
                        if (textureU + (s16)drawWidth >= REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS + 1) {
                            pieceWidth = REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS - textureU;
                            drawWidth  = drawWidth - pieceWidth;
                            textureU   = 0;
                        } else {
                            drawWidth = 0;
                        }
                        imageSprite    = gGpuPrimCursor;
                        gGpuPrimCursor = imageSprite + 1;
                        setSprt(imageSprite);
                        imageSprite->r0   = D_replay_bonus_801192AC;
                        imageSprite->x0   = penX - REPLAY_BONUS_CREDITS_HALF_WIDTH;
                        imageSprite->g0   = D_replay_bonus_801192AC;
                        imageSprite->b0   = D_replay_bonus_801192AC;
                        imageSprite->y0   = bottomY - heightValue;
                        imageSprite->w    = pieceWidth;
                        imageSprite->h    = heightValue;
                        imageSprite->clut = clutId;
                        imageSprite->u0   = textureU;
                        imageSprite->v0   = textureV;
                        // Prepending the page packet makes it execute before its sprite.
                        setaddr(imageSprite, getaddr(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX));
                        drawPage       = gGpuPrimCursor;
                        gGpuPrimCursor = drawPage + 1;
                        setaddr(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, imageSprite);
                        setlen(drawPage, 1);
                        drawPixelMode     = D_replay_bonus_8011929C[imageIndex].pixelMode;
                        texturePageColumn = (u32)(texturePageX & REPLAY_BONUS_STF_VRAM_X_MASK) >> REPLAY_BONUS_STF_VRAM_PAGE_X_SHIFT;
                        drawPage->code[0] = ((drawPixelMode & 3) << 7) | (s16)((s32)((D_replay_bonus_8011929C[imageIndex].tpageY & 0x100) << 16) >> 20) | texturePageColumn | ((s16)(D_replay_bonus_8011929C[imageIndex].tpageY & 0x200) * 4) | _get_mode(0, 1, 0);
                        texturePageX     += REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS >> pixelsPerVramWordShift;
                        penX              = penX + pieceWidth;
                        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, drawPage);
                    }
                    break;
            }
            commandIndex++;
        } while (commands[commandIndex].op != REPLAY_BONUS_STF_COMMAND_END);
    }
#undef REPLAY_BONUS_SELECT_CREDITS_ANCHOR
}

/// Relocates a writable STF credits file in place and publishes its drawing tables.
///
/// Returns 0 for a non-STF magic prefix, 1 otherwise; the resource-slot argument
/// is unused. Requires complete, aligned tables and valid file-relative byte
/// offsets in PS1 RAM. A positive glyph-table word marks an unrelocated file;
/// relocated pointers are negative signed words. Publishes the line count on
/// initial relocation only, so reusing a relocated file requires that count
/// still to describe it. The file must outlive every drawing-table pointer.
static s32 _replayBonusRelocateCreditsFile(ReplayBonusStfFile* file, s32 unusedResourceSlot)
{
    ReplayBonusStfLine* line;
    s32                 lineIndex;

    if (strncmp(file->magic, "STF", 3) != 0) {
        return 0;
    }

    // Convert serialized byte offsets to PS1 address words exactly once.
    if (file->glyphs.offset > 0) {
        file->glyphs.offset    += (s32)file;
        file->params.offset    += (s32)file;
        file->lineTable.offset += (s32)file;
        file->sprites.offset   += (s32)file;
        D_replay_bonus_80119298 = file->lineTable.pointer->lines;
        D_replay_bonus_801192A0 = file->lineTable.pointer->count;
        for (lineIndex = 0; lineIndex < D_replay_bonus_801192A0; lineIndex++) {
            line                    = D_replay_bonus_80119298;
            D_replay_bonus_80119298 = line + 1;
            line->cmds.offset      += (s32)file;
        }
    }

    D_replay_bonus_80119290 = file->glyphs.pointer;
    D_replay_bonus_80119294 = file->params.pointer;
    D_replay_bonus_80119298 = file->lineTable.pointer->lines;
    D_replay_bonus_8011929C = file->sprites.pointer;
    return 1;
}

/// Starts one credits picture decode and publishes its page for cross-fading.
///
/// Spawn argument 1 is a loaded picture-bitstream resource slot (0..49), whose
/// expanded stream fits the 240x176x2-byte decoder buffer. Borrows the credits'
/// picture record and VLC table until the decoder ends. Requests during decode
/// or cross-fade are discarded; only one worker may use the shared decoder.
/// Completion swaps the VRAM page and starts a 120-frame drawing countdown.
static void _replayBonusStartCreditsPictureTask(Task* task)
{
    enum { REPLAY_BONUS_PICTURE_START       = 0,
           REPLAY_BONUS_PICTURE_WAIT_DECODE = 1 };
    s32                       decodeResult;
    s32                       pictureState;
    ReplayBonusPictureDecode* picture;

    pictureState = task->state;
    switch (pictureState) {
        case REPLAY_BONUS_PICTURE_START:
            if ((u32)(D_replay_bonus_80119225 - REPLAY_BONUS_PICTURE_DECODING) < (u32)(REPLAY_BONUS_PICTURE_CROSS_FADING - REPLAY_BONUS_PICTURE_DECODING + 1)) {
                taskKill(task);
                break;
            }
            // Decode into the page opposite the currently displayed picture.
            picture                 = D_replay_bonus_801192BC;
            picture->resourceIndex  = task->spawnArg1.value;
            picture->vramX          = REPLAY_BONUS_PICTURE_VRAM_X;
            picture->vramY          = (D_replay_bonus_80119226 ^ 1) << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT;
            picture->width          = REPLAY_BONUS_PICTURE_WIDTH;
            picture->height         = REPLAY_BONUS_PICTURE_HEIGHT;
            D_replay_bonus_80119228 = taskSpawnFromTable(&D_replay_bonus_80118F6C, 0, 0, picture);
            D_replay_bonus_80119225 = REPLAY_BONUS_PICTURE_DECODING;
            task->state            += 1;
            break;
        case REPLAY_BONUS_PICTURE_WAIT_DECODE:
            if (taskPollKill(D_replay_bonus_80119228, &decodeResult) != 0) {
                // Publish the completed page; drawing owns the cross-fade countdown.
                D_replay_bonus_80119227  = REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
                D_replay_bonus_80119226 ^= 1;
                D_replay_bonus_80119225  = REPLAY_BONUS_PICTURE_CROSS_FADING;
                taskKill(task);
            }
            break;
    }
}

/// Fades the credits from black to their full text and picture brightness.
///
/// Spawn argument 1 is a positive duration of at most 32767 callback ticks.
/// Initializes the remaining countdown, then decrements it through zero and
/// kills the worker. Updates shared brightness even on its final callback;
/// only one fade worker may control that brightness at a time.
static void _replayBonusFadeCreditsInTask(Task* task)
{
    enum { REPLAY_BONUS_COUNTDOWN_SIGN_SHIFT = 16 };
    s32 fadeState;
    u16 remainingTicks;

    fadeState = task->state;
    switch (fadeState) {
        case REPLAY_BONUS_FADE_INITIALIZE:
            task->killCountdown = (u16)task->spawnArg1.value;
            task->state        += 1;
            break;
        case REPLAY_BONUS_FADE_RUN:
            remainingTicks      = task->killCountdown - 1;
            task->killCountdown = remainingTicks;
            if ((remainingTicks << REPLAY_BONUS_COUNTDOWN_SIGN_SHIFT) <= 0) {
                taskKill(task);
            }
            break;
    }
    _replayBonusSetCreditsFade(((s32)(task->killCountdown * REPLAY_BONUS_CREDITS_FADE_MAX) / (s32)task->spawnArg1.value) & REPLAY_BONUS_CREDITS_FADE_MAX);
}

/// Fades the credits' text and pictures to black over the requested duration.
///
/// Spawn argument 1 is a positive duration of at most 32767 callback ticks.
/// Initializes an elapsed countdown at zero, then increments to the duration
/// and kills the worker. Updates shared brightness on the final callback;
/// only one fade worker may control that brightness at a time.
static void _replayBonusFadeCreditsOutTask(Task* task)
{
    s32 fadeState;
    u16 elapsedTicks;

    fadeState = task->state;
    switch (fadeState) {
        case REPLAY_BONUS_FADE_INITIALIZE:
            task->killCountdown = 0;
            task->state        += 1;
            break;
        case REPLAY_BONUS_FADE_RUN:
            elapsedTicks        = task->killCountdown + 1;
            task->killCountdown = elapsedTicks;
            if ((s16)elapsedTicks >= task->spawnArg1.value) {
                taskKill(task);
            }
            break;
    }
    _replayBonusSetCreditsFade(((s32)(task->killCountdown * REPLAY_BONUS_CREDITS_FADE_MAX) / (s32)task->spawnArg1.value) & REPLAY_BONUS_CREDITS_FADE_MAX);
}

/// Selects a loaded data resource by ordinal and initializes its credits tables.
///
/// `dataResourceIndex` is zero-based among data-kind slots, rather than an
/// absolute resource-slot index. The caller supplies a writable STF resource
/// that remains loaded throughout credits drawing. A missing ordinal leaves
/// the current file and tables intact; the relocation result is ignored.
static void _replayBonusSelectCreditsResource(s32 dataResourceIndex)
{
    FsResourceSlot*     resourceSlot;
    s32                 dataResourcesSeen;
    s32                 slotIndex;
    s32                 resourceKind;
    ReplayBonusStfFile* file;

    dataResourcesSeen = 0;
    slotIndex         = dataResourcesSeen;
    resourceKind      = FILE_SYSTEM_RESOURCE_DATA;
    do {
        resourceSlot = &D_8006C338[slotIndex];
        if (resourceSlot->kind == resourceKind) {
            if (dataResourcesSeen == dataResourceIndex) {
                file                    = resourceSlot->data;
                D_replay_bonus_8011928C = file;
                _replayBonusRelocateCreditsFile(file, slotIndex);
                return;
            }
            dataResourcesSeen++;
        }
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(D_8006C338));
}

/* The package's data, in address order. */

/// Task descriptor of the picture decoder.
TaskDesc D_replay_bonus_80118F6C = { { { TASK_BODY_NONE, 0xC0 } }, replayBonusDecodePictureTask, { NULL } };

ShopTier D_replay_bonus_80118F78[SHOP_TIER_COUNT] = {
    { 0x38A4, { 0x6D, 0x37, 0x2 } },
    { 0x3E80, { 0x46, 0xA, 0x3A } },
    { 0xABE0, { 0x45, 0x3C, 0xA1 } },
    { 0xC738, { 0x42, 0xD, 0x6 } },
    { 0xDEA8, { 0x43, 0xB, 0x61 } },
    { 0xF230, { 0x44, 0xE, 0x38 } },
    { 0x101D0, { 0x6B, 0xA2, 0x39 } },
    { 0x10D88, { 0x8E, 0xAE, 0xAD } },
    { 0x11940, { 0x88, 0xA6, 0x36 } },
    { 0x124F8, { 0x90, 0xA7, 0x5 } },
    { 0x30D40, { 0x8B, 0xAA, 0x3 } },
    { 0x61A80, { 0x95, 0x3F, 0x7 } },
    { 0x7FFFFFFF, { 0x96, 0x3D, 0x3E } },
};

u8 D_replay_bonus_80119014[] = "You will lose the game clear\ndata if you quit now. Quit now\nand delete game clear data?";

u8 D_replay_bonus_8011906C[] = "data if you quit now. Quit now";

u16 D_replay_bonus_8011908C[80] = {
    0x9D,
    0x81,
    0x92,
    0x99,
    0x9A,
    0x8D,
    0x9C,
    0x9B,
    0x83,
    0x82,
    0x8A,
    0x127,
    0x88,
    0x84,
    0x8B,
    0x8C,
    0x8E,
    0x8F,
    0x90,
    0x96,
    0x107,
    0x98,
    0x95,
    0x80,
    0x9E,
    0x9F,
    0x93,
    0x94,
    0x61,
    0x62,
    0x69,
    0x64,
    0x63,
    0x6B,
    0x68,
    0x6A,
    0x67,
    0x65,
    0x60,
    0x66,
    0x6D,
    0x1,
    0x2,
    0x3,
    0x6,
    0x5,
    0x7,
    0x4,
    0x8,
    0xD,
    0xC,
    0x9,
    0xA,
    0x3A,
    0x3B,
    0x36,
    0x37,
    0x38,
    0x39,
    0x3C,
    0x3D,
    0x3E,
    0x3F,
    0xB,
    0xE,
    0x40,
    0x41,
    0x42,
    0x43,
    0x44,
    0x45,
    0x46,
    0x105,
    0x121,
    0x122,
    0x124,
    0x12F,
    0x130,
    0xFFFF,
    0x0,
};

UiListRowCallback D_replay_bonus_8011912C[1] = { _replayBonusDrawCurrentItemRow };

UiList D_replay_bonus_80119130 = { D_replay_bonus_8011912C, 1, { 1 }, 0, 0xF };

UiObjectDesc D_replay_bonus_80119154 = { 2, { -144, -104, 208, 160 }, 0x3C, 0, 0, 0xC0, replayBonusCompleteBonusPanelTask, 0 };

UiObjectDesc D_replay_bonus_80119170 = { 2, { -144, -96, 160, 160 }, 0x3C, 0, 0, 0xC0, replayBonusQuitWarningTask, 0 };

UiObjectDesc D_replay_bonus_8011918C = { 2, { -72, -64, 144, 56 }, 0x30, 0, 0, 0xC0, replayBonusBalancePanelTask, 0 };

UiObjectDesc D_replay_bonus_801191A8 = { 2, { -72, 8, 144, 56 }, 0x34, 0, 0, 0xC0, replayBonusBalancePanelTask, 0 };

UiObjectDesc D_replay_bonus_801191C4 = { 2, { -144, -104, 288, 208 }, 0x2C, 0, 0, 0xC0, replayBonusUnlockedItemPanelTask, 0 };

UiObjectDesc D_replay_bonus_801191E0 = { 2, { -144, -104, 288, 208 }, 0x28, 0, 0, 0xC0, replayBonusUnlockedItemPanelTask, 0 };

UiObjectDesc D_replay_bonus_801191FC = { 2, { -72, -32, 144, 40 }, 0x2C, 0, 0, 0xC0, replayBonusExtraBpPanelTask, 0 };

TaskDesc D_replay_bonus_80119218 = { { { TASK_BODY_NONE, 0xC0 } }, _replayBonusAwardScreenTask, { NULL } };

u8 D_replay_bonus_80119224 = 0;

u8 D_replay_bonus_80119225 = 0;

u8 D_replay_bonus_80119226 = 0;

u8 D_replay_bonus_80119227 = 0;

Task* D_replay_bonus_80119228 = NULL;

TaskDesc D_replay_bonus_8011922C[4] = {
    { { { TASK_BODY_NONE, 0x20 } }, _replayBonusCreditsTask, { NULL } },
    { { { TASK_BODY_NONE, 0x20 } }, _replayBonusFadeCreditsInTask, { NULL } },
    { { { TASK_BODY_NONE, 0x20 } }, _replayBonusFadeCreditsOutTask, { NULL } },
    { { { TASK_BODY_NONE, 0x20 } }, _replayBonusStartCreditsPictureTask, { NULL } },
};

u8* D_replay_bonus_8011925C = NULL;

u_long* D_replay_bonus_80119260 = NULL;

s16 D_replay_bonus_80119264 = 0;

u16 D_replay_bonus_80119266 = 0;

u16 D_replay_bonus_80119268 = 0;

u16 D_replay_bonus_8011926A = 0;

s16 D_replay_bonus_8011926C = 0;

s16 D_replay_bonus_8011926E = 0;

u16 D_replay_bonus_80119270 = 0;

/// Not zero and never read: the original toolchain left these two bytes in the
/// alignment gap before the totals.
u8 D_replay_bonus_80119272 = 0x43;

u8 D_replay_bonus_80119273 = 0x42;

ReplayBonusTotals D_replay_bonus_80119274 = { 0, 0, 0, 0, 0, 0 };

ReplayBonusStfFile* D_replay_bonus_8011928C = NULL;

ReplayBonusStfGlyph* D_replay_bonus_80119290 = NULL;

ReplayBonusStfParams* D_replay_bonus_80119294 = NULL;

ReplayBonusStfLine* D_replay_bonus_80119298 = NULL;

ReplayBonusStfSprite* D_replay_bonus_8011929C = NULL;

s32 D_replay_bonus_801192A0 = 0;

s32 D_replay_bonus_801192A4 = 0;

s32 D_replay_bonus_801192A8 = 0;

u8 D_replay_bonus_801192AC = 0;

s32 D_replay_bonus_801192B0 = 0;

s32 D_replay_bonus_801192B4 = 0;

u16 D_replay_bonus_801192B8 = 0;

ReplayBonusPictureDecode* D_replay_bonus_801192BC = NULL;

u8* D_replay_bonus_801192C0 = NULL;
