#include "gameplay/item_menu.h"

#include "types.h"

#include "attachments.h"
#include "hud_sprites.h"
#include "item_menu.h"
#include "items.h"
#include "menu.h"
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
/// `itemPickupRestoreFrameTimingTask`, `Gp_PickupExitTask`. Copied onto the stack by `func_800CE22C`.
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

const TaskFuncTable5 D_80096E70 = { { itemPickupPublishPlacedObjectTask, Gp_SpawnPickupUiTask, itemPickupHandleResultTask, itemPickupRestoreFrameTimingTask, Gp_PickupExitTask } };

// "EXP"
// "MP"

void func_800CE22C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_80096E70;
    sp.funcs[arg0->state](arg0);
}

void Gp_MenuExitCallback(Task* arg0)
{
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((Gp_PendingRelatedId != 0) && (Gp_RelatedPending != 0)) {
        if (sceneIsBattleActive() == 0) {
            Gp_PendingRelatedId = 0;
        } else if (Gp_PendingRelatedId > 0) {
            playerActorEnterReload(playerTask, 0, PLAYER_ACTOR_RELOAD_MENU);
        } else {
            playerActorEnterReload(playerTask, 1, PLAYER_ACTOR_RELOAD_MENU);
        }
        Gp_RelatedPending = 0;
    }
    if (Gp_HealPending == 1) {
        taskMessageDispatch(playerTask, 0x402, 0, 0);
        Gp_HealPending = 0;
    }
    if (Gp_UsedItemId != 0) {
        if (Gp_UsedItemId == 0x3E) {
            playerStateSetStatusEffects(0, PLAYER_STATUS_BERSERKER);
        }
        Gp_UsedItemId = 0;
    }
    displayReleaseMenuHold();
    taskCallExit(arg0);
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

void Gp_ItemMenuInit(UiObject* arg0, Task* arg1)
{
    void* mem;
    s32   scale;

    Wip_UiHolder = arg0;
    mem          = memCalloc(4, 0);
    if (mem != NULL) {
        arg1->work = mem;
        if (gGameSession->cutsceneHold == 1) {
            itemMenuClearPreviewItems();
            uiSpawnObject(&D_8010EAB4[8], 0, 1, 8, arg0);
            scale = 2;
        } else {
            uiSpawnObject(&D_8010EAB4[1], 0, 1, 8, arg0);
            scale = 1;
        }
        uiSetPanelContentSize(&(arg0)->panel, 0, uiGetTextRowsHeight(scale) + 1);
        arg0->panel.bounds.unsignedRect.y = 0x68 - arg0->panel.bounds.unsignedRect.h;
        arg1->state                       = arg1->state + 1;
    }
}

void Gp_ItemMenuTask(Task* arg0)
{
    UiObjectTaskFuncTable3 sp;

    sp = Gp_ItemMenuStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
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
