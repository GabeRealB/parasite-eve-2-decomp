/* Part of the library; see skull_stalker.h. Inline helpers the fragments use. */

/// Rebinds the second enemy's animation id `field_28C` to its two helper
/// slots: a changed id is remembered in `field_28E`, its frame count restarts
/// and both slots switch to it with a blend of 8; otherwise the count ticks and
/// the slots advance.
static __inline__ void skullStalkerTickAnim(Task* task)
{
    SkullStalkerWork* work = task->work;
    s32               i;

    if (work->field_28C != work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        for (i = 1; i < 3; i++) {
            func_800B4114(&work->context, i, work->field_28C, 0, 8);
        }
    } else {
        work->field_290++;
        for (i = 1; i < 3; i++) {
            Gp_AnimTickIndex(&work->context, i);
        }
    }
}
