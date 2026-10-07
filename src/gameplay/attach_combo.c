#include "attachments.h"

#include <psyq/rand.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/player_state.h"
#include "gameplay/scene_combat.h"

#include "main/wipsys.h"
#include <psyq/rand.h>

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

void Gp_UpdateAttachCombo(s32 arg0)
{
    PlayerStatus* cfg;

    if (arg0 == 0) {
        D_80114F28 = 1;
        return;
    }

    cfg = &gPlayerStatus;
    switch (Gp_StateC08.attachId) {
        case ATTACHMENT_ID_ANTIBODY_1:
        case ATTACHMENT_ID_ANTIBODY_2:
        case ATTACHMENT_ID_ANTIBODY_3: {
            AttachmentComboParam* param;
            s32                   lvl;
            s32                   count;
            s32                   time;

            lvl                       = Gp_StateC08.attachId % 10;
            param                     = &Gp_AttachParams[ATTACHMENT_INDEX_ANTIBODY][lvl - 1].combo;
            count                     = Gp_StateC08.antibodyCombo & ATTACHMENT_COMBO_STACK_MASK;
            time                      = param->ticks;
            Gp_StateC08.antibodyCombo = count;
            Gp_StateC08.antibodyTicks = time;
            if (Gp_StateC08.antibodyCombo < ATTACHMENT_COMBO_STACK_CAP) {
                Gp_StateC08.antibodyCombo++;
            }
            Gp_StateC08.antibodyCombo |= lvl << ATTACHMENT_COMBO_LEVEL_SHIFT;
            break;
        }
        case ATTACHMENT_ID_ENERGY_SHOT_1:
        case ATTACHMENT_ID_ENERGY_SHOT_2:
        case ATTACHMENT_ID_ENERGY_SHOT_3: {
            AttachmentComboParam* param;
            s32                   lvl;
            s32                   count;
            s32                   time;

            lvl                         = Gp_StateC08.attachId % 10;
            param                       = &Gp_AttachParams[ATTACHMENT_INDEX_ENERGY_SHOT][lvl - 1].combo;
            count                       = Gp_StateC08.energyShotCombo & ATTACHMENT_COMBO_STACK_MASK;
            time                        = param->ticks;
            Gp_StateC08.energyShotCombo = count;
            Gp_StateC08.energyShotTicks = time;
            if (Gp_StateC08.energyShotCombo < ATTACHMENT_COMBO_STACK_CAP) {
                Gp_StateC08.energyShotCombo++;
            }
            Gp_StateC08.energyShotCombo |= lvl << ATTACHMENT_COMBO_LEVEL_SHIFT;
            break;
        }
        case ATTACHMENT_ID_METABOLISM_1:
        case ATTACHMENT_ID_METABOLISM_2:
        case ATTACHMENT_ID_METABOLISM_3: {
            AttachmentComboParam* param;
            s32                   lvl;
            s32                   count;
            s32                   time;

            lvl                         = Gp_StateC08.attachId % 10;
            param                       = &Gp_AttachParams[ATTACHMENT_INDEX_METABOLISM][lvl - 1].combo;
            count                       = Gp_StateC08.metabolismCombo & ATTACHMENT_COMBO_STACK_MASK;
            time                        = param->ticks;
            Gp_StateC08.metabolismCombo = count;
            Gp_StateC08.metabolismTicks = time;
            if (Gp_StateC08.metabolismCombo == 0) {
                Gp_StateC08.metabolismCombo++;
            }
            Gp_StateC08.metabolismCombo |= lvl << ATTACHMENT_COMBO_LEVEL_SHIFT;
            playerStateSetStatusEffects(1, PLAYER_STATUS_ALL_EFFECTS);
            break;
        }
        case ATTACHMENT_ID_HEALING_1:
        case ATTACHMENT_ID_HEALING_2:
        case ATTACHMENT_ID_HEALING_3: {
            AttachmentLevelRow* params;
            s32                 row;
            s32                 min;
            s32                 max;
            s32                 heal;

            /* Healing, levels 1 to 3. */
            params = Gp_IdParamHi.rows;
            row    = ATTACHMENT_INDEX_HEALING * 3 + 1 + Gp_StateC08.attachId % 3;
            max    = params[row].column.outcome.healMax;
            min    = params[row].column.amount;
            if (min >= max || !isStateF0Active_()) {
                heal = min;
            } else {
                /* A random blend between the row's two amounts. */
                heal = (rand() & 0xFF) + 1;
                heal = (max * heal + min * (0x100 - heal)) >> 8;
                if (heal <= 0) {
                    heal = 1;
                }
            }
            cfg->hp += heal;
            if (cfg->hpMax < cfg->hp) {
                cfg->hp = cfg->hpMax;
            }
            break;
        }
    }
}
