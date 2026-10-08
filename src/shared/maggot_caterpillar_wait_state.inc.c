/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Wakes a dormant enemy on player proximity, a hit or scene alert class 2.
///
/// The player and enemy roots must share their parent frame. The strict X/Z
/// wake radius uses game units. During wake ticks 11..60 the root moves four
/// units along parent +Z, independently of facing. Tick 75 starts roam with
/// an idle delay selected by placement row 0..7 plus a random 0..15 ticks.
/// Reserves and releases one VECTOR on the initialized scratch stack.
static void _maggotCaterpillarWaitState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_WAIT_DORMANT          = 0,
        MAGGOT_CATERPILLAR_WAIT_WAKING           = 1,
        MAGGOT_CATERPILLAR_WAKE_MOVE_FIRST_FRAME = 11,
        MAGGOT_CATERPILLAR_WAKE_MOVE_END_FRAME   = 61,
        MAGGOT_CATERPILLAR_WAKE_END_FRAME        = 75,
        MAGGOT_CATERPILLAR_WAKE_ALERT_CLASS      = 2
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    waitStep;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    u32                    randomDelay;
    s32                    placementRow;
    VECTOR*                playerOffset;

    playerOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    waitStep     = work->step;
    coord        = actor->extra.tmd->coords;
    switch (waitStep) {
        case MAGGOT_CATERPILLAR_WAIT_DORMANT:
            playerOffset->vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            playerOffset->vy = 0;
            playerOffsetZ    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            playerOffset->vz = playerOffsetZ;
            playerOffsetX    = playerOffset->vx;
            if ((SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < MAGGOT_CATERPILLAR_WAKE_RANGE) || (work->struck != 0) || (gSceneCombatState.signals.bytes.enemyAlert == MAGGOT_CATERPILLAR_WAKE_ALERT_CLASS)) {
                work->step   = MAGGOT_CATERPILLAR_WAIT_WAKING;
                work->animId = MAGGOT_CATERPILLAR_ANIM_WAKE;
                sceneEngageBattle(1);
            }
            break;
        case MAGGOT_CATERPILLAR_WAIT_WAKING:
            if ((work->animFrame >= MAGGOT_CATERPILLAR_WAKE_MOVE_FIRST_FRAME) && (work->animFrame < MAGGOT_CATERPILLAR_WAKE_MOVE_END_FRAME)) {
                coord->coord.t[2] += 4;
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_WAKE_END_FRAME) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                placementRow       = ((Enemy*)actor->spawnArg2.pointer)->place->rowIndex;
                randomDelay        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = randomDelay;
                work->stateCounter = gMaggotCaterpillarIdleDelay[placementRow] + ((randomDelay >> 0x10) & 0xF);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
