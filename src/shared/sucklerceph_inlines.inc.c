/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Rebinds the Sucklerceph's animation id to its two helper slots unless
/// `field_2D2` suppresses it: a changed id is remembered, its frame count
/// restarts and both slots switch to it; otherwise the count ticks and the
/// slots advance.
static __inline__ void sucklercephTickAnim(Task* task)
{
    SucklercephWork* work = (SucklercephWork*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                animationSeekSlotWithBlend(&work->context, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                animationTickSlot(&work->context, i);
            }
        }
    }
}
