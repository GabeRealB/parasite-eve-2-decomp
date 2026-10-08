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

extern InventoryItemRow Gp_ItemTable2[];

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

/// Word cleared by `_worldCoordInitPlayerLighting`. Also written by `attachmentApplySelfEffect` and
/// read/cleared by `_worldCoordUpdatePlayerLighting`.
extern s32 D_80114F28;

/// Preview/release selector shared by the attachment area and self-effect dispatchers.
enum { ATTACHMENT_TARGET_PREVIEW = 0,
       ATTACHMENT_TARGET_RELEASE = 1 };

/// Previews or applies the active attachment's timed combo or Healing effect.
///
/// Preview requests a player-light pulse without changing combos or HP.
/// Release uses `Gp_StateC08.attachId`: Antibody/Energy Shot stack up to two,
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

/// Percent scale table used by `damageComputeReceived` when `Gp_StateC08.antibodyCombo`
/// is non-zero. Indexed by `((antibodyCombo / 16) - 1) * 2 + (s8)(antibodyCombo % 16)`.
extern u16 D_80113CFC[];

/// Percent scale table used by `damageComputePlayerAttack` / `damageRollCriticalHit` when
/// `Gp_StateC08.energyShotCombo` is non-zero. Indexed by
/// `((energyShotCombo / 16) - 1) * 2 + (s8)(energyShotCombo % 16)`; `damageComputePlayerAttack` reads
/// `field_0` and `damageRollCriticalHit` reads `field_2` of each 4-byte slot.
extern u16 D_80113D0C[][2];

/// Final percent scale applied by `damageComputePlayerAttack`, indexed by `gSceneCombatState.difficulty`.
extern u16 D_80113F90[];

extern const char D_800938AC[8];

void Gp_HudTask(HudState* hud);

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

/// Pending flags written by `_itemUseAttachedItem` and consumed by `menuApplyPendingItemUseTask`.
/// `Gp_HealPending == 1` requests `taskMessageDispatch(..., 0x402, ...)`.
extern s32 Gp_HealPending;

/// Attachment slot selected by `itemMenuArmorAttachmentPanelTask` and its child UI descriptor.
extern s32 D_8010F884;

extern UiObjectDesc D_8010F8B4;

/// Signed pending item id consumed by `equipmentLoadPendingConsumable`. `_itemUseAttachedItem`
/// stores the id for the primary consumable pair, its negation for the secondary pair.
/// `menuApplyPendingItemUseTask` also consumes it (with `Gp_RelatedPending`) via `playerActorEnterReload`.
extern s32 Gp_PendingRelatedId;

/// Non-zero when `Gp_PendingRelatedId` should be applied by `menuApplyPendingItemUseTask`.
extern s32 Gp_RelatedPending;

/// Pending id consumed by `menuApplyPendingItemUseTask`; `0x3E` also calls `playerStateSetStatusEffects`.
extern s32 Gp_UsedItemId;

extern UiObjectDesc D_8010F6FC;

extern UiObjectDesc D_8010F718[4];

extern UiObjectDesc D_8010F788;

extern UiObjectDesc D_8010F7A4;

extern UiObjectDesc D_8010F7C0[2];

extern UiObjectDesc D_8010F7F8;

extern UiList D_8010F81C;

extern UiObjectDesc D_8010F840;

extern TaskDesc D_8010F85C;

extern UiObjectDesc D_8010F868;

extern UiObjectDesc D_8010F898;

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
