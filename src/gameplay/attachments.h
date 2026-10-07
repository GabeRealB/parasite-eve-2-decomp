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

/// Word cleared by Gp_SetAttachState and incremented by Gp_UseItemTask.
extern s32 D_80114C34;

/// Read position in the recorded demo pad stream: button/count pairs.
extern u16* Gp_ReplayCursor;

u16 Gp_GetAttachParam(s32 arg0);

/// Word cleared by `_worldCoordInitPlayerLighting`. Also written by `Gp_UpdateAttachCombo` and
/// read/cleared by `_worldCoordUpdatePlayerLighting`.
extern s32 D_80114F28;

void Gp_UpdateAttachCombo(s32 arg0);

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

void Gp_ApplyAttachStats(s32 arg0, HudState* hud);

/// Pending flags written by `Gp_ApplyItemUse` and consumed by `Gp_MenuExitCallback`.
/// `Gp_HealPending == 1` requests `taskMessageDispatch(..., 0x402, ...)`.
extern s32 Gp_HealPending;

/// Attachment slot selected by `func_800D6334` and its child UI descriptor.
extern s32 D_8010F884;

extern UiObjectDesc D_8010F8B4;

/// Signed pending item id consumed by `Gp_FlushPendingRelated`. `Gp_ApplyItemUse`
/// stores the id for the primary consumable pair, its negation for the secondary pair.
/// `Gp_MenuExitCallback` also consumes it (with `Gp_RelatedPending`) via `func_801088D4`.
extern s32 Gp_PendingRelatedId;

/// Non-zero when `Gp_PendingRelatedId` should be applied by `Gp_MenuExitCallback`.
extern s32 Gp_RelatedPending;

/// Pending id consumed by `Gp_MenuExitCallback`; `0x3E` also calls `Gp_TriggerPeState`.
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

void Gp_AttachListTask(Task* arg0);

void Gp_SelectAmmoMenuTask(Task* arg0);

void Gp_DrawArmorSelectRow(UiList* arg0, UiObject* arg1);

void Gp_SelectArmorMenuTask(Task* arg0);

void Gp_ReloadPromptTask(Task* arg0);

void Gp_AttachPromptTask(Task* arg0);

void Gp_EquipPromptTask(Task* arg0);

void Gp_DrawLoadCmd(UiList* arg0, UiObject* arg1);

void Gp_DrawExchangeCmd(UiList* arg0, UiObject* arg1);

#endif // GAMEPLAY_PRIVATE_ATTACHMENTS_H
