/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `rate` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void incinBossSwitchAnim(Task* arg0)
{
    Actor403200Work* work = arg0->work;
    s32              i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].rate = work->field_7B6;
            func_800B4114(&work->anim0, i, work->field_7B3, 0,
                          gIncinBossAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].rate = work->field_7B6;
            func_800B4114(&work->anim2, i, work->field_7B3, 0,
                          gIncinBossAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].rate = work->field_7B6;
            func_800B4114(&work->anim4, i, work->field_7B3, 0,
                          gIncinBossAnimTransitions[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}
