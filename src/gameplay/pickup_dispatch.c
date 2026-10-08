#include "gameplay/item_menu.h"

#include "types.h"

#include "attachments.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "items.h"
#include "menu.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "player_actor.h"
#include "gameplay/player_state.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys_types.h"

/// Five-entry dispatcher table: `itemPickupPublishPlacedObjectTask`, `Gp_SpawnPickupUiTask`, `itemPickupHandleResultTask`,
/// `itemPickupRestoreFrameTimingTask`, `itemPickupExitTask`. Copied onto the stack by `itemPickupTask`.
extern const TaskFuncTable5 D_80096E70;

UiObjectTaskFunc D_8010D3A0[96] = {
    NULL,
    itemMenuApplySelectedHealingItem,
    itemMenuApplySelectedHealingItem,
    itemMenuApplySelectedHealingItem,
    NULL,
    itemMenuApplySelectedHealingItem,
    itemMenuApplySelectedHealingItem,
    itemMenuApplySelectedHealingItem,
    NULL,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyMpBoostPanel,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyPouchPanel,
    NULL,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    itemMenuApplyParasiteEnergyItem,
    NULL,
    NULL,
    NULL,
    itemMenuInvokeElementBoostPanel,
    itemMenuInvokeElementBoostPanel,
    itemMenuInvokeElementBoostPanel,
    itemMenuInvokeElementBoostPanel,
    NULL,
    NULL,
    itemMenuApplyHpBoostPanel,
    itemMenuApplySelectedHealingItem,
    NULL,
    NULL,
    NULL,
    NULL,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyWeaponAddonPanel,
    itemMenuApplyWeaponAddonPanel,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

const TaskFuncTable5 D_80096E70 = { { itemPickupPublishPlacedObjectTask, Gp_SpawnPickupUiTask, itemPickupHandleResultTask, itemPickupRestoreFrameTimingTask, itemPickupExitTask } };

// "EXP"
// "MP"

void itemPickupTask(Task* task)
{
    TaskFuncTable5 states;

    states = D_80096E70;
    states.funcs[task->state](task);
}

/// Starts a pending primary/secondary consumable reload, or discards it outside battle.
///
/// The signed item id remains for the reload cue to consume in battle, including
/// when scripted player control declines the request. Only the notification flag
/// is cleared here. playerTask must be the live player with loaded actor resources.
static inline void _menuApplyPendingConsumableReload(Task* playerTask)
{
    if ((Gp_PendingRelatedId != INVENTORY_ITEM_NONE) && (Gp_RelatedPending != 0)) {
        if (sceneIsBattleActive() == 0) {
            Gp_PendingRelatedId = INVENTORY_ITEM_NONE;
        } else if (Gp_PendingRelatedId > 0) {
            playerActorEnterReload(playerTask, EQUIPMENT_WEAPON_SUPPLY_PRIMARY, PLAYER_ACTOR_RELOAD_MENU);
        } else {
            playerActorEnterReload(playerTask, EQUIPMENT_WEAPON_SUPPLY_SECONDARY, PLAYER_ACTOR_RELOAD_MENU);
        }
        Gp_RelatedPending = 0;
    }
}

void menuApplyPendingItemUseTask(Task* task)
{
    enum {
        MENU_ITEM_USE_PRESENTATION_PENDING = 1,
        MENU_ITEM_EAU_DE_TOILETTE          = 0x3E,
        MENU_ITEM_USE_APPLY_STATUS_EFFECTS = 0
    };
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    // Resume deferred player actions only after the menu has restored world resources.
    _menuApplyPendingConsumableReload(playerTask);
    if (Gp_HealPending == MENU_ITEM_USE_PRESENTATION_PENDING) {
        taskMessageDispatch(playerTask, PLAYER_ACTOR_MESSAGE_ENTER_ITEM_USE, 0, 0);
        Gp_HealPending = 0;
    }
    if (Gp_UsedItemId != INVENTORY_ITEM_NONE) {
        if (Gp_UsedItemId == MENU_ITEM_EAU_DE_TOILETTE) {
            playerStateSetStatusEffects(MENU_ITEM_USE_APPLY_STATUS_EFFECTS, PLAYER_STATUS_BERSERKER);
        }
        Gp_UsedItemId = INVENTORY_ITEM_NONE;
    }
    displayReleaseMenuHold();
    taskCallExit(task);
}

/// Sets the menu-exit room/view restoration latch without normalizing it.
///
/// Menu entry clears it to 0; map entry and preview loads that replace room
/// resources set 1. Only the value 1 requests restoration on menu exit.
static void _itemMenuSetRoomRestorePending(s32 pending)
{
    D_80114D88 = pending;
}

/// Returns the menu-exit room/view restoration latch without consuming it.
static s32 _itemMenuGetRoomRestorePending(void)
{
    return D_80114D88;
}

void itemMenuInitializeCaptionTask(UiObject* object, Task* task)
{
    enum {
        ITEM_MENU_CAPTION_WORK_BYTES            = 4,
        ITEM_MENU_CAPTION_STATUS_COMMAND        = 1,
        ITEM_MENU_CAPTION_KEY_ITEMS_COMMAND     = 8,
        ITEM_MENU_CAPTION_CHILD_OPEN_TICKS      = 8,
        ITEM_MENU_CAPTION_CONTENT_MARGIN_PIXELS = 1,
        ITEM_MENU_CAPTION_BOTTOM_PIXELS         = 104
    };
    void* workAllocation;
    s32   rowCount;

    Wip_UiHolder = object;
    // The task owns four cleared bytes; the later first-byte clear has no proven reader.
    workAllocation = memCalloc(ITEM_MENU_CAPTION_WORK_BYTES, false);
    if (workAllocation != NULL) {
        task->work = workAllocation;
        if (gGameSession->cutsceneHold == true) {
            itemMenuClearPreviewItems();
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_CAPTION_KEY_ITEMS_COMMAND], 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CAPTION_CHILD_OPEN_TICKS, object);
            rowCount = 2;
        } else {
            uiSpawnObject(&D_8010EAB4[ITEM_MENU_CAPTION_STATUS_COMMAND], 0, USER_INTERFACE_PANEL_ACTIVE, ITEM_MENU_CAPTION_CHILD_OPEN_TICKS, object);
            rowCount = 1;
        }
        // Fit the prompt rows while keeping the outer bottom at a fixed screen Y.
        uiSetPanelContentSize(&object->panel, 0, uiGetTextRowsHeight(rowCount) + ITEM_MENU_CAPTION_CONTENT_MARGIN_PIXELS);
        object->panel.bounds.unsignedRect.y = ITEM_MENU_CAPTION_BOTTOM_PIXELS - object->panel.bounds.unsignedRect.h;
        task->state                         = task->state + 1;
    }
}

void itemMenuCaptionTask(Task* task)
{
    UiObjectTaskFuncTable3 states;
    UiObject*              object;

    states = Gp_ItemMenuStates;
    object = task->spawnArg2.pointer;
    states.funcs[task->state](object, task);
}

/// Draws two item-menu prompt rows from a borrowed encoded text stream.
///
/// Each row starts with large, left-aligned outlined text and the shared normal
/// UI color. X is contentLeft + 2; Y is contentTop + 15 and contentTop + 30,
/// in panel-content pixels. Inline styling starts afresh on the second row.
/// That row follows the first LF or escaped N/n; CR does not advance the
/// scanner. Without a break it draws an empty row at the terminating NUL.
/// Hidden panels suppress drawing but still scan for the second row.
///
/// Requires a live object and non-NULL text under `textDrawUiLine` and
/// `textSkipLines`'s stream/resource contracts, including a readable predecessor
/// byte if promptText starts with N/n. Neither input is modified or retained.
static inline void _itemMenuDrawTwoLinePrompt(const UiObject* object, const u8* promptText)
{
    enum {
        ITEM_MENU_PROMPT_LEFT_INSET_PIXELS = 2,
        ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS = 15
    };
    const u8* secondLine;
    u32       textColorRgb;

    textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS,
                   object->panel.contentTop.signedValue + ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, promptText, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    secondLine = textSkipLines(promptText, 1);
    textDrawUiLine(object, object->panel.contentLeft.signedValue + ITEM_MENU_PROMPT_LEFT_INSET_PIXELS,
                   object->panel.contentTop.signedValue + 2 * ITEM_MENU_PROMPT_ROW_HEIGHT_PIXELS, secondLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

void itemMenuDrawTaskPrompt(UiObject* object, const Task* task)
{
    enum {
        ITEM_MENU_PROMPT_INLINE_VALUE_MAX = 0xFFFF,
        ITEM_MENU_PROMPT_ABILITY_ID_COUNT = 0x100
    };
    TaskSpawnArg promptPayload;

    // Small payloads are catalogue selectors; larger words encode borrowed text addresses.
    promptPayload = task->spawnArg1;
    if (promptPayload.value != 0) {
        if (promptPayload.unsignedValue > ITEM_MENU_PROMPT_INLINE_VALUE_MAX) {
            _itemMenuDrawTwoLinePrompt(object, promptPayload.pointer);
        } else if ((u32)(promptPayload.value - ITEM_TEXT_PACKED_ID_FIRST) < (u32)ITEM_MENU_PROMPT_ABILITY_ID_COUNT) {
            itemMenuDrawAbilityDescription(object, promptPayload.value);
        }
    }
}

void itemMenuDrawDefaultItemIcon(const UiObject* object, s32 x, s32 y, s32 itemId)
{
    itemMenuDrawItemIcon(object, x, y, itemId, ITEM_MENU_ICON_DEFAULT);
}
