/* Part of the Moth library; see moth.h. */

/// Applies resolved grid correction or restores the position before an opposed move.
///
/// Borrows the root, work and the caller's reserved delta; `scratch` must equal
/// `scratchHead - 1`. Clears all four grid contacts after consuming them.
/// Correction uses signed integer halves of Q16.16; the opposed result restores
/// the whole-unit pre-move position. Neither delta pointer is retained.
static __inline__ void _mothApplyGridCorrection(GfxCoord* rootCoord, MothWork* work,
                                                const WorldCollisionDelta* scratchHead,
                                                const WorldCollisionDelta* scratch, s32 pushbackResult)
{
    s32 correctedZ;

    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratchHead[-1].fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->fixed.vy.halves.integer;
            correctedZ             = rootCoord->coord.t[2] + scratch->fixed.vz.halves.integer;
            rootCoord->coord.t[2]  = correctedZ;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
}

/// Applies room-grid pushback and starts the moth's death on player contact or attack.
///
/// Requires initialized work, a live root and enemy spawn argument. Reads all
/// four grid contacts: integer halves of Q16.16 correction move the root, while
/// opposed normals restore the position saved before the preceding move. A
/// player-body contact kills outright; an attack also rolls range-based damage,
/// awards its readout/life drain (zero damage becomes one) and spawns the hit effect.
/// Attack key low-byte bit 7 selects a live player/companion task (0/1); both
/// roots must share a parent frame. Clears both tables and releases one delta.
static void _mothContacts(Task* task)
{
    enum { MOTH_HIT_ACTOR_SLOT_SHIFT = 7 };

    MothWork*            work;
    GfxCoord*            rootCoord;
    s32                  pushbackResult;
    s32                  playerOffsetX;
    s32                  playerOffsetY;
    s32                  playerOffsetZ;
    s32                  attackDamage;
    u16                  contactKind;
    GfxCoord*            attackerRoot;
    WorldCollisionDelta* scratchHead;
    WorldCollisionDelta* scratch;

    work        = task->work;
    scratchHead = SCRATCH_STACK_CURSOR(WorldCollisionDelta);
    scratch     = (SCRATCH_STACK_CURSOR(WorldCollisionDelta) = scratchHead - 1);
    rootCoord   = task->extra.tmd->coords;
    // Consume the previous collision pass before the next flight step.
    pushbackResult = worldCollisionResolvePushback(work->gridContacts, scratch, ARRAY_SIZE(work->gridContacts), 0);
    _mothApplyGridCorrection(rootCoord, work, scratchHead, scratch, pushbackResult);
    contactKind = work->hitContacts[0].key.parts.kind;
    switch ((u32)contactKind) {
        case 0:
            break;
        case WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16:
            task->state                           = MOTH_TASK_DEATH;
            ((Enemy*)task->spawnArg2.pointer)->hp = 0;
            sceneEngageBattle(1);
            break;
        case WORLD_COLLISION_CONTACT_ATTACK >> 16:
            task->state        = contactKind;
            attackerRoot       = gPlayerActorTasks[(u8)work->hitContacts[0].key.parts.id >> MOTH_HIT_ACTOR_SLOT_SHIFT]->extra.tmd->coords;
            playerOffsetX      = attackerRoot->coord.t[0] - rootCoord->coord.t[0];
            scratch->vector.vx = playerOffsetX;
            playerOffsetY      = attackerRoot->coord.t[1] - rootCoord->coord.t[1];
            scratch->vector.vy = playerOffsetY;
            playerOffsetZ      = attackerRoot->coord.t[2] - rootCoord->coord.t[2];
            scratch->vector.vz = playerOffsetZ;
            attackDamage       = damageComputePlayerAttack(work->hitContacts[0].key.value,
                                                           SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetY * playerOffsetY) + (playerOffsetZ * playerOffsetZ)), 0, 0);
            if (attackDamage == 0) {
                attackDamage = 1;
            }
            worldTargetAddReadoutAmount(&((Enemy*)task->spawnArg2.pointer)->node, attackDamage, 0);
            damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->hitContacts[0].key.value, attackDamage, 0);
            ((Enemy*)task->spawnArg2.pointer)->hp = 0;
            effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[0].key.value), task->extra.tmd->coords, 0,
                           &work->hitEffectArg);
            break;
    }
    worldCollisionClearContacts(work->hitContacts);
    SCRATCH_STACK_RELEASE_BLOCK(WorldCollisionDelta);
}
