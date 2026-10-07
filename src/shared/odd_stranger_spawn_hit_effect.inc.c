/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Spawns a player attack's hit effect at an offset chosen by its bearing.
///
/// `hitYaw` is the signed hit bearing relative to the enemy's facing, in
/// 4096 units per turn, normally -2048..2048. Front, rear and signed side
/// sectors select an offset; its `pad` selects a live model part. `attackKey`
/// must name a valid player attack row. The effect arguments use part 1,
/// low half 0x300 and repeat count 2; the effect kind interprets the low half.
/// Requires one free scratch `SVECTOR` plus nested spawner capacity. Variant 1
/// keeps the offset in work; variant 2 lends scratch through the spawn call.
static void _oddStrangerSpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey)
{
    SVECTOR*         hitOffset;
    s32              yawMagnitude;
    OddStrangerWork* work;

    hitOffset    = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    yawMagnitude = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work         = task->work;
    if (yawMagnitude < ACTOR_TRANSFORM_ANGLE_TURN / 8) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *hitOffset = gOddStrangerHitOffsets[0];
                break;
            case 1:
                *hitOffset = gOddStrangerHitOffsets[1];
                break;
            case 2:
                *hitOffset = gOddStrangerHitOffsets[2];
                break;
            case 3:
                *hitOffset = gOddStrangerHitOffsets[3];
                break;
            default:
                *hitOffset = gOddStrangerHitOffsets[4];
                break;
        }
    } else if (yawMagnitude > 3 * ACTOR_TRANSFORM_ANGLE_TURN / 8) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *hitOffset = gOddStrangerHitOffsets[5];
                break;
            case 1:
                *hitOffset = gOddStrangerHitOffsets[6];
                break;
            default:
                *hitOffset = gOddStrangerHitOffsets[7];
                break;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = gOddStrangerHitOffsets[8];
        } else {
            *hitOffset = gOddStrangerHitOffsets[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = gOddStrangerHitOffsets[10];
        } else {
            *hitOffset = gOddStrangerHitOffsets[11];
        }
    }
    // The selected vector names both a local offset and the receiving model part.
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
#if ODD_STRANGER_HIT_FX_OFFSET
    work->effectOffset = *hitOffset;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitOffset->pad], &work->effectOffset, &work->effectArg);
#else
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitOffset->pad], hitOffset, &work->effectArg);
#endif
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
