/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the animation request in `field_414` on animation slots 1..8: kind 1
/// blends into animation `field_418` over `field_426` frames, kind 2 resets
/// the slots straight to it, and either records it as applied and moves to
/// kind 3, which counts frames in `field_41A`. Every frame each slot then
/// ticks at speed `field_41C`.
void madChaserTickAnim(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* start;
    s32            i;
    s32            j;
    s32            k;

    work = (MadChaserWork*)arg0->work;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            animationResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].rate = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].rate = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}
