/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Per-frame dispatch on the second enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1 and
/// the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
void skullStalkerReactionDispatch(Task* task)
{
    SkullStalkerWork* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            skullStalkerIdleTick(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2(task->spawnArg2.pointer) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}
