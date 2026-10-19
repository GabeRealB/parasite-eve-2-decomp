#include "replay_bonus_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

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

static const TaskDesc              _gReplayBonusPictureDecoderTaskDescriptor;
static const UiObjectDesc          _gReplayBonusCompleteBonusPanelDescriptor;
static const UiObjectDesc          _gReplayBonusQuitWarningPanelDescriptor;
static const UiObjectDesc          _gReplayBonusBalancePanelDescriptor;
static const UiObjectDesc          _gReplayBonusFirstAndThirdShopItemPanelDescriptor;
static const UiObjectDesc          _gReplayBonusSecondShopItemPanelDescriptor;
static const UiObjectDesc          _gReplayBonusExtraBpPanelDescriptor;
static u8                          _gReplayBonusCreditsPicturePhase;
static u8                          _gReplayBonusCreditsPicturePageIndex;
static u8                          _gReplayBonusCreditsPictureCrossFadeFramesLeft;
static Task*                       _gReplayBonusPictureDecoderTask;
static const ReplayBonusStfParams* _gReplayBonusCreditsParams;
static u8                          _gReplayBonusCreditsBrightness;
static ReplayBonusPictureDecode*   _gReplayBonusCreditsPictureRequest;

static u8                          _gReplayBonusNextCreditsPrimitiveBufferIndex;
static ReplayBonusStfFile*         _gReplayBonusCreditsFile;
static const ReplayBonusStfGlyph*  _gReplayBonusCreditsGlyphs;
static ReplayBonusStfLine*         _gReplayBonusCreditsLines;
static const ReplayBonusStfSprite* _gReplayBonusCreditsSprites;
static s32                         _gReplayBonusCreditsLineCount;
static s32                         _gReplayBonusCreditsScrollY;
static s32                         _gReplayBonusCreditsScrollFraction;
static s32                         _gReplayBonusCreditsDrawFrameCount;
static s32                         _gReplayBonusCreditsPrimitiveBytesUsed;
static u16                         _gReplayBonusCreditsPictureLeftX;
static u8*                         _gReplayBonusCreditsPrimitiveBuffer;

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
                task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusBalancePanelDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_BALANCE:
                if (gReplayBonusTotals.shopTier < 0) {
                    task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusExtraBpPanelDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                    task->state             = task->state + 2;
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusFirstAndThirdShopItemPanelDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                }
                break;
            case REPLAY_BONUS_SCREEN_FIRST_SHOP_ITEM:
                task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusSecondShopItemPanelDescriptor, 1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_SECOND_SHOP_ITEM:
                task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusFirstAndThirdShopItemPanelDescriptor, 2, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_LAST_AWARD:
                displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                replayBonusPrepareClearedSave();
                gDisplayState.gameMode  = DISPLAY_GAME_MODAL;
                task->spawnArg2.pointer = uiSpawnObject(&gMcDialogDescriptors[MEMORY_CARD_DIALOG_SAVE], 0, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
                break;
            case REPLAY_BONUS_SCREEN_SAVE:
                if (commandResult == USER_INTERFACE_LIST_COMMAND_YES) {
                    task->spawnArg2.pointer = itemMenuSpawnNotice(NULL, ITEM_MENU_NOTICE_SAVE_COMPLETE, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                    task->state             = task->state + 1;
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusQuitWarningPanelDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, 2, NULL);
                }
                break;
            case REPLAY_BONUS_SCREEN_QUIT_WARNING:
                if (commandResult == USER_INTERFACE_LIST_COMMAND_YES) {
                    task->spawnArg2.pointer = itemMenuSpawnNotice(NULL, ITEM_MENU_NOTICE_SAVE_CANCELLED, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                } else {
                    task->spawnArg2.pointer = uiSpawnObject(&gMcDialogDescriptors[MEMORY_CARD_DIALOG_SAVE], 1, USER_INTERFACE_PANEL_ACTIVE, 1, NULL);
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
/// save and catalog without changing their unlocks. An EXP value beyond
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
    return gReplayBonusShopTierLadder[selectedTier].items[itemColumn];
}

/// Returns 1 when an item id belongs to the Complete Bonus whitelist, otherwise 0.
///
/// Checks the 78 stored ids, excluding the trailing terminator. This tests
/// eligibility alone, without checking possession, quantity or identification.
static s32 _replayBonusIsListedItem(s32 itemId)
{
    const u16* whitelistItem;
    s32        whitelistIndex;

    whitelistItem  = gReplayBonusItemWhitelist;
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

#include "replay_bonus_total_bp.inc.c"

/// Returns the BP balance plus credits for the displayed list suffix.
///
/// Sums from the first visible item through the list's end and caps at eight
/// decimal digits. The owner's live work is a borrowed `s16` item-id array;
/// the row range must fit it and each id must meet `_replayBonusItemBp`'s
/// catalog bounds. Neither inventory nor the BP balance is changed.
static s32 _replayBonusGetListTotalBp(const UiList* list, const UiObject* object)
{
    return _replayBonusSumTotalBp(list, object);
}

/// Draws a Complete Bonus item's name and half-price BP credit, identifying it.
///
/// Requires a live list and UI object and a whitelisted catalog id. Row
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
/// Counts all three items of every tier, including any repeated catalog id.
/// Requires the gameplay item catalog to remain loaded; grants no currency.
static s32 _replayBonusGetShopCatalogBp(void)
{
    enum { REPLAY_BONUS_SHOP_BP_ROUNDING_UNIT = 100000 };
    const ShopTier* tier;
    s32             itemColumn;
    s32             totalBp;
    s32             tierIndex;

    totalBp   = 0;
    tier      = gReplayBonusShopTierLadder;
    tierIndex = totalBp;
    do {
        itemColumn = 0;
        do {
            totalBp += gItemDescriptors[tier->items[itemColumn]].price;
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
        task->spawnArg2.pointer = uiSpawnObject(&_gReplayBonusCompleteBonusPanelDescriptor, 0, USER_INTERFACE_PANEL_ACTIVE, REPLAY_BONUS_PANEL_OPEN_DELAY_TICKS, NULL);
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
/// Requires the replay package, gameplay catalog and live save to remain live, with
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
        REPLAY_BONUS_CREDITS_FADE_TICKS             = 180,
        REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS        = 30,
        REPLAY_BONUS_CREDITS_SCROLL_FRACTION_BITS   = 8,
        REPLAY_BONUS_CREDITS_SCROLL_FRACTION_MASK   = 0xFF,
        REPLAY_BONUS_CREDITS_AUDIO_GROUP            = 0,
        REPLAY_BONUS_CREDITS_AUDIO_STREAM_ID        = 1,
        REPLAY_BONUS_CREDITS_AUDIO_SUB_ID           = 11,
    };

    s32                         nextScrollY;
    s32                         scrollFixedPoint;
    s32                         lastScrollY;
    u16*                        vlcTable;
    u16                         startHoldTicksLeft;
    u16                         endHoldTicksLeft;
    const ReplayBonusStfParams* creditsParams;

    switch (task->state) {
        case REPLAY_BONUS_CREDITS_INITIALIZE:
            // Keep both picture pages black until the first decode and fade-in.
            _gReplayBonusCreditsPicturePageIndex           = 0;
            _gReplayBonusCreditsPictureCrossFadeFramesLeft = 0;
            _gReplayBonusCreditsBrightness                 = 0;
            _gReplayBonusCreditsPictureRequest             = memCalloc(sizeof(ReplayBonusPictureDecode), false);
            vlcTable                                       = replayBonusCreatePictureVlcTable();
            _gReplayBonusPictureDecoderTask                = NULL;
            _gReplayBonusCreditsPicturePhase               = REPLAY_BONUS_PICTURE_IDLE;
            _gReplayBonusCreditsPictureRequest->vlcTable   = vlcTable;
            _replayBonusSelectCreditsResource(0);
            displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            SetDispMask(true);
            displayConfigureFramebuffers(REPLAY_BONUS_CREDITS_DISPLAY_SETUP);
            _replayBonusClearCreditsPictureBuffers();
            _gReplayBonusCreditsScrollY             = -REPLAY_BONUS_CREDITS_VIEWPORT_HEIGHT_PIXELS;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            _gReplayBonusCreditsDrawFrameCount      = 0;
            task->killCountdown                     = _gReplayBonusCreditsParams->startHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
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
            _gReplayBonusCreditsDrawFrameCount += 1;
            _replayBonusDrawCreditsFrame();
            startHoldTicksLeft  = task->killCountdown - 1;
            task->killCountdown = startHoldTicksLeft;
            if ((s16)startHoldTicksLeft <= 0) {
                taskSpawnFromTable(gReplayBonusCreditsTaskDescriptors, REPLAY_BONUS_CREDITS_FADE_IN_TASK_INDEX, REPLAY_BONUS_CREDITS_FADE_TICKS, 0);
                task->state = REPLAY_BONUS_CREDITS_SCROLL;
            }
            if (padIsStartPressed() != 0) {
                taskSpawnFromTable(gReplayBonusCreditsTaskDescriptors, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS, 0);
                cdCmdCancelScene();
                task->state         = REPLAY_BONUS_CREDITS_END_HOLD;
                task->killCountdown = REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_SCROLL:
            _gReplayBonusCreditsDrawFrameCount += 1;
            _replayBonusDrawCreditsFrame();
            // Carry whole pixels into document Y, retaining the fractional byte.
            creditsParams                      = _gReplayBonusCreditsParams;
            scrollFixedPoint                   = _gReplayBonusCreditsScrollFraction + creditsParams->scrollSpeed;
            nextScrollY                        = _gReplayBonusCreditsScrollY + (scrollFixedPoint >> REPLAY_BONUS_CREDITS_SCROLL_FRACTION_BITS);
            _gReplayBonusCreditsScrollFraction = scrollFixedPoint;
            _gReplayBonusCreditsScrollY        = nextScrollY;
            _gReplayBonusCreditsScrollFraction = scrollFixedPoint & REPLAY_BONUS_CREDITS_SCROLL_FRACTION_MASK;
            lastScrollY                        = _gReplayBonusCreditsLines[_gReplayBonusCreditsLineCount - 1].y - REPLAY_BONUS_CREDITS_VIEWPORT_HEIGHT_PIXELS;
            if (lastScrollY < nextScrollY) {
                _gReplayBonusCreditsScrollY = lastScrollY;
                task->state                += 1;
                task->killCountdown         = creditsParams->endHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
            }
            if (padIsStartPressed() != 0) {
                cdCmdCancelScene();
                taskSpawnFromTable(gReplayBonusCreditsTaskDescriptors, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS, 0);
                task->killCountdown = REPLAY_BONUS_CREDITS_SKIP_FADE_TICKS;
                task->state        += 1;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_END_HOLD:
            _gReplayBonusCreditsDrawFrameCount += 1;
            _replayBonusDrawCreditsFrame();
            endHoldTicksLeft    = task->killCountdown - 1;
            task->killCountdown = endHoldTicksLeft;
            if ((s16)endHoldTicksLeft == REPLAY_BONUS_CREDITS_FADE_TICKS) {
                taskSpawnFromTable(gReplayBonusCreditsTaskDescriptors, REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX, REPLAY_BONUS_CREDITS_FADE_TICKS, 0);
            }
            if (task->killCountdown <= 0) {
                task->state = REPLAY_BONUS_CREDITS_WAIT_PICTURE;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_WAIT_PICTURE:
            // The request record remains borrowed until the picture worker finishes.
            if (_gReplayBonusCreditsPicturePhase != REPLAY_BONUS_PICTURE_DECODING) {
                SetDispMask(false);
                task->state += 1;
                return;
            }
            break;
        case REPLAY_BONUS_CREDITS_RESTORE_DISPLAY:
            streamFinishScene();
            // The separate VLC allocation is retired by the post-awards restart.
            memFree(_gReplayBonusCreditsPictureRequest);
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

    _gReplayBonusCreditsBrightness = REPLAY_BONUS_CREDITS_MAX_BRIGHTNESS - (fadeAmount >> REPLAY_BONUS_CREDITS_FADE_SHIFT);
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
    _gReplayBonusCreditsPrimitiveBytesUsed        = 0;
    _gReplayBonusCreditsPrimitiveBuffer           = gGpuPrimitiveBufferBase + gGpuPrimitiveBufferBytes + _gReplayBonusNextCreditsPrimitiveBufferIndex * REPLAY_BONUS_CREDITS_PRIMITIVE_BUFFER_BYTES;
    _gReplayBonusNextCreditsPrimitiveBufferIndex ^= 1;
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
    lineCount = _gReplayBonusCreditsLineCount;
    _replayBonusSelectCreditsPrimitiveBuffer();
    lastLine = lineCount - 1;

    // Retain up to two rows above the viewport and the first below its bottom edge.
    if (lineCount > 0) {
        scanLineCount  = lineCount;
        scanLine       = _gReplayBonusCreditsLines;
        scrollY        = _gReplayBonusCreditsScrollY;
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
        rowGuide                                = (LINE_F2*)(_gReplayBonusCreditsPrimitiveBuffer + _gReplayBonusCreditsPrimitiveBytesUsed);
        _gReplayBonusCreditsPrimitiveBytesUsed += sizeof(*rowGuide);
        // Retained row-guide packets consume space but are never linked for drawing.
        setLineF2(rowGuide);
        bottomY      = _gReplayBonusCreditsLines[lineIndex].y - (_gReplayBonusCreditsScrollY & REPLAY_BONUS_CREDITS_SCROLL_Y_MASK) - REPLAY_BONUS_CREDITS_HALF_HEIGHT;
        rowGuide->x0 = -REPLAY_BONUS_CREDITS_HALF_WIDTH;
        rowGuide->x1 = REPLAY_BONUS_CREDITS_HALF_WIDTH;
        rowGuide->r0 = 0x40;
        rowGuide->g0 = 0x80;
        rowGuide->b0 = 0x40;
        rowGuide->y0 = bottomY;
        rowGuide->y1 = bottomY;
        _replayBonusDrawCreditsRow(bottomY, _gReplayBonusCreditsLines[lineIndex].cmds.pointer);
    }

    drawPage       = gGpuPrimCursor;
    gGpuPrimCursor = drawPage + 1;
    setDrawTPage(drawPage, 0, 1, 0);
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, drawPage);

    if (_gReplayBonusCreditsPicturePhase != REPLAY_BONUS_PICTURE_CROSS_FADING) {
        pictureSprite  = gGpuPrimCursor;
        gGpuPrimCursor = pictureSprite + 1;
        setSprt(pictureSprite);
        pictureSprite->x0 = _gReplayBonusCreditsPictureLeftX - REPLAY_BONUS_CREDITS_HALF_WIDTH;
        pictureSprite->y0 = REPLAY_BONUS_PICTURE_SCREEN_TOP;
        pictureSprite->w  = REPLAY_BONUS_PICTURE_WIDTH;
        pictureSprite->h  = REPLAY_BONUS_PICTURE_HEIGHT;
        pictureSprite->r0 = _gReplayBonusCreditsBrightness;
        pictureSprite->g0 = _gReplayBonusCreditsBrightness;
        pictureSprite->b0 = _gReplayBonusCreditsBrightness;
        pictureSprite->u0 = 0;
        pictureSprite->v0 = 0;
        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, pictureSprite);

        drawPage       = gGpuPrimCursor;
        gGpuPrimCursor = drawPage + 1;
        setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, 0, REPLAY_BONUS_PICTURE_VRAM_X, _gReplayBonusCreditsPicturePageIndex << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);
        return;
    }

    // Blend the newly decoded page over the outgoing page for 120 frames.
    pictureSprite  = gGpuPrimCursor;
    gGpuPrimCursor = pictureSprite + 1;
    setSprt(pictureSprite);
    pictureBrightness = (_gReplayBonusCreditsBrightness * (REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES - _gReplayBonusCreditsPictureCrossFadeFramesLeft)) / REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
    pictureSprite->x0 = _gReplayBonusCreditsPictureLeftX - REPLAY_BONUS_CREDITS_HALF_WIDTH;
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
    setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, REPLAY_BONUS_PICTURE_BLEND_ADDITIVE, REPLAY_BONUS_PICTURE_VRAM_X, _gReplayBonusCreditsPicturePageIndex << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);

    pictureSprite  = gGpuPrimCursor;
    gGpuPrimCursor = pictureSprite + 1;
    setSprt(pictureSprite);
    pictureBrightness = (_gReplayBonusCreditsBrightness * _gReplayBonusCreditsPictureCrossFadeFramesLeft) / REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
    pictureSprite->x0 = _gReplayBonusCreditsPictureLeftX - REPLAY_BONUS_CREDITS_HALF_WIDTH;
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
    setDrawTPage(drawPage, 0, 1, getTPage(REPLAY_BONUS_PICTURE_TEXTURE_16_BIT, 0, REPLAY_BONUS_PICTURE_VRAM_X, (_gReplayBonusCreditsPicturePageIndex ^ 1) << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT));
    addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_PICTURE_OT_INDEX, drawPage);

    crossFadeFramesLeft                            = _gReplayBonusCreditsPictureCrossFadeFramesLeft - 1;
    _gReplayBonusCreditsPictureCrossFadeFramesLeft = crossFadeFramesLeft;
    if (!(crossFadeFramesLeft & 0xFF)) {
        _gReplayBonusCreditsPicturePhase = REPLAY_BONUS_PICTURE_IDLE;
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
        REPLAY_BONUS_STF_ANCHOR_CENTER       = 0,
        REPLAY_BONUS_STF_ANCHOR_LEFT         = 1,
        REPLAY_BONUS_STF_ANCHOR_RIGHT        = 2,
        REPLAY_BONUS_STF_DEFAULT_PALETTE     = 7,
        REPLAY_BONUS_STF_PALETTES_PER_ROW    = 4,
        REPLAY_BONUS_STF_CLUT_FIRST_Y        = 510,
        REPLAY_BONUS_STF_CLUT_FIRST_X        = 896,
        REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS = 256,
        REPLAY_BONUS_STF_PIXEL_MODE_8_BIT    = 1,
        REPLAY_BONUS_STF_PIXEL_MODE_16_BIT   = 2,
        REPLAY_BONUS_STF_PALETTE_COLORS      = 16,
        REPLAY_BONUS_STF_FONT_PAGE0_X        = 896,
        REPLAY_BONUS_STF_FONT_PAGE0_Y        = 256,
        REPLAY_BONUS_STF_FONT_PAGE1_X        = 960,
        REPLAY_BONUS_STF_FONT_PAGE1_Y        = 0,
        REPLAY_BONUS_STF_VRAM_X_MASK         = 0x3FF,
        REPLAY_BONUS_STF_VRAM_PAGE_X_SHIFT   = 6,
    };
    /// Sets a credits text, picture or sprite run's left edge from its column anchor.
    ///
    /// Captures the live relocated `_gReplayBonusCreditsParams` and this function's
    /// REPLAY_BONUS_STF_ANCHOR_* constants. `columnIndex` is 0..7; `anchorMode`'s
    /// low byte selects 0 centre, 1 left or 2 right. Other modes leave the output
    /// unchanged, so callers initialize their independent s16 `runLeftX` to zero.
    /// Inputs are side-effect-free scalars, each read once on the selected path.
    /// Width is a pixel span; anchors are pixels from the 640-pixel screen's left edge.
    /// Only centring narrows width to s16, then halves it toward zero; every
    /// output narrows to s16. Drawing later subtracts the screen's half-width.
#define REPLAY_BONUS_SET_CREDITS_RUN_LEFT_X(columnIndex, anchorMode, widthPixels, runLeftX)                         \
    {                                                                                                               \
        switch ((u8)(anchorMode)) {                                                                                 \
            case REPLAY_BONUS_STF_ANCHOR_CENTER:                                                                    \
                (runLeftX) = _gReplayBonusCreditsParams->columns[(columnIndex)].centerX - ((s16)(widthPixels) / 2); \
                break;                                                                                              \
            case REPLAY_BONUS_STF_ANCHOR_LEFT:                                                                      \
                (runLeftX) = _gReplayBonusCreditsParams->columns[(columnIndex)].leftX;                              \
                break;                                                                                              \
            case REPLAY_BONUS_STF_ANCHOR_RIGHT:                                                                     \
                (runLeftX) = _gReplayBonusCreditsParams->columns[(columnIndex)].rightX - (widthPixels);             \
                break;                                                                                              \
        }                                                                                                           \
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
                        drawWidth += _gReplayBonusCreditsGlyphs[commands[commandIndex + glyphOffset].arg].width;
                        glyphOffset++;
                    }
                    penX = 0;
                    REPLAY_BONUS_SET_CREDITS_RUN_LEFT_X(columnIndex, anchorMode, drawWidth, penX);
                    penX       -= REPLAY_BONUS_CREDITS_HALF_WIDTH;
                    glyphOffset = 0;
                    while (commands[commandIndex + glyphOffset].op == REPLAY_BONUS_STF_COMMAND_GLYPH) {
                        glyph                                   = &_gReplayBonusCreditsGlyphs[commands[commandIndex + glyphOffset].arg];
                        drawWidth                               = glyph->width;
                        heightValue                             = glyph->heightAndPage;
                        textureU                                = glyph->u;
                        textureV                                = glyph->v;
                        glyphQuad                               = (POLY_FT4*)(_gReplayBonusCreditsPrimitiveBuffer + _gReplayBonusCreditsPrimitiveBytesUsed);
                        _gReplayBonusCreditsPrimitiveBytesUsed += sizeof(*glyphQuad);
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
                        glyphQuad->r0     = _gReplayBonusCreditsBrightness;
                        glyphQuad->x0     = penX;
                        glyphQuad->x2     = penX;
                        glyphQuad->y2     = bottomY;
                        glyphQuad->y3     = bottomY;
                        glyphQuad->g0     = _gReplayBonusCreditsBrightness;
                        glyphQuad->b0     = _gReplayBonusCreditsBrightness;
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
                        REPLAY_BONUS_SET_CREDITS_RUN_LEFT_X(columnIndex, anchorMode, REPLAY_BONUS_PICTURE_WIDTH, penX);
                        _gReplayBonusCreditsPictureLeftX = penX;
                        taskSpawnFromTable(gReplayBonusCreditsTaskDescriptors, REPLAY_BONUS_CREDITS_PICTURE_TASK_INDEX, imageIndex + 1, 0);
                    }
                    break;
                case REPLAY_BONUS_STF_COMMAND_SPRITE:
                    imageIndex = commands[commandIndex].arg;
                    drawWidth  = _gReplayBonusCreditsSprites[imageIndex].width;
                    penX       = 0;
                    REPLAY_BONUS_SET_CREDITS_RUN_LEFT_X(columnIndex, anchorMode, drawWidth, penX);
                    pixelsPerVramWordShift = 2;
                    texturePageX           = _gReplayBonusCreditsSprites[imageIndex].tpageX;
                    textureU               = _gReplayBonusCreditsSprites[imageIndex].u;
                    // The chained V load retains the matching allocation of shared height storage.
                    textureV = heightValue = _gReplayBonusCreditsSprites[imageIndex].v;
                    heightValue            = _gReplayBonusCreditsSprites[imageIndex].height;
                    clutId                 = getClut(_gReplayBonusCreditsSprites[imageIndex].clutX, _gReplayBonusCreditsSprites[imageIndex].clutY);
                    pixelMode              = _gReplayBonusCreditsSprites[imageIndex].pixelMode;
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
                        imageSprite->r0   = _gReplayBonusCreditsBrightness;
                        imageSprite->x0   = penX - REPLAY_BONUS_CREDITS_HALF_WIDTH;
                        imageSprite->g0   = _gReplayBonusCreditsBrightness;
                        imageSprite->b0   = _gReplayBonusCreditsBrightness;
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
                        drawPixelMode     = _gReplayBonusCreditsSprites[imageIndex].pixelMode;
                        texturePageColumn = (u32)(texturePageX & REPLAY_BONUS_STF_VRAM_X_MASK) >> REPLAY_BONUS_STF_VRAM_PAGE_X_SHIFT;
                        drawPage->code[0] = ((drawPixelMode & 3) << 7) | (s16)((s32)((_gReplayBonusCreditsSprites[imageIndex].tpageY & 0x100) << 16) >> 20) | texturePageColumn | ((s16)(_gReplayBonusCreditsSprites[imageIndex].tpageY & 0x200) * 4) | _get_mode(0, 1, 0);
                        texturePageX     += REPLAY_BONUS_STF_TEXTURE_PAGE_PIXELS >> pixelsPerVramWordShift;
                        penX              = penX + pieceWidth;
                        addPrim(gGpuCurrentOt + REPLAY_BONUS_CREDITS_TEXT_OT_INDEX, drawPage);
                    }
                    break;
            }
            commandIndex++;
        } while (commands[commandIndex].op != REPLAY_BONUS_STF_COMMAND_END);
    }
#undef REPLAY_BONUS_SET_CREDITS_RUN_LEFT_X
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
        file->glyphs.offset          += (s32)file;
        file->params.offset          += (s32)file;
        file->lineTable.offset       += (s32)file;
        file->sprites.offset         += (s32)file;
        _gReplayBonusCreditsLines     = file->lineTable.pointer->lines;
        _gReplayBonusCreditsLineCount = file->lineTable.pointer->count;
        for (lineIndex = 0; lineIndex < _gReplayBonusCreditsLineCount; lineIndex++) {
            line                      = _gReplayBonusCreditsLines;
            _gReplayBonusCreditsLines = line + 1;
            line->cmds.offset        += (s32)file;
        }
    }

    _gReplayBonusCreditsGlyphs  = file->glyphs.pointer;
    _gReplayBonusCreditsParams  = file->params.pointer;
    _gReplayBonusCreditsLines   = file->lineTable.pointer->lines;
    _gReplayBonusCreditsSprites = file->sprites.pointer;
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
            if ((u32)(_gReplayBonusCreditsPicturePhase - REPLAY_BONUS_PICTURE_DECODING) < (u32)(REPLAY_BONUS_PICTURE_CROSS_FADING - REPLAY_BONUS_PICTURE_DECODING + 1)) {
                taskKill(task);
                break;
            }
            // Decode into the page opposite the currently displayed picture.
            picture                          = _gReplayBonusCreditsPictureRequest;
            picture->resourceIndex           = task->spawnArg1.value;
            picture->vramX                   = REPLAY_BONUS_PICTURE_VRAM_X;
            picture->vramY                   = (_gReplayBonusCreditsPicturePageIndex ^ 1) << REPLAY_BONUS_PICTURE_VRAM_Y_SHIFT;
            picture->width                   = REPLAY_BONUS_PICTURE_WIDTH;
            picture->height                  = REPLAY_BONUS_PICTURE_HEIGHT;
            _gReplayBonusPictureDecoderTask  = taskSpawnFromTable(&_gReplayBonusPictureDecoderTaskDescriptor, 0, 0, picture);
            _gReplayBonusCreditsPicturePhase = REPLAY_BONUS_PICTURE_DECODING;
            task->state                     += 1;
            break;
        case REPLAY_BONUS_PICTURE_WAIT_DECODE:
            if (taskPollKill(_gReplayBonusPictureDecoderTask, &decodeResult) != 0) {
                // Publish the completed page; drawing owns the cross-fade countdown.
                _gReplayBonusCreditsPictureCrossFadeFramesLeft = REPLAY_BONUS_PICTURE_CROSS_FADE_FRAMES;
                _gReplayBonusCreditsPicturePageIndex          ^= 1;
                _gReplayBonusCreditsPicturePhase               = REPLAY_BONUS_PICTURE_CROSS_FADING;
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
        resourceSlot = &gFsResourceSlots[slotIndex];
        if (resourceSlot->kind == resourceKind) {
            if (dataResourcesSeen == dataResourceIndex) {
                file                     = resourceSlot->data;
                _gReplayBonusCreditsFile = file;
                _replayBonusRelocateCreditsFile(file, slotIndex);
                return;
            }
            dataResourcesSeen++;
        }
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(gFsResourceSlots));
}

/* The package's data, in address order. */

enum {
    REPLAY_BONUS_PICTURE_DECODER_TASK_PRIORITY = 192,
    REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY     = 192,
    REPLAY_BONUS_CREDITS_TASK_PRIORITY         = 32,
    REPLAY_BONUS_COMPLETE_BONUS_ROW_HEIGHT     = 15,
};

/// Bodyless priority-192 worker that decodes one borrowed credits picture.
///
/// Spawn argument 2 points to the live decode request; argument 1 is unused.
/// The request, VLC table and resource bitstream must survive worker completion.
/// Only one worker may use the shared MDEC workspace at a time.
static const TaskDesc _gReplayBonusPictureDecoderTaskDescriptor SECTION(".data") = {
    .header   = { .fields = { TASK_BODY_NONE, REPLAY_BONUS_PICTURE_DECODER_TASK_PRIORITY } },
    .callback = replayBonusDecodePictureTask,
    .data     = { .model = NULL },
};

/// Catalog ids stored in this package's item whitelist and shop ladder.
///
/// Ordinary ids index the gameplay catalog; collection ids index its key
/// catalog. Their stored representations remain unsigned/signed halfwords.
enum {
    REPLAY_BONUS_ITEM_RECOVERY_1         = 0x1,
    REPLAY_BONUS_ITEM_RECOVERY_2         = 0x2,
    REPLAY_BONUS_ITEM_RECOVERY_3         = 0x3,
    REPLAY_BONUS_ITEM_STIM               = 0x4,
    REPLAY_BONUS_ITEM_COLA               = 0x5,
    REPLAY_BONUS_ITEM_MP_BOOST_1         = 0x6,
    REPLAY_BONUS_ITEM_MP_BOOST_2         = 0x7,
    REPLAY_BONUS_ITEM_PENICILLIN         = 0x8,
    REPLAY_BONUS_ITEM_SMG_CLIP_HOLDER    = 0x9,
    REPLAY_BONUS_ITEM_RIFLE_CLIP_HOLDER  = 0xa,
    REPLAY_BONUS_ITEM_LIPSTICK           = 0xb,
    REPLAY_BONUS_ITEM_SNAIL_MAGAZINE     = 0xc,
    REPLAY_BONUS_ITEM_BELT_POUCH         = 0xd,
    REPLAY_BONUS_ITEM_MD_PLAYER          = 0xe,
    REPLAY_BONUS_ITEM_SKULL_CRYSTAL      = 0x36,
    REPLAY_BONUS_ITEM_MEDICINE_WHEEL     = 0x37,
    REPLAY_BONUS_ITEM_HOLY_WATER         = 0x38,
    REPLAY_BONUS_ITEM_OFUDA              = 0x39,
    REPLAY_BONUS_ITEM_FLARE              = 0x3a,
    REPLAY_BONUS_ITEM_PEPPER_SPRAY       = 0x3b,
    REPLAY_BONUS_ITEM_PROTEIN_CAPSULE    = 0x3c,
    REPLAY_BONUS_ITEM_RINGERS_SOLUTION   = 0x3d,
    REPLAY_BONUS_ITEM_EAU_DE_TOILETTE    = 0x3e,
    REPLAY_BONUS_ITEM_HUNTER_GOGGLES     = 0x3f,
    REPLAY_BONUS_ITEM_GPS                = 0x40,
    REPLAY_BONUS_ITEM_COMBAT_LIGHT       = 0x41,
    REPLAY_BONUS_ITEM_HAMMER             = 0x42,
    REPLAY_BONUS_ITEM_PYKE               = 0x43,
    REPLAY_BONUS_ITEM_JAVELIN            = 0x44,
    REPLAY_BONUS_ITEM_M203               = 0x45,
    REPLAY_BONUS_ITEM_M9                 = 0x46,
    REPLAY_BONUS_ITEM_LEATHER_JACKET     = 0x60,
    REPLAY_BONUS_ITEM_TACTICAL_ARMOR     = 0x61,
    REPLAY_BONUS_ITEM_COMBAT_ARMOR       = 0x62,
    REPLAY_BONUS_ITEM_ASSAULT_SUIT       = 0x63,
    REPLAY_BONUS_ITEM_PASGT_VEST         = 0x64,
    REPLAY_BONUS_ITEM_TACTICAL_VEST      = 0x65,
    REPLAY_BONUS_ITEM_EOD_SUIT           = 0x66,
    REPLAY_BONUS_ITEM_TURTLE_VEST        = 0x67,
    REPLAY_BONUS_ITEM_CHICKEN_PLATE      = 0x68,
    REPLAY_BONUS_ITEM_NBC_SUIT           = 0x69,
    REPLAY_BONUS_ITEM_PSY_SUIT           = 0x6a,
    REPLAY_BONUS_ITEM_AYA_SPECIAL        = 0x6b,
    REPLAY_BONUS_ITEM_MONK_ROBE          = 0x6d,
    REPLAY_BONUS_ITEM_P08_SNAIL_MAGAZINE = 0x80,
    REPLAY_BONUS_ITEM_M93R               = 0x81,
    REPLAY_BONUS_ITEM_M950               = 0x82,
    REPLAY_BONUS_ITEM_P08                = 0x83,
    REPLAY_BONUS_ITEM_P229               = 0x84,
    REPLAY_BONUS_ITEM_MONGOOSE           = 0x88,
    REPLAY_BONUS_ITEM_GRENADE_PISTOL     = 0x8a,
    REPLAY_BONUS_ITEM_MM1                = 0x8b,
    REPLAY_BONUS_ITEM_PA3                = 0x8c,
    REPLAY_BONUS_ITEM_SP12               = 0x8d,
    REPLAY_BONUS_ITEM_AS12               = 0x8e,
    REPLAY_BONUS_ITEM_M4A1_RIFLE         = 0x8f,
    REPLAY_BONUS_ITEM_M249               = 0x90,
    REPLAY_BONUS_ITEM_TONFA_BATON        = 0x92,
    REPLAY_BONUS_ITEM_M4A1_PLUS_1        = 0x93,
    REPLAY_BONUS_ITEM_M4A1_PLUS_2        = 0x94,
    REPLAY_BONUS_ITEM_HYPERVELOCITY      = 0x95,
    REPLAY_BONUS_ITEM_GUNBLADE           = 0x96,
    REPLAY_BONUS_ITEM_M4A1_HAMMER        = 0x98,
    REPLAY_BONUS_ITEM_M4A1_BAYONET       = 0x99,
    REPLAY_BONUS_ITEM_M4A1_GRENADE       = 0x9a,
    REPLAY_BONUS_ITEM_M4A1_PYKE          = 0x9b,
    REPLAY_BONUS_ITEM_M4A1_JAVELIN       = 0x9c,
    REPLAY_BONUS_ITEM_MP5A5              = 0x9d,
    REPLAY_BONUS_ITEM_MP5A5_PLUS_1       = 0x9e,
    REPLAY_BONUS_ITEM_MP5A5_PLUS_2       = 0x9f,
    REPLAY_BONUS_ITEM_9MM_HYDRA          = 0xa1,
    REPLAY_BONUS_ITEM_9MM_SPARTAN        = 0xa2,
    REPLAY_BONUS_ITEM_44_MAGNUM          = 0xa6,
    REPLAY_BONUS_ITEM_44_MAEDA_SP        = 0xa7,
    REPLAY_BONUS_ITEM_AIRBURST           = 0xaa,
    REPLAY_BONUS_ITEM_FIREFLY            = 0xad,
    REPLAY_BONUS_ITEM_RSLUG              = 0xae,
    REPLAY_BONUS_ITEM_YOSHIDA_CARD       = 0x122,
    REPLAY_BONUS_ITEM_TEDDY_BEAR         = 0x124,
    REPLAY_BONUS_ITEM_AERIS_MAGAZINE     = 0x127,
    REPLAY_BONUS_ITEM_WHITELIST_END      = 0xFFFF,
    REPLAY_BONUS_SHOP_FINAL_EXP_CEILING  = 0x7FFFFFFF,
};

const ShopTier gReplayBonusShopTierLadder[SHOP_TIER_COUNT] SECTION(".data") = {
    { 14500, { REPLAY_BONUS_ITEM_MONK_ROBE, REPLAY_BONUS_ITEM_MEDICINE_WHEEL, REPLAY_BONUS_ITEM_RECOVERY_2 } },
    { 16000, { REPLAY_BONUS_ITEM_M9, REPLAY_BONUS_ITEM_RIFLE_CLIP_HOLDER, REPLAY_BONUS_ITEM_FLARE } },
    { 44000, { REPLAY_BONUS_ITEM_M203, REPLAY_BONUS_ITEM_PROTEIN_CAPSULE, REPLAY_BONUS_ITEM_9MM_HYDRA } },
    { 51000, { REPLAY_BONUS_ITEM_HAMMER, REPLAY_BONUS_ITEM_BELT_POUCH, REPLAY_BONUS_ITEM_MP_BOOST_1 } },
    { 57000, { REPLAY_BONUS_ITEM_PYKE, REPLAY_BONUS_ITEM_LIPSTICK, REPLAY_BONUS_ITEM_TACTICAL_ARMOR } },
    { 62000, { REPLAY_BONUS_ITEM_JAVELIN, REPLAY_BONUS_ITEM_MD_PLAYER, REPLAY_BONUS_ITEM_HOLY_WATER } },
    { 66000, { REPLAY_BONUS_ITEM_AYA_SPECIAL, REPLAY_BONUS_ITEM_9MM_SPARTAN, REPLAY_BONUS_ITEM_OFUDA } },
    { 69000, { REPLAY_BONUS_ITEM_AS12, REPLAY_BONUS_ITEM_RSLUG, REPLAY_BONUS_ITEM_FIREFLY } },
    { 72000, { REPLAY_BONUS_ITEM_MONGOOSE, REPLAY_BONUS_ITEM_44_MAGNUM, REPLAY_BONUS_ITEM_SKULL_CRYSTAL } },
    { 75000, { REPLAY_BONUS_ITEM_M249, REPLAY_BONUS_ITEM_44_MAEDA_SP, REPLAY_BONUS_ITEM_COLA } },
    { 200000, { REPLAY_BONUS_ITEM_MM1, REPLAY_BONUS_ITEM_AIRBURST, REPLAY_BONUS_ITEM_RECOVERY_3 } },
    { 400000, { REPLAY_BONUS_ITEM_HYPERVELOCITY, REPLAY_BONUS_ITEM_HUNTER_GOGGLES, REPLAY_BONUS_ITEM_MP_BOOST_2 } },
    { REPLAY_BONUS_SHOP_FINAL_EXP_CEILING, { REPLAY_BONUS_ITEM_GUNBLADE, REPLAY_BONUS_ITEM_RINGERS_SOLUTION, REPLAY_BONUS_ITEM_EAU_DE_TOILETTE } },
};

const u8 gReplayBonusQuitWarningText[] SECTION(".data") = "You will lose the game clear\ndata if you quit now. Quit now\nand delete game clear data?";

const u8 gReplayBonusQuitWarningWidthText[] SECTION(".data") = "data if you quit now. Quit now";

const u16 gReplayBonusItemWhitelist[REPLAY_BONUS_ITEM_WHITELIST_COUNT + 2] SECTION(".data") = {
    REPLAY_BONUS_ITEM_MP5A5,
    REPLAY_BONUS_ITEM_M93R,
    REPLAY_BONUS_ITEM_TONFA_BATON,
    REPLAY_BONUS_ITEM_M4A1_BAYONET,
    REPLAY_BONUS_ITEM_M4A1_GRENADE,
    REPLAY_BONUS_ITEM_SP12,
    REPLAY_BONUS_ITEM_M4A1_JAVELIN,
    REPLAY_BONUS_ITEM_M4A1_PYKE,
    REPLAY_BONUS_ITEM_P08,
    REPLAY_BONUS_ITEM_M950,
    REPLAY_BONUS_ITEM_GRENADE_PISTOL,
    REPLAY_BONUS_ITEM_AERIS_MAGAZINE,
    REPLAY_BONUS_ITEM_MONGOOSE,
    REPLAY_BONUS_ITEM_P229,
    REPLAY_BONUS_ITEM_MM1,
    REPLAY_BONUS_ITEM_PA3,
    REPLAY_BONUS_ITEM_AS12,
    REPLAY_BONUS_ITEM_M4A1_RIFLE,
    REPLAY_BONUS_ITEM_M249,
    REPLAY_BONUS_ITEM_GUNBLADE,
    INVENTORY_COLLECTION_ID_MENDEL_JOURNAL,
    REPLAY_BONUS_ITEM_M4A1_HAMMER,
    REPLAY_BONUS_ITEM_HYPERVELOCITY,
    REPLAY_BONUS_ITEM_P08_SNAIL_MAGAZINE,
    REPLAY_BONUS_ITEM_MP5A5_PLUS_1,
    REPLAY_BONUS_ITEM_MP5A5_PLUS_2,
    REPLAY_BONUS_ITEM_M4A1_PLUS_1,
    REPLAY_BONUS_ITEM_M4A1_PLUS_2,
    REPLAY_BONUS_ITEM_TACTICAL_ARMOR,
    REPLAY_BONUS_ITEM_COMBAT_ARMOR,
    REPLAY_BONUS_ITEM_NBC_SUIT,
    REPLAY_BONUS_ITEM_PASGT_VEST,
    REPLAY_BONUS_ITEM_ASSAULT_SUIT,
    REPLAY_BONUS_ITEM_AYA_SPECIAL,
    REPLAY_BONUS_ITEM_CHICKEN_PLATE,
    REPLAY_BONUS_ITEM_PSY_SUIT,
    REPLAY_BONUS_ITEM_TURTLE_VEST,
    REPLAY_BONUS_ITEM_TACTICAL_VEST,
    REPLAY_BONUS_ITEM_LEATHER_JACKET,
    REPLAY_BONUS_ITEM_EOD_SUIT,
    REPLAY_BONUS_ITEM_MONK_ROBE,
    REPLAY_BONUS_ITEM_RECOVERY_1,
    REPLAY_BONUS_ITEM_RECOVERY_2,
    REPLAY_BONUS_ITEM_RECOVERY_3,
    REPLAY_BONUS_ITEM_MP_BOOST_1,
    REPLAY_BONUS_ITEM_COLA,
    REPLAY_BONUS_ITEM_MP_BOOST_2,
    REPLAY_BONUS_ITEM_STIM,
    REPLAY_BONUS_ITEM_PENICILLIN,
    REPLAY_BONUS_ITEM_BELT_POUCH,
    REPLAY_BONUS_ITEM_SNAIL_MAGAZINE,
    REPLAY_BONUS_ITEM_SMG_CLIP_HOLDER,
    REPLAY_BONUS_ITEM_RIFLE_CLIP_HOLDER,
    REPLAY_BONUS_ITEM_FLARE,
    REPLAY_BONUS_ITEM_PEPPER_SPRAY,
    REPLAY_BONUS_ITEM_SKULL_CRYSTAL,
    REPLAY_BONUS_ITEM_MEDICINE_WHEEL,
    REPLAY_BONUS_ITEM_HOLY_WATER,
    REPLAY_BONUS_ITEM_OFUDA,
    REPLAY_BONUS_ITEM_PROTEIN_CAPSULE,
    REPLAY_BONUS_ITEM_RINGERS_SOLUTION,
    REPLAY_BONUS_ITEM_EAU_DE_TOILETTE,
    REPLAY_BONUS_ITEM_HUNTER_GOGGLES,
    REPLAY_BONUS_ITEM_LIPSTICK,
    REPLAY_BONUS_ITEM_MD_PLAYER,
    REPLAY_BONUS_ITEM_GPS,
    REPLAY_BONUS_ITEM_COMBAT_LIGHT,
    REPLAY_BONUS_ITEM_HAMMER,
    REPLAY_BONUS_ITEM_PYKE,
    REPLAY_BONUS_ITEM_JAVELIN,
    REPLAY_BONUS_ITEM_M203,
    REPLAY_BONUS_ITEM_M9,
    INVENTORY_COLLECTION_ID_ARMORY_CARDKEY,
    INVENTORY_COLLECTION_ID_BOWMANS_CARD,
    REPLAY_BONUS_ITEM_YOSHIDA_CARD,
    REPLAY_BONUS_ITEM_TEDDY_BEAR,
    INVENTORY_COLLECTION_ID_PIERCE_RESCUE_BONUS,
    INVENTORY_COLLECTION_ID_SOLDIER_RESCUE_BONUS,
    REPLAY_BONUS_ITEM_WHITELIST_END,
    0,
};

/// Single row renderer shared by every item of the Complete Bonus list.
///
/// The panel enables USER_INTERFACE_LIST_SHARED_ROW_CALLBACK before dispatch;
/// entry zero identifies and draws the owner's current item and half-price BP.
/// The list borrows this table for the loaded package's lifetime.
static UiListRowCallback _gReplayBonusCompleteBonusRowCallbacks[1] = { _replayBonusDrawCurrentItemRow };

UiList gReplayBonusCompleteBonusList = {
    .rowCallbacks    = _gReplayBonusCompleteBonusRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .wrapNavigation  = 0,
    .rowHeight       = REPLAY_BONUS_COMPLETE_BONUS_ROW_HEIGHT,
};

/// Title-style recipe for the Complete Bonus item-credit and totals panel.
///
/// The award controller opens it as an active root with unused argument 1.
/// Its callback fits the initial centred-screen pixel bounds to the item list.
/// Spawning copies this bodyless priority-192 recipe, retaining its callback;
/// replay and gameplay must remain loaded through the panel's closure.
static const UiObjectDesc _gReplayBonusCompleteBonusPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 208, 160 },
    .otIndexSeed     = 60,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusCompleteBonusPanelTask,
};

/// Title-style recipe for the unsaved game-clear warning and its Yes/No child.
///
/// Spawned as an active root after declining the save prompt. Its callback
/// fits the initial centred-screen pixel bounds to the warning text.
/// Spawning copies the bodyless priority-192 recipe; replay and gameplay must
/// stay loaded through closure of the warning's tree.
static const UiObjectDesc _gReplayBonusQuitWarningPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -96, 160, 160 },
    .otIndexSeed     = 60,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusQuitWarningTask,
};

/// Title-style recipe for the completed run's Balance panel.
///
/// Spawn argument 1 is zero; the active root later opens NEXT REPLAY BONUS
/// as a child. Bounds are centred-screen pixels. Spawning copies the bodyless
/// priority-192 recipe; replay and gameplay must remain loaded until closure.
static const UiObjectDesc _gReplayBonusBalancePanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -72, -64, 144, 56 },
    .otIndexSeed     = 48,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusBalancePanelTask,
};

const UiObjectDesc gReplayBonusNextReplayBonusPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -72, 8, 144, 56 },
    .otIndexSeed     = 52,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusBalancePanelTask,
};

/// Item-information recipe for columns zero and two of the unlocked shop tier.
///
/// The controller opens each as an active root; argument 1 selects the column.
/// Bounds are centred-screen pixels, with OT base tag 44. Spawning copies this
/// bodyless priority-192 recipe. Keep replay, gameplay and the selected item
/// resources loaded until the information panel closes.
static const UiObjectDesc _gReplayBonusFirstAndThirdShopItemPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 288, 208 },
    .otIndexSeed     = 44,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusUnlockedItemPanelTask,
};

/// Item-information recipe for column one of the unlocked shop tier.
///
/// Opened as an active root with argument 1 equal to 1. Centred-screen pixel
/// bounds match the other item panels, but its OT base is tag 40. Spawning
/// copies this bodyless priority-192 recipe. Keep replay, gameplay and the
/// selected item resources loaded until the information panel closes.
static const UiObjectDesc _gReplayBonusSecondShopItemPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 288, 208 },
    .otIndexSeed     = 40,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusUnlockedItemPanelTask,
};

/// Title-style recipe for the BP award replacing a fully unlocked shop tier.
///
/// Opened as an active root with unused argument 1. Bounds are centred-screen
/// pixels and its ordering-table base is tag 44. Spawning copies the bodyless
/// priority-192 recipe; replay and gameplay must remain loaded until closure.
static const UiObjectDesc _gReplayBonusExtraBpPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -72, -32, 144, 40 },
    .otIndexSeed     = 44,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = REPLAY_BONUS_AWARD_PANEL_TASK_PRIORITY,
    .contentCallback = replayBonusExtraBpPanelTask,
};

enum { REPLAY_BONUS_AWARD_SCREEN_TASK_PRIORITY = 0xC0 };
TaskDesc gReplayBonusAwardScreenTaskDescriptor = { { { TASK_BODY_NONE, REPLAY_BONUS_AWARD_SCREEN_TASK_PRIORITY } }, _replayBonusAwardScreenTask, { NULL } };

/// Next credits packet arena to select (0 first 64-KiB arena, 1 second).
///
/// Toggles after each selection and persists for the loaded package's lifetime.
static u8 _gReplayBonusNextCreditsPrimitiveBufferIndex = 0;

/// Credits picture lifecycle (0 idle, 1 decoding, 2 cross-fading).
///
/// Decode and cross-fade reject further picture requests. The drawing callback
/// returns the phase to idle after 120 cross-fade draws; the credits controller
/// waits only for decoding before retiring the borrowed request.
static u8 _gReplayBonusCreditsPicturePhase = REPLAY_BONUS_PICTURE_IDLE;

/// Current credits picture's VRAM page (0 Y=0, 1 Y=256), both at X=640.
///
/// Decoding targets the opposite page. Completion toggles this selector before
/// cross-fading the new page over the previous one; drawing keeps both live.
static u8 _gReplayBonusCreditsPicturePageIndex = 0;

/// Draw callbacks remaining in the credits picture's 120-frame cross-fade.
///
/// Starts at 120 when decoding completes. Drawing uses complementary fractions
/// of the shared credits brightness for both pages, then decrements to zero.
static u8 _gReplayBonusCreditsPictureCrossFadeFramesLeft = 0;

/// Decoder task handle retained until the picture-request worker polls its exit.
///
/// Reset to NULL when credits start, then replaced by each serialized decode.
/// Polling releases the task; the retained handle is not cleared afterwards and
/// must not be used again before the next spawn replaces it.
static Task* _gReplayBonusPictureDecoderTask = NULL;

const TaskDesc gReplayBonusCreditsTaskDescriptors[REPLAY_BONUS_CREDITS_TASK_COUNT] SECTION(".data") = {
    [REPLAY_BONUS_CREDITS_CONTROLLER_TASK_INDEX] = { { { TASK_BODY_NONE, REPLAY_BONUS_CREDITS_TASK_PRIORITY } }, _replayBonusCreditsTask, { NULL } },
    [REPLAY_BONUS_CREDITS_FADE_IN_TASK_INDEX]    = { { { TASK_BODY_NONE, REPLAY_BONUS_CREDITS_TASK_PRIORITY } }, _replayBonusFadeCreditsInTask, { NULL } },
    [REPLAY_BONUS_CREDITS_FADE_OUT_TASK_INDEX]   = { { { TASK_BODY_NONE, REPLAY_BONUS_CREDITS_TASK_PRIORITY } }, _replayBonusFadeCreditsOutTask, { NULL } },
    [REPLAY_BONUS_CREDITS_PICTURE_TASK_INDEX]    = { { { TASK_BODY_NONE, REPLAY_BONUS_CREDITS_TASK_PRIORITY } }, _replayBonusStartCreditsPictureTask, { NULL } },
};

u_long* gReplayBonusPictureStripPixels = NULL;

u_long* gReplayBonusPictureDctBitstream = NULL;

s16 gReplayBonusPictureWidth = 0;

s16 gReplayBonusPictureHeight = 0;

u16 gReplayBonusPictureVramX = 0;

u16 gReplayBonusPictureVramY = 0;

s16 gReplayBonusPictureStripPending = REPLAY_BONUS_PICTURE_STRIP_UPLOADED;

s16 gReplayBonusPictureStripsUploaded = 0;

u16 gReplayBonusPictureStripBufferIndex = 0;

/// Byte initialized to 0x43 with no observed accesses; its role is unproven.
static u8 D_replay_bonus_80119272 = 0x43;

/// Byte initialized to 0x42 with no observed accesses; its role is unproven.
static u8 D_replay_bonus_80119273 = 0x42;

ReplayBonusTotals gReplayBonusTotals = {
    .totalExp     = 0,
    .totalBp      = 0,
    .nextExp      = 0,
    .nextBp       = 0,
    .shopTier     = 0,
    .extraBonusBp = 0,
};

/// Borrowed data resource selected as the credits STF document.
///
/// Selection records the resource before validating its magic. Drawing uses
/// the separately published tables; this retained pointer has no reader.
static ReplayBonusStfFile* _gReplayBonusCreditsFile = NULL;

/// Borrowed font-cell table published from the relocated credits document.
///
/// Glyph commands supply byte indices; each must address a complete live cell.
/// The serialized file supplies no glyph count here and must outlive drawing.
static const ReplayBonusStfGlyph* _gReplayBonusCreditsGlyphs = NULL;

/// Borrowed scroll timing and layout columns from the relocated STF document.
///
/// Scroll speed is 8.8 pixels per draw callback; initial/final holds use six
/// callbacks per stored unit. Row commands select column indices 0..7. This
/// view is read-only and remains valid only while the credits file is loaded.
static const ReplayBonusStfParams* _gReplayBonusCreditsParams = NULL;

/// Borrowed credits rows, paired with `_gReplayBonusCreditsLineCount`.
///
/// Rows have increasing document Y. Relocation temporarily advances this
/// pointer while fixing command offsets, then restores the first row. Keep
/// the writable document loaded through drawing; command lists end at the
/// STF command terminator rather than at a length stored in each row.
static ReplayBonusStfLine* _gReplayBonusCreditsLines = NULL;

/// Borrowed image descriptors published from the relocated credits document.
///
/// Sprite commands supply byte indices into this read-only view. No count is
/// published; every selected descriptor must fit the file and remain loaded.
static const ReplayBonusStfSprite* _gReplayBonusCreditsSprites = NULL;

/// Number of rows copied from the STF line table on its first relocation.
///
/// Credits playback requires at least one row and a complete array. Selecting
/// an already relocated file retains this count, so it must still describe
/// that file's `_gReplayBonusCreditsLines` array.
static s32 _gReplayBonusCreditsLineCount = 0;

/// Top of the credits viewport in whole document pixels.
///
/// Starts at -480 and stops at the last row's bottom Y minus 480. Drawing
/// retains only bits 1..23 before converting to centred screen coordinates.
static s32 _gReplayBonusCreditsScrollY = 0;

/// Fractional pixel remainder of the credits' 8.8 scroll, normally 0..255.
///
/// Each step stores the full speed-plus-remainder sum, updates whole-pixel Y,
/// then stores the low eight bits. It is initialized on package load rather
/// than reset when a credits controller starts.
static s32 _gReplayBonusCreditsScrollFraction = 0;

/// Shared RGB modulation of credits glyphs, row sprites and picture pages.
///
/// Range 0..127: zero is black, 127 is full credits brightness. Fade workers
/// derive it from byte fade amounts; picture cross-fades scale both page levels
/// by it. Reset to black when the credits controller starts.
static u8 _gReplayBonusCreditsBrightness = 0;

/// Draw callbacks elapsed during credits' initial hold, scroll and final hold.
///
/// Reset by the controller; only its increments read the retained count.
static s32 _gReplayBonusCreditsDrawFrameCount = 0;

/// Allocated bytes in the selected credits row-guide/glyph packet arena.
///
/// Reset on arena selection, then advanced by each complete packet's sizeof.
/// The frame's packets must fit 64 KiB; allocation performs no bounds check.
static s32 _gReplayBonusCreditsPrimitiveBytesUsed = 0;

/// Picture's left edge in pixels from the credits viewport's left edge.
///
/// A new picture command applies its selected column alignment and stores
/// the low 16 bits of the resulting X. Drawing subtracts 320 for centred
/// screen coordinates; the position persists until another picture command.
static u16 _gReplayBonusCreditsPictureLeftX = 0;

/// Primary-heap request reused for each serialized credits picture decode.
///
/// The credits controller owns the record until decoding finishes. Workers
/// borrow it and its separately allocated auxiliary-heap VLC table. The record
/// is freed during display restoration without clearing this pointer; the VLC
/// table survives awards until the post-awards restart resets the heaps.
static ReplayBonusPictureDecode* _gReplayBonusCreditsPictureRequest = NULL;

/// Byte base of the active credits row-guide/glyph packet arena.
///
/// Borrows one of two 64-KiB arenas immediately after the normal GPU arena,
/// reserved by ending memory setup. `_gReplayBonusCreditsPrimitiveBytesUsed`
/// is a byte offset; typed packet views remain word-aligned. Keep storage and
/// queued packets live through GPU completion before that arena is reused.
static u8* _gReplayBonusCreditsPrimitiveBuffer = NULL;
