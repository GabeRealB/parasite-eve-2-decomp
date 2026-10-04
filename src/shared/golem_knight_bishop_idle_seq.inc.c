/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the hidden wait between attacks. Step 0 rolls the wait into `timer`,
/// shortened by `attackCount`, or skips it after a feint; a broken feint goes
/// straight to testing a spot with `counterattacking` set. Step 1 counts the
/// wait down. Step 2 picks the next attack: the box approach (step 5) when
/// `golemKnightBishopPlayerInBox` finds the player in a box and the last
/// attack was not one, otherwise a grab (step 3) or a strike (step 4) by an
/// LCG draw weighted by `lastAttack` and `repeatCount`, placing the target
/// and enabling the probes. Steps 3 and 4 start that attack when
/// `probeContacts` shows the spot clear, and otherwise pick again; steps 3 to
/// 5 walk `attackCount` up to `GOLEM_KNIGHT_BISHOP_IDLE_LIMIT`.
void golemKnightBishopIdleSeq(Task* arg0)
{
    GolemKnightBishopWork* work;

    work = arg0->work;
    switch (work->step) {
        case 0:
            work->forwardSpeed     = 0;
            work->flickerStage     = 0;
            work->counterattacking = 0;
            if (work->feintBroken != 0) {
                gRandomLcgState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step             = gGolemKnightBishopIdleSteps[(gRandomLcgState >> 16) & 0xF] + 2;
                work->counterattacking = 1;
                golemKnightBishopPlaceTarget(arg0);
                work->feinting = 0;
            } else if (work->feinting == 0) {
                work->timer = (gGolemKnightBishopIdleWaits[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF] * (0x10 - work->attackCount)) / 16;
                work->step  = 1;
            } else {
                work->timer    = 0;
                work->step     = 2;
                work->feinting = 0;
            }
            break;
        case 1:
            work->timer--;
            if (work->timer <= 0) {
                work->step  = 2;
                work->timer = 0;
            }
            break;
        case 2:
            if (work->lastAttack != GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH && golemKnightBishopPlayerInBox(arg0) != 0) {
                work->step        = 5;
                work->repeatCount = 0;
                break;
            }
            if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->repeatCount + 10) {
                    work->step        = 4;
                    work->repeatCount = 0;
                } else {
                    work->step = 3;
                    work->repeatCount++;
                }
            } else if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->repeatCount + 8) {
                    work->step        = 3;
                    work->repeatCount = 0;
                } else {
                    work->step = 4;
                    work->repeatCount++;
                }
            } else {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step        = ((gRandomLcgState >> 16) & 0xF) < 8 ? 3 : 4;
                work->repeatCount = 0;
            }
            golemKnightBishopPlaceTarget(arg0);
            break;
        case 3:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->step       = 0;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = 2;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            work->spotProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(work->probeContacts);
            break;
        case 4:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                work->step       = 0;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = 2;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(work->probeContacts);
            break;
        case 5:
            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            work->step       = 0;
            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            Gp_ClearRec18Occupied(work->aimBeamContacts);
            if (work->attackCount < GOLEM_KNIGHT_BISHOP_IDLE_LIMIT) {
                work->attackCount++;
            }
            break;
    }
}
