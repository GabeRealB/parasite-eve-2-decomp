#ifndef GAMEPLAY_ITEM_MENU_H
#define GAMEPLAY_ITEM_MENU_H

#include "types.h"

#include "gameplay/action_prompt.h"

#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory menu tasks, item panels, prompts and their shared descriptors.

/// Item-move `UiObjectDesc` table. `Gp_ItemMoveTask` spawns `[0]` / `[1]`
/// and, when `spawnArg1 == 1`, `[9]`.
/// `[4]` is the popup `Gp_ItemMoveRow` spawns on confirm when
/// `owner->state == 1`, `[5]` the quantity-selection popup `itemMenuDrawTransferMoveRow`
/// opens when moving ammo stacks, `[9]` also the "Move items" confirmation
/// `Gp_ItemMoveChild` spawns when the pane is closed with items still selected
/// (`itemMenuCanMoveAllItems` result as arg1), and `[10]` an extra descriptor spawned
/// after the `[0]` / `[1]` pair.
extern UiObjectDesc D_8010D6F4[];

/// Named as a task entry by the enemy descriptor tables in the map UI overlays.
void Gp_ItemPickupTilt(Task* arg0);

void func_800B65B0(Task* task);

/// Draws a menu prompt from the task's first spawn payload.
///
/// Words above 0xFFFF are borrowed encoded text addresses: draws at most two
/// large, left-aligned outlined lines, 15 pixels apart, using the panel's normal
/// text color. A packed P.E. id in 0x300..0x3FF instead draws its description
/// and encoded level's casting cost (omitted at level 0). Those ids must satisfy
/// the ability catalogue and cost-table bounds; other small values draw nothing.
/// Text starts two pixels right of contentLeft and 15 pixels below contentTop;
/// hidden panels skip text drawing, but the line scanner still reads the source.
/// Requires a live object/task and loaded menu/font resources with writable
/// primitive/OT storage. The payload and any text stay owned by the caller.
/// Text must satisfy `textDrawUiLine` and `textSkipLines`, including a readable
/// predecessor byte when its first byte is N/n. The task is never changed.
void itemMenuDrawTaskPrompt(UiObject* object, const Task* task);

/// Queues a normal-size item icon using the live save's identification state.
///
/// x/y locate the 14-by-14 icon's bottom-left in pixels relative to the live object's
/// panel content origin. itemId is a valid inventory id (0 is an empty slot)
/// or a synthetic P.E. id; no enlargement, dimming or highlight is requested.
/// Borrows the object without modifying or retaining it. Requires loaded menu
/// textures and writable primitive/OT storage, even for a hidden panel.
void itemMenuDrawDefaultItemIcon(const UiObject* object, s32 x, s32 y, s32 itemId);

/// Runs the placed-object pickup prompt through its five task states.
///
/// task->state must be 0..4; dispatch has no bounds check. spawnArg2 borrows
/// the live source Enemy through result handling; its workType must satisfy
/// itemPickupPublishPlacedObjectTask's kind/pack contract. States publish the place,
/// open its pickup/save prompt, handle the result, restore frame timing and
/// exit, in that order. The prompt-opening state sets spawnArg1 to the live root
/// UiObject after successful allocation. Requires gameplay and its menu/text
/// resources to remain loaded and stage-managed UI primitive storage while
/// drawing. The final state releases that storage and requests stage mode exit;
/// a dispatched handler may release the task, which is not accessed afterwards.
void itemPickupTask(Task* task);

/// Draws an item's name, icon and optional equipment mark in a menu row.
///
/// x/y are pixels at the icon's bottom-left relative to the panel content
/// origin; the name starts at x + 17, y - 6. itemId must satisfy `itemGetText`
/// and the item-icon catalogue contract. Ordinary P.E. items 15..50 also show
/// level 1..3. attachmentState 0 omits the E/L/A mark, 1 allows E/L, and 2
/// also allows A. Hidden panels draw nothing. Requires menu/text textures and
/// writable GPU primitive and ordering-table storage.
void itemMenuDrawItemRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState);

/// Draws a right-aligned signed quantity in a recessed box beside an item row.
///
/// x/y are panel-relative row pixels; the number ends at x + 132, y - 3.
/// quantity follows `textItoaSigned`'s range and saturation; colorRgb is packed
/// 24-bit RGB. The origin-Y subtraction wraps to u16 before adding y.
/// Requires text textures and writable GPU primitive and ordering-table storage.
void itemMenuDrawQuantity(const UiObject* object, s32 x, s32 y, s32 quantity, s32 colorRgb);

/// Selects a requested item preview and queues its display-resource load if changed.
///
/// The low byte of loadProfile selects slot/profile 0 (menu), 1 (preview) or
/// 2 (relocated preview); it must be in 0..2. When that slot's id changes,
/// records itemId verbatim and marks the other two slots empty (-1), then
/// requests the load. Slots 3/4 are untouched. An unchanged id does nothing.
/// The loader may ignore the id (including 0 and -1) or suppress the request;
/// the recorded id still changes. Resource readiness must be checked separately.
void itemMenuSetPreviewItem(s32 itemId, s32 loadProfile);

/// Marks all five requested item-preview slots empty (-1).
///
/// The slot ids change immediately; loaded resources and queued CD requests
/// remain owned by the preview loader. Resource readiness is checked separately.
void itemMenuClearPreviewItems(void);

/// Dialog layouts in the low nibble of the content task's first spawn argument.
enum {
    ITEM_MENU_DIALOG_YES_SELECTED        = 0,
    ITEM_MENU_DIALOG_OK                  = 1,
    ITEM_MENU_DIALOG_CANCEL              = 2,
    ITEM_MENU_DIALOG_NO_SELECTED         = 3,
    ITEM_MENU_DIALOG_LAYOUT_MASK         = 0xF,
    ITEM_MENU_DIALOG_SYSTEM_CURSOR_SOUND = 0x10
};

/// Opens a Yes/No child menu with No selected, transferring input from parent.
///
/// parent must be a live task-owned UI object. Returns its task-owned child,
/// or NULL without changing parent on allocation failure. The child opens after
/// two nominal 60-Hz ticks, at the parent's lower-right content corner. Its
/// CONFIRM result carries `USER_INTERFACE_LIST_COMMAND_YES` or `_NO`.
UiObject* itemMenuSpawnYesNoMenuDefaultNo(UiObject* parent);

/// Item-information spawn flags above the low 16-bit inventory/packed item id.
enum {
    ITEM_MENU_INFO_ITEM_ID_MASK        = 0xFFFF,
    ITEM_MENU_INFO_RELOCATED_PREVIEW   = 0x10000,
    ITEM_MENU_INFO_NEXT_REPLAY         = 0x20000,
    ITEM_MENU_INFO_UNRELOCATED_PREVIEW = 0x40000
};

/// Displays item specifications, identifies the item and polls dismissal input.
///
/// spawnArg1 carries the low 16-bit item id and ITEM_MENU_INFO_* flags;
/// spawnArg2 borrows the live task-owned UiObject. Preview/caption loading must
/// already be requested. NEXT_REPLAY changes the title and suppresses the
/// dismissal sound; RELOCATED_PREVIEW takes precedence over UNRELOCATED_PREVIEW.
/// Replaces any previous information panel and installs `itemMenuInfoTaskExit`.
/// Captions start with five metadata lines, followed by description lines ending
/// at NUL or \\Z. Loaded retail captions produce at most 72 description rows;
/// alternate data must fit the list's signed-byte indices. Enemy ids >= 0x500
/// use a tall preview and unscrolled text. Confirm/Cancel/Triangle returns
/// CONFIRM, Menu returns CANCEL; input waits for the CD queue to become idle.
/// Uses two-VBlank timing while open and restores every-VBlank timing on input.
void itemMenuInfoTask(Task* task);

/// Item-preview texture selectors, exclusive scale choices and drawing flags.
///
/// Selectors 0/1/2 correspond to menu, preview and relocated-preview load profiles.
/// Scale choices use GTE factors 2560/4096 and 2720/4096 respectively. SMALL
/// overrides TALL. HIDDEN suppresses only the picture; its recessed frame remains.
enum {
    ITEM_MENU_PREVIEW_TEXTURE_MENU      = 0,
    ITEM_MENU_PREVIEW_TEXTURE_DIRECT    = 1,
    ITEM_MENU_PREVIEW_TEXTURE_RELOCATED = 2,
    ITEM_MENU_PREVIEW_TEXTURE_MASK      = 0xF,
    ITEM_MENU_PREVIEW_SCALE_EQUIPMENT   = 0x10,
    ITEM_MENU_PREVIEW_SCALE_SHOP        = 0x20,
    ITEM_MENU_PREVIEW_SCALE_MASK        = 0xF0,
    ITEM_MENU_PREVIEW_HIDDEN            = 0x100,
    ITEM_MENU_PREVIEW_SMALL             = 0x200,
    ITEM_MENU_PREVIEW_TALL              = 0x400
};

/// Queues an item-preview picture and its recessed frame in panel-relative pixels.
///
/// left/top locate the picture's top-left relative to the content origin.
/// Default dimensions are 128x96, SMALL uses 80x60, TALL uses 128x127; scale
/// flags change screen dimensions while retaining the texture extent. Unknown
/// texture selectors use MENU and unknown scale choices leave dimensions intact.
/// Borrows object without changing it; panel visibility does not suppress drawing.
/// Requires loaded preview textures and writable GPU primitive/OT storage unless
/// HIDDEN is set, which still requires storage for the frame.
void itemMenuDrawPreview(const UiObject* object, s32 left, s32 top, s32 flags);

extern ActionPrompt D_80114D28[2];

/// Sets the shared prompt to an item's first description, or Empty for item id 0.
///
/// Nonzero ids must satisfy `itemGetText`'s catalogue contract; identification
/// selects the description form. The prompt borrows the returned text, so its
/// defining image must remain loaded until the prompt is replaced or closed.
/// Does nothing if no prompt panel is registered.
void itemMenuSetItemDescriptionPrompt(s32 itemId);

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

/// Queues the current saved area's shop session with a stock selector.
///
/// Selects the loaded room's descriptor from the live save's stage and area,
/// ignoring its room and view. The room must stay loaded through the queued
/// task's use of its descriptor. The selector is forwarded unchanged: low
/// 16 bits choose the stock set, high 16 bits choose category 0..3; room callers
/// use 0x10, 0x20/0x21, 0x30..0x33 or 0x40 with the initial category zero.
/// Clears the shared UI holder even in unsupported areas. Returns 0 for an
/// unsupported area, 1 after requesting a supported shop; 1 does not prove that
/// the display-mode queue accepted the request.
s32 shopOpenSession(s32 stockSelector);

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

#endif // GAMEPLAY_ITEM_MENU_H
