/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Per-frame dispatch of the first enemy on its reaction state `state`:
/// 0 is the dormant arm `sucklercephDormantTick` and 1 the live handler
/// `sucklercephAwakeTick`. State 3 suppresses the rebind until
/// `Gp_TickObjFlag2` reports the reaction over, then returns the enemy to the
/// live stage, and ends with a step of the root. States 4 and 5 collapse the
/// enemy: both scale its second part at the base factor, count frames and
/// spawn the 0x60080 effect every 0x10; state 5 also counts those spawns and,
/// on the third, arms the death - a five-frame countdown, the death phase
/// reset and task state 2, with the enemy's HP cleared. Both collapse states
/// end by suppressing the rebind.
void sucklercephReactionDispatch(Task* arg0)
{
    SucklercephWork* work;
    Enemy*           enemy;
    u16              frames;

    work = arg0->work;
    switch (work->state) {
        case SUCKLERCEPH_STATE_DORMANT:
            sucklercephDormantTick(arg0);
            return;
        case SUCKLERCEPH_STATE_AWAKE:
            sucklercephAwakeTick(arg0);
            return;
        case SUCKLERCEPH_STATE_STATUS_HOLD:
            work->animFrozen = 1;
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->animFrozen   = 0;
                work->state        = SUCKLERCEPH_STATE_AWAKE;
                work->awakeStage   = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
                work->forwardSpeed = 0;
            }
            sucklercephStep(arg0);
            return;
        case SUCKLERCEPH_STATE_PUFFING:
            work->swellScale = ONE;
            sucklercephScalePart(arg0, &arg0->extra.tmd->coords[1]);
            frames           = work->animFrames + 1;
            work->animFrames = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, arg0->extra.tmd->coords, 0x400, &gSucklercephCollapseFxOffset);
                work->animFrames = 0;
            }
            work->animFrozen = 1;
            return;
        default:
            return;
        case SUCKLERCEPH_STATE_PUFFING_DEATH:
            work->swellScale = ONE;
            sucklercephScalePart(arg0, &arg0->extra.tmd->coords[1]);
            frames           = work->animFrames + 1;
            work->animFrames = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, arg0->extra.tmd->coords, 0x400, &gSucklercephCollapseFxOffset);
                work->animFrames = 0;
                work->swellFrames++;
                if (work->swellFrames >= 3) {
                    enemy               = arg0->spawnArg2.pointer;
                    arg0->killCountdown = 5;
                    work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
            work->animFrozen = 1;
    }
}
