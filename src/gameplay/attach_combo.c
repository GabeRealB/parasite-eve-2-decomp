#include "attachments.h"

#include <psyq/rand.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/player_state.h"
#include "gameplay/scene_combat.h"

#include "main/wipsys.h"

s32 D_80114F28;

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
        D_80114F28 = ATTACHMENT_SELF_LIGHT_PULSE_REQUESTED;
        return;
    }

    // The packed id selects the release effect; previews never alter these timers.
    player = &gPlayerStatus;
    switch (Gp_StateC08.attachId) {
        case ATTACHMENT_ID_ANTIBODY_1:
        case ATTACHMENT_ID_ANTIBODY_2:
        case ATTACHMENT_ID_ANTIBODY_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;

            level      = Gp_StateC08.attachId % 10;
            comboParam = &Gp_AttachParams[ATTACHMENT_INDEX_ANTIBODY][level - 1].combo;
            _attachmentRefreshStackedCombo(&Gp_StateC08.antibodyCombo, &Gp_StateC08.antibodyTicks, comboParam, level);
            break;
        }
        case ATTACHMENT_ID_ENERGY_SHOT_1:
        case ATTACHMENT_ID_ENERGY_SHOT_2:
        case ATTACHMENT_ID_ENERGY_SHOT_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;

            level      = Gp_StateC08.attachId % 10;
            comboParam = &Gp_AttachParams[ATTACHMENT_INDEX_ENERGY_SHOT][level - 1].combo;
            _attachmentRefreshStackedCombo(&Gp_StateC08.energyShotCombo, &Gp_StateC08.energyShotTicks, comboParam, level);
            break;
        }
        case ATTACHMENT_ID_METABOLISM_1:
        case ATTACHMENT_ID_METABOLISM_2:
        case ATTACHMENT_ID_METABOLISM_3: {
            const AttachmentComboParam* comboParam;
            s32                         level;
            s32                         stackCount;
            s32                         durationFrames;

            level                       = Gp_StateC08.attachId % 10;
            comboParam                  = &Gp_AttachParams[ATTACHMENT_INDEX_METABOLISM][level - 1].combo;
            stackCount                  = Gp_StateC08.metabolismCombo & ATTACHMENT_COMBO_STACK_MASK;
            durationFrames              = comboParam->ticks;
            Gp_StateC08.metabolismCombo = stackCount;
            Gp_StateC08.metabolismTicks = durationFrames;
            if (Gp_StateC08.metabolismCombo == 0) {
                Gp_StateC08.metabolismCombo++;
            }
            Gp_StateC08.metabolismCombo |= level << ATTACHMENT_COMBO_LEVEL_SHIFT;
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

            levelRows = Gp_IdParamHi.rows;
            rowIndex  = ATTACHMENT_INDEX_HEALING * ATTACHMENT_AREA_LEVEL_COUNT + 1 + Gp_StateC08.attachId % ATTACHMENT_AREA_LEVEL_COUNT;
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
