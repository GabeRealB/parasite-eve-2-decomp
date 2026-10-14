/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Restarts body tracks 1..17 at the requested clip's beginning and latches it as playing.
///
/// Requires an initialized live rig and a loaded `animClip` with every body
/// track. Applies `animStep` through the slots' signed byte rate; slot 0 is
/// untouched. Does not change the request or frame counter.
static void _stalkerZebraIvoryRestartClip(Task* task)
{
    StalkerZebraIvoryWork* work;
    s32                    slotIndex;

    work      = (StalkerZebraIvoryWork*)task->work;
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animStep;
        animationResetSlot(&work->rig.anim, slotIndex, work->animClip);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->animPlaying = work->animClip;
}

/// Reseeds every body track with the requested rate and blend from frame zero.
///
/// `workArg` must be a side-effect-free pointer to the initialized carrier work;
/// `slotIndexArg` must be a writable s32 local used only as the loop index here.
/// Both expressions are evaluated repeatedly. Slots 1..17 require a loaded
/// clip with a track for each driven model part. animStep narrows to each slot's
/// signed byte rate (16 means one frame per tick); animBlend is measured in
/// whole normal-rate frames. Leaves slotIndexArg at 18 and preserves root slot 0.
/// Does not consume animBlend or latch animPlaying; the caller does that.
/// Captures no locals and expands to one statement; undefined after this fragment.
#define STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(workArg, slotIndexArg)                                                        \
    (void)({                                                                                                                \
        (slotIndexArg) = 1;                                                                                                 \
        do {                                                                                                                \
            (workArg)->rig.slots[(slotIndexArg)].rate = (workArg)->animStep;                                                \
            animationSeekSlotWithBlend(&(workArg)->rig.anim, (slotIndexArg), (workArg)->animClip, 0, (workArg)->animBlend); \
            (slotIndexArg)++;                                                                                               \
        } while ((slotIndexArg) < ARRAY_SIZE((workArg)->rig.slots));                                                        \
    })

/// Applies the requested body clip with a transition measured in normal-rate frames.
///
/// Drives slots 1..17 of an initialized live rig and latches `animPlaying`.
/// `animClip` must select a loaded set supporting every body track; `animStep`
/// narrows to the slots' signed byte rate (16 is one normal frame per tick).
/// Changing clip consumes `animBlend`. Repeating the clip preserves that duration:
/// Zebra changes only the rate, while Ivory also reseeds the blend.
static void _stalkerZebraIvoryBlendClip(Task* task)
{
    StalkerZebraIvoryWork* work;
    s32                    slotIndex;

    work = (StalkerZebraIvoryWork*)task->work;
    if (work->animPlaying == work->animClip) {
#if STALKER_ZEBRA_IVORY_REBLEND_SAME_CLIP
        STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(work, slotIndex);
#else
        slotIndex = 1;
        do {
            work->rig.slots[slotIndex].rate = work->animStep;
            slotIndex++;
        } while (slotIndex < ARRAY_SIZE(work->rig.slots));
#endif
    } else {
        STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS(work, slotIndex);
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}

#undef STALKER_ZEBRA_IVORY_BLEND_BODY_TRACKS

/// Converts a normal-rate frame count to ticks at the requested animation rate.
///
/// Returns 0 for zero `animStep`. Division uses eight fractional bits before
/// dropping four to account for the sixteenth-frame rate, then narrows to s16.
/// For nonnegative frames and a positive rate this rounds down; callers supply
/// counts that fit the signed-halfword result. Negative inputs retain the same
/// signed division, shift and truncation behavior.
static s16 _stalkerZebraIvoryFrameToTicks(Task* task, s16 normalFrames)
{
    enum { STALKER_ZEBRA_IVORY_FRAME_FRACTION_BITS = 8,
           STALKER_ZEBRA_IVORY_RATE_FRACTION_BITS  = 4 };
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    if (work->animStep == 0) {
        return 0;
    }
    return (s16)(((normalFrames << STALKER_ZEBRA_IVORY_FRAME_FRACTION_BITS) / work->animStep) >> STALKER_ZEBRA_IVORY_RATE_FRACTION_BITS);
}

/// Steps root yaw toward a target's X/Z with a one-sixteenth-turn dead zone.
///
/// Target and root translation must share the same parent coordinate frame;
/// only target X/Z are read and no pointer is retained. The displacement
/// narrows to signed halfwords before normalization. `turnStep` is a positive
/// increment in 4096ths of a turn (callers use 24..36); it is not clamped to
/// the heading error. Wraps the error to [-2048, 2047], changes yaw only beyond
/// +/-256, and narrows stored yaw to s16. Marks the root dirty without rebuilding
/// its matrix; the caller applies the updated rotation.
static void _stalkerZebraIvoryTurnToward(Task* task, const SVECTOR* target, s32 turnStep)
{
    enum { STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE = 256,
           STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT = 20 };
    StalkerZebraIvoryWork* work = task->work;
    GfxCoord*              rootCoord;
    SVECTOR                targetDirection;
    s32                    yawError;
    s32                    targetYaw;
    u16                    currentYaw;

    rootCoord               = task->extra.tmd->coords;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    targetDirection.vx      = target->vx - rootCoord->coord.t[0];
    targetDirection.vy      = 0;
    targetDirection.vz      = target->vz - rootCoord->coord.t[2];
    VectorNormalSS(&targetDirection, &targetDirection);
    targetYaw  = ratan2(targetDirection.vx, targetDirection.vz);
    currentYaw = work->yaw;
    // Sign-extend the 12-bit wrapped heading error before choosing turn direction.
    yawError = ((currentYaw - targetYaw) << STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT) >> STALKER_ZEBRA_IVORY_YAW_SIGN_SHIFT;
    if (yawError > STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE) {
        work->yaw = currentYaw - turnStep;
    } else if (yawError < -STALKER_ZEBRA_IVORY_TURN_DEAD_ZONE) {
        work->yaw = currentYaw + turnStep;
    }
}
