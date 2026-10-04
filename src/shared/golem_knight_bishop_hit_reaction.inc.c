/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Picks the reaction to the hit just taken, from the enemy's HP and the
/// damage `arg1`: at or below zero HP it stops `appearSound` and
/// `vanishSound` and enters a death sequence (the collapse, or the kneel
/// death while `downedPose` is set); below a tenth of `hpMax` the kneel (or
/// the kneel hit while `downedPose` is set); otherwise, when `reactionLock`
/// is clear or `flickerStage` set, the light flinch for damage below 0x50 and
/// the heavy one above. A sequence change restarts `step` and switches
/// `strikeBody` off; the two `downedPose` variants are skipped while
/// `reactionLock` holds the running sequence.
void golemKnightBishopPickHitReaction(Task* arg0, s32 arg1)
{
    Enemy*                 enemy = arg0->spawnArg2.pointer;
    s16                    hp    = enemy->hp;
    GolemKnightBishopWork* work  = arg0->work;
    u32                    state = 0;
    s32                    max;

    if (hp <= 0) {
        state = 6;
        if (work->downedPose == 0) {
            state = 5;
        }
        if (work->appearSound != 0) {
            SndEvt_EnqueueType7(work->appearSound, 1);
            work->appearSound = 0;
        }
        if (work->vanishSound != 0) {
            SndEvt_EnqueueType7(work->vanishSound, 1);
            work->vanishSound = 0;
        }
    } else if (max = enemy->param->hpMax, hp < max / 10) {
        state = 4;
        if (work->downedPose == 0) {
            state = 3;
        }
    } else if (work->reactionLock == 0 || work->flickerStage != 0) {
        work->reactionLock = 0;
        state              = 2;
        if (arg1 < 0x50) {
            state = 1;
        }
    }

    switch (state) {
        case 0:
            break;
        case 1:
            work->sequence          = GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH;
            work->step              = 0;
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 2:
            work->sequence          = GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH;
            work->step              = 0;
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 3:
            work->sequence          = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL;
            work->step              = 0;
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 4:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT;
                work->step     = 0;
            }
            break;
        case 5:
            work->sequence          = GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH;
            work->step              = 0;
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 6:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH;
                work->step     = 0;
            }
            break;
    }
}
