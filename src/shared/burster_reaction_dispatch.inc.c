/* Part of the burster library; see burster.h. */

/// Per-frame dispatch of the first enemy on its reaction state `field_2B2`:
/// 0 is the dormant arm `bursterDormantTick` and 1 the live handler
/// `bursterAwakeTick`. State 3 suppresses the rebind until
/// `Gp_TickObjFlag2` reports the reaction over, then returns the enemy to the
/// live stage, and ends with a step of the root. States 4 and 5 collapse the
/// enemy: both scale its second part at the base factor, count frames and
/// spawn the 0x60080 effect every 0x10; state 5 also counts those spawns and,
/// on the third, arms the death - a five-frame countdown, the death phase
/// reset and task state 2, with the enemy's HP cleared. Both collapse states
/// end by suppressing the rebind.
void bursterReactionDispatch(Task* arg0)
{
    Actor104600Work* work;
    Enemy*           enemy;
    u16              frames;

    work = (Actor104600Work*)arg0->work;
    switch (work->field_2B2) {
        case 0:
            bursterDormantTick(arg0);
            return;
        case 1:
            bursterAwakeTick(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            bursterStep(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            bursterScalePart(arg0, &arg0->extra.tmd->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, arg0->extra.tmd->coords, 0x400, &gBursterCollapseFxOffset);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            bursterScalePart(arg0, &arg0->extra.tmd->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, arg0->extra.tmd->coords, 0x400, &gBursterCollapseFxOffset);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2.pointer;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}
