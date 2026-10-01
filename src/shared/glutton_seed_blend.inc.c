/* Part of the Glutton library; see glutton.h. */

/// Seed the blend: `field_7BE` to 0x30 and `field_7C0` to 0x800. Slots 1 and up
/// of the first animation context of each of the three pairs take that rate,
/// and the same slots of the second context are reset to animation
/// `field_7BC`.
void gluttonSeedBlend(Task* task)
{
    GluttonWork* work;
    s32          i;

    work            = task->work;
    work->field_7BE = 0x30;
    work->field_7C0 = 0x800;
    i               = 1;
    do {
        work->slots0[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim1, i, work->field_7BC);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->slots2[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim3, i, work->field_7BC);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->slots4[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim5, i, work->field_7BC);
        i++;
    } while (i < 4);
}
