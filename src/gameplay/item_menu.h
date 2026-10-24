#ifndef GAMEPLAY_PRIVATE_ITEM_MENU_H
#define GAMEPLAY_PRIVATE_ITEM_MENU_H

#include "types.h"

#include "gameplay/item_menu.h"
#include "gameplay/map.h"
#include "menu.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory menu tasks, item panels, prompts and their shared descriptors.

/// Bodyless root panel for container/carried-item transfers.
///
/// Spawn mode 1 presents battle rewards and opens the All/Select/Discard menu;
/// other modes begin item-box browsing. The published place kind selects a
/// valid container range. Only one screen may bind the shared transfer state
/// at a time. `uiSpawnObject` copies the recipe synchronously; the callback,
/// menu resources and selected ranges must stay live through panel teardown.
extern UiObjectDesc gItemMenuTransferScreenDescriptor;

/// Draws a transfer-pane inventory row and handles command or swap-partner input.
///
/// Borrows the shared row list and live task-owned object. Its owner carries pane
/// index 0/1; currentItemIndex and the pane's selected row must fit the copied
/// range and its live backing table. During swap selection, the source row is
/// highlighted. The selected row of an active or suspended-active pane updates
/// preview and description. Consumable rows show total quantities.
/// Browsing Confirm opens Move/Switch commands; Triangle on a nonempty row opens
/// details. Swap Confirm publishes the candidate row or opens a restriction
/// notice: cross-pane swaps reject restricted items, item-box ammunition and
/// equipped carried weapons/armor. Same-pane candidates bypass those checks.
/// Publishes the active row pointer for other menu callbacks without owning it;
/// the transfer screen and shared work must remain live. Requires loaded menu
/// resources and writable GPU primitive/ordering-table storage.
void itemMenuDrawTransferInventoryRow(UiList* list, UiObject* object);

/// Draws the transfer popup's Move row and handles its confirmation.
///
/// Borrows a live row list/object whose owner carries pane index 0/1; the pane's
/// selected row and both range backings must be valid. On active-row Confirm,
/// checks opposite-pane capacity and transfer/equipment restrictions. Item-box
/// ammunition opens the quantity panel; battlefield ammunition moves whole.
/// Other transferable rows move whole, clearing removable loads on unequipped
/// weapons first. Successful whole-row moves return CONFIRM; notices or an
/// allocated quantity child deactivate popup input. Requires menu/text resources
/// and writable GPU storage. Does not retain the list or object.
void itemMenuDrawTransferMoveRow(UiList* list, UiObject* object);

/// Draws Switch and requests selection of a transfer swap partner.
///
/// Borrows a live row list/object; its owner carries pane index 0/1 and its
/// parent is the inventory pane. That pane's selected row must be valid.
/// Active-row Confirm checks battle-field NO_DISCARD, the Acropolis M93R
/// restriction and selected carried weapon/armor, opening a notice when blocked.
/// Otherwise publishes the begin-swap command 0x23; this callback does not
/// exchange rows. Requires menu/text resources and writable GPU storage.
void itemMenuDrawSwitchRow(UiList* list, UiObject* object);

/// Publishes a placed object's pickup id, flag index and pack quantity.
///
/// spawnArg2 borrows the placed Enemy through this callback. Publishes its
/// placeKey low byte as the object flag index and workType as its place kind.
/// Kinds 0..0x9F publish one item; already-owned armor/weapon kinds become
/// Belt Pouch/Ringer. The other branch requires consumable kinds 0xA0..0xBF
/// and reads their pack quantities without a bounds check or bank normalization.
/// Producer exclusion of banked/reserved kinds from that read is unproven.
/// Marks publication ready, restores every-VBlank timing, clears the UI holder
/// and advances the pickup dispatcher after a one-tick countdown.
void itemPickupPublishPlacedObjectTask(Task* task);

/// Notice title for the belt-pouch armor attachment-slot confirmation panel.
///
/// The first seven bytes are the NUL-terminated label. The final stored byte
/// is ASCII F, with no observed text access or established purpose.
extern const char gItemMenuPouchNoticeLabel[8];

/// Item-use panel callbacks indexed by ordinary item id 0..95.
///
/// NULL slots have no supported use action. A callback receives the live panel
/// object and its owner task, whose spawnArg1 retains the item id; consuming
/// handlers also borrow the stable selected carried row. Gameplay owns these
/// callbacks for the panel's lifetime. Callers must enforce the index domain.
extern UiObjectTaskFunc gItemMenuUseHandlers[96];

/// Applies deferred item-use actions after menu resource restoration and exits.
///
/// Requires the live player task and restored actor/animation/session resources.
/// A signed pending consumable id selects primary (>0) or secondary (<0) reload
/// in battle; outside battle the id is discarded. Clears its notification flag,
/// but retains the id for the reload cue, even if scripted control rejects it.
/// Pending medicine presentation requests the player's item-use animation;
/// Eau de Toilette then requests Berserker, subject to equipment resistance.
/// Clears the presentation/item notifications, releases the menu display hold
/// and invokes task's exit callback. The caller must have acquired that hold.
void menuApplyPendingItemUseTask(Task* task);

/// Registers the menu caption panel and opens its initial command child.
///
/// object must be the live task-owned UiObject in task->spawnArg2. Entry state
/// is zero and work is empty. Registers the prompt holder before allocating
/// four cleared primary-heap bytes owned by the task; failure leaves state zero
/// for retry. The work bytes' purpose is unproven; only a later byte-zero clear
/// is observed. Scripted hold opens Key Items and clears preview selections;
/// otherwise opens Status. The child starts active after eight nominal 60-Hz
/// ticks. Child allocation failure still advances to caption update state one.
/// Fits two or one fifteen-pixel rows respectively, plus one content pixel,
/// preserving frame margins and placing the outer bottom at screen-centered
/// Y=104. Requires initialized panel layout and loaded menu resources.
void itemMenuInitializeCaptionTask(UiObject* object, Task* task);

/// Runs the menu caption panel's initialization, update or delayed command state.
///
/// task->state must be 0..2; dispatch has no bounds check. spawnArg2 holds
/// the live task-owned UiObject. State zero opens the initial child, one draws
/// the prompt and waits for its result, and two waits to open the selected
/// command before returning to one. Prompt payloads in spawnArg1 follow
/// itemMenuDrawTaskPrompt's contracts. Menu, text and UI resources must stay
/// loaded for dispatch; the selected callback may start closing the UI tree.
void itemMenuCaptionTask(Task* task);

/// Number of stage-minus-one entries in the menu map directories (stages 1..5).
enum { MENU_MAP_STAGE_COUNT = 5 };

/// Area-to-page and world-to-map mappings selected by stage minus one.
///
/// Requires stage 1..5 and that stage's map overlay to remain loaded. Within a
/// table, the live area ID indexes a record directly, including reserved slot
/// zero. Page-availability scans stop at the 0xFFFF page marker; Dryfield night
/// retains its missing-terminator scan into adjacent overlay data.
extern const MenuMapArea* gMenuMapAreaTables[MENU_MAP_STAGE_COUNT];

/// Current-area name tables selected by stage minus one.
///
/// All five entries are populated; the renderer still checks for NULL. Name
/// records use area minus one, so area must be nonzero and within the selected
/// table. Keep that stage's map overlay loaded while its text is borrowed.
extern const MenuMapAreaName* const gMenuMapAreaNameTables[MENU_MAP_STAGE_COUNT];

/// Area-fill shape tables selected by stage minus one.
///
/// Requires stage 1..5 and its map overlay. Area IDs index the selected table
/// directly; `gMenuMapAreaShapeCounts` bounds the ordinary walk including slot
/// zero. Neo Ark adds two substitution records beyond that walk. Each model
/// also requires the selected page's map-picture overlay to remain loaded.
extern const MenuMapAreaShape* const gMenuMapAreaShapeTables[MENU_MAP_STAGE_COUNT];

/// Page-zero-terminated icon lists selected by stage minus one.
///
/// Requires stage 1..5 and its map overlay to remain loaded. List indices fit
/// in a byte; records select an area and page rather than using either as an
/// array index. Each stage's complete list includes its page-zero terminator.
extern const MenuMapIcon* const gMenuMapIconTables[MENU_MAP_STAGE_COUNT];

/// Page-zero-terminated flag-marker lists selected by stage minus one.
///
/// Requires stage 1..5 and its map overlay to remain loaded. Each byte-sized
/// list index selects the same index in that stage's marker-flag word table;
/// the page-zero terminator has no corresponding flag lookup.
extern const MenuMapMarker* gMenuMapMarkerTables[MENU_MAP_STAGE_COUNT];

/// Page-shading and navigation flag tables selected by stage minus one.
///
/// Requires stage 1..5 and its loaded map overlay. A selected page indexes
/// its table directly, with slot zero reserved as the left boundary. Entries
/// are nibble flag IDs: 0 uses visited-area fallback, 0x80 rejects navigation,
/// and 0xFF stops navigation. Shading still tests the entry as a nibble ID;
/// Neo Ark disables navigation and uses 0x80 on its second page.
extern const u8* const gMenuMapPageFlagTables[MENU_MAP_STAGE_COUNT];

/// Inclusive upper page limits for navigation, selected by stage minus one.
///
/// The five values are 2, 3, 3, 6, 3. These do not bound every selectable
/// current-area page: Acropolis's isolated MIST page is 3 and stops navigation
/// through its 0xFF page flag. The selected page and the limit must both fit
/// the stage's separately sized page-flag table.
extern const u8 gMenuMapNavigationLastPages[MENU_MAP_STAGE_COUNT];

/// Recipe for the map's objective-help text panel.
///
/// Content is unused; it loads `gMenuMapHelpFileIndex` and adds an area-name
/// child. Bounds use screen-centered pixels and the ordering-table seed counts
/// tags. The map parent suspends its own input until this child completes.
extern const UiObjectDesc gMenuMapHelpPanelDesc;

/// Ordinary area-shape walk lengths selected by stage minus one.
///
/// Includes reserved area slot zero. The first four cover their whole shape
/// table; Neo Ark walks slots 0..33 and separately substitutes records 34/35
/// for Island/Bridge on the alternate altar picture.
extern const u8 gMenuMapAreaShapeCounts[MENU_MAP_STAGE_COUNT];

/// Selected picture page shared by the singleton map screen.
///
/// Stores the low byte of the current area's `MenuMapArea.page`, then changes
/// during navigation. Ordinary pages are 1..6; the no-page marker narrows to
/// zero, and an end marker would narrow to 0xFF. Shape/icon/marker comparisons
/// use a signed-byte view; page-table indices and file-number sums use the
/// unsigned byte. Map records and picture resources must stay loaded for use.
extern u8 gMenuMapSelectedPage;

/// Picture-file offset for the selected map page (0 ordinary, 1 or 3 alternate).
///
/// Recomputed on each picture load. Offset 1 selects Shelter reservoir page 6
/// before its event completes; offset 3 selects Neo Ark page 1 with altar
/// switch state 1 and enables the Island/Bridge shape substitutions. The map
/// file number is 30000 + stage*100 + selected page + this offset.
extern u8 gMenuMapPictureOffset;

/// Single shared row callback for every ability in each P.E. element list.
///
/// The element lists set `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK`, so entry
/// zero handles all two or three visible abilities. The writable table-pointer
/// type follows `UiList`; this table's callback is never replaced.
extern UiListRowCallback gItemMenuPeAbilityRowCallbacks[1];

/// Read-only P.E. list titles, indexed 0 Fire, 1 Wind, 2 Water, 3 Earth.
///
/// The strings and pointer table remain valid for the gameplay image's lifetime.
extern const char* const gItemMenuPeElementTitles[4];

/// Mutable P.E. ability lists, indexed 0 Fire, 1 Wind, 2 Water, 3 Earth.
///
/// Each element panel initializes and borrows its own list. The four sibling
/// panels occupy a two-by-two grid: vertical navigation changes the index by
/// one and horizontal navigation by two, within this array. Selection and
/// scrolling persist in this singleton storage until the next initialization.
extern UiList gItemMenuPeElementLists[4];

/// Current-objective byte latched as the map Help display-resource file index.
///
/// Starts at 1; opening Help replaces it with `EVENT_FLAG_CURRENT_OBJECTIVE`'s
/// whole byte. Resource group 8 loads file 20800 + this byte, which must exist.
/// It remains valid throughout the child's asynchronous load; no resource-range
/// check is made.
extern u8 gMenuMapHelpFileIndex;

/// Frameless root recipe for the map screen opened from gameplay or a menu.
///
/// Content starts at zero; closing reuses it as the image-restoration latch.
/// Bounds use screen-centered pixels and the ordering-table seed counts tags;
/// the callback owns picture loading, navigation and closing.
extern const UiObjectDesc gMenuMapPanelDesc;

/// Untitled recipe displaying the player's current area name above map Help.
///
/// Content is unused; the handler fits its bounds to the loaded area name.
/// Bounds use screen-centered pixels and the ordering-table seed counts tags.
extern const UiObjectDesc gMenuMapAreaNamePanelDesc;

/// NUL-terminated Discard command text for an inventory row.
extern const u8 gItemMenuDiscardText[];

/// NUL-terminated Item command text that opens the hotspot's collected-item picker.
///
/// Borrowed encoded bytes, valid for the gameplay overlay's lifetime.
extern const u8 gItemMenuHotspotItemText[5];

/// NUL-terminated hotspot action text for every action kind except Push (1).
extern const u8 gItemMenuHotspotExamineText[];

/// NUL-terminated hotspot action text for the Push action kind (1).
extern const u8 gItemMenuHotspotPushText[];

/// Upgrade command text for a P.E. ability whose learned level is zero.
extern const u8 gItemMenuPeReviveText[];

/// Upgrade command text for a P.E. ability with a nonzero learned level.
extern const u8 gItemMenuPeStrengthenText[];

/// Cancel command text for the P.E. command list.
///
/// The twelve-byte storage also retains an unreferenced terminated "Use" string.
extern const u8 gItemMenuPeCancelText[12];

/// Notice text when the weapon-choice list has no applicable carried weapon.
extern const u8 gItemMenuNoApplicableWeaponText[];

/// Area-preview label in the P.E. specifications panel.
extern const u8 gItemMenuPeAreaEffectLabel[];

/// Casting-cost label shared by P.E. descriptions and specifications.
extern const u8 gItemMenuPeCastingCostLabel[];

/// ATP-loss parameter label in the P.E. specifications panel.
extern const u8 gItemMenuPeAtpLossLabel[];

/// Start of the prompt slice and full extent of the item-menu message catalog.
enum {
    ITEM_MENU_NOTICE_PROMPT_FIRST = 16,
    ITEM_MENU_NOTICE_TEXT_COUNT   = 33
};

/// Complete item-menu message catalog, indexed by notice id 0..32.
///
/// Entries are borrowed terminated byte strings with gameplay-overlay lifetime.
/// The prompt slice starts at id 16; its first entry asks to discard.
/// Consumers use the low 16 bits of a task payload and require an in-range id.
extern const u8* const gItemMenuNoticeTexts[ITEM_MENU_NOTICE_TEXT_COUNT];

/// Shared writable Use/Attach and Key Item list, with wrapping and ten-pixel rows.
///
/// The two callbacks and item count are fixed. Fitting recomputes the viewport
/// and clears scroll/input state; selection persists when the live save's
/// cursorMode is zero. Updates mutate selection and draw state. This singleton
/// list remains borrowed throughout the Item command panel's lifetime.
extern UiList gItemMenuItemCommandList;

/// Shared writable P.E. Upgrade/Cancel list, with wrapping and ten-pixel rows.
///
/// Its task restores the two row callbacks and row counts when opened; generic
/// list updates retain navigation, selection and drawing state here.
extern UiList gItemMenuPeCommandList;

/// Recipe for the P.E. ability's Upgrade/Cancel command popup.
///
/// Content is a packed P.E. menu id: element 0..3, ability 0..2, level 0..3.
/// The caller anchors it to the selected row; its handler fits the shared
/// two-row command list. Bounds use screen-centered pixels; depth counts tags.
extern const UiObjectDesc gItemMenuPeCommandPanelDesc;

/// Latched acceptance of the hotspot's Examine/Push row (0 none, 1 confirmed).
///
/// Cleared when the hotspot menu host starts and set only on action-row Confirm.
/// Persists after the menu closes; it does not report visibility or Item-row use.
extern s32 gItemMenuHotspotActionConfirmed;

/// Copied initial hotspot command-panel X in screen-centered pixels.
///
/// The opener stores the full word before queuing the host; construction narrows
/// it to the panel's signed halfword. The pending singleton menu owns this value.
extern s32 gItemMenuHotspotScreenX;

/// Copied initial hotspot command-panel Y in screen-centered pixels.
///
/// The opener stores the full word before queuing the host; construction narrows
/// it to the panel's signed halfword. The pending singleton menu owns this value.
extern s32 gItemMenuHotspotScreenY;

/// Unread copy of the action kind supplied when a hotspot menu is queued.
///
/// The queued task word selects Push (1) or Examine (every other value). This
/// separate full-word copy has no C or assembly reader; its purpose is unproven.
extern s32 gItemMenuHotspotActionKindCopy;

/// Map-screen task handlers: 0 open, 1 wait for page, 2 navigate, 3 close.
///
/// `Task::state` must be 0..3. The live panel is borrowed through spawnArg2;
/// the dispatcher copies the complete table before calling one handler.
extern const TaskFuncTable4 gMenuMapStateHandlers;

/// Title of the map objective-help panel.
///
/// The eight-byte storage retains 0x61, 0x7E, 0xC6 after "Help" and its NUL;
/// title drawing stops at the NUL, before those retained bytes.
extern const char gMenuMapHelpTitle[8];

/// NUL-terminated Use label of the Item menu's Use/Attach command row.
extern const u8 gItemMenuUseAttachCommandText[];

/// NUL-terminated Key Item label opening the collected-item command picker.
extern const u8 gItemMenuKeyItemCommandText[];

/// NUL-terminated Map command label in the item menu.
extern const u8 gItemMenuMapCommandText[];

/// Attention title of the item discard confirmation panel.
extern const char gItemMenuDiscardTitle[];

/// Title of the timed item-menu notice panel.
extern const char gItemMenuNoticeTitle[];

/// Title of the panel showing the next P.E. level before purchase.
extern const char gItemMenuPeNextLevelTitle[];

/// Experience-cost heading in the P.E. level purchase panel.
extern const u8 gItemMenuPeUpgradeExpLabel[];

/// Cost heading above the EXP charged for a P.E. level purchase.
extern const u8 gItemMenuPeUpgradeCostLabel[];

/// Bonus heading above the MP increase earned by a P.E. level purchase.
extern const u8 gItemMenuPeUpgradeBonusLabel[];

/// MP-bonus heading in the P.E. level purchase panel.
extern const u8 gItemMenuPeUpgradeMpLabel[];

/// Title of the current-level P.E. specifications panel.
///
/// The sixteen-byte storage retains a quote byte after the title's terminator;
/// title drawing reads only the terminated string.
extern const char gItemMenuPeSpecificationsTitle[16];

/// Applies a weapon add-on and shows the previous weapon, result and returned items.
///
/// object and task must remain live; spawnArg1 selects SMG/Rifle Clip Holder,
/// Snail Magazine, Hammer, Pyke, Javelin, M203 or M9. State 0 consumes the
/// selected carried row in `gItemMenuSelectedRow`, changes the carried weapon variant,
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

/// Opens the selected nonempty item's information child on a Triangle press.
///
/// Borrows the current selected inventory row and live parent object. Plays the
/// confirm sound and disables parent input even if child allocation fails.
/// The child receives the item id by value and starts active after one tick.
void itemMenuOpenInfoOnTriangle(UiObject* parentObject);

/// Draws an item row with a recessed icon slot, including an empty slot for id 0.
///
/// Uses `itemMenuDrawItemRow`'s coordinates, catalog and drawing contract.
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

/// Builds and updates the selected inventory item's command popup.
///
/// State zero builds the shared command list from the selected row (NULL means
/// empty), fits the panel and clips its lower edge to screen Y=70. Later states
/// process list input and child results, restoring focus when a child confirms.
/// spawnArg2 must hold the task's live UiObject; spawnArg1 selects commands:
/// 0 inventory, 1 weapon exchange, 2 consumable exchange, 3 armor exchange,
/// 4 armor attachment. The shared list/selection permit one popup at a time.
void itemMenuCommandTask(Task* task);

/// Applies an inventory recovery item or Healing P.E., then shows restored stats.
///
/// object and task must remain live through the panel updates. healingId is
/// Recovery1/2/3 (1..3), Cola (5), MP Boost1/2 (6..7), Ringer's Solution (61),
/// or a validated packed Healing P.E. id. Item use borrows `gItemMenuSelectedRow`
/// and consumes one only when the affected HP/MP is below maximum. Spell use
/// requires the caller to check MP affordability. State 0 applies the effect
/// once; later updates wait for the displayed HP/MP to catch up, then count
/// down from 188 on active updates. Confirm/Cancel or timeout reports DISMISS and
/// synchronizes the displayed values with live stats.
void itemMenuApplyHealingPanel(UiObject* object, Task* task, s32 healingId);

/// Consumes a selected item, raises one ordinary P.E. level and refills MP.
///
/// itemId must be an ordinary P.E. item id 15..50; it identifies one of twelve
/// abilities and a level 1..3. State 0 consumes `gItemMenuSelectedRow` once, raises the
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
/// spawnArg2 borrows the live task-owned UiObject. `gItemMenuEquipmentDetailOpen` values
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
/// `inventoryCanAddItem`'s catalog contract.
void itemPickupPanelTask(Task* task);

/// Opens a Yes/No child menu with Yes selected, transferring input from parent.
///
/// Uses `itemMenuSpawnYesNoMenuDefaultNo`'s positioning, ownership, failure and
/// result contract, with the initial selection on Yes instead.
UiObject* itemMenuSpawnYesNoMenu(UiObject* parent);

/// Published place-kind banks and the shared pickup-panel descriptor index.
enum {
    ITEM_PICKUP_PLACE_BANK_ITEM       = 0,
    ITEM_PICKUP_PLACE_BANK_KEY_ITEM   = 1,
    ITEM_PICKUP_PLACE_BANK_SAVE_POINT = 8,
    ITEM_PICKUP_PANEL_DESCRIPTOR      = 49
};

/// Opens the published place's pickup, save-point or item-box root after a delay.
///
/// Pickup dispatcher state 1 decrements `killCountdown` in task updates and tries
/// allocation once it is nonpositive. Failure retries without resetting the
/// counter, repeating any save capture or primitive-buffer preparation.
/// The published kind/id/quantity must remain stable with their resources loaded.
/// Banks 0/1 use the pickup panel, bank 8 captures player save state, and other
/// banks prepare stage UI primitive storage for an item-box panel. Success
/// borrows the root in `spawnArg1`, sets `uiOpen` and advances to result handling;
/// the UI task owns that root through animated closing.
void itemPickupOpenPromptTask(Task* task);

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

/// Ends the pickup task after its closing delay and releases stage UI resources.
///
/// Decrements once per dispatcher update. At zero, kills the task, releases the
/// stage task primitive buffer and requests the stage mode task's exit, in that
/// order. The pickup must have finished using that buffer before this state.
void itemPickupExitTask(Task* task);

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

/// Draws a collected-item row and opens its Use or information child panel.
///
/// Borrows live list/object storage and loaded item/menu resources. The current
/// row must be a valid collected-bit ordinal. Row positions are panel-relative
/// pixels; colorRgb is packed RGB. Hidden panels skip drawing. An active panel
/// or active child selection updates the selected row's description and primary
/// preview, invalidating only profiles 1 and 2 of the five-slot preview cache.
/// Enabled row input opens Use on Confirm (a notice during cutscene hold), or
/// information on Triangle, transferring input to the child. Only the ordinary
/// Use-command path checks allocation failure before deactivating its parent.
void itemMenuDrawCollectedItemRow(UiList* list, UiObject* object);

/// Updates the Key Item menu's collected-item list and handles its child panels.
///
/// spawnArg2 borrows the task's live UiObject; spawnArg1 == 0 resizes the list
/// for ten text rows and creates preview profile 1 on initialization. Other
/// values preserve the supplied layout. Uses singleton collected-list state,
/// with rows addressed by collected-bit ordinal rather than inventory slot.
/// Empty refreshes retain selectedItemIndex == -1. Requires loaded menu/item
/// resources, live save/input state and a closed circular child-task ring.
/// Menu cancels; Cancel confirms return value 1 except during cutscene hold,
/// when it cancels. Child cancellation propagates; child confirmation closes
/// that panel tree and restores parent input. The callback retains its task.
void itemMenuCollectedItemsTask(Task* task);

/// Updates one element's PE list and transfers focus through the four sibling panels.
///
/// spawnArg1 selects fire/wind/water/earth (0..3); spawnArg2 is the live owned
/// object. Requires the four siblings in that order and saved levels 0..3.
/// The third ability appears when learned or when both preceding abilities
/// reach level 3. Horizontal moves retain the row, clamped to the target list;
/// vertical moves select its first/last row. Child acceptance restores input.
void itemMenuPeElementTask(Task* task);

/// Draws Use and opens the selected ordinary item's use panel on active Confirm.
///
/// Borrows the selected carried row with an item id in 1..95. The command panel
/// becomes inactive even if spawning fails; the list reports input consumed.
void itemMenuDrawUseRow(UiList* list, UiObject* object);

/// Releases an item-information panel and clears its published holder if still current.
///
/// The panel publishes its task-owned object in `gItemMenuInfoPanel` when it starts.
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
/// Zero removes the loads selected by `gItemMenuWeaponLoadSelection` (1 primary, 2 secondary,
/// other values both), preserving built-in supplies. A single selected item's
/// id is copied into the argument's low byte before clearing for later drawing.
/// Starts a 188-tick counter, including opening/inactive callbacks. An active
/// panel yields DISMISS on timeout or Confirm/Cancel, and CANCEL on Menu.
void itemMenuReloadNoticeTask(Task* task);

/// Displays a timed Attach notice naming the item supplied in the task argument.
///
/// `spawnArg1.value` is a catalog item id and `spawnArg2.pointer` the live
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
/// `abilityId` is a packed PE catalog id in 0x300..0x3FF. Its low two bits
/// select the level; zero still draws the description but omits the cost.
/// Borrows the live object's panel and queues text and bar primitives this frame.
void itemMenuDrawAbilityDescription(UiObject* object, s32 abilityId);

/// Draws Sort and sorts the live carried range on active Confirm.
///
/// While choosing a manual reorder destination, the selected Sort row is dimmed
/// and skipped. Otherwise selection supplies the reorder help text. Coordinates
/// are list-row pixels relative to the panel content.
void itemMenuDrawSortRow(UiList* list, UiObject* object);

/// Propagates child dismissal/cancellation and closes accepted child dialogs.
///
/// Borrows a live object and ownerTask with a circular ring of task-owned
/// `UiObject`s in `spawnArg2`. CONFIRM detaches and starts closing the child tree,
/// then reactivates the parent; DISMISS and CANCEL copy to the parent's result.
/// The saved sibling and current ring head determine where traversal stops,
/// including after closing the previous head. Release happens in later updates.
void itemMenuApplyChildDialogResults(UiObject* object, Task* ownerTask);

/// Counts carried armor rows other than the equipped armor and shows four rows.
///
/// Borrows the live carried range, which must fit its readable backing table.
/// Duplicate ids count separately; quantities and attachment markers are ignored.
/// Writes only the list's item count and visible-row count. `unusedObject` is ignored.
void itemMenuSizeUnequippedArmorList(UiList* list, const UiObject* unusedObject);

/// Fits and horizontally centers a two-line Equipped notice for a catalog item.
///
/// `itemId` must satisfy `itemGetText`'s name lookup contract. Requires initialized
/// panel bounds and content coordinates under `uiSetPanelContentSize`'s contract.
/// Width covers the item name plus its period allowance or "Equipped", with
/// five margin pixels; height is two fifteen-pixel rows plus one margin pixel.
/// Signed halving rounds the negative outer width down before the halfword store.
void itemMenuSizeEquippedNotice(UiPanel* panel, s32 itemId);

/// Draws Equipped and the named item followed by a period in a fitted notice.
///
/// `itemId` must satisfy `itemGetText`'s name lookup contract. Borrows the panel and
/// loaded text/GPU resources under `textDrawUiLine`'s contract. Baselines are
/// 15 and 30 pixels below `contentTop`, two pixels right of `contentLeft`. The item
/// name uses RGB 0x037A78; the verb and period use the normal menu text color.
void itemMenuDrawEquippedNotice(const UiObject* object, s32 itemId);

/// Draws the main menu's P. Energy command and opens its ability list on Confirm.
///
/// Training mode dims and skips the row. Otherwise a selected row supplies help
/// while the panel is active or suspends active input. Confirmation publishes
/// command 12 and CONFIRM; coordinates are pixels relative to panel content.
void itemMenuDrawPeCommandRow(UiList* list, UiObject* object);

/// Draws Options, restores its preview resources on selection and accepts Confirm.
///
/// Borrows the live list row and its task-owned object. A selected row supplies
/// help in active or suspended-active mode. The owner's content word latches
/// preview reset as 2; entering that value drops queued loads, loads the menu
/// resource and clears item previews. Active Confirm publishes command 36 and
/// CONFIRM to the owner's object. Text coordinates use unsigned halfword views
/// of row/content pixels, narrowed into the signed text request.
void itemMenuDrawOptionsCommandRow(UiList* list, UiObject* object);

/// Draws the main menu's Exit command and cancels the menu on active Confirm.
///
/// A selected row supplies help while the panel is active or suspends active
/// input. Coordinates are pixels relative to panel content. Sets CANCEL
/// without a command id or a sound request.
void itemMenuDrawExitCommandRow(UiList* list, UiObject* object);

/// Draws the equipped weapon and loads, with passive Armor and P. Energy children.
///
/// `spawnArg2.pointer` is the live owned summary object; state starts at zero.
/// The first update opens both child summaries with inactive input and no delay.
/// Every update clears the result and draws the Weapon title and the summary
/// two pixels right of contentLeft, fifteen below contentTop. Menu text,
/// textures, player equipment and the primitive buffer must be ready.
void itemMenuWeaponSummaryTask(Task* task);

/// Draws one scrolling item-information description row.
///
/// The owner's low spawn halfword is the item id. Rows 0/1 of ordinary items
/// use identified catalog descriptions; remaining rows, and all key-item
/// rows, read the loaded text chunk from line 5 onward. The caller must keep
/// that chunk ready and readable through the selected line. Row indices are
/// nonnegative; the initial two-row test retains signed-byte narrowing.
void itemMenuDrawDescriptionRow(UiList* list, UiObject* object);

/// Draws the collected-item Use command and starts its room-use dialog on Confirm.
///
/// The selected row in the shared collected-item list must remain stable until
/// the spawned dialog reads it. Borrows a live row/object and loaded text/GPU
/// resources. Confirmation sounds before spawning an active child with a
/// one-tick opening delay, then suspends parent input even if spawning fails.
/// Text coordinates use unsigned halfword row/content views in pixel units.
void itemMenuDrawKeyItemUseRow(UiList* list, UiObject* object);

/// Draws Move and publishes the list's MOVE action on active Confirm.
///
/// The parent resolves the transfer; this row only publishes the command.
void itemMenuDrawMoveRow(UiList* list, UiObject* object);

/// Draws Exchange and opens the armor-attachment item picker on active Confirm.
///
/// Borrows a live attachment command row/object and loaded text/GPU resources.
/// The active child opens after sixteen ticks and keeps its DISMISS result;
/// its signed origin is (-8, -92) pixels from screen center. Confirmation
/// suspends parent input and consumes the list action even if spawning fails.
/// The label uses unsigned halfword row/content coordinates in pixel units.
void itemMenuDrawAttachmentExchangeRow(UiList* list, UiObject* object);

/// Applies the selected inventory recovery item through its task-owned healing panel.
///
/// Borrows the live `gItemMenuSelectedRow`; its item id must satisfy
/// `itemMenuApplyHealingPanel`'s inventory-item contract. The dispatch supplies
/// object == task->spawnArg2.pointer and task == object->owner.
void itemMenuApplySelectedHealingItem(UiObject* object, Task* task);

/// Dispatches each use-panel update to the selected ordinary item's handler.
///
/// `spawnArg1.value` is an item id in 0..95, used without narrowing or a bounds
/// check. `spawnArg2.pointer` borrows the live panel object; the handler receives
/// that object and its owner. A NULL handler leaves the panel untouched. The
/// selected inventory row must remain valid for handlers that consume it.
void itemMenuUseItemTask(Task* task);

/// Applies the task-selected ordinary PE item through its use panel.
///
/// `spawnArg1.value` is 15..50, selecting one of twelve abilities at level 1..3.
/// Requires the live owned object and selected row under
/// `itemMenuInvokeParasiteEnergyItem`'s consumption and lifetime contract.
void itemMenuApplyParasiteEnergyItem(UiObject* object, Task* task);

/// Draws OK and publishes the list's OK command when its active row is confirmed.
void itemMenuDrawOkRow(UiList* list, UiObject* object);

/// Draws Cancel and publishes the list's Cancel command on Confirm.
void itemMenuDrawCancelRow(UiList* list, UiObject* object);

/// Draws Yes; Confirm accepts it, while Cancel moves the active list to the No row.
void itemMenuDrawYesRow(UiList* list, UiObject* object);

/// Draws No and accepts that command on either Confirm or Cancel on its active row.
void itemMenuDrawNoRow(UiList* list, UiObject* object);

/// Draws the loaded map page and handles page navigation, Help and map dismissal.
///
/// State 2 in the map task table; spawnArg2 borrows the live map object.
/// Requires stage 1..5, loaded map/draw resources and a nonzero current page
/// within the stage's page-flag table. Navigation limits are 2, 3, 3, 6 and 3;
/// Acropolis page 3 is isolated by its 0xFF flag. The table must include the
/// current index and candidates through the limit; 0xFF disables navigation.
/// Candidate arithmetic narrows to u8.
/// Active Cancel/Select publishes command 0x101 and CONFIRM; Menu reports CANCEL.
/// A page change holds drawing and enters state 1 to await its CD load. Triangle
/// latches the objective help selector and opens the help child. Child CONFIRM
/// restores input and closes Help; child CANCEL closes the map. Closing enters
/// state 3 and also restores view resources in the garbage incinerator.
void menuMapNavigateTask(Task* mapTask);

/// Loads the map help text, opens the current area's name panel and handles closing input.
///
/// `spawnArg2.pointer` is the live owned help object; state starts at zero.
/// The global text chunk must remain loaded through state 2. Active Cancel or
/// Triangle reports CONFIRM with resultValue 1; Menu reports CANCEL. The name
/// panel borrows the same stage/area tables as `menuMapAreaNameTask`.
void menuMapHelpTask(Task* task);

/// Fits and draws the current area's name above the map picture.
///
/// Requires stage 1..5 and a nonzero area within that stage's loaded name table;
/// `spawnArg2.pointer` is the live owned object. State zero measures the large
/// text and gives the panel four pixels of horizontal padding. Each update draws
/// outlined text at the content origin; a NULL stage table suppresses the draw.
void menuMapAreaNameTask(Task* task);

/// Dispatches the map panel's opening, page-load wait, input and closing updates.
///
/// `mapTask->state` must be 0..3 and `spawnArg2.pointer` its live owned UiObject.
/// Opening/input borrow the current stage's loaded map tables. The handler table
/// is copied locally before dispatch; handlers own state changes and closing timing.
void menuMapTask(Task* mapTask);

/// Saves displaced map-texture storage and loads the current area's map page.
///
/// State 0 of the map task. Requires stage 1..5, an area in its loaded map
/// table, and a live task-owned `UiObject` in spawnArg2. Rebuilds marked-area
/// bits, narrows the area's page to its low byte and queues its picture before
/// advancing to state 1; the no-page marker consequently selects zero. Normal
/// navigation requires a nonzero page. With keepGraphics zero, saves the
/// 128x256 VRAM strip at (896,0) in the word-aligned captured-frame workspace
/// before `gGpuPrimitiveBufferBase`. Its first 0x10000 bytes and the heap base must
/// remain intact until closing restores them. Requires CD request capacity;
/// the page load completes asynchronously.
void menuMapOpenTask(Task* mapTask);

/// Waits for the map page load, draws the ready page and resumes map input state.
///
/// `mapTask` is in state 1 of `gMenuMapStateHandlers` with its live `UiObject` in `spawnArg2`.
/// Its current stage/page indices must fit the loaded tables, and those tables
/// and page resources must remain available.
/// A busy CD queue adds one animation tick to offset opening's subsequent tick;
/// idle sets one tick, draws the player, flags, picture, areas and page arrows,
/// and advances to state 2. Page changes reuse this state after the initial load.
void menuMapWaitForPageTask(Task* mapTask);

/// Draws the closing map and restores displaced graphics once its countdown ends.
///
/// State 3 of the map task, armed with killCountdown 4 and spawnArg1.value 0.
/// Decrements once per callback: draws cursor, flags, picture and areas at 3/2,
/// leaves count 1 undrawn, then restores at zero. With keepGraphics zero,
/// uploads the retained room image before restoring the saved VRAM strip;
/// requires the unchanged backup/base from `menuMapOpenTask` and live retained
/// view resources. Sets spawnArg1 to 1 even when graphics are kept, suppressing
/// later work. Drawing needs the live object and loaded stage/page resources.
/// The UI lifecycle owns task/object release; this callback releases neither.
void menuMapCloseTask(Task* mapTask);

/// Updates the Item command list containing Use/Attach and Key Item.
///
/// `spawnArg2` is the live task-owned `UiObject`; its owner starts in state zero.
/// Fits the shared list once and draws it each update. Active Cancel plays the
/// cancel sound and reports CONFIRM; Menu reports CANCEL. Child CONFIRM restores
/// parent input before closing that subtree, DISMISS becomes parent CONFIRM,
/// and CANCEL propagates. Child objects and their circular task ring must be live.
/// A child CONFIRM must leave a nonempty ring for the subsequent traversal.
void itemMenuItemCommandTask(Task* task);

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

/// Updates the hotspot's Examine/Push and Item command menu.
///
/// `spawnArg2.pointer` is the live owned object; spawnArg1 selects Examine (0)
/// or Push (1). State zero fits the list and shifts only right/bottom overflow
/// within centred-screen limits (150, 110). Menu reports CANCEL; active Cancel
/// reports CONFIRM with the cancel sound. The first child's CANCEL or CONFIRM
/// takes precedence; no child is closed here. The shared list is used serially.
void itemMenuHotspotCommandTask(Task* task);

/// Hosts the hotspot command popup and restores frame timing after it closes.
///
/// State starts at zero; spawnArg1 forwards the full action-choice word
/// (1 Push, otherwise Examine). The initial X/Y words supply signed pixels
/// from screen center, truncated to halfwords. The host retains the spawned
/// root object in spawnArg2, clears the action-accepted latch and runs UI at
/// one VBlank per update. Subsequent active updates require that spawn to have
/// succeeded. CANCEL or CONFIRM starts closing; ten further callbacks precede
/// restoration of two-VBlank timing, primitive-buffer release and mode exit.
/// The accepted latch remains readable after teardown until the next start.
void itemMenuHotspotMenuTask(Task* task);

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

/// Draws the main menu's Map command and requests the map screen on Confirm.
///
/// A selected row supplies help while the panel is active or suspends active
/// input. Active Confirm publishes command 0x100 and CONFIRM; coordinates are
/// pixels relative to panel content.
void itemMenuDrawMapCommandRow(UiList* list, UiObject* object);

/// Draws Discard and opens the selected stack's discard dialog on active Confirm.
///
/// Borrows the stable selected carried row. The dialog applies restrictions and
/// asks for confirmation; this command panel becomes inactive even if spawning fails.
void itemMenuDrawDiscardRow(UiList* list, UiObject* object);

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

/// Draws and accepts the hotspot menu's Examine or Push action.
///
/// The live object's owner carries the action kind in spawnArg1: 1 selects
/// Push, 0 and other values select Examine. Active Confirm latches acceptance
/// until the next hotspot menu starts and sets the object's result to CONFIRM.
/// Coordinates are list-row pixels relative to panel content.
void itemMenuDrawHotspotActionRow(UiList* list, UiObject* object);

/// Draws the hotspot Item command and opens the item-menu command tree on Confirm.
///
/// Borrows a live row/object and loaded text/GPU resources. Active Confirm
/// spawns an active child with a one-tick delay before requesting its sound,
/// then hides the command panel and suspends input even if spawning fails.
/// The child caption task opens Status normally, or Key Items in scripted hold.
/// The label uses unsigned halfword row/content coordinates in pixel units.
void itemMenuDrawHotspotItemRow(UiList* list, UiObject* object);

/// Displays the selected menu preview in a separate panel without polling input.
///
/// `spawnArg2` is the live task-owned `UiObject`; state starts at zero. `spawnArg1`
/// zero selects a 132x100-pixel content area; nonzero selects 132x131 and the
/// tall picture. Uses the current menu preview resources without requesting a
/// load. A busy CD queue suppresses the picture, retaining its recessed frame.
/// Resets the object's result to NONE each update; picture inset is two pixels.
void itemMenuPreviewPanelTask(Task* task);

/// Encoded help text for the selected Parasite Energy menu command.
extern u8 gItemMenuReleasePeHelpText[];

/// Draws a weapon's selected consumable and remaining load, and accepts reloading.
///
/// `list->currentItemIndex` is 1 for the primary load or 2 for the secondary;
/// row zero belongs to the equipped weapon. Rows use the pane's last capacity
/// refresh, so weapon replacement can change saved loads before later rows draw.
/// Active input publishes that index as the load selector. Normal Confirm opens compatible consumable choices;
/// destination Confirm loads the selected carried item to capacity, returning
/// parent focus if incompatible. A zero loaded quantity can retain its item id.
/// Requires a live equipped weapon, selected source in destination mode, menu
/// tree and text/GPU resources. Row positions are content-relative pixels.
void itemMenuDrawWeaponLoadRow(UiList* list, UiObject* object);

/// Draws an armor attachment slot and accepts attachment replacement.
///
/// `list->currentItemIndex` is zero-based within the armor's attachment capacity;
/// carried rows store positions as index + 1. Empty slots open attachment choices,
/// occupied slots open item commands. Destination Confirm attaches an eligible
/// selected source, then detaches the prior occupant; no-attachment items return
/// focus to the inventory parent. For consumables, the count excludes all weapon
/// loads. Requires a valid live carried range, selected source in destination
/// mode, menu tree and drawing resources; row coordinates are content-relative
/// pixels. Saved row pointers borrow storage and may change contents after sorting.
void itemMenuDrawArmorAttachmentRow(UiList* list, UiObject* object);

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

/// Draws the menu caption until its command delay expires, then opens the selected panel.
///
/// object is the live caption-strip UiObject and task is its owner in state 2;
/// killCountdown counts dispatcher updates and is decremented before the test.
/// spawnArg1 uses `itemMenuDrawTaskPrompt`'s borrowed text/P.E. payload contract.
/// resultValue is a populated descriptor index in 0..49, or 0x100 for Map and
/// 0x101 to return from Map. Dispatch returns to state 1 even if spawning fails.
/// Captions use one row for the main menu, two otherwise; the P.E. caption ends
/// at screen Y 76 instead of 104. Menu resources and the UI tree must stay live.
void itemMenuDispatchCommand(UiObject* object, Task* task);

/// Runs the main menu's six command rows and forwards results from its child panels.
///
/// task->spawnArg2 is the live UiObject. State zero opens the player summary
/// and fits the shared list; later updates reset the result and poll input.
/// Active Cancel/Menu and child CANCEL leave the menu. Child DISMISS forwards
/// its command as CONFIRM; child CONFIRM closes that child and restores input.
/// Requires the shared command list, live child ring and menu drawing resources.
void itemMenuMainPanelTask(Task* task);

/// Draws the player summary's fixed heading, side image, HP/MP, EXP and BP.
///
/// task->spawnArg2 is the live UiObject. State zero opens the weapon summary
/// and seeds displayed HP/MP from the live player; later draws advance upward
/// under `itemMenuDrawPlayerStats`'s contract. Maximum HP/MP must be positive.
/// Requires loaded menu textures and writable GPU packet/ordering-table storage.
void itemMenuPlayerSummaryTask(Task* task);

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

/// Draws a loose carried-item row and accepts commands or a reorder destination.
///
/// `list->currentItemIndex` is zero-based among unattached, unequipped carried rows;
/// the final row invokes Sort. Saved row quantities include loaded consumables,
/// so the displayed stock subtracts all weapon loads. Active-row Confirm opens
/// commands in normal mode or moves the selected source to this row in destination
/// mode. The selected source borrows the live carried table, whose range must fit
/// its backing; moving rows replaces the contents at retained row addresses.
/// Requires the current menu tree and text/GPU resources. Row coordinates are
/// content-relative pixels; no list or object pointer is retained.
void itemMenuDrawReorderableItemRow(UiList* list, UiObject* object);

/// Sets row counts for carried weapons or their compatible consumable loads.
///
/// consumableItemId == INVENTORY_ITEM_NONE lists every carried weapon with
/// four visible rows. Otherwise each matching primary/secondary load adds
/// one row only for an attached or equipped weapon; a weapon can add two.
/// Nonzero ids must be consumables 0xA0..0xBF. Counts narrow to the list's
/// bytes, all counted rows are visible, and row height is fifteen pixels.
/// The live carried range must fit its item table; no pointer is retained.
void itemMenuSetWeaponChoiceRows(UiList* list, s32 consumableItemId);

/// Runs the carried-item pane and transfers Left input to its equipment panes.
///
/// task->spawnArg2 is the live inventory UiObject. State zero spawns weapon
/// and armour children before initializing the carried list/count strip.
/// The inventory pane hides while item details are open; when they close it
/// limits the hidden delay to sixteen ticks plus the opening animation.
/// Left transfers signed screen-pixel Y to the weapon pane above the armour
/// pane's top edge, otherwise to armour. Both equipment panes
/// must have spawned successfully and remain live while focus can transfer.
/// Requires the live carried range, shared lists and menu drawing resources.
void itemMenuInventoryPanelTask(Task* task);

/// Draws the equipped-weapon row and accepts weapon replacement.
///
/// Used for row zero of the equipped-weapon list; adds a ten-pixel gap before
/// its load rows. Normal Confirm opens the carried-weapon chooser when another
/// selection exists, otherwise a notice. Destination Confirm equips a selected
/// weapon after clearing the previous weapon's removable loads; other source
/// kinds restore parent focus. The equipped nonzero weapon must have a carried
/// row, and destination mode requires a live selected source. The list/object,
/// inventory-pane parent and menu drawing resources must remain live.
void itemMenuDrawEquippedWeaponRow(UiList* list, UiObject* object);

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

/// Updates the equipped-armor header and its scrolling attachment-slot list.
///
/// `task->spawnArg2` is the live UiObject with an inventory-pane parent. State zero
/// initializes the list, one focuses attachments and two focuses equipped armor.
/// Up from armor moves to the weapon pane; Right maps unsigned cursor-Y bits to
/// the inventory's scrolled row and transfers focus. Incoming screen-pixel Y
/// selects the header or an attachment row. Confirm opens armor choices or equips
/// a selected armor destination; accepted child dialogs restore focus and may
/// enter destination selection. The pane hides while item details are open.
/// Requires the live carried table, parent/weapon panes, shared lists and menu
/// resources throughout the task; resets the object's result each update.
void itemMenuArmorPanelTask(Task* task);

/// Caption-strip phases indexed by its owning task's state in 0..2.
///
/// Entry 0 initializes the prompt holder and first child; 1 draws the prompt
/// and waits for a child result; 2 waits for the dispatch delay, opens the
/// selected command panel and returns to phase 1. The caption task copies the
/// three callbacks before dispatch; the table lives with the gameplay overlay.
extern const UiObjectTaskFuncTable3 gItemMenuCaptionStates;

/// Past-tense suffix after the consumed weapon and add-on rows, including its period.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuAddonUsedText[6];

/// Past-tense suffix after the replacement weapon and returned add-on rows, including its period.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuAddonCreatedText[9];

/// Prompt for the selected weapon-load row when no consumable is loaded.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuNoAmmunitionText[17];

/// Prompt for the selected armor attachment slot when it has no occupant.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuNoAttachmentsText[18];

/// Shared mutable scrolling viewport for the item-information description.
///
/// Initialization replaces both row counts with the loaded description length,
/// caps visible rows at six, resets selection/scrolling and enables the single
/// shared row callback. Row counts must fit 0..127; the seed is one 15-pixel row.
/// Drawing suspends row input; panel input scrolls the retained viewport.
/// Only one information panel uses this gameplay-owned list at a time.
extern UiList gItemMenuDescriptionList;

/// Ammunition SPECIAL labels indexed by `WeaponAttackRow.hitReaction` in 0..9.
///
/// Entries 3/4/5/6/7/9 are poison/burst/piercing/explosion/incendiary/flash;
/// 0/1/2/8 are empty. Borrows NUL-terminated byte text for gameplay's lifetime.
extern const u8* gItemMenuAmmoSpecialAttributeTexts[10];

/// Specifications captions indexed by the consumable classification's low nibble.
///
/// Entries 1..6 label 9mm, .40 Magnum, .44 Magnum, 40mm, 12 gauge and 5.56mm;
/// the others are empty. Text embeds column/font/Y-offset/outline commands and
/// is borrowed read-only for the gameplay overlay's lifetime.
extern const u8* gItemMenuCaliberTexts[16];

/// Armor SPECIAL labels in feature-bit order, from bit zero through bit twelve.
///
/// Bits 10/11 have empty labels. The specifications panel scans in ascending
/// bit order and displays at most two set bits. Borrows NUL-terminated byte
/// text for gameplay's lifetime; resistance labels include an orange command.
extern const u8* gItemMenuArmorFeatureTexts[13];

/// Borrowed label table selected by the current equipment-stat draw.
///
/// Selects weapon Range/Rate/Weight, consumable Power or armor HP/MP/attachment
/// labels. Drawing indexes only that table's 3/1/3 entries; the selection
/// remains afterward and owns no text. Tables and text live with gameplay.
extern const u8* const* gItemMenuEquipmentStatLabels;

/// Equipment or attachment details are open (0 normal panes, 1 hide them).
///
/// Inventory-panel initialization clears this halfword. Detail initialization
/// sets 1; its closing updates restore 0. Inventory, weapon, armor and count
/// panes respond to those exact values. Teardown alone does not clear it.
extern u16 gItemMenuEquipmentDetailOpen;

/// Values written by menu flow to its room/view restoration latch.
enum {
    ITEM_MENU_ROOM_RESTORE_CLEAR   = 0,
    ITEM_MENU_ROOM_RESTORE_PENDING = 1
};

/// Room/view resources must be restored on menu exit exactly when this word is 1.
///
/// Menu entry clears it to 0; opening the map or replacing room resources with
/// a nonzero preview load profile sets 1. Accessors preserve arbitrary s32 values
/// without normalizing or consuming them, so other nonzero values do not restore.
extern s32 gItemMenuRoomRestorePending;

/// Carried-item destination protocol (0 browse commands, 1 choose a destination).
///
/// Inventory initialization resets it; an accepted Move/Switch command sets 1.
/// `gItemMenuSelectedRow` must then retain the live saved source row while inventory,
/// equipment or attachment rows select its destination. Completion or Cancel
/// restores 0. The global owns no row; leaving the menu adds no separate reset.
extern s32 gItemMenuDestinationSelectionMode;

/// Weapon-load selector for menu choices (0 both, 1 primary, 2 secondary).
///
/// Equip/general-load commands write BOTH; active load rows publish their
/// one-based row index. Consumers borrow it through choice and notice tasks.
/// Some selectors treat other values as both; producers publish only 0..2.
/// BSS starts at BOTH; menu entry/teardown does not independently reset it.
extern s32 gItemMenuWeaponLoadSelection;

/// Indices of the equipment panes beside the carried-item destination list.
enum {
    ITEM_MENU_DESTINATION_WEAPON_PANE = 0,
    ITEM_MENU_DESTINATION_ARMOR_PANE  = 1,
    ITEM_MENU_DESTINATION_PANE_COUNT  = 2
};

/// Borrowed weapon and armor destination-pane objects for the active inventory tree.
///
/// Inventory initialization replaces both entries with its newly spawned
/// children. Focus transfer requires both spawns to succeed and the tree to
/// remain live. Their tasks own and release the objects; tree teardown does
/// not clear these pointers, so they must not be used after that tree closes.
extern UiObject* gItemMenuDestinationPanes[ITEM_MENU_DESTINATION_PANE_COUNT];

/// Item ids for the active weapon-consumable choice rows, including zero for removal.
///
/// Only the prefix below `gItemMenuConsumableChoiceList.itemCount` is valid;
/// rebuilding leaves the tail unchanged. BOTH load selection can append three
/// primary and three secondary occurrences, retaining duplicates. PRIMARY or
/// SECONDARY appends at most three choices plus a removal row when nonempty.
/// Producers must keep `gItemMenuWeaponLoadSelection` in 0..2. Storage lives
/// with gameplay and is shared by one live consumable-choice session at a time.
extern s32 gItemMenuConsumableChoiceItemIds[6];

/// Complete notice when the room refuses the selected collected key item.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuKeyItemRefusedText[21];

/// First-line verb above the collected item name after the room accepts its use.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuKeyItemUsedText[5];

/// Prompt while choosing a carried-item, equipment or armor-attachment destination.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuSelectDestinationText[20];

/// First-line verb in equipment and armor-attachment confirmation notices.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuEquippedText[9];

/// First-line verb above the collected item name in pickup notices.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemPickupObtainedText[9];

/// First-line verb above the newly loaded consumable name.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuLoadedText[7];

/// First-line verb above a named consumable removed from a weapon load.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuRemovedText[8];

/// Complete one-line notice when ammunition removal has no single item name.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuRemovedAmmunitionText[20];

/// First-line verb above the Parasite Energy item name after invocation.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuInvokedText[8];

/// Use-command label shared by carried-item and collected-key-item menus.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuUseText[4];

/// Move-command label for choosing a carried item's destination.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuMoveText[5];

/// Command text for the no-consumable row that removes a weapon's loaded ammunition.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuRemoveAmmunitionText[18];

/// Load-command label for assigning a consumable to a compatible weapon.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuLoadText[5];

/// Exchange-command label for equipment and armor-attachment replacement.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuExchangeText[9];

/// Command text for the row that clears the selected armor attachment slot.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuDetachArmorText[18];

/// NUL-terminated byte text for the item-menu dialog's Yes answer.
extern u8 gItemMenuYesText[4];

/// NUL-terminated byte text for the item-menu dialog's No answer.
extern u8 gItemMenuNoText[3];

/// NUL-terminated byte text for the item-menu dialog's OK acknowledgement.
extern u8 gItemMenuOkText[3];

/// NUL-terminated byte text for the item-menu dialog's Cancel answer.
extern u8 gItemMenuCancelText[7];

/// Question offered before collecting the published pickup item.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemPickupQuestionText[19];

/// Warning shown when the carried inventory cannot accept the pickup.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemPickupInventoryFullText[16];

/// NUL-only second line of the inventory-full pickup warning.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemPickupInventoryFullSecondLineText[1];

/// Sort-command label for the final row of the carried-item list.
///
/// NUL-terminated encoded bytes, borrowed for the gameplay overlay's lifetime.
extern const u8 gItemMenuSortText[5];

/// Uppercase ammunition heading shared by weapon summaries and the load-detail pane.
///
/// NUL-terminated byte text borrowed for the gameplay overlay's lifetime.
extern u8 gItemMenuAmmoLabelText[5];

/// NUL-terminated period appended after an item name in menu action notices.
extern u8 gItemMenuPeriodText[2];

/// Shared list state for the six main-menu commands.
///
/// Rows are Use/Attach, Key Item, P.E., Map, Options and Exit, in that order.
/// All six are visible, with wrapped navigation and eight-pixel spacing.
/// Panel setup retains selection when saved cursorMode is 0 and resets it
/// otherwise. Only one root panel may use this gameplay-owned state at a time.
extern UiList gItemMenuMainCommandList;

/// Shared scrolling list of reorderable carried rows followed by Sort.
///
/// Setup replaces the ten-row seed, clamps navigation, enables its shared row
/// callback and resets selection/scrolling. Equipped armor, equipped weapons
/// and attached rows are excluded; empty carried rows remain destinations.
/// Up to nine rows are visible at fifteen-pixel spacing. The inventory and
/// equipment panes share this state in one live inventory tree at a time.
extern UiList gItemMenuInventoryList;

/// Shared list for the equipped weapon and its available load slots.
///
/// Row 0 is the weapon, 1 its primary load, and 2 its secondary load. Setup
/// refreshes the active prefix to one, two or three rows, resets selection and
/// uses clamped navigation with sixteen-pixel spacing. Inventory/armor focus
/// transfers reuse this gameplay-owned state in one live menu tree at a time.
extern UiList gItemMenuEquippedWeaponList;

/// Shared scrolling list of the equipped armor's attachment slots.
///
/// Setup replaces the one-row seed with the armor's capacity, enables its
/// shared row callback and initializes the viewport with at most three visible
/// slots. Rows are sixteen pixels apart; selection is zero-based and corresponds
/// to the one-based saved attachment slot. The item picker borrows that
/// selection until it returns. Only one
/// active armor pane may use this gameplay-owned state at a time.
extern UiList gItemMenuArmorAttachmentList;

/// Shared list of carried armor-attachment candidates and a final detach command.
///
/// Setup replaces the one-row seed, shows four rows, enables its shared row
/// callback and resets selection/scrolling. Candidates include items attached
/// elsewhere but exclude empty rows, forbidden attachments and the equipped
/// weapon. The selected destination slot belongs to `gItemMenuArmorAttachmentList`.
/// One live picker may use this gameplay-owned state at a time.
extern UiList gItemMenuAttachmentCandidateList;

/// Empty requested-preview id and total backing-slot count, including reset-only slots.
enum {
    ITEM_MENU_PREVIEW_EMPTY      = -1,
    ITEM_MENU_PREVIEW_SLOT_COUNT = 5
};

/// Requested item ids for the menu's display-resource preview slots.
///
/// Slots 0/1/2 select menu/preview/relocated-preview load profiles. A selection
/// is published before loading, with the other two ids set to -1, even when the
/// loader declines the request. These ids do not establish resource readiness.
/// Menu entry and pickup setup reset all five words to -1; slots 3/4 have only
/// reset accesses and their purpose is unproven. Storage lives with gameplay;
/// clearing ids neither cancels CD requests nor releases loaded resources.
extern s32 gItemMenuPreviewItemIds[ITEM_MENU_PREVIEW_SLOT_COUNT];

/// Shared one-row Use-command list for the selected collected key item.
///
/// Wrapped navigation and ten-pixel spacing; the callback uses the selection
/// retained in `gItemMenuCollectedList`. One live command panel may use this
/// gameplay-owned state at a time, through completion of its child use notice.
extern UiList gItemMenuKeyItemCommandList;

/// Shared scrolling list of collected items selected from the saved bitmap.
///
/// Setup replaces the one-row seed with the collected count, enables its
/// shared row callback and resets selection/scrolling. Rows use clamped
/// navigation and fifteen-pixel spacing. Selection indexes collected items,
/// not carried inventory rows, and remains available to the key-item use task.
/// One live collected-item panel may use this gameplay-owned state at a time.
extern UiList gItemMenuCollectedList;

/// Weapon-stat labels in Range, Rate and Weight order.
///
/// Each index accompanies the corresponding weapon comparison value. The
/// three NUL-terminated encoded strings are borrowed for gameplay's lifetime.
extern const u8* const gItemMenuWeaponStatLabels[3];

/// Consumable-stat label for the single Power comparison value.
///
/// Borrows NUL-terminated encoded text for the gameplay overlay's lifetime.
extern const u8* const gItemMenuConsumableStatLabels[1];

/// Armor-stat labels in added HP, added MP and attachment-slot-count order.
///
/// The three NUL-terminated encoded strings accompany those comparison
/// values and are borrowed for the gameplay overlay's lifetime.
extern const u8* const gItemMenuArmorStatLabels[3];

/// Shared mutable list for carried weapons, optionally filtered by a consumable.
///
/// Setup replaces the 25-row seed with the current choice count, sets 15-pixel
/// spacing and enables its single row callback; unfiltered choices show four
/// rows. Filtered choices retain primary/secondary duplicates. The list task
/// resets selection and first-visible row on initialization; the outer weapon
/// selection task can read the retained selection before that reset.
/// One live choice session may use this gameplay-owned list at a time.
extern UiList gItemMenuWeaponChoiceList;

/// Shared mutable list for a weapon's compatible consumables and removal choice.
///
/// Setup replaces the three-row seed, enables its single callback and resets
/// selection/scrolling. BOTH selects at most six primary/secondary occurrences;
/// a single load selects at most three plus a removal row when nonempty.
/// Rows index `gItemMenuConsumableChoiceItemIds`; entries are item ids, with zero for removal.
/// One live choice session may use this gameplay-owned list at a time.
extern UiList gItemMenuConsumableChoiceList;

/// Shared scrolling list of carried armor rows excluding the equipped armor.
///
/// Setup replaces the one-row seed, shows four rows, enables its shared row
/// callback and resets selection/scrolling. Clamped navigation uses fifteen
/// pixels between rows. The comparison renderer also uses this list to identify
/// armor stats. One live armor chooser may use this gameplay-owned state at a time.
extern UiList gItemMenuArmorChoiceList;

/// Mutable row handlers for the selected carried-item or equipment command popup.
///
/// Setup writes an active prefix of zero to three callbacks and leaves the
/// rest unchanged. The six-entry seed is Use, Move, Move, Move, Discard, Sort;
/// all six slots remain allocated. `gItemMenuCommandList.itemCount` limits
/// dispatch to the current prefix. One live popup may use this gameplay-owned
/// callback table and list at a time.
extern UiListRowCallback gItemMenuCommandRowCallbacks[6];

/// Shared command-popup list for the selected carried item or equipment row.
///
/// Setup rebuilds the callback prefix and both counts (zero to three rows).
/// Uses wrapped navigation, ten-pixel spacing and the mutable
/// `gItemMenuCommandRowCallbacks`. The three-row seed is replaced before use;
/// one live popup may use this gameplay-owned state at a time.
extern UiList gItemMenuCommandList;

/// Mutable row drawers for the shared item-menu choice dialog.
///
/// Setup selects entry 0 for OK/Cancel, or both entries in Yes/No order. The
/// list borrows this table for the overlay's lifetime; only one dialog using
/// this shared table and list may be active at a time. Both entries start as OK.
extern UiListRowCallback gItemMenuDialogRowCallbacks[2];

/// Shared mutable list state for the item-menu OK, Cancel and Yes/No dialogs.
///
/// Starts with one visible row, clamped navigation and 15-pixel row spacing.
/// Dialog setup supplies the row count, initial selection and cursor-sound
/// policy, then fits the panel to the list. Its table and state live with the
/// overlay and are reused by successive dialogs; concurrent users would share
/// selection, scrolling and results.
extern UiList gItemMenuDialogList;

/// Panel/task descriptor for an item-menu choice dialog.
///
/// Seeds a 48-by-32-pixel panel at (0, 0), ordering-table base 4 and a bodyless
/// priority-192 task. The content callback fits it to the selected choice list;
/// the spawner anchors the task-owned child to its parent's content corner.
extern UiObjectDesc gItemMenuDialogDesc;

/// Packed GPU CLUT selectors for the item menu's ordinary icon palettes.
///
/// Palette indices are independent of icon-shape categories: 1 medicine/key
/// items, 2 fallback, 3 9mm, 4 .44 Magnum, 5 12-gauge, 6 5.56mm, 7 40mm,
/// 8 battery-powered weapons, 9 armor/other items. These CLUTs lie on VRAM row
/// 242. Index 0 repeats index 1; indices 10/11 are zero. No current drawer
/// selects those three entries and their purpose is unproven. P.E. icons
/// bypass this table and supply a CLUT directly.
extern const u16 gItemMenuIconPaletteCluts[12];

/// Encoded ADD HP label for armor maximum-HP bonuses.
extern const u8 gItemMenuArmorHpBonusLabel[];

/// Encoded ADD MP label for armor maximum-MP bonuses.
extern const u8 gItemMenuArmorMpBonusLabel[];

/// Encoded P.Energy title and command-row label for Parasite Energy.
extern const u8 gItemMenuParasiteEnergyTitle[];

/// Encoded label for the item-menu game-settings command.
extern const u8 gItemMenuOptionsCommandText[];

/// Encoded label for the item-menu Exit command.
extern const u8 gItemMenuExitCommandText[];

/// Encoded slash separating current values from their maximum or capacity.
extern const u8 gItemMenuFractionSeparatorText[];

/// Encoded HP label for player health and armor HP bonuses.
extern const u8 gItemMenuHpLabel[];

/// Encoded MP label for player energy and armor MP bonuses.
extern const u8 gItemMenuMpLabel[];

/// Encoded EXP label for the player's experience-point total.
extern const u8 gItemMenuExpLabel[];

/// Encoded BP label for the player's BP total.
extern const u8 gItemMenuBpLabel[];

/// Encoded title for armor summary, selection and detail panels.
extern const u8 gItemMenuArmorTitle[];

/// Encoded Attachments title and armor-summary attachment-list label.
extern const u8 gItemMenuAttachmentsTitle[];

/// Encoded title shared by weapon summary, equipment detail and weapon panels.
extern const u8 gItemMenuWeaponTitle[];

/// Encoded E marker identifying the currently equipped item.
extern const u8 gItemMenuEquippedMarkerText[];

/// Encoded title for the carried-item list and ordinary-item pickup panels.
extern const u8 gItemMenuItemTitle[];

/// Encoded heading for the armor panel's attachment-slot list.
extern const u8 gItemMenuArmorAttachmentsHeading[];

/// Encoded title for an item awarded as next-replay supply.
extern const u8 gItemMenuNextReplaySupplyTitle[];

/// Encoded title for the ordinary item-information panel.
extern const u8 gItemMenuSpecificationsTitle[];

/// Encoded heading for a weapon's firing controls.
extern const u8 gItemMenuWeaponOperationLabel[];

/// Encoded dash for an armor HP or MP bonus of zero.
extern const u8 gItemMenuNoArmorBonusText[];

/// Encoded ATTACHMENTS label for an armor item's attachment-slot count.
extern const u8 gItemMenuArmorAttachmentSlotsLabel[];

/// Encoded heading for an armor item's special features.
extern const u8 gItemMenuArmorFeaturesLabel[];

/// Encoded POWER label for a consumable's base attack amount.
extern const u8 gItemMenuConsumablePowerLabel[];

/// Encoded CAPACITY label for carried consumable quantity and stack limit.
extern const u8 gItemMenuConsumableCapacityLabel[];

/// Encoded SPECIAL label for a consumable's nonzero hit-reaction effect.
extern const u8 gItemMenuConsumableSpecialLabel[];

/// Encoded heading for weapons compatible with the selected consumable.
extern const u8 gItemMenuApplicableWeaponsLabel[];

/// Encoded Notice title for item-use, pickup and equipment-result panels.
extern const u8 gItemMenuNoticePanelTitle[];

/// Encoded title for the key-item list and key-item pickup panels.
extern const u8 gItemMenuKeyItemTitle[];

/// Encoded Weight label for the weapon comparison's weight column.
extern const u8 gItemMenuWeaponWeightLabel[];

/// Encoded Rate label for the weapon comparison's rate column.
extern const u8 gItemMenuWeaponRateLabel[];

/// Encoded Range label for the weapon comparison's range column.
extern const u8 gItemMenuWeaponRangeLabel[];

/// Encoded Power label for the ammunition comparison's base attack amount.
extern const u8 gItemMenuAmmoPowerLabel[];

/// Encoded Attach. label for the armor comparison's attachment-slot count.
extern const u8 gItemMenuArmorAttachmentCountLabel[];

/// Encoded title for the no-compatible-weapon notice.
extern const u8 gItemMenuAttentionTitle[];

/// Encoded title for the weapon-selection panel.
extern const u8 gItemMenuWeaponSelectionTitle[];

/// Encoded title for equipment confirmation notices.
extern const u8 gItemMenuEquipTitle[];

/// Encoded title for the ammunition-selection panel.
extern const u8 gItemMenuAmmoSelectionTitle[];

/// Encoded title for the armor-selection panel.
extern const u8 gItemMenuArmorSelectionTitle[];

/// Encoded title for ammunition-loading and removal notices.
extern const u8 gItemMenuReloadTitle[];

/// Encoded title for an attachment confirmation notice.
extern const u8 gItemMenuAttachTitle[];

/// Encoded prompt describing the game-settings command.
extern const u8 gItemMenuOptionsHelpText[];

/// Encoded prompt for removing ammunition from the selected weapon load slot.
extern const u8 gItemMenuRemoveAmmoHelpText[];

/// Encoded prompt describing the inventory Sort command.
///
/// The first NUL ends the prompt. The retained 0xDD byte after it is not
/// displayed; its original purpose is unproven.
extern const u8 gItemMenuSortHelpText[36];

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

/// Draws and handles one armor-attachment candidate or the trailing detach row.
///
/// The shared list's `currentItemIndex` selects a carried candidate, excluding
/// empty/forbidden items and the equipped weapon; the final row detaches.
/// `gItemMenuArmorAttachmentList.selectedItemIndex` selects the destination armor slot (zero-based).
/// Confirm detaches its existing occupant, assigns the candidate if present and
/// dismisses the picker. Triangle opens item information with relocated preview.
/// Consumables display only quantity not loaded into weapons. Requires a valid
/// carried range, valid slot selection and loaded menu/text/primitive resources.
/// Both arguments borrow live UI storage; only rows with active input mutate it.
void itemMenuDrawAttachmentCandidateRow(UiList* list, UiObject* object);

/// Runs the armor attachment candidate picker and its selected-item preview.
///
/// spawnArg2 borrows the live task-owned UiObject; its parent must own another
/// live UiObject. Uses the singleton candidate list and carried inventory, with
/// a trailing detach row whose preview item is zero. Initialization hides the
/// parent and adds a 76-pixel preview header. Menu returns CANCEL; Cancel returns
/// DISMISS. spawnArg1 == 0 converts DISMISS to CONFIRM after child-result handling;
/// any nonzero argument retains DISMISS. Requires loaded menu/preview resources.
void itemMenuAttachmentItemPickerTask(Task* task);

/// Transfer-pane range indices used by the shared screen binding.
enum {
    ITEM_MENU_PANE_CONTAINER = 0,
    ITEM_MENU_PANE_CARRIED   = 1,
    ITEM_MENU_PANE_COUNT     = 2
};

/// Container/carried range descriptors borrowed by the single active transfer screen.
///
/// Initialization copies container selector 0..11 into pane 0 and the live save's
/// carried range into pane 1. Copies retain their backing table and row extents;
/// items can move in either direction. Initial descriptors select indirect rows
/// 0..59 and 60..119; that backing must be live if used before screen binding.
/// Storage remains valid until gameplay unloads; bindings are replaced on opening.
extern InventoryItemRange gItemMenuTransferRanges[ITEM_MENU_PANE_COUNT];

/// Mutable container/carried list controls for the single active transfer screen.
///
/// Indexed by ITEM_MENU_PANE_* after removing the battle-field flag. Each pane
/// binds its count to the corresponding range and enables the single shared
/// row callback. Selection is a range-relative item row, independent per pane;
/// viewport and drawing state are retained between updates and reset on opening.
extern UiList gItemMenuTransferPaneLists[ITEM_MENU_PANE_COUNT];

/// Mutable row callbacks for the active transfer-action popup, with at most three rows.
///
/// Empty rows offer Switch. Occupied rows offer Move, optionally Switch, then
/// optionally Use for supported consumables. The popup's item/visible counts
/// select the initialized prefix; a NULL or stale trailing slot is never
/// dispatched. Only the active transfer screen may rebuild this shared table.
extern UiListRowCallback gItemMenuTransferActionCallbacks[3];

/// All command label for moving every container item into carried inventory.
extern u8 gItemMenuTransferAllText[];

/// Select command label for choosing individual transfers.
extern u8 gItemMenuTransferSelectText[];

/// Discard command label for closing the battle-reward transfer without its remaining items.
extern u8 gItemMenuTransferDiscardText[];

/// Retained fourth transfer-exit label; the three-row command list does not dispatch it.
extern u8 gItemMenuTransferEndText[];

/// Encoded Move command label for transferring the selected row to the other pane.
extern u8 gItemMenuTransferMoveText[];

/// Encoded instructions for the ammunition quantity and confirmation controls.
extern u8 gItemMenuAmmoSplitHelpText[];

/// Encoded notice that loaded carried rounds cannot cross to the container.
extern u8 gItemMenuAmmoLoadedNoticeText[];

/// Encoded notice that the proposed stack has reached its per-item capacity.
extern u8 gItemMenuAmmoCapacityNoticeText[];

/// Encoded Switch command label for choosing a row to exchange across panes.
extern u8 gItemMenuTransferSwitchText[];

/// Returns whether the transfer destination has rows for every source item.
///
/// Returns 0 for an empty source, otherwise 0 or 1 from the row-capacity check.
/// Both singleton ranges must fit their loaded tables. Each occupied source
/// row needs a new row except a consumable (0xA0..0xBF) already present at the
/// destination. Quantities, stack limits and equipped-item restrictions are
/// not checked. No range or inventory contents are changed.
s32 itemMenuCanMoveAllItems(void);

/// Runs the carried-weapon choice list, optionally filtered by a consumable.
///
/// spawnArg1 is 0 for equipment selection or a consumable id 0xA0..0xBF for
/// loading; spawnArg2 borrows the live task-owned UiObject. State zero sizes
/// the singleton list, state one updates it and resolves child results. An
/// empty list shows a notice for 188 callback ticks. Menu returns CANCEL;
/// Cancel returns CONFIRM from the list, while empty-notice acceptance returns
/// DISMISS. The carried range and menu resources must remain live.
void itemMenuWeaponChoiceListTask(Task* task);

/// Layout and comparison modes for `itemMenuDrawEquipmentStats`.
enum {
    ITEM_MENU_EQUIPMENT_STATS_SUMMARY = 0,
    ITEM_MENU_EQUIPMENT_STATS_COMPARE = 1
};

/// Draws an equipment item's values, optionally compared with equipped values.
///
/// Weapon ids 0x80..0x9F show range/rate/weight, consumables 0xA0..0xBF show
/// power, and armor 0x60..0x7F shows HP/MP bonuses and attachment slots; other
/// ids draw nothing. SUMMARY begins 28 pixels below the content top; COMPARE
/// begins eight pixels below it and adds colors and increase/decrease/equal
/// icons. Lower weapon weight is better; other increases are better. Other
/// mode values use the upper layout without comparison. `unused` is ignored.
/// Requires a live object, loaded menu textures and writable GPU/OT storage;
/// panel visibility does not suppress drawing. Armor requires an equipped
/// armor selector 1..32. Consumables require an equipped weapon selector 1..32
/// and a selected load id 0 or 0xA0..0xBF; reload mode 2 selects secondary.
/// No pointers are retained; the shared caption-table pointer is replaced.
void itemMenuDrawEquipmentStats(const UiObject* object, s32 itemId, s32 displayMode, s32 unused);

/// Shows the equipped or selected item beside an equipment-choice panel.
///
/// spawnArg1 selects weapon (0), loaded consumable (1), armor (2), or the
/// published carried row (other values); spawnArg2 borrows the live UiObject.
/// Consumables require an equipped weapon selector 1..32 and a selected load
/// id 0 or 0xA0..0xBF; reload mode 2 selects secondary. Armor requires selector
/// 1..32. The carried row, when non-NULL, must stay readable and select an item
/// accepted by the menu's name/icon/preview APIs. Its pane omits equipment stats.
/// State 0 allocates one cached s32 item id on the primary heap and marks the
/// detail panel open. Initialization uses the existing menu-profile preview;
/// subsequent item changes request profile 0. State 2 hides the picture until
/// the CD queue is idle, then state 1 displays it on the next callback.
/// Closing clears the shared detail visibility flag; task teardown frees work.
/// Requires successful work allocation, live menu textures and GPU/OT storage.
void itemMenuEquipmentDetailTask(Task* task);

/// Draws and handles a carried-weapon choice for equipment or consumable loading.
///
/// The owner's spawnArg1 is 0 to equip, or a consumable id 0xA0..0xBF to load.
/// The list index must select a row counted by `itemMenuSetWeaponChoiceRows`;
/// filtered rows can repeat a weapon for its two compatible loads. Selection
/// updates help text and the filtered preview. Active-row Confirm equips and
/// opens load-slot selection, or packs weapon/consumable ids for the reload
/// notice. Triangle opens specifications. Publishes the weapon resultValue
/// and deactivates input after opening a child. Borrows the live list/object,
/// carried range and menu resources; no input pointer is retained.
void itemMenuDrawWeaponChoiceRow(UiList* list, UiObject* object);

/// Runs the weapon equipment-choice panel with preview and stat comparison.
///
/// spawnArg1 must be 0 (unfiltered equipment selection); spawnArg2 borrows the
/// live UiObject and the task must have a live UI parent. State 0 hides that
/// parent and opens its equipped-weapon detail child after sixteen nominal
/// 60-Hz ticks. Uses the singleton weapon list and readable carried range.
/// selectedItemIndex must already identify a carried weapon on every callback,
/// including initialization: the preview scan precedes the list's index reset
/// and does not stop at the range's row count.
/// Active or temporarily suspended active control requests the selected weapon's
/// relocated preview (profile 2). Other control uses profile 0 only when the
/// selected weapon is equipped.
/// A returned empty id or a busy CD queue hides the picture. Drawing precedes list
/// input processing. Clears resultValue and maps list DISMISS to CONFIRM;
/// other results follow `itemMenuWeaponChoiceListTask`. Menu resources and
/// the parent/object must remain live; drawing requires GPU/OT storage.
void itemMenuWeaponSelectionTask(Task* task);

/// Draws and handles a weapon's consumable choice or its Remove Ammo row.
///
/// Uses `gItemMenuConsumableChoiceItemIds` built by `itemMenuBuildConsumableChoiceList`; the
/// index must be below that list's itemCount. The owner's spawnArg1 low
/// halfword is weapon id 0x80..0x9F. Nonzero rows require a carried item row
/// and display available rounds/supply units, adding back this weapon's load
/// in reload mode 0. Zero is the removal command. Active-row Confirm packs
/// weapon/consumable ids for the reload notice; Triangle opens specifications
/// for nonzero rows. Both suspend parent input. Borrows the live object/list,
/// range and menu resources; no input pointer is retained.
void itemMenuDrawConsumableChoiceRow(UiList* list, UiObject* object);

/// Builds the shared choices of consumables a weapon can load or remove.
///
/// weaponItemId must be 0x80..0x9F, reload mode must be 0..2, and the live
/// carried range must fit its table.
/// Reload mode 0 searches both loads and adds back this weapon's current
/// quantity; mode 1 searches primary and mode 2 secondary using only unloaded
/// stock. Keeps positive quantities in catalog order without deduplication.
/// Single-load modes append item id 0 (Remove Ammo) only after a positive
/// choice. Both-load mode writes at most six ids; a single load writes at most
/// three ids plus the command. Sets itemCount and visibleRowCount, narrowing
/// them to bytes. Saved inventory and loads are unchanged; unused id slots
/// remain intact and no pointer is retained.
void itemMenuBuildConsumableChoiceList(UiList* list, s32 weaponItemId);

/// First spawn payload flag for a consumable choice opened after weapon equip.
///
/// The low halfword remains the weapon item id. With an empty list, this flag
/// permits an Equipped notice for melee capability or a nonempty weapon load;
/// an unavailable-ammunition notice returns CONFIRM instead of DISMISS.
enum { ITEM_MENU_LOAD_AFTER_EQUIP = 0x10000 };

/// Controls a weapon's loadable-consumable list or its empty-list notice.
///
/// spawnArg1's low halfword must be weapon id 0x80..0x9F, optionally combined
/// with `ITEM_MENU_LOAD_AFTER_EQUIP`. spawnArg2 borrows the task-owned UiObject.
/// Uses the singleton consumable list, `gItemMenuConsumableChoiceItemIds` and reload mode 0..2;
/// the live carried range must satisfy `itemMenuBuildConsumableChoiceList`.
/// Initialization sizes the list and limits its bottom to 70 centered pixels.
/// A nonempty list accepts input on later callbacks. An empty list shows a
/// 188-callback-tick notice, counting even while inactive; expiry is exactly
/// zero. Menu returns CANCEL. List Cancel returns CONFIRM; notice dismissal
/// returns DISMISS except the flagged unavailable-ammunition case. Child
/// DISMISS/CANCEL propagate; CONFIRM closes the child before reactivating input.
/// Object, parent/children and menu resources must remain live, with writable
/// GPU/OT storage. No inventory quantity or equipment selection changes here.
void itemMenuConsumableChoiceListTask(Task* choiceTask);

/// Adds comparison statistics and a relocated preview to the consumable picker.
///
/// Uses `itemMenuConsumableChoiceListTask`'s payload, ownership and range contract.
/// On entering its nonempty list, adds 76 pixels above the rows, hides the
/// parent and opens the equipped-consumable detail panel after sixteen ticks.
/// List input precedes selected-item statistics and preview; the selected index
/// must fit the built choice list. Id 0 (Remove Ammo) omits stats and hides the
/// picture. Active or suspended-active control requests profile 2; a busy CD
/// queue also hides the picture. Maps DISMISS to CONFIRM for the parent dialog.
void itemMenuAmmoSelectionTask(Task* task);

/// Controls the carried-armor picker with comparison statistics and preview.
///
/// spawnArg2 borrows the task-owned UiObject; the live carried range must fit
/// its readable table. Keeps carried row order and duplicate ids, excluding
/// every row of the equipped armor id. Initializes four visible rows, hides
/// the parent and opens its equipped-armor detail panel after sixteen ticks.
/// List input precedes lookup, statistics and the relocated preview. A missing
/// selection becomes item 0 and hides the picture; a busy CD queue hides it too.
/// Active or suspended-active control requests profile 2. Menu returns CANCEL;
/// Cancel and child DISMISS become CONFIRM. Accepted children close before
/// input reactivation. Requires live parent/children, menu resources and writable
/// GPU/OT storage. This task itself changes no equipment or inventory quantities.
void itemMenuArmorSelectionTask(Task* task);

#endif // GAMEPLAY_PRIVATE_ITEM_MENU_H
