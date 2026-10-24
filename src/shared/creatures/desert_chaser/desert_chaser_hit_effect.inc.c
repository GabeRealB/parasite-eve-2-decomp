/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Spawns the player's attack effect at an attachment chosen from the relative hit yaw.
///
/// hitYaw is a signed bearing in 4096 units per turn, normally [-0x800, 0x800];
/// hitKey is the damaging contact's packed attack key. The twelve carrier
/// anchors pair part-local positions with model coordinate selectors. The chosen
/// anchor and spawn record remain in the armed work after scratch is released.
static void _desertChaserHitEffect(Task* task, s16 hitYaw, s32 hitKey)
{
    // Signed spawn-argument halves retained by the hit-effect dispatcher.
    enum {
        DESERT_CHASER_HIT_EFFECT_ARGUMENT_LOW  = 0x100,
        DESERT_CHASER_HIT_EFFECT_ARGUMENT_HIGH = 2
    };
    ActorHitAnchor*   hitAnchor;
    s32               yawMagnitude;
    DesertChaserWork* work;

    hitAnchor    = SCRATCH_STACK_RESERVE_BLOCK(ActorHitAnchor);
    yawMagnitude = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work         = task->work;
    // Front, rear and side bearings choose different attachment groups.
    if (yawMagnitude < 0x200) {
        RANDOM_LCG_ADVANCE();
        switch ((s32)RANDOM_LCG_DRAW() & 3) {
            case 0:
                *hitAnchor = _gDesertChaserHitAnchors[0];
                break;
            case 1:
                *hitAnchor = _gDesertChaserHitAnchors[1];
                break;
            case 2:
                *hitAnchor = _gDesertChaserHitAnchors[2];
                break;
            case 3:
                *hitAnchor = _gDesertChaserHitAnchors[3];
                break;
            default:
                *hitAnchor = _gDesertChaserHitAnchors[4];
                break;
        }
    } else if (yawMagnitude > 0x600) {
        RANDOM_LCG_ADVANCE();
        // The retained mask selects only 0 or 2; the case-1 offset is unreachable.
        switch ((s32)RANDOM_LCG_DRAW() & 2) {
            case 0:
                *hitAnchor = _gDesertChaserHitAnchors[5];
                break;
            case 1:
                *hitAnchor = _gDesertChaserHitAnchors[6];
                break;
            default:
                *hitAnchor = _gDesertChaserHitAnchors[7];
                break;
        }
    } else if (hitYaw > 0) {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            *hitAnchor = _gDesertChaserHitAnchors[8];
        } else {
            *hitAnchor = _gDesertChaserHitAnchors[9];
        }
    } else {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            *hitAnchor = _gDesertChaserHitAnchors[10];
        } else {
            *hitAnchor = _gDesertChaserHitAnchors[11];
        }
    }
    work->effectArg.coord      = &task->extra.tmd->coords[hitAnchor->anchor.partIndex];
    work->effectArg.spawnArgLo = DESERT_CHASER_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = DESERT_CHASER_HIT_EFFECT_ARGUMENT_HIGH;
    work->hitAnchor            = *hitAnchor;
    effectSpawnHit(damageGetPlayerAttackEffectId(hitKey), &task->extra.tmd->coords[hitAnchor->anchor.partIndex], &work->hitAnchor.position, &work->effectArg);
    SCRATCH_STACK_RELEASE_BLOCK(ActorHitAnchor);
}
