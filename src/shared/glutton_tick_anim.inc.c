/* Part of the Glutton library; see glutton.h. */

/// Per-frame animation step. `field_7B0` 1 re-seeds the block through
/// `gluttonSwitchAnim`, 2 resets every slot of the three even members
/// from `field_7B3` outright; either way the block is armed (`field_7B0` 3, the
/// frame counter and the 0x20-byte scratch at `field_7D0` cleared). Then the
/// slots are advanced: plainly while `field_7B1` is clear, otherwise through the
/// blended path, which clears `field_7B1` again once the first pair's slot 1
/// reports done. The three trailing flags run the shared reaction helpers.
void gluttonTickAnim(Task* arg0)
{
    Actor403200Work* work = arg0->work;
    Actor403200Work* w;
    s32              i;

    if (work->field_7B0 == 1) {
        gluttonSwitchAnim(arg0);
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    } else if (work->field_7B0 == 2) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim0, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim2, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim4, i, w->field_7B3);
        }
        w->field_7B2    = w->field_7B3;
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    }

    if (work->field_7BA == 2) {
        gluttonSeedBlend(arg0);
        work->field_7BA = 3;
    }

    work->field_7B4++;

    if (work->field_7B1 == 0) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim0, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim2, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim4, i);
        }
    } else {
        gluttonTickBlended(arg0);
        if (work->slots1[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->field_7B1 = 0;
        }
    }

    if (work->field_EF4 != 0) {
        gluttonPitchNeck(arg0, work->field_EFE);
    }
    if (work->field_EF6 != 0) {
        gluttonTurnNeck(arg0, work->field_7C4);
    }
    if (work->field_EF8 != 0) {
        gluttonPoseLimb(arg0);
    }
}
