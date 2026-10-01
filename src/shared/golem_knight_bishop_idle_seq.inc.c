/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// State machine on `field_6CE`: 0 rolls a `field_6D4` wait, 1 counts it
/// down, 2 picks state 3 or 4 from `field_70E` and an LCG draw offset by
/// `field_710` (or 5 when `golemKnightBishopPlayerInBox` reports a box hit), and 3-5
/// settle the result, walking `field_70C` up to 12.
void golemKnightBishopIdleSeq(Task* arg0)
{
    GolemKnightBishopWork* work;

    work = arg0->work;
    switch (work->field_6CE) {
        case 0:
            work->field_6C8 = 0;
            work->field_6EC = 0;
            work->field_6EE = 0;
            if (work->field_6E8 != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6CE = gGolemKnightBishopIdleSteps[(gRandomLcgState >> 16) & 0xF] + 2;
                work->field_6EE = 1;
                golemKnightBishopPlaceTarget(arg0);
                work->field_6E4 = 0;
            } else if (work->field_6E4 == 0) {
                work->field_6D4 = (gGolemKnightBishopIdleWaits[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF] * (0x10 - work->field_70C)) / 16;
                work->field_6CE = 1;
            } else {
                work->field_6D4 = 0;
                work->field_6CE = 2;
                work->field_6E4 = 0;
            }
            break;
        case 1:
            work->field_6D4--;
            if ((s16)work->field_6D4 <= 0) {
                work->field_6CE = 2;
                work->field_6D4 = 0;
            }
            break;
        case 2:
            if (work->field_70E != 3 && golemKnightBishopPlayerInBox(arg0) != 0) {
                work->field_6CE = 5;
                work->field_710 = 0;
                break;
            }
            if (work->field_70E == 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->field_710 + 10) {
                    work->field_6CE = 4;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 3;
                    work->field_710++;
                }
            } else if (work->field_70E == 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->field_710 + 8) {
                    work->field_6CE = 3;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 4;
                    work->field_710++;
                }
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6CE = ((gRandomLcgState >> 16) & 0xF) < 8 ? 3 : 4;
                work->field_710 = 0;
            }
            golemKnightBishopPlaceTarget(arg0);
            break;
        case 3:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 1;
                work->field_6CE = 0;
                work->field_70E = 1;
                if (work->field_70C < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            work->field_5DA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 4:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 2;
                work->field_6CE = 0;
                work->field_70E = 2;
                if (work->field_70C < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 5:
            work->field_6CC = 3;
            work->field_6CE = 0;
            work->field_70E = 3;
            Gp_ClearRec18Occupied(&work->field_644);
            if (work->field_70C < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                work->field_70C++;
            }
            break;
    }
}
