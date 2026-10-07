/* Part of the Glutton library; see glutton.h. */

/// Restarts all driving slots of the host and escorts 0 and 1.
static __inline__ void _gluttonRestartDrivingRigs(Task* task)
{
    GluttonWork* rigWork = task->work;
    s32          slotIndex;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(rigWork->hostRig.slots); slotIndex++) {
        rigWork->hostRig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->hostRig.anim, slotIndex, rigWork->animId);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort0Rig.slots); slotIndex++) {
        rigWork->escort0Rig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->escort0Rig.anim, slotIndex, rigWork->animId);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort1Rig.slots); slotIndex++) {
        rigWork->escort1Rig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->escort1Rig.anim, slotIndex, rigWork->animId);
    }
    rigWork->appliedAnimId = rigWork->animId;
}

/// Advances all driving slots of the host and escorts 0 and 1.
static __inline__ void _gluttonAdvanceDrivingRigs(Task* task)
{
    GluttonWork* rigWork = task->work;
    s32          slotIndex;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(rigWork->hostRig.slots); slotIndex++) {
        rigWork->hostRig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->hostRig.anim, slotIndex);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort0Rig.slots); slotIndex++) {
        rigWork->escort0Rig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->escort0Rig.anim, slotIndex);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort1Rig.slots); slotIndex++) {
        rigWork->escort1Rig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->escort1Rig.anim, slotIndex);
    }
}

/// Advances the Glutton's three driving rigs and applies enabled joint overrides.
///
/// Requires the host's live work, TMD body, escorts 0, 1 and 4, and initialized
/// rigs with loaded animation resources. BLEND seeks a changed clip, RESTART
/// resets it unconditionally; both clear the clip cue-cache block and elapsed
/// ticks. A requested secondary clip is seeded before playback. Plain ticks
/// advance host slots 1..7 and escort slots 0..3 at `animRate`; blended ticks use
/// both pose sources and end blending when secondary host slot 1 reaches a
/// boundary. Pitch, yaw and limb overrides follow playback in that order.
static void _gluttonTickAnim(Task* task)
{
    enum { GLUTTON_BLEND_STEP_RESTART = 2,
           GLUTTON_BLEND_STEP_PLAYING = 3 };
    GluttonWork* work = task->work;

    if (work->animStep == GLUTTON_ANIM_STEP_BLEND) {
        _gluttonSwitchAnim(task);
        work->animStep  = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    } else if (work->animStep == GLUTTON_ANIM_STEP_RESTART) {
        _gluttonRestartDrivingRigs(task);
        work->animStep  = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    }

    if (work->blendStep == GLUTTON_BLEND_STEP_RESTART) {
        _gluttonSeedBlend(task);
        work->blendStep = GLUTTON_BLEND_STEP_PLAYING;
    }

    work->animTicks++;

    if (work->blending == 0) {
        _gluttonAdvanceDrivingRigs(task);
    } else {
        _gluttonTickBlended(task);
        if (work->hostBlendRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blending = 0;
        }
    }

    // Apply procedural joints after the clips have written their poses.
    if (work->neckPitchEnabled != 0) {
        _gluttonPitchNeck(task, work->neckPitchTarget);
    }
    if (work->neckYawEnabled != 0) {
        _gluttonTurnNeck(task, work->neckYawTarget);
    }
    if (work->limbPoseEnabled != 0) {
        _gluttonPoseLimb(task);
    }
}
