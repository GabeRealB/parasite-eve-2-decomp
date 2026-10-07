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

void func_800CB6FC(UiObject* arg0, Task* arg1);

/// `arg5` is supplied by the attachment menu but unused by this renderer.
void Gp_DrawStackLeft(UiObject* arg0, s32 arg1, s32 arg2, InventoryItemRow* arg3, s32 arg4, s32 arg5);

void Gp_CheckItemInfoButton(UiObject* arg0);

void Gp_DrawItemNameRow(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

void Gp_ItemRowSelect(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3);

void Gp_ItemCmdMenuTask(Task* arg0);

void Gp_UseHealItemPanel(UiObject* arg0, Task* arg1, s32 arg2);

void Gp_InvokePeItemPanel(UiObject* arg0, Task* arg1, s32 arg2);

void func_800CC41C(UiObject* arg0, Task* arg1);

void Gp_YesNoMenuTask(Task* arg0);

void Gp_PeListPanelTask(Task* arg0);

void Gp_ItemCountHeaderTask(Task* arg0);

void Gp_PickupTask(Task* arg0);

UiObject* func_800CD814(UiObject* arg0);

void Gp_SpawnPickupUiTask(Task* arg0);

void Gp_PickupResultTask(Task* arg0);

void func_800CE188(Task* arg0);

void Gp_PickupExitTask(Task* arg0);

/// 10-byte records selected by `damageGetPlayerAttackHitCooldown` when the id's 0x8000 bit is
/// clear. Indexed by `id & 0x7F`.
extern WeaponAttackRow Gp_IdParamLo[];

void Gp_UseKeyItemRow(Task* arg0);

void Gp_KeyItemSubMenuTask(Task* arg0);

void Gp_DrawCollectedRow(UiList* arg0, UiObject* arg1);

void Gp_KeyItemMenuTask(Task* arg0);

void func_800D29B0(Task* arg0);

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

void func_800CFA34(UiObject* arg0, Task* arg1);

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

void Gp_DrawReviveCmd(UiList* arg0, UiObject* arg1);

void Gp_PeCommandMenuTask(Task* arg0);

void Gp_DiscardWarnTask(Task* arg0);

void Gp_DrawPeSlotRow(UiList* arg0, UiObject* arg1);

void Gp_PeUpgradePanelTask(Task* arg0);

void Gp_MapMenuListTask(Task* arg0);

void Gp_MapScreenTask(Task* arg0);

void Gp_DrawUseAttachCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawKeyItemCmd(UiList* arg0, UiObject* arg1);

/// Draws the PE release/strengthen menu's Cancel row and confirms the parent on selection.
///
/// Writes the object's result, without publishing a list command id.
void itemMenuDrawPeCancelRow(UiList* list, UiObject* object);

void Gp_DrawMapCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawDiscardCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawNextLevelCmd(Task* arg0);

void func_800D573C(Task* arg0);

void Gp_DrawSpecsCmd(Task* arg0);

void Gp_DrawExaminePushCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawItemCmd(UiList* arg0, UiObject* arg1);

void func_800D5A48(Task* arg0);

extern char Gp_StrReleasePe[];

void Gp_DrawWeaponSlotRow2(UiList* prompt, UiObject* obj);

void func_800C41A4(UiList* prompt, UiObject* obj);

void Gp_DrawItemIcon(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_800C2538(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_800C22D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_DrawHpMpStats(UiPanel* arg0, s32 arg1);

void Gp_DrawEquipSummary(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);

void Gp_UiPromptDispatch(UiObject* arg0, Task* arg1);

void Gp_StatusPanelTask(Task* arg0);

void Gp_HpMpBarTask(Task* arg0);

void Gp_ArmorStatsPanelTask(Task* arg0);

void Gp_PeGridPanelTask(Task* arg0);

void Gp_DrawItemOrderRow(UiList* arg0, UiObject* arg1);

void Gp_CountAmmoRows(UiList* arg0, s32 arg1);

void Gp_ItemDestCursorTask(Task* arg0);

void Gp_DrawWeaponSlotRow(UiList* prompt, UiObject* obj);

void Gp_WeaponMenuTask(Task* arg0);

void Gp_ArmorMenuTask(Task* arg0);

InventoryItemRow* Gp_NthEquippableRec(InventoryItemRange* arg0, s32 arg1, s32 arg2);

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

/// CLUT ids for the ten item-category icons drawn by `Gp_DrawItemIcon`,
/// indexed by the icon index that function derives from the item id.
extern const u16 D_80096F88[12];

#define D_8010EAD0 D_8010EAB4[1]

#define D_8010EC3C D_8010EAB4[14]

#define D_8010ECAC D_8010EAB4[18]

#define D_8010ECC8 D_8010EAB4[19]

#define D_8010ECE4 D_8010EAB4[20]

#define D_8010ED00 D_8010EAB4[21]

#define D_8010EE6C D_8010EAB4[34]

#define D_8010EF14 D_8010EAB4[40]

#define D_8010EF84 D_8010EAB4[44]

#define D_8010F010 D_8010EAB4[49]

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

void Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1);

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
