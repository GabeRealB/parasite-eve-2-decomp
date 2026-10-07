#ifndef GAMEPLAY_PRIVATE_ITEM_MENU_H
#define GAMEPLAY_PRIVATE_ITEM_MENU_H

#include "types.h"

#include "gameplay/item_menu.h"
#include "gameplay/map.h"
#include "menu.h"
#include "weapon_data.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory menu tasks, item panels, prompts and their shared descriptors.

extern UiObjectDesc D_8010D6D8;

/// List-item callback for an inventory row. Looks up
/// `Gp_MoveScanSrc[owner->spawnArg1.value]` at `field_8`, highlights the move-source
/// row in `0x37A78`, draws the item (and ammo count for ids `0xA0..0xBF`),
/// then on confirm either opens the stack/info popup (`owner->state == 1`)
/// or starts a move / restriction prompt.
void Gp_ItemMoveRow(UiList* arg0, UiObject* arg1);

/// Move action in `Gp_ItemActionFns`. Draws `Gp_StrMove2`, checks destination
/// capacity and item/equipment restrictions, then opens a prompt or quantity
/// selector, or transfers the selected stack and sets `result` to confirm.
void func_800BD6DC(UiList* arg0, UiObject* arg1);

/// List-item confirm for `Gp_ItemActionFns`. Draws `Gp_StrSwitch`, then on confirm
/// looks up the selected inventory row and inlines `Gp_ItemUseRestricted` against
/// `owner->parent->flags`. A true result opens prompt `0x1E`; dest inventory
/// (`spawnArg1 == 1`) plus an equipped weapon/armor (`field_21+0x7F` /
/// `field_23+0x5F`) opens prompt `7`; otherwise `result = 0x23`.
void Gp_ItemActionConfirm(UiList* arg0, UiObject* arg1);

/// First state of the `D_80096E70` dispatcher. `spawnArg2` is the picked-up
/// object's `Enemy`: copies the low byte of its `placeKey` and its `workType`
/// into `Gp_PubItemId` / `Gp_PubItemLoc`, remaps owned 0x60–0x7F
/// items to 0xD and 0x80–0x9F items to 0x3D, then publishes a stack
/// count in `Gp_PubItemQty`.
void Gp_PublishItemObj(Task* arg0);

extern UiObjectDesc D_8010D348;

extern UiObjectTaskFunc D_8010D3A0[96];

void Gp_MenuExitCallback(Task* arg0);

void Gp_ItemMenuInit(UiObject* arg0, Task* arg1);

void Gp_ItemMenuTask(Task* arg0);

/// Per-stage table of `MenuMapArea` arrays. Index is `GameSession.location.loc.stage - 1`.
extern MenuMapArea* Gp_MapRecTables[];

/// Per-stage table of `MenuMapAreaName` arrays. Index is `GameSession.location.loc.stage - 1`.
/// A NULL entry skips the name draw (`Gp_DrawMapName`).
extern MenuMapAreaName* Gp_MapNameTables[];

/// Per-stage table of `MenuMapAreaShape` arrays. Index is `GameSession.location.loc.stage - 1`.
extern MenuMapAreaShape* Gp_MapMarkTables[];

/// Per-stage table of `MenuMapIcon` arrays. Index is `GameSession.location.loc.stage - 1`.
extern MenuMapIcon* D_8010F0CC[];

/// Per-stage table of `MenuMapMarker` arrays. Index is `GameSession.location.loc.stage - 1`.
extern MenuMapMarker* D_8010F0E0[];

/// Per-stage table of GameFlag nibble ids, indexed by room (`Gp_MapRoomId`).
/// Index is `GameSession.location.loc.stage - 1`.
extern u8* Gp_MapFlagIds[];

/// Highest selectable map room id per stage. Index is `GameSession.location.loc.stage - 1`.
extern u8 D_8010F130[];

/// Map-screen child prompt spawned by `Gp_MapTaskState2`.
extern UiObjectDesc D_8010F15C;

/// Per-stage `MenuMapAreaShape` counts. Index is `GameSession.location.loc.stage - 1`.
extern u8 Gp_MapMarkCounts[];

/// Current room id copied from `MenuMapArea.page` by `Gp_GetMapRoomId`.
extern u8 Gp_MapRoomId;

/// Room-id offset applied by `Gp_EnqueueMapRoomCd` (0, or 1 / 3 for two flagged rooms).
extern u8 Gp_MapRoomOff;

/// Shared row callback and the fire/wind/water/earth panel captions.
extern UiListRowCallback D_8010F620[1];

extern u8* D_8010F644[4];

/// Four P.Energy slot lists; navigation selects adjacent elements of this array.
extern UiList D_80114DF8[4];

extern u8 D_8010F13D;

extern UiObjectDesc D_8010F140;

extern UiObjectDesc D_8010F178;

extern char Gp_StrDiscard2[];

extern char Gp_StrItem2[];

extern char Gp_StrExamine[];

extern char Gp_StrPush[];

extern char Gp_StrRevive[];

extern char Gp_StrStrengthen[];

extern char Gp_StrCancel2[0xC];

extern u8 Gp_StrNoWeaponEq[];

extern char Gp_StrAreaEffect[];

extern char Gp_StrCastCost[];

extern char Gp_StrAtpLoss[];

extern u8* Gp_NoticeTexts[];

extern u8* Gp_PromptTexts[];

extern UiList D_8010F5D0;

extern UiList D_8010F5FC;

extern UiObjectDesc D_8010F670;

extern s32 D_80114E88;

extern s32 D_80114E8C;

extern s32 D_80114E90;

extern s32 D_80114E94;

extern const TaskFuncTable4 Gp_MapTaskStates;

/// "Help". The three bytes after the terminator are not zero: the original
/// toolchain left them in the alignment gap.
extern const char Gp_StrHelp[8];

extern const char Gp_StrUse2[];

extern const char Gp_StrKeyItem2[];

extern const char Gp_StrMap[];

extern const char Gp_StrAttention2[];

extern const char Gp_StrNotice3[];

extern const char Gp_StrNextLevel[];

extern const char D_8009720C[];

extern const char Gp_StrCost[];

extern const char Gp_StrBonus[];

extern const char D_80097220[];

/// "Specifications". The byte after the terminator is not zero: the original
/// toolchain left it in the alignment gap.
extern const char Gp_StrSpecs2[16];

/// Applies a weapon add-on and shows the previous weapon, result and returned items.
///
/// object and task must remain live; spawnArg1 selects SMG/Rifle Clip Holder,
/// Snail Magazine, Hammer, Pyke, Javelin, M203 or M9. State 0 consumes the
/// selected carried row in `Gp_SelItemRec`, changes the carried weapon variant,
/// transfers its loads and gives back displaced add-ons. The task owns the
/// allocated result work. Refusal delegates to `itemMenuNoticeTask` without
/// consuming the item. Active result updates count down from 188; timeout or
/// Confirm/Cancel returns DISMISS, and Menu returns CANCEL.
void itemMenuApplyWeaponAddonPanel(UiObject* object, Task* task);

/// Draws a carried consumable's quantity remaining outside weapon loads.
///
/// A NULL row or an id outside 0xA0..0xBF draws nothing. row is borrowed from
/// the live carried range; loaded rounds/supply units are subtracted from qty.
/// Uses `itemMenuDrawQuantity`'s row pixels and GPU-storage contract, with the
/// origin-Y subtraction promoted to s32 rather than wrapping to u16. Drawing
/// is independent of panel visibility. unused is ignored; colorRgb is 24-bit RGB.
void itemMenuDrawUnloadedConsumableQuantity(const UiObject* object, s32 x, s32 y, const InventoryItemRow* row, s32 colorRgb, s32 unused);

void Gp_CheckItemInfoButton(UiObject* arg0);

/// Draws an item row with a recessed icon slot, including an empty slot for id 0.
///
/// Uses `itemMenuDrawItemRow`'s coordinates, catalogue and drawing contract.
/// The 14-pixel icon frame is drawn even when the panel is hidden; an empty
/// slot uses a dark fill, while a populated slot has an unfilled frame.
void itemMenuDrawItemSlotRow(const UiObject* object, s32 x, s32 y, s32 itemId, s32 colorRgb, s32 attachmentState);

/// Requests and draws the selected attachment's equipment-scale item preview.
///
/// loadProfile must be 0..2; selection updates use its low byte under
/// `itemMenuSetPreviewItem`'s contract. A nonzero item is requested only when
/// either control half denotes ACTIVE. Id 0 or a non-idle CD queue suppresses
/// the picture while retaining its frame. The preview starts at contentLeft/Top
/// + 2 pixels and uses `itemMenuDrawPreview`'s resource contract. unusedList
/// is ignored; object and the preview resources are borrowed for this update.
void itemMenuUpdateSelectionPreview(UiList* unusedList, const UiObject* object, s32 itemId, s32 loadProfile);

void Gp_ItemCmdMenuTask(Task* arg0);

/// Applies an inventory recovery item or Healing P.E., then shows restored stats.
///
/// object and task must remain live through the panel updates. healingId is
/// Recovery1/2/3 (1..3), Cola (5), MP Boost1/2 (6..7), Ringer's Solution (61),
/// or a validated packed Healing P.E. id. Item use borrows `Gp_SelItemRec`
/// and consumes one only when the affected HP/MP is below maximum. Spell use
/// requires the caller to check MP affordability. State 0 applies the effect
/// once; later updates wait for the displayed HP/MP to catch up, then count
/// down from 188 on active updates. Confirm/Cancel or timeout reports DISMISS and
/// synchronizes the displayed values with live stats.
void itemMenuApplyHealingPanel(UiObject* object, Task* task, s32 healingId);

/// Consumes a selected item, raises one ordinary P.E. level and refills MP.
///
/// itemId must be an ordinary P.E. item id 15..50; it identifies one of twelve
/// abilities and a level 1..3. State 0 consumes `Gp_SelItemRec` once, raises the
/// saved level only if lower, recalculates maximum MP and fills live/displayed
/// MP. object and task must stay live. The 188-update countdown runs even while
/// inactive; active Menu returns CANCEL, and timeout or Confirm/Cancel returns
/// DISMISS. Item selection and carried-row validity are the caller's responsibility.
void itemMenuInvokeParasiteEnergyItem(UiObject* object, Task* task, s32 itemId);

/// Uses an elemental item to invoke the next available level in its P.E. group.
///
/// spawnArg1 must be Skull Crystal, Medicine Wheel, Holy Water or Ofuda
/// (54..57), selecting one of four three-ability groups. State 0 chooses the
/// lower of the first two levels, preferring the first on a tie; when both
/// reach 3 it chooses the third. An entirely capped group still invokes its
/// third ability at level 3. The derived ordinary item id is retained in
/// extraState for later updates under `itemMenuInvokeParasiteEnergyItem`'s
/// live-object, selected-row and dismissal contract.
void itemMenuInvokeElementBoostPanel(UiObject* object, Task* task);

/// Updates the shared OK, Cancel or Yes/No dialog list and publishes its answer.
///
/// task->spawnArg2 is its live UiObject. The first spawn argument selects an
/// `ITEM_MENU_DIALOG_*` layout and optional system cursor sound; other low-nibble
/// values select Yes/No with Yes selected. State 0 sizes and positions the list.
/// Active confirmation returns CONFIRM with a `USER_INTERFACE_LIST_COMMAND_*`
/// resultValue. Each update clears result before polling the shared list.
void itemMenuDialogTask(Task* task);

/// Runs the four elemental P.E. lists and their EXP/current/maximum-MP header.
///
/// spawnArg2 borrows the live task-owned UiObject. State 0 spawns the four
/// element children, with only the first active. Child CONFIRM sets resultValue
/// to 1; child CONFIRM/CANCEL propagates its result. Multiple child results
/// are processed in sibling-ring order without closing any children here.
void itemMenuParasiteEnergyListTask(Task* task);

/// Draws occupied carried rows against capacity in the inventory's Total header.
///
/// spawnArg2 borrows the live task-owned UiObject. `Gp_ItemCountShow` values
/// 1/0 request hiding/reopening, with a sixteen-tick reopening delay. Counts
/// are inventory rows, including equipment rows, rather than item quantities.
/// Resets result to NONE and draws the header independently of its visibility.
void itemMenuInventoryCountTask(Task* task);

/// Runs the published pickup's title, confirmation or inventory-full child panels.
///
/// spawnArg2 borrows the live task-owned UiObject. Published id/quantity must
/// remain stable until teardown. A nonzero spawnArg1 collects the item bit
/// immediately and opens a timeout-only obtained notice with a Yes answer;
/// zero checks inventory capacity and asks Yes/No or shows the full warning.
/// CONFIRM closes that child and activates the root; CANCEL propagates its
/// result and answer. Ordinary ids passed to the capacity predicate must obey
/// `inventoryCanAddItem`'s catalogue contract.
void itemPickupPanelTask(Task* task);

/// Opens a Yes/No child menu with Yes selected, transferring input from parent.
///
/// Uses `itemMenuSpawnYesNoMenuDefaultNo`'s positioning, ownership, failure and
/// result contract, with the initial selection on Yes instead.
UiObject* itemMenuSpawnYesNoMenu(UiObject* parent);

void Gp_SpawnPickupUiTask(Task* arg0);

/// Finishes a placed-object prompt, records an accepted pickup and closes its UI.
///
/// task->spawnArg1 borrows the live root UiObject and spawnArg2 its source Enemy.
/// CANCEL plus a Yes answer represents a collected item after its notice ends:
/// clear the source kind, then share the CONFIRM closing path. Ordinary pickup
/// banks 0/1 store place state 2 on Yes, preserving replenishable state 3.
/// Both closing paths release the UI hold and start a twelve-update delay before
/// the next pickup dispatcher state restores frame timing.
void itemPickupHandleResultTask(Task* task);

/// Waits for pickup closing to finish, then restores two-VBlank gameplay timing.
///
/// Counts down once per dispatcher update. At zero, advances to the exit state
/// with a one-update delay; the following state releases the UI primitive buffer.
void itemPickupRestoreFrameTimingTask(Task* task);

void Gp_PickupExitTask(Task* arg0);

/// 10-byte records selected by `damageGetPlayerAttackHitCooldown` when the id's 0x8000 bit is
/// clear. Indexed by `id & 0x7F`.
extern WeaponAttackRow Gp_IdParamLo[];

/// Sends the selected collected item to the room and displays its use result.
///
/// spawnArg2 borrows the live task-owned UiObject. The collected-item list must
/// have a valid selection and a live room task must be registered when state 0
/// dispatches ROOM_MESSAGE_USE_KEY_ITEM.
/// A used reply shows an item-name notice for up to 188 nominal 60-Hz ticks;
/// a no-notice reply starts hiding immediately. Every other reply shows refusal.
/// Acceptance of a used notice or Menu returns CANCEL. Refusal acceptance returns
/// CONFIRM during a cutscene hold, otherwise DISMISS. Uses the panel's active
/// state for input and timeout acceptance; the countdown also runs while inactive.
void itemMenuUseKeyItemTask(Task* task);

/// Runs a collected item's Use command menu and resolves its child panels.
///
/// spawnArg2 borrows the live task-owned UiObject. Fits the singleton command
/// list to a 96-pixel-wide panel on state 0. Cancel returns CONFIRM, Menu or a
/// child's CANCEL propagates CANCEL. A child's CONFIRM closes that child and
/// restores command input; its DISMISS returns CONFIRM to the parent item list.
void itemMenuKeyItemCommandTask(Task* task);

void Gp_DrawCollectedRow(UiList* arg0, UiObject* arg1);

void Gp_KeyItemMenuTask(Task* arg0);

/// Updates one element's PE list and transfers focus through the four sibling panels.
///
/// spawnArg1 selects fire/wind/water/earth (0..3); spawnArg2 is the live owned
/// object. Requires the four siblings in that order and saved levels 0..3.
/// The third ability appears when learned or when both preceding abilities
/// reach level 3. Horizontal moves retain the row, clamped to the target list;
/// vertical moves select its first/last row. Child acceptance restores input.
void itemMenuPeElementTask(Task* task);

void Gp_DrawUsePrompt(UiList* arg0, UiObject* arg1);

/// Releases an item-information panel and clears its published holder if still current.
///
/// The panel publishes its task-owned object in `D_80067634` when it starts.
/// An older panel's exit must not clear a replacement's holder. The standard UI
/// exit releases the object and its children even if the holder belongs elsewhere.
void itemMenuInfoTaskExit(Task* task);

/// Sizes, draws and polls a timed Notice panel for its owning task.
///
/// `spawnArg2.pointer` is the live task-owned UiObject. The low half of
/// `spawnArg1` selects text 0..32; a zero high half converts acceptance to
/// DISMISS, while a nonzero high half preserves CONFIRM. The counter begins at
/// 188 callback ticks and runs during opening and inactive frames too.
/// Active panels accept on timeout or Confirm/Cancel; Menu yields CANCEL.
void itemMenuNoticeTask(Task* task);

/// Loads or removes weapon consumables and displays a timed Reload notice.
///
/// `spawnArg2.pointer` is the live task-owned UiObject; state starts at zero.
/// Bits 0..7 of `spawnArg1.value` are the consumable id, bits 8..15 the weapon
/// id (0x80..0x9F); other bits are ignored. A nonzero consumable (0xA0..0xBF)
/// is identified and loaded to capacity from the live carried range. The range
/// must fit its table and contain the weapon; the load result is not checked.
/// Zero removes the loads selected by `Gp_ReloadMode` (1 primary, 2 secondary,
/// other values both), preserving built-in supplies. A single selected item's
/// id is copied into the argument's low byte before clearing for later drawing.
/// Starts a 188-tick counter, including opening/inactive callbacks. An active
/// panel yields DISMISS on timeout or Confirm/Cancel, and CANCEL on Menu.
void itemMenuReloadNoticeTask(Task* task);

/// Displays a timed Attach notice naming the item supplied in the task argument.
///
/// `spawnArg1.value` is a catalogue item id and `spawnArg2.pointer` the live
/// task-owned UiObject; state starts at zero. Sizes the panel and plays the
/// equipment sound once the panel opens. The 188-tick counter also advances
/// while opening or inactive. An active panel yields DISMISS on timeout or
/// Confirm/Cancel, and CANCEL on Menu. Attachment state is left intact.
void itemMenuAttachNoticeTask(Task* task);

/// Selects carried equipment and displays a timed Equip notice.
///
/// `spawnArg1.value` is a weapon id (0x80..0x9F) or armor id (0x60..0x7F), and
/// `spawnArg2.pointer` the live task-owned UiObject; state starts at zero.
/// A weapon and its previous selection, if any, must have carried rows. On a
/// changed weapon, the old row inherits the new row's positive armor slot;
/// otherwise its removable loads are cleared. The new weapon is detached and
/// identified. Changed armor refreshes statistics and detaches attachments.
/// The carried range must fit its table; armor mode and selector must satisfy
/// `equipmentEquipCarriedArmor`'s contract. The sound waits for the open panel;
/// the 188-tick counter includes opening/inactive callbacks. Active panels yield
/// DISMISS on timeout or Confirm/Cancel, and CANCEL on Menu.
void itemMenuEquipNoticeTask(Task* task);

/// Draws two identified PE description lines and the learned level's casting-cost bar.
///
/// `abilityId` is a packed PE catalogue id in 0x300..0x3FF. Its low two bits
/// select the level; zero still draws the description but omits the cost.
/// Borrows the live object's panel and queues text and bar primitives this frame.
void itemMenuDrawAbilityDescription(UiObject* object, s32 abilityId);

void Gp_DrawSortCmd(UiList* arg0, UiObject* arg1);

void func_800CF148(UiObject* arg0, Task* arg1);

void func_800CF090(UiList* arg0, UiObject* arg1);

void Gp_SizeEquippedPanel(UiPanel* arg0, s32 arg1);

void func_800CF6E8(UiObject* arg0, s32 arg1);

void Gp_DrawPeEnergyCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawOptionCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawExitCmd(UiList* arg0, UiObject* arg1);

void Gp_WeaponSummaryTask(Task* arg0);

/// Draws one scrolling item-information description row.
///
/// The owner's low spawn halfword is the item id. Rows 0/1 of ordinary items
/// use identified catalogue descriptions; remaining rows, and all key-item
/// rows, read the loaded text chunk from line 5 onward. The caller must keep
/// that chunk ready and readable through the selected line. Row indices are
/// nonnegative; the initial two-row test retains signed-byte narrowing.
void itemMenuDrawDescriptionRow(UiList* list, UiObject* object);

void Gp_DrawUseCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawMovePrompt(UiList* arg0, UiObject* arg1);

void Gp_DrawExchangeSlotCmd(UiList* arg0, UiObject* arg1);

/// Applies the selected inventory recovery item through its task-owned healing panel.
///
/// Borrows the live `Gp_SelItemRec`; its item id must satisfy
/// `itemMenuApplyHealingPanel`'s inventory-item contract. The dispatch supplies
/// object == task->spawnArg2.pointer and task == object->owner.
void itemMenuApplySelectedHealingItem(UiObject* object, Task* task);

void func_800CFA60(Task* arg0);

void func_800CFAA8(UiObject* arg0, Task* arg1);

/// Draws OK and publishes the list's OK command when its active row is confirmed.
void itemMenuDrawOkRow(UiList* list, UiObject* object);

/// Draws Cancel and publishes the list's Cancel command on Confirm.
void itemMenuDrawCancelRow(UiList* list, UiObject* object);

/// Draws Yes; Confirm accepts it, while Cancel moves the active list to the No row.
void itemMenuDrawYesRow(UiList* list, UiObject* object);

/// Draws No and accepts that command on either Confirm or Cancel on its active row.
void itemMenuDrawNoRow(UiList* list, UiObject* object);

void Gp_MapTaskState2(Task* arg0);

void Gp_HelpPanelTask(Task* arg0);

void Gp_DrawMapName(Task* arg0);

void Gp_MapTask(Task* arg0);

void Gp_MapPanelInit(Task* arg0);

void Gp_MapFirstDrawTask(Task* arg0);

void Gp_MapDrawTask(Task* arg0);

void Gp_PeMenuListTask(Task* arg0);

/// Draws Revive for an unlearned PE ability or Strengthen for a learned one.
///
/// The owner's spawnArg1 is the packed ability id, with level 0..3 in its
/// low two bits. Active Confirm opens an EXP purchase dialog for levels 0..2,
/// or a maximum-level notice for level 3, then suspends this panel's input.
void itemMenuDrawPeUpgradeRow(UiList* list, UiObject* object);

/// Runs the selected PE ability's Revive/Strengthen and Cancel command list.
///
/// spawnArg1 is a packed PE id; spawnArg2 borrows its live owned object.
/// State 0 sizes the shared two-row list and loads the next-level preview
/// unless already at level 3. Cancel returns CONFIRM; Menu and child CANCEL
/// propagate CANCEL. Child CONFIRM closes the child and restores input;
/// child DISMISS finishes this command menu with CONFIRM.
void itemMenuPeCommandTask(Task* task);

/// Rejects a restricted discard or confirms removal of the selected whole stack.
///
/// Borrows the stable selected row and a live task-owned object with a parent
/// command panel. Protected items, loaded consumables and selected equipment
/// show a notice. Otherwise a default-No dialog offers Yes/No; Yes clears
/// the item's saved equipment selections and removes the whole carried stack.
/// Either answer confirms the parent command panel.
void itemMenuDiscardTask(Task* task);

/// Draws one PE ability and its learned level, with command and specification input.
///
/// The owner selects element 0..3; currentItemIndex selects ability 0..2;
/// saved levels are 0..3. Level zero dims the row. Selection updates the
/// help/preview; Confirm opens the upgrade commands, Triangle specifications.
/// The command dialog takes input only after a successful spawn.
void itemMenuDrawPeAbilityRow(UiList* list, UiObject* object);

/// Shows and confirms the EXP purchase of the selected PE ability's next level.
///
/// spawnArg1 is a packed PE id at level 0..2; spawnArg2 is the live owned object.
/// EXP cost uses the next level, discounted to 4/5 in positive game modes or
/// 2/5 after a normal-game clear, then narrowed to its low 16 bits. Yes checks
/// affordability again, spends EXP, stores the next level and restores MP to
/// the recalculated maximum. Insufficient EXP shows a notice. No or completed
/// purchase returns DISMISS; the next-level preview is a child panel.
void itemMenuPeUpgradeTask(Task* task);

void Gp_MapMenuListTask(Task* arg0);

void Gp_MapScreenTask(Task* arg0);

/// Draws Use, publishes menu command 6 on Confirm and supplies the selected-row help.
///
/// The command opens the carried inventory/attachment list. Help is shown for
/// active input or suspended active mode; only an active list row handles input.
void itemMenuDrawUseAttachCommandRow(UiList* list, UiObject* object);

/// Draws Key Item, publishes menu command 8 on Confirm and supplies selected-row help.
///
/// The command opens the collected key-item list. Help is shown for active
/// input or suspended active mode; only an active list row handles input.
void itemMenuDrawKeyItemCommandRow(UiList* list, UiObject* object);

/// Draws the PE release/strengthen menu's Cancel row and confirms the parent on selection.
///
/// Writes the object's result, without publishing a list command id.
void itemMenuDrawPeCancelRow(UiList* list, UiObject* object);

void Gp_DrawMapCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawDiscardCmd(UiList* arg0, UiObject* arg1);

/// Draws a PE ability's next-level preview, parameter comparisons and descriptions.
///
/// spawnArg1 is a packed PE id at level 0..2; spawnArg2 is the live owned
/// preview object. The title draws with active styling while preserving its
/// control word. Level-zero abilities suppress previous-level comparison.
/// This child remains passive; its parent owns purchase input.
void itemMenuPeNextLevelTask(Task* task);

/// Applies a task-selected recovery item or Healing PE through the healing panel.
///
/// spawnArg2 is the live owned object; spawnArg1 must satisfy
/// `itemMenuApplyHealingPanel`'s healing-id and MP-affordability contract.
void itemMenuHealingTask(Task* task);

/// Draws the selected PE ability's specifications and polls dismissal input.
///
/// spawnArg1 is its packed id, level 0..3; spawnArg2 is the live owned object.
/// Unlearned abilities show level-one parameters. Once the preview resource
/// is ready, its text from line 4 onward supplies the extended description.
/// Active Confirm/Cancel/Triangle returns CONFIRM; Menu returns CANCEL.
void itemMenuPeSpecificationsTask(Task* task);

void Gp_DrawExaminePushCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawItemCmd(UiList* arg0, UiObject* arg1);

void func_800D5A48(Task* arg0);

extern char Gp_StrReleasePe[];

void Gp_DrawWeaponSlotRow2(UiList* prompt, UiObject* obj);

void func_800C41A4(UiList* prompt, UiObject* obj);

/// Drawing modifiers for itemMenuDrawItemIcon; unknown bits are ignored.
enum {
    ITEM_MENU_ICON_DEFAULT          = 0,
    ITEM_MENU_ICON_FORCE_IDENTIFIED = 1,
    ITEM_MENU_ICON_ENLARGED         = 2,
    ITEM_MENU_ICON_DIMMED           = 4,
    ITEM_MENU_ICON_HIGHLIGHTED      = 8
};

/// Item-row attachment states: only 2 requests the A fallback status mark.
enum {
    ITEM_MENU_ATTACHMENT_MARK_AUTOMATIC  = 0,
    ITEM_MENU_ATTACHMENT_MARK_UNATTACHED = 1,
    ITEM_MENU_ATTACHMENT_MARK_ATTACHED   = 2
};

/// Queues an inventory or packed P.E. icon relative to a panel's content origin.
///
/// x/y are pixels at the icon's bottom-left. itemId must be a valid inventory
/// id (0 is an empty slot), or a synthetic P.E. id. Inventory ids and packed P.E.
/// ids 0x300..0x33F choose their category/ability atlas cells; higher P.E.
/// ids use the fallback cell. ITEM_MENU_ICON_* flags force identification,
/// enlarge by two pixels per edge, dim the icon or draw its highlight tile.
/// Requires resident menu textures and writable OT/primitive storage.
void itemMenuDrawItemIcon(const UiObject* object, s32 x, s32 y, s32 itemId, s32 flags);

/// Draws a P.E. level mark and signed decimal level at the right of an item row.
///
/// x/y are panel-relative row pixels; the mark starts at x + 108 and the
/// number ends at x + 124. Callers normally supply levels 1..3; level must
/// satisfy `textItoaSigned`'s input range.
/// colorRgb is packed 24-bit RGB. Requires menu textures and GPU storage.
void itemMenuDrawParasiteEnergyLevel(const UiObject* object, s32 x, s32 y, s32 level, s32 colorRgb);

/// Draws the E, L or A status mark beside an inventory item row.
///
/// x/y are panel-relative row pixels. E means equipped armor/weapon or a
/// selected consumable, irrespective of remaining load. Otherwise L means
/// the weapon has a nonzero load whose consumable is carried; A is the
/// fallback only when attachmentState is ITEM_MENU_ATTACHMENT_MARK_ATTACHED.
/// The equipped path submits E twice. Requires text textures and GPU storage.
void itemMenuDrawEquipmentMarker(const UiObject* object, s32 x, s32 y, s32 itemId, s32 attachmentState);

/// Draws HP/MP values and meters plus EXP and BP in a menu panel.
///
/// topOffset is a pixel offset below contentTop, before the eight-pixel inset.
/// Each call advances displayed HP/MP upward by one toward live values;
/// decreases are not applied here. Maximums must be positive. Requires
/// initialized display values, menu/text textures and writable GPU storage.
void itemMenuDrawPlayerStats(const UiPanel* panel, s32 topOffset);

/// Draws the equipped weapon and its available consumable-load rows.
///
/// x/rowY are content-relative pixels at the weapon row's baseline. The live
/// weapon selector must be 0..32. Empty slots retain their row, quantities are
/// drawn only for nonempty loads, and the Tonfa Baton has no load rows.
/// object is borrowed unchanged; menu textures, text and writable GPU storage
/// must be ready. unused is ignored by this drawing routine.
void itemMenuDrawWeaponSummary(const UiObject* object, s32 x, s32 rowY, s32 unused);

void Gp_UiPromptDispatch(UiObject* arg0, Task* arg1);

void Gp_StatusPanelTask(Task* arg0);

void Gp_HpMpBarTask(Task* arg0);

/// Draws the equipped armour's HP/MP bonuses and attachment-slot overview.
///
/// task->spawnArg2 supplies the live UiObject. The armour selector must be
/// 0..32; zero draws only the title and separator. Up to ten attachment slots
/// are shown five per line, using the first carried row at each stored slot
/// without checking its quantity. The carried range must fit its item table.
/// Clears the object's result each frame; requires menu textures and GPU space.
void itemMenuArmorSummaryTask(Task* task);

/// Draws the menu's four-element, three-ability Parasite Energy summary.
///
/// task->spawnArg2 supplies the live UiObject. Learned levels come from the
/// current save or training table. The third ability is shown when learned
/// or when both preceding abilities reach level three. Marks and captions
/// share the panel's ordering-table layer; the result is cleared each draw.
void itemMenuParasiteEnergySummaryTask(Task* task);

void Gp_DrawItemOrderRow(UiList* arg0, UiObject* arg1);

/// Sets row counts for carried weapons or their compatible consumable loads.
///
/// consumableItemId == INVENTORY_ITEM_NONE lists every carried weapon with
/// four visible rows. Otherwise each matching primary/secondary load adds
/// one row only for an attached or equipped weapon; a weapon can add two.
/// Nonzero ids must be consumables 0xA0..0xBF. Counts narrow to the list's
/// bytes, all counted rows are visible, and row height is fifteen pixels.
/// The live carried range must fit its item table; no pointer is retained.
void itemMenuSetWeaponChoiceRows(UiList* list, s32 consumableItemId);

void Gp_ItemDestCursorTask(Task* arg0);

void Gp_DrawWeaponSlotRow(UiList* prompt, UiObject* obj);

/// Updates the equipped-weapon and load-slot pane of the item destination menu.
///
/// task->spawnArg2 supplies the live UiObject, with an inventory-pane parent.
/// State zero initializes the shared list; every frame refreshes available rows.
/// Right transfers focus at the cursor's screen Y, Down at the list end moves
/// to armour, Cancel returns to the parent, and Menu publishes CANCEL. Child
/// confirmations restore focus; command 0x23 also enters item-swapping mode.
/// The pane hides while the item-detail panel is open. Requires the live menu
/// tree, shared lists and drawing resources for the task's lifetime.
void itemMenuWeaponPanelTask(Task* task);

void Gp_ArmorMenuTask(Task* arg0);

/// Three-entry dispatcher table: `Gp_ItemMenuInit`, `Gp_UiPromptUpdate`, `Gp_UiPromptDispatch`.
extern const UiObjectTaskFuncTable3 Gp_ItemMenuStates;

extern char Gp_StrUsedDot[];

extern char Gp_StrCreatedDot[];

/// Column caption table drawn across the top of the P.Energy attach panel.
extern MenuParasiteEnergyCaption D_8010E844[4];

/// Holder text for a weapon slot with no ammunition loaded.
extern char Gp_StrAmmoNone[];

extern char Gp_StrAttachNone[];

extern UiList D_8010E910;

extern u8* D_8010E7C0[];

extern u8* Gp_CaliberNameTbl[];

extern u8* Gp_FeatNameTbl[];

extern u8** D_80114D80;

extern u16 Gp_ItemCountShow;

extern s32 D_80114D88;

extern s32 Gp_ItemOrderMode;

extern s32 Gp_ReloadMode;

extern UiObject* D_80114D98[2];

extern s32 Gp_AttachListIds[6];

extern char Gp_StrNoUseNow[];

extern char Gp_StrUsed[];

extern char Gp_StrSelectDest[];

extern char Gp_StrEquipped[];

extern char Gp_StrObtained[];

extern char Gp_StrLoaded[];

extern char Gp_StrRemoved[];

extern char Gp_StrRemovedAmmo[];

extern char Gp_StrInvoked[];

extern char Gp_StrUse[];

extern char Gp_StrMove[];

extern char Gp_StrRemoveAmmo[];

extern char Gp_StrLoad[];

extern char Gp_StrExchange[];

extern char Gp_StrRemoveArmor[];

extern char Gp_StrYes[];

extern char Gp_StrNo[];

extern char Gp_StrOk[];

extern char Gp_StrCancel[];

extern char Gp_StrPickupAsk[];

extern char Gp_StrInvFull[];

extern char D_8010E588[];

extern char Gp_StrSort[];

extern char Gp_StrAmmoCaps[];

extern char Gp_StrDot[];

extern UiList D_8010E820;

extern UiList D_8010E854;

extern UiList D_8010E884;

extern UiList D_8010E8AC;

extern UiList D_8010E8D4;

extern s32 Gp_PreviewItems[5];

extern UiList D_8010E938;

extern UiList D_8010E960;

extern u8* D_8010E984[3];

extern u8* D_8010E990[1];

extern u8* D_8010E994[3];

extern UiList D_8010E9A4;

extern UiList D_8010E9CC;

extern UiList D_8010E9F4;

extern UiListRowCallback Gp_ItemCmdFns[6];

extern UiList D_8010EA30;

extern UiListRowCallback Gp_DialogCmdFns[2];

extern UiList D_8010EA74;

extern UiObjectDesc D_8010EA98;

/// CLUT ids for the ten item-category icons drawn by `itemMenuDrawItemIcon`,
/// indexed by the icon index that function derives from the item id.
extern const u16 D_80096F88[12];

extern const char Gp_StrAddHp[];

extern const char Gp_StrAddMp[];

extern const char Gp_StrPEnergy[];

extern const char Gp_StrOption[];

extern const char Gp_StrExit[];

extern const char Gp_StrSlash[];

extern const char Gp_StrHp[];

extern const char Gp_StrMp[];

extern const char Gp_StrExp[];

extern const char Gp_StrBp[];

extern const char Gp_StrArmor[];

extern const char Gp_StrAttachments[];

extern const char Gp_StrWeaponTitle[];

extern const char Gp_StrE[];

extern const char Gp_StrItemHdr[];

extern const char Gp_StrAttachments2[];

extern const char Gp_StrNextReplay[];

extern const char Gp_StrSpecs[];

extern const char Gp_StrOperation[];

extern const char D_8009707C[];

extern const char Gp_StrAttachments3[];

extern const char Gp_StrSpecialFeat[];

extern const char Gp_StrPowerCaps[];

extern const char Gp_StrCapacity[];

extern const char Gp_StrSpecial[];

extern const char Gp_StrApplicableWpn[];

extern const char Gp_StrNotice[];

extern const char Gp_StrKeyItem[];

extern const char gGpStrWeight[];

extern const char gGpStrRate[];

extern const char gGpStrRange[];

extern const char gGpStrPower[];

extern const char gGpStrAttachDot[];

extern const char Gp_StrAttention[];

extern const char Gp_StrSelectWeapon[];

extern const char Gp_StrEquip[];

extern const char Gp_StrSelectAmmo[];

extern const char Gp_StrSelectArmor[];

extern const char Gp_StrReload[];

extern const char Gp_StrAttach[];

extern char Gp_StrCustomizeHelp[];

extern char Gp_StrRemoveAmmoHelp[];

extern char Gp_StrChangeOrderHelp[36];

/// Queues an item's display-resource load, replacing requests for the same profile.
///
/// The low byte of `loadProfile` must be `CD_COMMAND_DISPLAY_LOAD_MENU`,
/// `CD_COMMAND_DISPLAY_LOAD_PREVIEW` or `CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW`
/// (0..2); higher bits are ignored. The latest request matching each other
/// profile's load policy and image offsets is requeued in profile order. The
/// active head is retained; other tail requests are discarded. Matching inspects
/// load arguments without checking the opcode and includes the head.
///
/// Item 0, negative ids and ids 0x180..0x2FF do nothing. Ordinary/key items below
/// 0x180, packed PE ids 0x300..0x4FF and high display ids >=0x500 select separate
/// resource groups; the final file index is narrowed to one byte. A completed
/// scene payload, or debug mode -1 outside demo 12, suppresses all changes.
/// Profiles 1 and 2 mark room resources for restoration when leaving the menu.
/// Requires the scratch-stack and CD-ring capacity of `cdCmdEnqueueDisplayResource`.
/// Saved requests are copied locally; no queue-entry pointer is retained.
void itemMenuEnqueuePreviewLoad(s32 itemId, s32 loadProfile);

void Gp_DrawRemoveArmorRow(UiList* prompt, UiObject* obj);

void Gp_EquipSelectMenuTask(Task* arg0);

/// Source item-table scan (`Gp_CanMoveItems` / item-move UI). field_0 is the
/// start index, field_1 the entry count, field_2 the table id.
extern InventoryItemRange Gp_MoveScanSrc;

/// Dest item-table scan immediately after `Gp_MoveScanSrc` (`Gp_CanMoveItems`).
extern InventoryItemRange Gp_MoveScanDst;

/// Pair of inventory UiLists indexed by `Task::spawnArg1` (source / dest).
/// `selectedItemIndex` is the range-relative row passed to `inventoryGetRow`.
extern UiList Gp_InvLists[];

/// Action-button callbacks for `Gp_ItemActionList`, filled by `Gp_FillItemActions`.
extern UiListRowCallback Gp_ItemActionFns[];

extern u8 Gp_StrAll[];

extern u8 Gp_StrSelect[];

extern u8 Gp_StrDiscard[];

extern u8 Gp_StrEnd[];

extern u8 Gp_StrMove2[];

extern char Gp_StrSetAmmoHelp[];

extern char Gp_StrAmmoLocked[];

extern char Gp_StrMaxCapacity[];

extern char Gp_StrSwitch[];

s32 Gp_CanMoveItems(void);

void Gp_AmmoListTask(Task* arg0);

void func_800C7DA8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

void Gp_EquipSummaryTask(Task* arg0);

void Gp_DrawAmmoRow(UiList* prompt, UiObject* obj);

void Gp_SelectWeaponMenuTask(Task* arg0);

void Gp_DrawRemoveAmmoRow(UiList* prompt, UiObject* obj);

void Gp_BuildAttachList(UiList* arg0, s32 arg1);

#endif // GAMEPLAY_PRIVATE_ITEM_MENU_H
