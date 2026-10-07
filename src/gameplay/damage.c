#include "gameplay/damage.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/collision.h"
#include "damage.h"
#include "gameplay/enemy.h"
#include "item_menu.h"
#include "items.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_combat.h"
#include "weapon_data.h"

#include "main/random.h"
#include "main/mc.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

/// Player attack keys select a weapon row or an attachment-level row.
///
/// The distance row occupies bits 8..13. Bit 14 chooses the alternate critical
/// percentage; its other attack behavior is outside this calculation.
enum {
    DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_SHIFT = 8,
    DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_MASK  = 0x3F,
    DAMAGE_PLAYER_ATTACK_ALTERNATE_CRITICAL = 0x4000,
    DAMAGE_LIFE_DRAIN_FIRST_ROW             = 0x19,
    DAMAGE_LIFE_DRAIN_LEVEL_COUNT           = 3,
};

/// Distance classes and table columns of the critical-hit calculation.
enum {
    DAMAGE_DISTANCE_BAND_UNITS                 = 1000,
    DAMAGE_DISTANCE_FARTHEST_CLASS             = 5,
    DAMAGE_CRITICAL_PERCENT_COLUMN             = 6,
    DAMAGE_ALTERNATE_CRITICAL_PERCENT_COLUMN   = 7,
    DAMAGE_ENERGY_SHOT_CRITICAL_PERCENT_COLUMN = 1,
};

/// Scratch-stack block for measuring how far an enemy's body is from the player.
///
/// `offset` and `world` are one point carried through three spaces. It enters
/// as `Enemy::bodyPos`, relative to the enemy's coordinate; that coordinate's
/// world matrix rotates and then translates it into `world`; and `offset`
/// finally takes it relative to the player's world origin, whose length is the
/// distance. Coordinates are signed game units.
///
/// The rotation reads `offset` as a 16-bit `SVECTOR` although it is stored as
/// 32-bit words, so the rotated vector is (low half of X, high half of X, low
/// half of Y) and Z does not take part. That is the game's own behavior.
///
/// Reserve the complete record on the scratch stack; none of its members
/// survive the matching release.
typedef struct {
    VECTOR offset; // Body point relative to the enemy's coordinate, then relative to the player; fourth word unused
    VECTOR world;  // Body point in world axes: rotated, then moved to its world position; fourth word unused
} _DamagePlayerDistanceScratch;
STATIC_ASSERT_SIZEOF(_DamagePlayerDistanceScratch, 0x20);

/// Per-sub-id damage rows used by `Gp_ComputeDamage`. The row is the id's
/// `(id >> 8) & 0x3F` nibble pair; the column is the class picked from
/// `D_80113864` (or 5). The selected entry is scaled `<< 8` then / 100.
extern u16 D_80113568[][8];

/// Column table used for explosions (`WeaponAttackRow::hitReaction` 6), indexed
/// by the distance class picked from `D_80113864`. Scaled `<< 12` then / 100
/// by `damageRollCriticalHit`.
extern u16 D_80113858[];

/// Distance/hit class table for `D_80113568`, indexed by `hits / 1000` (or by
/// `SquareRoot0(distance) / 1000` in `damageRollCriticalHit`) when that value is
/// below 0x10. `Gp_ComputeDamage` only keeps the low byte of the entry.
extern u16 D_80113864[];

static void _damageApplyEnemyReaction(Enemy* enemy, s32 attackKey);

/// Reads the weapon/PE reaction selected by a player attack key.
///
/// The low seven bits must select an existing row: 0..46 for weapon attacks,
/// or 0..54 in `AttachmentLevelTable` with bit 15 set. Category bits are ignored.
/// Returns the stored reaction halfword unchanged.
static inline u16 _damageGetPlayerAttackReaction(s32 attackKey)
{
    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        return Gp_IdParamLo[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction;
    }
    return Gp_IdParamHi.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.outcome.hitReaction;
}

/// Packs one attack entry into a category-4 collision key for its victim.
///
/// `attack` must point to a live entry; it is borrowed and unchanged. The low
/// 12 power bits occupy bits 0..11, and the low 4 reaction bits occupy bits
/// 12..15. The result is in 0x40000..0x4FFFF, including category 4 when power
/// is zero; this helper never returns the zero/no-contact key.
static inline s32 _damagePackAttackEntry(const DamageAttack* attack)
{
    s32 attackKey = attack->power & DAMAGE_ATTACK_POWER_MASK;

    attackKey |= (attack->reaction & DAMAGE_ATTACK_REACTION_MASK) << DAMAGE_ATTACK_REACTION_SHIFT;
    attackKey |= DAMAGE_ATTACK_CATEGORY;
    return attackKey;
}

/// Returns the enemy-to-player distance used by critical-hit chance, in game units.
///
/// Requires a live enemy coordinate, a live player TMD with at least its first
/// coordinate, and a complete, word-aligned writable `scratch` block. The
/// player's cached `workm` must already be current and in the same composition
/// space as the enemy's; ordinary model coordinates compose into view space.
/// Composes the enemy's coordinate chain and overwrites the scratch vectors
/// and GTE rotation/vector/result registers without reserving or releasing storage.
///
/// The staged 32-bit body coordinates are read as signed halfwords (low X,
/// high X, low Y), so body Z does not participate in the Q12 matrix rotation.
/// Translation, player-relative subtraction and squared length retain 32-bit
/// arithmetic before `SquareRoot0`; the sum is neither widened nor clamped.
static inline s32 _damageMeasurePlayerDistance(const Enemy* enemy, const Task* playerTask,
                                               _DamagePlayerDistanceScratch* scratch)
{
    const GfxCoord* playerCoord;

    // Keep the staged word layout: the GTE consumes its first three halfwords.
    actorRenderComposeCoord(enemy->coord);
    scratch->offset.vx = enemy->bodyPos.vx;
    scratch->offset.vy = enemy->bodyPos.vy;
    scratch->offset.vz = enemy->bodyPos.vz;

    gte_ApplyMatrix(&enemy->coord->workm, &scratch->offset, &scratch->world);

    scratch->world.vx = enemy->coord->workm.t[0] + scratch->world.vx;
    scratch->world.vy = enemy->coord->workm.t[1] + scratch->world.vy;
    scratch->world.vz = enemy->coord->workm.t[2] + scratch->world.vz;

    // Subtract the player's origin in the same composed coordinate space.
    playerCoord        = playerTask->extra.tmd->coords;
    scratch->offset.vx = scratch->world.vx - playerCoord->workm.t[0];
    scratch->offset.vy = scratch->world.vy - playerCoord->workm.t[1];
    scratch->offset.vz = scratch->world.vz - playerCoord->workm.t[2];

    return SquareRoot0(scratch->offset.vx * scratch->offset.vx +
                       scratch->offset.vy * scratch->offset.vy + scratch->offset.vz * scratch->offset.vz);
}

u16 D_80113568[47][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 100, 80, 70, 60, 50, 50, 30, 10 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 40 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 60, 60, 50, 40, 30, 30, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 15 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 80, 80, 80, 80, 80, 80, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 20, 5 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 5 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
};
u16 D_80113858[6] = {
    120,
    100,
    80,
    40,
    40,
    40,
};
u16 D_80113864[16] = {
    0,
    1,
    2,
    2,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

u32 Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3)
{
    u8  flag;
    u32 dmg;

    flag = 0;
    if ((arg0 & 0xFFFF0000) != 0x20000) {
        return 0;
    }

    if ((arg0 & 0x8000) == 0) {
        u8  lo;
        u32 base;
        u32 raw;
        u32 rand;
        s32 pct;
        u8  col;
        s32 sel;
        s32 val;
        s32 mult;
        u32 tmp;
        s32 extra;

        if ((arg0 & 0x80) == 0) {
            if ((arg0 & 0x7F) < 0x21) {
                flag = 1;
            }
        }
        lo   = arg0 & 0x7F;
        arg0 = (arg0 >> 8) & 0x3F;
        raw  = Gp_IdParamLo[lo].amount;
        base = raw << 8;
        if (flag != 0) {
            if ((gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) != 0) {
                base = base * 150 / 100;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rand            = gRandomLcgState >> 16;
        pct             = (u16)(rand % 20) + 100;
        base            = base * pct / 100;

        col = arg1 / 1000;
        if (col < 0x10) {
            sel = (u8)D_80113864[col];
        } else {
            sel = 5;
        }
        val = (D_80113568[arg0][sel] << 8) / 100;

        if (arg2 == 0) {
            mult = 0x100;
        } else if (Gp_IdParamLo[lo].hitReaction == arg2) {
            mult = arg3;
        } else {
            mult = 0x100;
        }

        tmp = base * val >> 8;
        dmg = tmp * mult >> 16;

        if (flag != 0) {
            extra = Gp_StateC08.energyShotCombo;
            if (extra != 0) {
                dmg = dmg * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][0] / 100;
            }
            if (equipmentHasEffect(EQUIPMENT_EFFECT_SKULL_CRYSTAL) != 0) {
                dmg = dmg * 120 / 100;
            }
        }

        dmg = dmg * D_80113F90[gSceneCombatState.difficulty] / 100;
        if (dmg == 0) {
            if (base != 0) {
                dmg = 1;
            }
        }
    } else {
        u32 rnd;
        s32 pc;

        dmg = Gp_IdParamHi.rows[arg0 & 0x7F].column.amount;
        if ((arg0 & 0x7F) >= 0x19 && (arg0 & 0x7F) < 0x1C) {
            if (gSceneCombatState.peTargetCount != 0) {
                dmg = dmg / gSceneCombatState.peTargetCount;
            } else {
                dmg = 0;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rnd             = gRandomLcgState >> 16;
        pc              = (u16)(rnd % 10) + 100;
        dmg             = dmg * pc / 100;

        if (equipmentHasEffect(EQUIPMENT_EFFECT_OFUDA) != 0) {
            dmg = dmg * 150 / 100;
        }

        dmg = dmg * D_80113F90[gSceneCombatState.difficulty] / 100;
    }
    return dmg;
}

s32 damageComputeReceived(s32 attackKey, s32 unused, s32* outReaction, s32 victimIsCompanion)
{
    enum { DAMAGE_RECEIVED_SCALE_FRACTION_BITS = 8 };
    u32 damage;
    s32 power;
    u32 scale;
    s32 antibodyCombo;
    s32 hp;
    u16 hpBand;

    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != DAMAGE_ATTACK_CATEGORY) {
        return 0;
    }

    power = attackKey & DAMAGE_ATTACK_POWER_MASK;
    if (outReaction != NULL) {
        *outReaction = ((u32)attackKey >> DAMAGE_ATTACK_REACTION_SHIFT) & DAMAGE_ATTACK_REACTION_MASK;
    }

    if (victimIsCompanion == 0) {
        hp            = gPlayerStatus.hp;
        hpBand        = D_80113F54[hp / 10];
        scale         = Gp_DmgRows[gSceneCombatState.difficulty].playerPercent[hpBand] << DAMAGE_RECEIVED_SCALE_FRACTION_BITS;
        antibodyCombo = Gp_StateC08.antibodyCombo;
        if (antibodyCombo != 0) {
            scale = scale * D_80113CFC[(antibodyCombo / (1 << ATTACHMENT_COMBO_LEVEL_SHIFT) - 1) * 2 + (s8)(antibodyCombo % (1 << ATTACHMENT_COMBO_LEVEL_SHIFT))] / 100;
        }
    } else {
        hp     = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp;
        hpBand = D_80113F54[hp / 10];
        scale  = Gp_DmgRows[gSceneCombatState.difficulty].companionPercent[hpBand] << DAMAGE_RECEIVED_SCALE_FRACTION_BITS;
    }

    // Truncate the Q8 scale before multiplying, but preserve nonzero hit power.
    scale  = scale / 100;
    damage = power * scale >> DAMAGE_RECEIVED_SCALE_FRACTION_BITS;
    if (damage == 0 && power != 0) {
        damage = 1;
    }
    return damage;
}

s32 damageRollCriticalHit(const Enemy* enemy, u32 attackKey, s32 chanceMultiplier)
{
    Task*                         playerTask;
    _DamagePlayerDistanceScratch* scratch;
    s32                           playerDistance;
    u16                           distanceClass;
    s32                           distanceRow;
    s32                           criticalColumn;
    s32                           distanceScale;
    s32                           chance;
    u16                           baseChance;
    s32                           energyShotCombo;
    s32                           roll;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask == NULL) {
        return 0;
    }
    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) != 0) {
        return 0;
    }

    baseChance = (enemy->param->critChance << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    if (baseChance == 0) {
        return 0;
    }

    scratch        = SCRATCH_STACK_RESERVE_BLOCK(_DamagePlayerDistanceScratch);
    playerDistance = _damageMeasurePlayerDistance(enemy, playerTask, scratch);

    distanceClass = playerDistance / DAMAGE_DISTANCE_BAND_UNITS;
    distanceClass = distanceClass < ARRAY_SIZE(D_80113864) ? D_80113864[distanceClass] : DAMAGE_DISTANCE_FARTHEST_CLASS;

    // Distance and the attack's critical column scale the enemy's Q12 base chance.
    distanceRow = (attackKey >> DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_SHIFT) & DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_MASK;
    if (Gp_IdParamLo[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction == DAMAGE_PLAYER_REACTION_EXPLOSION) {
        distanceScale = (D_80113858[distanceClass] << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    } else {
        distanceScale = (D_80113568[distanceRow][distanceClass] << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    }

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ALTERNATE_CRITICAL) != 0) {
        criticalColumn = DAMAGE_ALTERNATE_CRITICAL_PERCENT_COLUMN;
    } else {
        criticalColumn = DAMAGE_CRITICAL_PERCENT_COLUMN;
    }

    chance = (((D_80113568[distanceRow][criticalColumn] << DAMAGE_CHANCE_FRACTION_BITS) / 100) * baseChance >> DAMAGE_CHANCE_FRACTION_BITS) *
                 distanceScale >>
             DAMAGE_CHANCE_FRACTION_BITS;
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
        chance <<= 1;
    }

    energyShotCombo = Gp_StateC08.energyShotCombo;
    if (energyShotCombo != 0) {
        chance = chance * D_80113D0C[(energyShotCombo / (1 << ATTACHMENT_COMBO_LEVEL_SHIFT) - 1) * 2 + (s8)(energyShotCombo % (1 << ATTACHMENT_COMBO_LEVEL_SHIFT))][DAMAGE_ENERGY_SHOT_CRITICAL_PERCENT_COLUMN] / 100;
    }
    if (chanceMultiplier != 0) {
        chance *= chanceMultiplier;
    }

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    roll            = gRandomLcgState >> 16 & DAMAGE_CHANCE_DRAW_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_DamagePlayerDistanceScratch);
    return roll < chance;
}

/// Starts the stagger, buildup or poison reaction of a player attack on an enemy.
///
/// Requires a live parameter record and a key accepted by
/// `_damageGetPlayerAttackReaction`. Buildup restarts immediately; poison rolls
/// the enemy kind's percent chance, then restarts its pulse countdown. Weapon
/// attacks use grade 0; attachment attacks take the active level digit, except
/// the low-six-bit row 49 buildup reaction, which also uses grade 0. Other
/// reactions leave the enemy unchanged.
static void _damageApplyEnemyReaction(Enemy* enemy, s32 attackKey)
{
    s32 poisonPercent;
    s32 poisonChance;
    s32 roll;

    switch (_damageGetPlayerAttackReaction(attackKey)) {
        case DAMAGE_PLAYER_REACTION_NONE:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            enemy->reactionFlags |= ENEMY_REACTION_STAGGER;
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            enemy->buildupStep    = 0;
            enemy->buildupTimer   = 0;
            enemy->reactionFlags |= ENEMY_REACTION_BUILDUP;
            if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
                enemy->buildupGrade = 0;
                return;
            }
            if ((attackKey & DAMAGE_BUILDUP_UNGRADED_ROW_MASK) == DAMAGE_BUILDUP_UNGRADED_ROW) {
                enemy->buildupGrade = 0;
                return;
            }
            enemy->buildupGrade = Gp_StateC08.attachId % 10U;
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            poisonPercent   = enemy->param->damageOverTimeChance;
            poisonChance    = (poisonPercent << DAMAGE_CHANCE_FRACTION_BITS) / 100;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            roll            = gRandomLcgState >> 16 & DAMAGE_CHANCE_DRAW_MASK;
            if (roll < poisonChance) {
                enemy->damageOverTimePulse = 0;
                enemy->reactionFlags      |= ENEMY_REACTION_DAMAGE_OVER_TIME;
                gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                enemy->damageOverTimeDelay = (gRandomLcgState >> 16 & ENEMY_DAMAGE_OVER_TIME_DELAY_JITTER) + ENEMY_DAMAGE_OVER_TIME_DELAY_BASE;
                if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
                    enemy->damageOverTimeGrade = 0;
                    return;
                }
                enemy->damageOverTimeGrade = Gp_StateC08.attachId % 10U;
            }
            break;
    }
}

s32 damagePackEnemyAttackKey(const Enemy* enemy, s32 attackIndex)
{
    if (enemy->param == NULL) {
        return 0;
    }
    return _damagePackAttackEntry(&enemy->param->attacks[attackIndex]);
}

s32 damagePackAttackKey(const DamageAttack* attacks, s32 attackIndex)
{
    if (attacks == NULL) {
        return 0;
    }
    return _damagePackAttackEntry(&attacks[attackIndex]);
}

void damageAccumulateLifeDrainHp(const Enemy* enemy, s32 attackKey, s32 damage, s32 unused)
{
    s32 remainingHp;

    if ((u32)((attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK) - DAMAGE_LIFE_DRAIN_FIRST_ROW) < DAMAGE_LIFE_DRAIN_LEVEL_COUNT) {
        remainingHp = enemy->hp;
        if ((u32)remainingHp < (u32)damage) {
            gSceneCombatState.lifeDrainHp += remainingHp;
            return;
        }
        gSceneCombatState.lifeDrainHp += damage;
    }
}
