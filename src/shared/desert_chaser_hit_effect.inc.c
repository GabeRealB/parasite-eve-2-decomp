/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Spawns the player's attack effect at an attachment chosen from the relative hit yaw.
///
/// hitYaw is a signed bearing in 4096 units per turn, normally [-0x800, 0x800];
/// hitKey is the damaging contact's packed attack key. The twelve carrier
/// offsets store a part index in pad. The chosen offset and spawn record live
/// in the armed work block after the temporary scratch vector is released.
static void _desertChaserHitEffect(Task* task, s16 hitYaw, s32 hitKey)
{
    // Signed spawn-argument halves retained by the hit-effect dispatcher.
    enum {
        DESERT_CHASER_HIT_EFFECT_ARGUMENT_LOW  = 0x100,
        DESERT_CHASER_HIT_EFFECT_ARGUMENT_HIGH = 2
    };
    SVECTOR*          hitOffset;
    s32               yawMagnitude;
    DesertChaserWork* work;

    hitOffset    = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    yawMagnitude = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work         = task->work;
    // Front, rear and side bearings choose different attachment groups.
    if (yawMagnitude < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *hitOffset = gDesertChaserHitOffsets[0];
                break;
            case 1:
                *hitOffset = gDesertChaserHitOffsets[1];
                break;
            case 2:
                *hitOffset = gDesertChaserHitOffsets[2];
                break;
            case 3:
                *hitOffset = gDesertChaserHitOffsets[3];
                break;
            default:
                *hitOffset = gDesertChaserHitOffsets[4];
                break;
        }
    } else if (yawMagnitude > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        // The retained mask selects only 0 or 2; the case-1 offset is unreachable.
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *hitOffset = gDesertChaserHitOffsets[5];
                break;
            case 1:
                *hitOffset = gDesertChaserHitOffsets[6];
                break;
            default:
                *hitOffset = gDesertChaserHitOffsets[7];
                break;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = gDesertChaserHitOffsets[8];
        } else {
            *hitOffset = gDesertChaserHitOffsets[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = gDesertChaserHitOffsets[10];
        } else {
            *hitOffset = gDesertChaserHitOffsets[11];
        }
    }
    work->effectArg.coord      = &task->extra.tmd->coords[hitOffset->pad];
    work->effectArg.spawnArgLo = DESERT_CHASER_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = DESERT_CHASER_HIT_EFFECT_ARGUMENT_HIGH;
    work->hitOffset            = *hitOffset;
    effectSpawnHit(damageGetPlayerAttackEffectId(hitKey), &task->extra.tmd->coords[hitOffset->pad], &work->hitOffset, &work->effectArg);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
