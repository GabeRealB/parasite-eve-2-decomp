#include "gameplay/room_effects.h"

/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block of the per-frame hit handler.
///
/// Reserved for the length of the call. Only the two vectors are used; what
/// the rest of the block was laid out for is unproven.
typedef struct {
    WorldCollisionDelta delta;          // push-back resolved from a contact table, in its fixed-point view; then, as a whole-unit vector, the player's position less the golem's root, for a weapon hit
    byte                field_10[0x10]; // never accessed; role unproven
    SVECTOR             effectOffset;   // where the hit effect appears, as an offset from part 3 along that part's axes
    byte                field_28[8];    // never accessed; role unproven
} _GolemKnightBishopHitScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopHitScratch, 0x30);

/// Resolves contacts, applies attack damage and updates the GOLEM's hit response.
///
/// `task` owns a live GOLEM model/work block and Enemy spawn argument. Contact
/// arrays cover their full declared extent and are cleared after consumption.
/// Grid push-back uses the integer halves of Q16 deltas, restoring the previous
/// root position when opposed; attacks measure full XYZ distance in world units.
/// Feints only break, cooldown blocks further attacks, and an active grab records
/// a weapon break instead of immediately changing sequence. Hit effects suppress
/// consecutive repeated keys within this pass. Reserves 48 scratch bytes.
static void _golemKnightBishopTakeHits(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE = 5,
        GOLEM_KNIGHT_BISHOP_ATTACK_INTERRUPT     = 9,
        GOLEM_KNIGHT_BISHOP_GRAB_HOLDING         = 1,
        GOLEM_KNIGHT_BISHOP_GRAB_BREAK_WEAPON    = 2,
        GOLEM_KNIGHT_BISHOP_HIT_EFFECT_BEHIND    = -150,
        GOLEM_KNIGHT_BISHOP_HIT_EFFECT_FRONT     = 200,
    };
    s32                           lastEffectKey;
    GolemKnightBishopWork*        work;
    _GolemKnightBishopHitScratch* previousTop;
    _GolemKnightBishopHitScratch* scratch;
    _GolemKnightBishopHitScratch* reservedBlock;
    Enemy*                        enemy;
    GfxCoord*                     root;
    s32                           contactIndex;
    s32                           damage;
    s32                           attackAttribute;
    s32                           cooldownFrames;
    s16                           cooldownRemaining;
    s16                           effectForwardOffset;

    lastEffectKey = 0;
    work          = task->work;
    // Keep the cursor, reservation and working views distinct for the matched address calculations.
    previousTop                                        = SCRATCH_STACK_CURSOR(_GolemKnightBishopHitScratch);
    reservedBlock                                      = previousTop - 1;
    SCRATCH_STACK_CURSOR(_GolemKnightBishopHitScratch) = reservedBlock;
    scratch                                            = reservedBlock;
    root                                               = task->extra.tmd->coords;
    enemy                                              = task->spawnArg2.pointer;

    // Resolve floor motion before horizontal hurt-body obstruction.
    switch (worldCollisionResolvePushback(work->groundContacts, &scratch->delta, ARRAY_SIZE(work->groundContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            root->coord.t[0] += previousTop[-1].delta.fixed.vx.halves.integer;
            root->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            root->coord.t[0] = work->prevRootPos.vx;
            root->coord.t[1] = work->prevRootPos.vy;
            root->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);

    if (work->hurtBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (worldCollisionResolvePushback(work->hurtContacts, &scratch->delta, ARRAY_SIZE(work->hurtContacts), NULL)) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                root->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                root->coord.t[0] = work->prevRootPos.vx;
                root->coord.t[2] = work->prevRootPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        cooldownRemaining = work->hitCooldown - 1;
        work->hitCooldown = cooldownRemaining;
        if (cooldownRemaining <= 0) {
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->hitCooldown     = 0;
            work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
        }
    }

    /// Places the impact effect on part 3, on the side facing the hit.
    ///
    /// Local to this handler. Captures task, work, scratch, contactIndex and
    /// effectForwardOffset, all live locals of this handler. Use only inside a
    /// braced statement block: it expands to multiple statements. It writes the
    /// three effect-offset components and the signed-halfword temporary, then
    /// performs one effect request. The contact key expression is read once.
#define GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT()                                                \
    scratch->effectOffset.vx = 0;                                                             \
    scratch->effectOffset.vy = 0;                                                             \
    effectForwardOffset      = GOLEM_KNIGHT_BISHOP_HIT_EFFECT_BEHIND;                         \
    if (work->hitFromFront == 1) {                                                            \
        effectForwardOffset = GOLEM_KNIGHT_BISHOP_HIT_EFFECT_FRONT;                           \
    }                                                                                         \
    scratch->effectOffset.vz = effectForwardOffset;                                           \
    effectSpawnHit(damageGetPlayerAttackEffectId(work->hurtContacts[contactIndex].key.value), \
                   &task->extra.tmd->coords[3], &scratch->effectOffset,                       \
                   &work->hitEffectArg)

    // Process attack keys in table order; the shared cooldown can suppress later hits.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hurtContacts); contactIndex++) {
        switch ((u32)work->hurtContacts[contactIndex].key.value >> 16) {
            case 0:
                break;
            case (WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16):
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                }
                break;
            case (WORLD_COLLISION_CONTACT_ATTACK >> 16):
                if (work->hitCooldown != 0) {
                    break;
                }
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                    break;
                }
                scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
                scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
                scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
                // The complement and logical shift yield one for a nonnegative facing dot product.
                work->hitFromFront = (u32) ~(scratch->delta.vector.vx * root->coord.m[0][2] +
                                             scratch->delta.vector.vy * root->coord.m[1][2] +
                                             scratch->delta.vector.vz * root->coord.m[2][2]) >>
                                     31;
                damage          = damageComputePlayerAttack(work->hurtContacts[contactIndex].key.value,
                                                            SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx +
                                                                        scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                                        scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                            0, 0);
                attackAttribute = damageGetPlayerAttackReaction(work->hurtContacts[contactIndex].key.value);
                if ((u16)attackAttribute == GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE) {
                    damage *= 2;
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 2, NULL);
                }
                if (damageRollCriticalHit(enemy, work->hurtContacts[contactIndex].key.value, 0) != 0) {
                    damage *= 4;
                    if ((u16)attackAttribute != GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE) {
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 0, NULL);
                    }
                }
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                damageAccumulateLifeDrainHp(enemy, work->hurtContacts[contactIndex].key.value, damage, 0);
                enemy->hp             -= damage;
                work->interruptDamage += damage;
                switch ((u16)attackAttribute) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_POISON:
                    case 4:
                    case GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->flickerStage == 0) {
                            work->flickerStage = 1;
                            work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_ATTACK_INTERRUPT:
                        work->interruptDamage += GOLEM_KNIGHT_BISHOP_HIT_WEIGHT;
                        break;
                }
                if (lastEffectKey != work->hurtContacts[contactIndex].key.value) {
                    lastEffectKey = work->hurtContacts[contactIndex].key.value;
                    GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT();
                }
                cooldownFrames = damageGetPlayerAttackHitCooldown(work->hurtContacts[contactIndex].key.value);
                if (cooldownFrames > 0) {
                    work->hitCooldown = cooldownFrames;
                }
                if (work->fadeState >= GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM) {
                    work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_WHITE;
                }
                if (work->grabStage != GOLEM_KNIGHT_BISHOP_GRAB_HOLDING) {
                    _golemKnightBishopPickHitReaction(task, damage);
                } else {
                    work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_WEAPON;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->hurtContacts);
    if (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->strikeContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopHitScratch);
}

#undef GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT
