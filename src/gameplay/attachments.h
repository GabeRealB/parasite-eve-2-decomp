#ifndef GAMEPLAY_PRIVATE_ATTACHMENTS_H
#define GAMEPLAY_PRIVATE_ATTACHMENTS_H

#include "types.h"

#include "damage.h"
#include "hud.h"
#include "weapon_data.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

struct Enemy;

// Attachment parameters, combination state and menu support.

/// Records a Parasite Energy attack in an enemy's borrowed contact table.
///
/// `enemy` must be non-NULL; a NULL contact table leaves it and the cast's
/// target count unchanged. Otherwise the writable table must end in
/// WORLD_COLLISION_CONTACT_LAST. The first empty entry is used, or the final
/// entry is replaced when all are occupied. `attackKey` is the complete packed
/// attack identity supplied by the targeting scan. Distance, point and
/// response components are zeroed, pad halfwords and existing flags are
/// preserved, and OCCUPIED is set. Each write increments the current PE cast's
/// byte-sized target count, including replacements, with byte wraparound.
/// No enemy or contact pointer is retained.
void attachmentAddTargetContact(const struct Enemy* enemy, s32 attackKey);

/// Returns one parameter of the level selected by a packed PE menu id.
///
/// Only bits 0..5 of `abilityId` are read: element (4..5), energy (2..3),
/// level (0..1). They select row `(element * 3 + energy) * 3 + level`, at most
/// 39; the caller supplies an `ATTACHMENT_LEVEL_*` column in 0..7. Level zero
/// reads that row directly rather than substituting level one. EXP cost is
/// discounted to 4/5 for positive game modes, or 2/5 for a cleared normal game.
/// Returns the low 16 bits in s32 and does not change saved or table state.
s32 attachmentGetPackedLevelValue(s32 abilityId, s32 column);

// Shared HUD/replay work.
extern HudHpMp Gp_HpMpWork;

/// Current replay buttons and remaining frame count.
extern u16 Gp_ReplayButtons;

extern u16 Gp_ReplayFramesLeft;

/// Word cleared by _attachmentArmCast and incremented by _attachmentUpdateAndDrawHud.
extern s32 D_80114C34;

/// Read position in the recorded demo pad stream: button/count pairs.
extern u16* Gp_ReplayCursor;

/// Returns one level parameter of the active spell or item attachment.
///
/// `column` selects an `ATTACHMENT_LEVEL_*` halfword in 0..7. The active
/// ability index must be 0..17. Spells (0..11) use saved learned levels, or
/// training levels in stage 1 area 20 with player resource variant 4. Zero
/// becomes level 1; berserker raises levels below 3 by one. Item attachments
/// (12..17) always use level 1 and receive no berserker level increase.
/// Learned levels must be 0..3; no bounds checks or table writes occur.
u16 attachmentGetActiveLevelValue(s32 column);

/// Pending attachment-preview pulse in the player's directional light colours.
///
/// Zero means no request; nonzero requests one green pulse on the next normal
/// player-lighting update, which consumes it. Initialization clears it; light-
/// probe updates retain it. This signed word owns no model or matrix storage.
extern s32 gAttachmentPlayerLightPulsePending;

/// Preview/release selector shared by the attachment area and self-effect dispatchers.
enum { ATTACHMENT_TARGET_PREVIEW = 0,
       ATTACHMENT_TARGET_RELEASE = 1 };

/// Previews or applies the active attachment's timed combo or Healing effect.
///
/// Preview requests a player-light pulse without changing combos or HP.
/// Release uses `gAttachmentState.attachId`: Antibody/Energy Shot stack up to two,
/// Metabolism starts one stack and clears statuses, and Healing adds capped HP.
/// Other ids do nothing on release. Timers count active frames; levels are 1..3.
/// Healing outside battle uses the row minimum; battle rolls a 1/256 blend.
void attachmentApplySelfEffect(s32 releaseEffects);

/// Percentages `damageIsEnemyDamageOverTimeExpired` scales an enemy's `param->damageOverTimeTicks` by,
/// one per `Enemy.damageOverTimeGrade`: how long the damage-over-time reaction lasts.
extern u16 D_80113D28[];

/// Percentages `damageTickEnemyBuildup` scales an enemy's `param->buildupSteps` by, one
/// per `Enemy.buildupGrade`: how far the buildup reaction builds up.
extern u16 D_80113D30[];

/// Percentages of an enemy's `param->hpMax` that one pulse of the
/// damage-over-time reaction removes, one per `Enemy.damageOverTimeGrade`.
extern u16 D_80113D38[];

/// Damage-scale rows used by `damageComputeReceived`. Indexed by `gSceneCombatState.difficulty`.
extern DamageReceivedScaleRow Gp_DmgRows[];

/// Column index table for `Gp_DmgRows`, indexed by signed HP / 10.
extern u16 D_80113F54[];

/// Percent scale table used by `damageComputeReceived` when `gAttachmentState.antibodyCombo`
/// is non-zero. Indexed by `((antibodyCombo / 16) - 1) * 2 + (s8)(antibodyCombo % 16)`.
extern u16 D_80113CFC[];

/// Percent scale table used by `damageComputePlayerAttack` / `damageRollCriticalHit` when
/// `gAttachmentState.energyShotCombo` is non-zero. Indexed by
/// `((energyShotCombo / 16) - 1) * 2 + (s8)(energyShotCombo % 16)`; `damageComputePlayerAttack` reads
/// `field_0` and `damageRollCriticalHit` reads `field_2` of each 4-byte slot.
extern u16 D_80113D0C[][2];

/// Final percent scale applied by `damageComputePlayerAttack`, indexed by `gSceneCombatState.difficulty`.
extern u16 D_80113F90[];

extern const char D_800938AC[8];

/// Advances the HUD's battle sequence, menu requests and visible frame updates.
///
/// Borrows persistent writable HUD state from the live play-clock task.
/// Requires initialized player/save/session, combat, attachment, display, pad
/// and rendering state. Menu requests are handed to the display on the next
/// call and dropped if a request or display hold prevents acceptance.
/// A pending view-gate task delays menu and battle-transition requests.
/// End-delay frames tick only while actors run; transitions borrow this HUD
/// until their tasks finish. Battle-end supply reload requires weapon 1..32
/// and the player-task requirements of the end-action handlers.
/// Rebuilds suppression before drawing status/radar/target HP and updating PE;
/// suppressed frames pause the PE update and item-grant cooldown. Demo scenes
/// also require room for two textured quads in the current GPU packet arena.
void hudUpdateAndDraw(HudState* hud);

/// Previews an attachment's target area or releases its targeting/self effect.
///
/// `releaseEffects` is ATTACHMENT_TARGET_PREVIEW or ATTACHMENT_TARGET_RELEASE;
/// selects wheelIndex or activeIndex respectively, each in 0..17. Spell learned
/// levels must be 0..3; unlearned spells use level one, training and Berserker
/// adjust the effective level, and item slots use level one. Area dimensions
/// convert from hundreds to whole world units. Clears the cast target count and
/// life-drain total on every call. Outside battle only Healing is dispatched.
/// `hud` may be NULL; otherwise updates its range overlay during active battle.
/// Borrows live player/combat/attachment state and targeting resources.
void attachmentDispatchTargetArea(s32 releaseEffects, HudState* hud);

/// Releases the active ability's target or self effect without a HUD preview.
///
/// Uses activeIndex (0..17), the learned/training level and live targeting state
/// as required by `attachmentDispatchTargetArea`. Outside battle only Healing
/// is applied. Resets cast target/life-drain totals and always returns zero.
s32 attachmentReleaseTargetEffects(void);

/// Deferred item-use presentation request values; storage remains a signed word.
enum {
    ITEM_MENU_USE_PRESENTATION_NONE    = 0,
    ITEM_MENU_USE_PRESENTATION_PENDING = 1
};

/// Requests the player's item-use presentation after the menu restores world resources.
///
/// Zero means none; one requests `PLAYER_ACTOR_MESSAGE_ENTER_ITEM_USE` for
/// successful medicine, ward or perfume use. Attachment-panel entry clears it;
/// menu exit consumes it even when scripted player control rejects presentation.
extern s32 gItemMenuUsePresentationPending;

/// Retained zero-based selection in the equipped armor's attachment slots.
///
/// With a positive slot count, selection wraps within the armor's slots (at most ten).
/// Opening resets an index beyond that count to zero; closing preserves it.
/// Inventory rows store the corresponding one-based attachment position.
extern s32 gItemMenuSelectedAttachmentIndex;

/// Companion weapon-summary panel for attachment use; its content argument is unused.
extern UiObjectDesc gItemMenuAttachmentUseWeaponPanelDesc;

/// Deferred weapon-consumable selection, with the load encoded in the sign.
///
/// Zero means none; +0xA0..+0xBF selects primary and -0xA0..-0xBF secondary.
/// Selecting an attached consumable replaces it and requests a menu reload.
/// Outside battle, menu exit discards it; in battle it survives until the loading
/// cue, including a rejected scripted reload. Loading consumes the magnitude
/// and clears the selection before attempting the transfer, even on failure.
extern s32 gEquipmentPendingConsumableId;

/// Menu-exit reload notification values; storage remains a signed word.
enum {
    EQUIPMENT_MENU_RELOAD_NONE    = 0,
    EQUIPMENT_MENU_RELOAD_PENDING = 1
};

/// Requests menu-exit handling of the signed pending weapon-consumable selection.
///
/// Written as zero or one, with any nonzero value treated as pending. With a
/// nonzero `gEquipmentPendingConsumableId`, menu exit cancels outside battle or
/// requests the selected reload, then clears this notification independently of
/// that id. A notification without an id is retained; loading clears only the id.
extern s32 gEquipmentMenuReloadPending;

/// Most recent non-equipment attachment-use attempt awaiting menu-exit effects.
///
/// Zero means none. The item id is recorded before checking whether ordinary
/// item use succeeds; weapon, consumable and armor ids do not replace it.
/// Attachment-panel entry clears it. Menu exit applies Berserker for Eau de
/// Toilette (0x3E), then clears any recorded id independently of presentation.
extern s32 gItemMenuAttemptedUseItemId;

/// Discard confirmation/restriction panel for the borrowed selected inventory row.
///
/// The content argument starts at zero; a restriction changes it to a notice id.
/// Initial bounds are replaced with bounds fitted to the prompt or notice text.
extern UiObjectDesc gItemMenuDiscardPanelDesc;

/// Number of element-panel recipes, in Fire, Wind, Water, Earth order.
enum { ITEM_MENU_PE_ELEMENT_PANEL_COUNT = 4 };

/// Four PE element-list panel recipes in Fire, Wind, Water, Earth order.
///
/// Each receives its element index (0..3) as the content argument. Their screen
/// positions form two columns; each callback fits its own two/three-ability list.
extern UiObjectDesc gItemMenuPeElementPanelDescs[ITEM_MENU_PE_ELEMENT_PANEL_COUNT];

/// Timed item-menu notice panel, fitted from its initial 1x1 bounds to the text.
///
/// Content low 16 bits index `gItemMenuNoticeTexts` (0..32). A nonzero signed
/// upper half requests CONFIRM on acknowledgement; zero requests DISMISS.
extern UiObjectDesc gItemMenuNoticeDesc;

/// PE level-purchase panel with a Yes/No child and next-level specifications.
///
/// Content is a packed PE menu id: element 0..3, ability 0..2, current level 0..2.
/// The panel shows the next level's EXP cost/MP bonus and checks funds on Yes.
extern UiObjectDesc gItemMenuPeUpgradePanelDesc;

/// Indices of the PE auxiliary panel recipes.
enum {
    ITEM_MENU_PE_AUXILIARY_NEXT_LEVEL  = 0,
    ITEM_MENU_PE_AUXILIARY_HEALING     = 1,
    ITEM_MENU_PE_AUXILIARY_PANEL_COUNT = 2
};

/// PE auxiliary recipes: next-level specifications, then a healing result panel.
///
/// The upgrade panel spawns entry zero with the current packed PE id (level 0..2).
/// Entry one delegates its content argument to `itemMenuApplyHealingPanel` and
/// has no current spawn site.
extern UiObjectDesc gItemMenuPeAuxiliaryPanelDescs[ITEM_MENU_PE_AUXILIARY_PANEL_COUNT];

/// Current-level PE specifications panel opened from an element's selected ability.
///
/// Content is a packed PE menu id: element 0..3, ability 0..2, level 0..3.
/// Unlearned level zero displays level one; loaded preview text supplies detail.
extern UiObjectDesc gItemMenuPeSpecificationsPanelDesc;

/// Mutable singleton two-row hotspot list: Examine/Push, followed by Item.
///
/// Both rows remain visible and navigation wraps. Fitting/input update this
/// storage in place, so it supports one active hotspot command panel at a time.
extern UiList gItemMenuHotspotCommandList;

/// Hotspot command-panel recipe; its content word selects Examine or Push.
///
/// Exactly one draws Push; all other values draw Examine. The Item row is always
/// present. The host supplies screen-centered pixels and the callback fits the
/// panel to the shared list, clipping its lower/right edges inside the screen.
extern UiObjectDesc gItemMenuHotspotCommandPanelDesc;

/// Bodyless host task for a hotspot command panel and its delayed teardown.
///
/// First spawn argument forwards the full Examine/Push choice to the panel;
/// the second starts at zero and becomes its owned root UI object pointer.
extern TaskDesc gItemMenuHotspotMenuTaskDesc;

/// Recessed item-preview panel, resized from its initial short placeholder bounds.
///
/// Content zero selects a 132x100-pixel interior; nonzero selects 132x131.
/// The collected-item menu uses zero. A pending CD request hides the picture.
extern UiObjectDesc gItemMenuPreviewPanelDesc;

/// Equipped-armor attachment-use root panel with a companion weapon summary.
///
/// Its unused content argument starts at zero. Opening resets item-use
/// notifications and bounds the retained attachment selection to the armor.
extern UiObjectDesc gItemMenuArmorAttachmentPanelDesc;

/// Draws an unequipped carried armor choice and opens equip or information panels.
///
/// currentItemIndex is zero-based in the carried armor list, excluding equipped
/// armor; a missing row resolves to item 0. The selected description stays visible
/// when either control half denotes active mode. Hidden panels omit the row's
/// name, E/L marker and icon. Active Confirm equips the choice; Triangle opens its
/// information with a relocated preview. Either request makes the parent inactive
/// even if allocation fails. Borrows the live list/object and carried save;
/// requires the range to fit its table and menu/text textures and GPU storage.
void itemMenuDrawArmorChoiceRow(UiList* list, UiObject* object);

/// Draws Load and opens a weapon-slot or compatible-weapon choice on active Confirm.
///
/// `Gp_SelItemRec` must remain a live carried row. A weapon id 0x80..0x9F
/// opens its load-slot list and resets the removal selector to both slots;
/// a consumable id 0xA0..0xBF opens the compatible-weapon list. Other ids only
/// play the confirmation sound. A recognized request consumes row input and
/// makes the parent inactive even if allocation fails; a successful child is
/// positioned at the command row. Drawing uses unsigned row-coordinate views
/// and requires menu/text textures and writable GPU storage, even when hidden.
void itemMenuDrawLoadRow(UiList* list, UiObject* object);

/// Draws Exchange and opens the ammunition, weapon or armor replacement list.
///
/// Active Confirm uses the borrowed selected row: consumable ids 0xA0..0xBF
/// or a missing/empty row select ammunition for the equipped weapon, weapon ids
/// 0x80..0x9F select a carried weapon, and armor ids 0x60..0x7F select armor.
/// The ammunition path requires an equipped weapon selector in 1..32. Other ids
/// only play the confirmation sound. Recognized requests consume row input and
/// make the parent inactive even if allocation fails. Children open after sixteen
/// callback ticks at screen-centered (-8, -92) pixels. Drawing uses unsigned row
/// coordinates and requires menu/text textures and writable GPU storage, even hidden.
void itemMenuDrawExchangeRow(UiList* list, UiObject* object);

#endif // GAMEPLAY_PRIVATE_ATTACHMENTS_H
