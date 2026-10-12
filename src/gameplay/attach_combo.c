#include "attachments.h"

#include <psyq/rand.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/player_state.h"
#include "gameplay/scene_combat.h"

#include "main/wipsys.h"

/// Low-nibble mask for the stack count in the packed attachment combo byte.
///
/// Refresh retains these four bits and replaces the high-nibble cast level.
/// Valid counts are 0..2 for Antibody/Energy Shot and 0..1 for Metabolism;
/// extraction also retains pre-existing values outside those domains.
#define ATTACHMENT_COMBO_STACK_MASK 0xF

/// Maximum stack reached by refreshing a valid Antibody or Energy Shot combo.
///
/// Counts below two increment; counts already at or above two are retained.
/// This does not clamp malformed low-nibble values back to two.
#define ATTACHMENT_COMBO_STACK_CAP 2

/// Packed Metabolism cast ID: water group, first ability, level one.
#define ATTACHMENT_ID_METABOLISM_1 311

/// Packed Metabolism cast ID: water group, first ability, level two.
#define ATTACHMENT_ID_METABOLISM_2 312

/// Packed Metabolism cast ID: water group, first ability, level three.
#define ATTACHMENT_ID_METABOLISM_3 313

/// Packed Healing cast ID: water group, second ability, level one.
#define ATTACHMENT_ID_HEALING_1 321

/// Packed Healing cast ID: water group, second ability, level two.
#define ATTACHMENT_ID_HEALING_2 322

/// Packed Healing cast ID: water group, second ability, level three.
#define ATTACHMENT_ID_HEALING_3 323

/// Packed Antibody cast ID: earth group, first ability, level one.
#define ATTACHMENT_ID_ANTIBODY_1 411

/// Packed Antibody cast ID: earth group, first ability, level two.
#define ATTACHMENT_ID_ANTIBODY_2 412

/// Packed Antibody cast ID: earth group, first ability, level three.
#define ATTACHMENT_ID_ANTIBODY_3 413

/// Packed Energy Shot cast ID: earth group, second ability, level one.
#define ATTACHMENT_ID_ENERGY_SHOT_1 421

/// Packed Energy Shot cast ID: earth group, second ability, level two.
#define ATTACHMENT_ID_ENERGY_SHOT_2 422

/// Packed Energy Shot cast ID: earth group, second ability, level three.
#define ATTACHMENT_ID_ENERGY_SHOT_3 423

s32 gAttachmentPlayerLightPulsePending;

/// Inline copy of `sceneIsBattleActive`.
static __inline__ s32 isStateF0Active_(void);

/// Inline copy of `sceneIsBattleActive`.
static __inline__ s32 isStateF0Active_(void)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
        return 1;
    }
    return 0;
}

/// Refreshes an antibody or energy-shot combo and records the latest cast's level.
///
/// Borrows distinct live combo/timer storage and a readable duration row.
/// `castLevel` must be 1..3 and replaces the old high nibble, including when
/// recasting at a lower level. The low nibble increments from 0 or 1 and retains
/// values 2..15. Duration narrows to a signed halfword; positive values count
/// eligible attachment updates, which pause while the wheel is open.
static inline void _attachmentRefreshStackedCombo(s8* packedCombo, s16* remainingFrames, const AttachmentComboParam* durationRow, s32 castLevel)
{
    s32 stackCount;
    s32 durationFrames;

    stackCount       = *packedCombo & ATTACHMENT_COMBO_STACK_MASK;
    durationFrames   = durationRow->ticks;
    *packedCombo     = stackCount;
    *remainingFrames = durationFrames;
    if (*packedCombo < ATTACHMENT_COMBO_STACK_CAP) {
        (*packedCombo)++;
    }
    *packedCombo |= castLevel << ATTACHMENT_COMBO_LEVEL_SHIFT;
}

void attachmentApplySelfEffect(s32 releaseEffects)
{
    enum { ATTACHMENT_SELF_LIGHT_PULSE_REQUESTED = 1,
           ATTACHMENT_SELF_CLEAR_STATUSES        = 1 };
    PlayerStatus* player;

    if (releaseEffects == ATTACHMENT_TARGET_PREVIEW) {
        gAttachmentPlayerLightPulsePending = ATTACHMENT_SELF_LIGHT_PULSE_REQUESTED;
        return;
    }

    // The packed id selects the release effect; previews never alter these timers.
    player = &gPlayerStatus;
    switch (gAttachmentState.attachId) {
        case ATTACHMENT_ID_ANTIBODY_1:
        case ATTACHMENT_ID_ANTIBODY_2:
        case ATTACHMENT_ID_ANTIBODY_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;

            level      = gAttachmentState.attachId % 10;
            comboParam = &gAttachmentAreaTable[ATTACHMENT_INDEX_ANTIBODY][level - 1].combo;
            _attachmentRefreshStackedCombo(&gAttachmentState.antibodyCombo, &gAttachmentState.antibodyTicks, comboParam, level);
            break;
        }
        case ATTACHMENT_ID_ENERGY_SHOT_1:
        case ATTACHMENT_ID_ENERGY_SHOT_2:
        case ATTACHMENT_ID_ENERGY_SHOT_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;

            level      = gAttachmentState.attachId % 10;
            comboParam = &gAttachmentAreaTable[ATTACHMENT_INDEX_ENERGY_SHOT][level - 1].combo;
            _attachmentRefreshStackedCombo(&gAttachmentState.energyShotCombo, &gAttachmentState.energyShotTicks, comboParam, level);
            break;
        }
        case ATTACHMENT_ID_METABOLISM_1:
        case ATTACHMENT_ID_METABOLISM_2:
        case ATTACHMENT_ID_METABOLISM_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;
            s32                         stackCount;
            s32                         durationFrames;

            level                            = gAttachmentState.attachId % 10;
            comboParam                       = &gAttachmentAreaTable[ATTACHMENT_INDEX_METABOLISM][level - 1].combo;
            stackCount                       = gAttachmentState.metabolismCombo & ATTACHMENT_COMBO_STACK_MASK;
            durationFrames                   = comboParam->ticks;
            gAttachmentState.metabolismCombo = stackCount;
            gAttachmentState.metabolismTicks = durationFrames;
            if (gAttachmentState.metabolismCombo == 0) {
                gAttachmentState.metabolismCombo++;
            }
            gAttachmentState.metabolismCombo |= level << ATTACHMENT_COMBO_LEVEL_SHIFT;
            playerStateSetStatusEffects(ATTACHMENT_SELF_CLEAR_STATUSES, PLAYER_STATUS_ALL_EFFECTS);
            break;
        }
        case ATTACHMENT_ID_HEALING_1:
        case ATTACHMENT_ID_HEALING_2:
        case ATTACHMENT_ID_HEALING_3: {
            const AttachmentLevelRow* levelRows;
            s32                       rowIndex;
            s32                       minimumHp;
            s32                       maximumHp;
            s32                       restoredHp;

            // Outside combat use minimum HP; battle blends toward the maximum.
            enum { ATTACHMENT_HEAL_BLEND_SCALE = 256,
                   ATTACHMENT_HEAL_RANDOM_MASK = 255,
                   ATTACHMENT_HEAL_BLEND_SHIFT = 8 };

            levelRows = gAttachmentLevelTable.rows;
            rowIndex  = ATTACHMENT_INDEX_HEALING * ATTACHMENT_LEVEL_COUNT + 1 + gAttachmentState.attachId % ATTACHMENT_LEVEL_COUNT;
            maximumHp = levelRows[rowIndex].column.outcome.healMax;
            minimumHp = levelRows[rowIndex].column.amount;
            if (minimumHp >= maximumHp || !isStateF0Active_()) {
                restoredHp = minimumHp;
            } else {
                // Sample 1..256; fixed-point rounding can still yield the minimum HP.
                restoredHp = (rand() & ATTACHMENT_HEAL_RANDOM_MASK) + 1;
                restoredHp = (maximumHp * restoredHp + minimumHp * (ATTACHMENT_HEAL_BLEND_SCALE - restoredHp)) >> ATTACHMENT_HEAL_BLEND_SHIFT;
                if (restoredHp <= 0) {
                    restoredHp = 1;
                }
            }
            player->hp += restoredHp;
            if (player->hpMax < player->hp) {
                player->hp = player->hpMax;
            }
            break;
        }
    }
}
