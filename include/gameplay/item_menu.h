#ifndef GAMEPLAY_ITEM_MENU_H
#define GAMEPLAY_ITEM_MENU_H

#include "types.h"

#include "gameplay/action_prompt.h"

#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory menu tasks, item panels, prompts and their shared descriptors.

/// Item-move `UiObjectDesc` table. `Gp_ItemMoveTask` spawns `[0]` / `[1]`
/// and, when `spawnArg1 == 1`, `[9]`.
extern UiObjectDesc D_8010D6F4[];

/// Extra `UiObjectDesc` spawned after the `D_8010D6F4` pair.
#define D_8010D80C D_8010D6F4[10]

/// Named as a task entry by the enemy descriptor tables in the map UI overlays.
void Gp_ItemPickupTilt(Task* arg0);

void func_800B65B0(Task* task);

void Gp_DrawPromptLines(UiObject* arg0, Task* arg1);

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_800CE22C(Task* arg0);

void Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

void Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_SetPreviewItem(s32 arg0, s32 arg1);

void Gp_ClearPreviewItems(void);

UiObject* func_800CD89C(UiObject* arg0);

void func_800C5F70(Task* arg0);

void func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

extern ActionPrompt D_80114D28[2];

void Gp_SetHolderItemText(s32 arg0);

/// Notice text indices used by item-menu and save-result notices.
enum {
    ITEM_MENU_NOTICE_CANNOT_MOVE_EQUIPPED_ITEM = 7,
    ITEM_MENU_NOTICE_SAVE_CANCELLED            = 0xF,
    ITEM_MENU_NOTICE_SAVE_COMPLETE             = 0x11,
    ITEM_MENU_NOTICE_NO_USE_NOW                = 0x12,
    ITEM_MENU_NOTICE_NO_OTHER_WEAPON           = 0x14,
    ITEM_MENU_NOTICE_NO_OTHER_ARMOR            = 0x15,
    ITEM_MENU_NOTICE_CANNOT_MOVE_ITEM          = 0x1E
};

/// Whether accepting a notice dismisses its parent operation or confirms to it.
enum {
    ITEM_MENU_NOTICE_RESULT_DISMISS = 0,
    ITEM_MENU_NOTICE_RESULT_CONFIRM = 1
};

/// Opens a timed notice as a child of `parent`, or as a root when it is NULL.
///
/// `noticeId` selects text 0..32. `returnConfirmation` selects the result mode
/// above: timeout or Confirm/Cancel yields DISMISS for 0, CONFIRM for 1;
/// the Menu button yields CANCEL in either mode. `unused` is ignored.
/// The task owns the returned object until teardown; allocation failure returns
/// NULL. The notice starts active with a one-tick opening delay.
UiObject* itemMenuSpawnNotice(UiObject* parent, s32 noticeId, s32 unused, s32 returnConfirmation);

s32 func_800D4D2C(s32 arg0);

/// Returns whether the most recently started hotspot menu's action row was accepted.
///
/// The menu task clears the flag when it starts; accepting Examine/Push sets 1.
/// Cancellation and selecting Item leave it at 0. Reading does not consume it.
/// Queuing a new menu alone does not clear the previous result.
s32 itemMenuIsHotspotActionConfirmed(void);

/// Queues the Examine/Push and Item command menu at a hotspot's screen position.
///
/// Coordinates are signed pixels from the screen center, with Y downward;
/// the panel stores their low 16 bits. `actionKind` is 0 for Examine or 1 for
/// Push; other values also draw Examine. Only one queued display-mode request
/// is accepted at a time. Returns 1 even if that queue rejects the request.
s32 itemMenuOpenHotspotCommands(s32 screenX, s32 screenY, s32 actionKind);

/// Returns the item id selected for preview slot 0, or -1 when that slot is empty.
///
/// This is the requested item, including while its resources are still loading;
/// callers must separately check resource readiness. Does not change the preview.
s32 itemMenuGetPrimaryPreviewItem(void);

extern char Gp_StrEmpty[];

/// Draws a framed proportional meter in panel-relative pixels.
///
/// left/right bound its width and centerY locates the two-pixel fill.
/// value/maximum use the same units (including fixed-point quantities).
/// For left < right, maximum must be nonzero and the signed scaling product
/// must fit s32. Fill clips at width - 2 and is omitted at nonpositive width;
/// left >= right draws nothing. colorRgb is the fill's packed 24-bit RGB.
/// Requires resident menu textures and writable OT/primitive storage.
void itemMenuDrawMeter(const UiPanel* panel, s32 left, s32 right, s32 centerY, s32 maximum, s32 value, u32 colorRgb);

/// Command-indexed menu descriptors. Zero rows reserve unused command IDs.
extern UiObjectDesc D_8010EAB4[50];

void Gp_MenuRootTask(Task* arg0);

#define D_8010EFA0 D_8010EAB4[45]

#endif // GAMEPLAY_ITEM_MENU_H
