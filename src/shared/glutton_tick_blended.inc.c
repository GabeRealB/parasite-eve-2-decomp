/* Part of the Glutton library; see glutton.h. */

/// Advance the three animation pairs with the second context of each blended
/// in at weight `field_7C0`: every slot of the second context ticks at
/// `field_7BE`, every slot of the first at `field_7B6` less 3, and the pose
/// written to the first is the mix of the two.
void gluttonTickBlended(Task* arg0)
{
    AnimationPose    pose0;
    AnimationPose    pose1;
    Actor403200Work* work     = arg0->work;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].rate = work->field_7BE;
            work->slots0[i].rate = work->field_7B6 - 3;
            animationTickSlotPose(&work->anim0, i, &pose0, 0);
            animationTickSlotPose(&work->anim1, i, &pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].rate = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].rate = work->field_7BE;
        work->slots2[i].rate = work->field_7B6 - 3;
        animationTickSlotPose(&work->anim2, i, &pose0, 0);
        animationTickSlotPose(&work->anim3, i, &pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].rate = work->field_7BE;
        work->slots4[i].rate = work->field_7B6 - 3;
        animationTickSlotPose(&work->anim4, i, &pose0, 0);
        animationTickSlotPose(&work->anim5, i, &pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}
