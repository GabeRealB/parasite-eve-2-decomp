/* Part of the factory lift library; see factory_lift.h. */

/// Arms a hatch swing when its open flag changes between closed and open.
///
/// Runs `FACTORY_HATCH_STATE_WATCH` on a live task holding `FactoryHatchWork`.
/// Only a 0-to-1 edge selects `FACTORY_HATCH_STATE_OPEN`, and only a 1-to-0
/// edge selects `FACTORY_HATCH_STATE_CLOSE`; either restarts the movement step.
/// Other nibble values are recorded but arm no movement. Returns 0 even when
/// arming a swing, so the dispatcher preserves the newly selected state.
static s32 _factoryHatchWatch(Task* task)
{
    enum { FACTORY_HATCH_FLAG_CLOSED = 0,
           FACTORY_HATCH_FLAG_OPEN   = 1 };
    FactoryHatchWork* work = task->work;
    u8                flag = gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN);

    if (flag == FACTORY_HATCH_FLAG_OPEN && work->prevFlag == FACTORY_HATCH_FLAG_CLOSED) {
        work->state = FACTORY_HATCH_STATE_OPEN;
        work->step  = 0;
    } else if (flag == FACTORY_HATCH_FLAG_CLOSED && work->prevFlag == FACTORY_HATCH_FLAG_OPEN) {
        work->state = FACTORY_HATCH_STATE_CLOSE;
        work->step  = 0;
    }
    work->prevFlag = flag;
    return 0;
}
