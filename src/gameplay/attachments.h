#ifndef GAMEPLAY_PRIVATE_ATTACHMENTS_H
#define GAMEPLAY_PRIVATE_ATTACHMENTS_H

#include "types.h"

#include "hud.h"
#include "weapon_data.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

struct Enemy;

// Attachment parameters, combination state and menu support.

/// Player request to start an armed attachment's cast countdown on the next HUD update.
///
/// The HUD consumes this bit in armed/casting mode and publishes the charge phase.
/// Target effects and resource spending wait for countdown expiry.
#define ATTACHMENT_FLAG_START_CAST 4

/// One-update signal to start the current attachment's charge visuals.
///
/// Stored in `AttachmentState.effectPhase` when the countdown starts; the player
/// effect dispatcher spawns the level-selected charge task. The next HUD update
/// resets the signal to idle independently of the running countdown or PE tasks.
#define ATTACHMENT_EFFECT_CHARGE (-1)

/// One-update signal to spawn the current attachment's released PE effect.
///
/// Published after countdown expiry and sound-load readiness, alongside resource
/// spending. Applying target/self effects is a separate request from the PE task;
/// the next HUD update resets this phase signal to idle.
#define ATTACHMENT_EFFECT_RELEASED 1

/// No pending or handled cast sound-file load in `AttachmentState.soundStep`.
///
/// Cast completion, cancellation and battle transitions restore this value.
/// It describes the request state, not sound-script playback or CD completion.
#define ATTACHMENT_SOUND_IDLE 0

/// Antibody ability slot: earth group's first spell, decimal cast family 41.
///
/// Levels 1..3 select this area's rows 0..2 to refresh its stacked protection timer.
#define ATTACHMENT_INDEX_ANTIBODY 9

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

/// Elapsed attachment-HUD updates while an armed cast's sound is queued.
///
/// Arming clears this signed word. Queued updates increment it; the first
/// positive result completes the sound request and clears it again, including
/// outside battle when no sound file is enqueued.
extern s32 gAttachmentSoundQueueTicks;

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

/// Weapon-damage and critical-chance multipliers for one Energy Shot combo.
///
/// Both are unsigned whole percentages: multiply the incoming damage or chance,
/// then divide by 100 with the consumer's integer truncation. A value of 100
/// preserves that input. The fields scale independent calculations; neither
/// is an additive bonus or an absolute critical probability.
typedef struct {
    u16 damagePercent;   // Weapon HP-damage multiplier, in whole percent
    u16 criticalPercent; // Critical-chance multiplier, in whole percent
} AttachmentEnergyShotComboScale;
STATIC_ASSERT_SIZEOF(AttachmentEnergyShotComboScale, 4);

/// Energy Shot weapon-damage and critical-chance scales for level/stack combos.
///
/// Row 0 is the neutral 100/100 scale. With an active combo, level 1..3 and
/// stack 1..2 select `(level - 1) * 2 + stack` (rows 1..6). The high nibble
/// holds the latest cast's level and the low nibble the accumulated stack.
/// A cleared combo skips the table. Borrowed rows live with gameplay.
extern AttachmentEnergyShotComboScale gAttachmentEnergyShotComboScales[7];

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
/// suppressed frames pause the PE update and HUD input delay. Demo scenes
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
/// `gItemMenuSelectedRow` must remain a live carried row. A weapon id 0x80..0x9F
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
