/* Part of the Glutton library; see glutton.h. */

/// Per-frame animation step. `animStep` `GLUTTON_ANIM_STEP_BLEND` re-seeds the
/// three driving rigs through `gluttonSwitchAnim`, `GLUTTON_ANIM_STEP_RESTART`
/// resets every slot of them from `animId` outright; either way the step moves
/// on to `GLUTTON_ANIM_STEP_PLAYING` with `animTicks` and `clip` cleared. Then the
/// slots are advanced: plainly while `blending` is clear, otherwise through the
/// blended path, which clears `blending` again once `hostBlendRig`'s slot 1
/// reports done. The three trailing flags run the neck pitch, the neck yaw and
/// the limb pose.
void gluttonTickAnim(Task* arg0)
{
    GluttonWork* work = arg0->work;
    GluttonWork* w;
    s32          i;

    if (work->animStep == GLUTTON_ANIM_STEP_BLEND) {
        gluttonSwitchAnim(arg0);
        work->animStep  = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    } else if (work->animStep == GLUTTON_ANIM_STEP_RESTART) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->hostRig.slots[i].rate = w->animRate;
            animationResetSlot(&w->hostRig.anim, i, w->animId);
        }
        for (i = 0; i < 4; i++) {
            w->escort0Rig.slots[i].rate = w->animRate;
            animationResetSlot(&w->escort0Rig.anim, i, w->animId);
        }
        for (i = 0; i < 4; i++) {
            w->escort1Rig.slots[i].rate = w->animRate;
            animationResetSlot(&w->escort1Rig.anim, i, w->animId);
        }
        w->appliedAnimId = w->animId;
        work->animStep   = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks  = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    }

    if (work->blendStep == 2) {
        gluttonSeedBlend(arg0);
        work->blendStep = 3;
    }

    work->animTicks++;

    if (work->blending == 0) {
        w = arg0->work;
        for (i = 1; i < 8; i++) {
            w->hostRig.slots[i].rate = w->animRate;
            animationTickSlot(&w->hostRig.anim, i);
        }
        for (i = 0; i < 4; i++) {
            w->escort0Rig.slots[i].rate = w->animRate;
            animationTickSlot(&w->escort0Rig.anim, i);
        }
        for (i = 0; i < 4; i++) {
            w->escort1Rig.slots[i].rate = w->animRate;
            animationTickSlot(&w->escort1Rig.anim, i);
        }
    } else {
        gluttonTickBlended(arg0);
        if (work->hostBlendRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blending = 0;
        }
    }

    if (work->neckPitchEnabled != 0) {
        gluttonPitchNeck(arg0, work->neckPitchTarget);
    }
    if (work->neckYawEnabled != 0) {
        gluttonTurnNeck(arg0, work->neckYawTarget);
    }
    if (work->limbPoseEnabled != 0) {
        gluttonPoseLimb(arg0);
    }
}
