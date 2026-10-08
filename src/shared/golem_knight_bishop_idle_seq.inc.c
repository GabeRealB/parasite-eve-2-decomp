/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Waits while hidden, chooses an attack and checks its placement.
///
/// The wait scales down with attack count; a broken feint goes straight to a
/// counterattack placement. One-frame grid probes must be resolved between
/// placement and the next sequence update. Their shared contact record decides
/// whether the grab or strike spot is clear. The roll tables have 16 entries;
/// failed placement retries the pick without changing retained attack history.
static void _golemKnightBishopIdleSeq(Task* task)
{
    enum { GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE = 16 };
    enum {
        GOLEM_KNIGHT_BISHOP_IDLE_START       = 0,
        GOLEM_KNIGHT_BISHOP_IDLE_WAIT        = 1,
        GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK = 2,
        GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB   = 3,
        GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE = 4,
        GOLEM_KNIGHT_BISHOP_IDLE_START_BOX   = 5,
    };
    GolemKnightBishopWork* work;

    work = task->work;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_IDLE_START:
            work->forwardSpeed     = 0;
            work->flickerStage     = GOLEM_KNIGHT_BISHOP_FLICKER_NONE;
            work->counterattacking = 0;
            if (work->feintBroken != 0) {
                gRandomLcgState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step             = gGolemKnightBishopIdleSteps[(gRandomLcgState >> 16) & 0xF] + GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->counterattacking = 1;
                _golemKnightBishopPlaceTarget(task);
                work->feinting = 0;
            } else if (work->feinting == 0) {
                work->timer = (gGolemKnightBishopIdleWaits[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF] * (GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE - work->attackCount)) / GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE;
                work->step  = GOLEM_KNIGHT_BISHOP_IDLE_WAIT;
            } else {
                work->timer    = 0;
                work->step     = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->feinting = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                work->step  = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->timer = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK:
            if (work->lastAttack != GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH && _golemKnightBishopPlayerInBox(task) != 0) {
                work->step        = GOLEM_KNIGHT_BISHOP_IDLE_START_BOX;
                work->repeatCount = 0;
                break;
            }
            if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->repeatCount + 10) {
                    work->step        = GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                    work->repeatCount = 0;
                } else {
                    work->step = GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB;
                    work->repeatCount++;
                }
            } else if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->repeatCount + 8) {
                    work->step        = GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB;
                    work->repeatCount = 0;
                } else {
                    work->step = GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                    work->repeatCount++;
                }
            } else {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step        = ((gRandomLcgState >> 16) & 0xF) < 8 ? GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB : GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                work->repeatCount = 0;
            }
            _golemKnightBishopPlaceTarget(task);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            work->spotProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            worldCollisionClearContacts(work->probeContacts);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            worldCollisionClearContacts(work->probeContacts);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_START_BOX:
            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            worldCollisionClearContacts(work->aimBeamContacts);
            if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                work->attackCount++;
            }
            break;
    }
}
