/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Picks the damage reaction for the hit just taken, from the enemy's HP and
/// the damage `arg1`: at or below zero HP it silences both queued sound events
/// and enters the death sequence (9, or 10 while `field_6F0` is set); below a
/// tenth of `hpMax` the low-HP sequence (7, or 8 while `field_6F0` is set);
/// otherwise, when `field_6F2` is clear or `field_6EC` set, the flinch sequence
/// 5 for damage below 0x50 and 6 above. A sequence change restarts its state
/// and clears bit 0x8000 of `field_582`; the `field_6F0` variants leave a
/// sequence already held by `field_6F2` running.
void golemKnightBishopPickHitReaction(Task* arg0, s32 arg1)
{
    Enemy*                 enemy = arg0->spawnArg2.pointer;
    s16                    hp    = enemy->hp;
    GolemKnightBishopWork* work  = arg0->work;
    u32                    state = 0;
    s32                    max;

    if (hp <= 0) {
        state = 6;
        if (work->field_6F0 == 0) {
            state = 5;
        }
        if (work->field_6B8 != 0) {
            SndEvt_EnqueueType7(work->field_6B8, 1);
            work->field_6B8 = 0;
        }
        if (work->field_6BC != 0) {
            SndEvt_EnqueueType7(work->field_6BC, 1);
            work->field_6BC = 0;
        }
    } else if (max = enemy->param->hpMax, hp < max / 10) {
        state = 4;
        if (work->field_6F0 == 0) {
            state = 3;
        }
    } else if (work->field_6F2 == 0 || work->field_6EC != 0) {
        work->field_6F2 = 0;
        state           = 2;
        if (arg1 < 0x50) {
            state = 1;
        }
    }

    switch (state) {
        case 0:
            break;
        case 1:
            work->field_6CC  = 5;
            work->field_6CE  = 0;
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 2:
            work->field_6CC  = 6;
            work->field_6CE  = 0;
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 3:
            work->field_6CC  = 7;
            work->field_6CE  = 0;
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 4:
            if (work->field_6F2 == 0) {
                work->field_6CC = 8;
                work->field_6CE = 0;
            }
            break;
        case 5:
            work->field_6CC  = 9;
            work->field_6CE  = 0;
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 6:
            if (work->field_6F2 == 0) {
                work->field_6CC = 10;
                work->field_6CE = 0;
            }
            break;
    }
}
