/* Part of the Glutton library; see glutton.h. */

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `rate` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void gluttonSwitchAnim(Task* arg0)
{
    GluttonWork* work = arg0->work;
    s32          i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].rate = work->field_7B6;
            animationSeekSlotWithBlend(&work->anim0, i, work->field_7B3, 0,
                                       gGluttonAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].rate = work->field_7B6;
            animationSeekSlotWithBlend(&work->anim2, i, work->field_7B3, 0,
                                       gGluttonAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].rate = work->field_7B6;
            animationSeekSlotWithBlend(&work->anim4, i, work->field_7B3, 0,
                                       gGluttonAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}
