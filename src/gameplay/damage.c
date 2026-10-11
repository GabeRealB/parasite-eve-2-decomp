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
    DAMAGE_DISTANCE_BAND_UNITS               = 1000,
    DAMAGE_DISTANCE_FARTHEST_CLASS           = 5,
    DAMAGE_CRITICAL_PERCENT_COLUMN           = 6,
    DAMAGE_ALTERNATE_CRITICAL_PERCENT_COLUMN = 7,
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

static void _damageApplyEnemyReaction(Enemy* enemy, s32 attackKey);

/// Reads the weapon/PE reaction selected by a player attack key.
///
/// The low seven bits must select an existing row: 0..46 for weapon attacks,
/// or 0..54 in `AttachmentLevelTable` with bit 15 set. Category bits are ignored.
/// Returns the stored reaction halfword unchanged.
static inline u16 _damageGetPlayerAttackReaction(s32 attackKey)
{
    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        return gWeaponAttackRows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction;
    }
    return gAttachmentLevelTable.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.outcome.hitReaction;
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

/// Attack-key distance and critical-hit percentages.
///
/// Rows 0..46 use key bits 8..13, independently of the low-seven-bit power row.
/// Columns 0..5 scale weapon damage and non-explosion critical chance by distance
/// class; columns 6/7 scale ordinary/bit-14 critical chance. Values are whole
/// percentages, converted to Q8 for damage or Q12 for chance. Row 0 is empty;
/// rows 33..46 include enemy attacks that can damage other enemies.
static u16 _gDamageAttackDistanceCriticalPercent[47][8] = {
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
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Actor 01200 burst; mine-cavern target blast.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Actor 04000 burst.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Sucklerceph blast.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Maggot/Caterpillar flame.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Attack producer unproven.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Watcher beam variant 0.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Watcher beam variant 1.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Watcher beam variant 2.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Watcher beam variant 3.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Watcher gun.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // GOLEM grenade.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Attack producer unproven.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Amoeba fireball variant 0.
    { 100, 100, 100, 100, 100, 100, 0, 0 }, // Amoeba fireball variant 1.
};
/// Explosion critical-chance percentages for distance classes 0..5.
///
/// Replaces the attack row's distance percentage for reaction EXPLOSION only;
/// the ordinary or alternate critical percentage still comes from that row.
static u16 _gDamageExplosionCriticalDistancePercent[6] = {
    120,
    100,
    80,
    40,
    40,
    40,
};
/// Maps 1000-world-unit distance bands to attack distance classes.
///
/// Classes 0/1/2/3/4 cover 0..999, 1000..1999, 2000..3999, 4000..7999 and
/// 8000..15999 units; later bands use class 5 in the consumers. Damage narrows
/// the band and selected class to u8; critical chance narrows the band to u16.
/// These unsigned halfwords preserve both consumers' distinct access widths.
static u16 _gDamageDistanceBandClasses[16] = {
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

u32 damageComputePlayerAttack(u32 attackKey, u32 playerDistance, s32 scaledReaction, s32 reactionScale)
{
    enum {
        DAMAGE_PLAYER_ATTACK_CATEGORY            = 0x20000,
        DAMAGE_PLAYER_ATTACK_COMPANION           = 0x80,
        DAMAGE_PLAYER_WEAPON_MODIFIER_ROW_LIMIT  = 33,
        DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS = 8,
        DAMAGE_PLAYER_ATTACK_SCALE_ONE           = 1 << DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS,
        DAMAGE_WEAPON_DAMAGE_JITTER_RANGE        = 20,
        DAMAGE_PE_DAMAGE_JITTER_RANGE            = 10,
        DAMAGE_BERSERKER_PERCENT                 = 150,
        DAMAGE_SKULL_CRYSTAL_PERCENT             = 120,
        DAMAGE_OFUDA_PERCENT                     = 150,
    };
    u8  applyPlayerWeaponModifiers;
    u32 damage;

    applyPlayerWeaponModifiers = 0;
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != DAMAGE_PLAYER_ATTACK_CATEGORY) {
        return 0;
    }

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        u8  weaponRow;
        u32 baseDamageQ8;
        u32 weaponRoll;
        s32 weaponPercent;
        u8  distanceBand;
        s32 distanceClass;
        s32 distanceScaleQ8;
        s32 reactionScaleQ8;
        u32 distanceDamageQ8;
        s32 energyShotCombo;

        if ((attackKey & DAMAGE_PLAYER_ATTACK_COMPANION) == 0) {
            if ((attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK) < DAMAGE_PLAYER_WEAPON_MODIFIER_ROW_LIMIT) {
                applyPlayerWeaponModifiers = 1;
            }
        }
        // After saving the weapon row, only the distance-scale row remains needed.
        weaponRow    = attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK;
        attackKey    = (attackKey >> DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_SHIFT) & DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_MASK;
        baseDamageQ8 = gWeaponAttackRows[weaponRow].amount << DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS;
        if (applyPlayerWeaponModifiers != 0) {
            if ((gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) != 0) {
                baseDamageQ8 = baseDamageQ8 * DAMAGE_BERSERKER_PERCENT / 100;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        weaponRoll      = gRandomLcgState >> 16;
        weaponPercent   = (u16)(weaponRoll % DAMAGE_WEAPON_DAMAGE_JITTER_RANGE) + 100;
        baseDamageQ8    = baseDamageQ8 * weaponPercent / 100;

        // Retain byte narrowing of both the distance bucket and its mapped class.
        distanceBand = playerDistance / DAMAGE_DISTANCE_BAND_UNITS;
        if (distanceBand < ARRAY_SIZE(_gDamageDistanceBandClasses)) {
            distanceClass = (u8)_gDamageDistanceBandClasses[distanceBand];
        } else {
            distanceClass = DAMAGE_DISTANCE_FARTHEST_CLASS;
        }
        distanceScaleQ8 = (_gDamageAttackDistanceCriticalPercent[attackKey][distanceClass] << DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS) / 100;

        if (scaledReaction == 0) {
            reactionScaleQ8 = DAMAGE_PLAYER_ATTACK_SCALE_ONE;
        } else if (gWeaponAttackRows[weaponRow].hitReaction == scaledReaction) {
            reactionScaleQ8 = reactionScale;
        } else {
            reactionScaleQ8 = DAMAGE_PLAYER_ATTACK_SCALE_ONE;
        }

        // Keep both unsigned products and their separate fixed-point truncation points.
        distanceDamageQ8 = baseDamageQ8 * distanceScaleQ8 >> DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS;
        damage           = distanceDamageQ8 * reactionScaleQ8 >> (2 * DAMAGE_PLAYER_ATTACK_SCALE_FRACTION_BITS);

        if (applyPlayerWeaponModifiers != 0) {
            energyShotCombo = gAttachmentState.energyShotCombo;
            if (energyShotCombo != 0) {
                damage = damage * gAttachmentEnergyShotComboScales[(energyShotCombo / (1 << ATTACHMENT_COMBO_LEVEL_SHIFT) - 1) * 2 + (s8)(energyShotCombo % (1 << ATTACHMENT_COMBO_LEVEL_SHIFT))].damagePercent / 100;
            }
            if (equipmentHasEffect(EQUIPMENT_EFFECT_SKULL_CRYSTAL) != 0) {
                damage = damage * DAMAGE_SKULL_CRYSTAL_PERCENT / 100;
            }
        }

        damage = damage * gDamageAttackDifficultyPercent[gSceneCombatState.difficulty] / 100;
        if (damage == 0) {
            if (baseDamageQ8 != 0) {
                damage = 1;
            }
        }
    } else {
        u32 peRoll;
        s32 pePercent;

        // Life Drain shares one base amount among the cast's recorded targets.
        damage = gAttachmentLevelTable.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.amount;
        if ((attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK) >= DAMAGE_LIFE_DRAIN_FIRST_ROW &&
            (attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK) < DAMAGE_LIFE_DRAIN_FIRST_ROW + DAMAGE_LIFE_DRAIN_LEVEL_COUNT) {
            if (gSceneCombatState.peTargetCount != 0) {
                damage = damage / gSceneCombatState.peTargetCount;
            } else {
                damage = 0;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        peRoll          = gRandomLcgState >> 16;
        pePercent       = (u16)(peRoll % DAMAGE_PE_DAMAGE_JITTER_RANGE) + 100;
        damage          = damage * pePercent / 100;

        if (equipmentHasEffect(EQUIPMENT_EFFECT_OFUDA) != 0) {
            damage = damage * DAMAGE_OFUDA_PERCENT / 100;
        }

        damage = damage * gDamageAttackDifficultyPercent[gSceneCombatState.difficulty] / 100;
    }
    return damage;
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
        antibodyCombo = gAttachmentState.antibodyCombo;
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
    distanceClass = distanceClass < ARRAY_SIZE(_gDamageDistanceBandClasses) ? _gDamageDistanceBandClasses[distanceClass] : DAMAGE_DISTANCE_FARTHEST_CLASS;

    // Distance and the attack's critical column scale the enemy's Q12 base chance.
    distanceRow = (attackKey >> DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_SHIFT) & DAMAGE_PLAYER_ATTACK_DISTANCE_ROW_MASK;
    if (gWeaponAttackRows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction == DAMAGE_PLAYER_REACTION_EXPLOSION) {
        distanceScale = (_gDamageExplosionCriticalDistancePercent[distanceClass] << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    } else {
        distanceScale = (_gDamageAttackDistanceCriticalPercent[distanceRow][distanceClass] << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    }

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ALTERNATE_CRITICAL) != 0) {
        criticalColumn = DAMAGE_ALTERNATE_CRITICAL_PERCENT_COLUMN;
    } else {
        criticalColumn = DAMAGE_CRITICAL_PERCENT_COLUMN;
    }

    chance = (((_gDamageAttackDistanceCriticalPercent[distanceRow][criticalColumn] << DAMAGE_CHANCE_FRACTION_BITS) / 100) * baseChance >> DAMAGE_CHANCE_FRACTION_BITS) *
                 distanceScale >>
             DAMAGE_CHANCE_FRACTION_BITS;
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
        chance <<= 1;
    }

    energyShotCombo = gAttachmentState.energyShotCombo;
    if (energyShotCombo != 0) {
        chance = chance * gAttachmentEnergyShotComboScales[(energyShotCombo / (1 << ATTACHMENT_COMBO_LEVEL_SHIFT) - 1) * 2 + (s8)(energyShotCombo % (1 << ATTACHMENT_COMBO_LEVEL_SHIFT))].criticalPercent / 100;
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
            enemy->buildupGrade = gAttachmentState.attachId % 10U;
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
                enemy->damageOverTimeGrade = gAttachmentState.attachId % 10U;
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
