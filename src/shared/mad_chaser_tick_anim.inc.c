/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Captures and blends all animated parts into the requested clip.
///
/// The work's nine-slot context and pose storage must be initialized; slot zero
/// is excluded. The caller retains ownership of the loaded clips and work.
static __inline__ void _madChaserBlendAnimSlots(MadChaserWork* requestWork)
{
    s32 blendSlot;

    for (blendSlot = 1; blendSlot < ARRAY_SIZE(requestWork->slots); blendSlot++) {
        requestWork->slots[blendSlot].rate = requestWork->animRate;
        animationSeekSlotWithBlend(&requestWork->anim, blendSlot, requestWork->animId, 0, requestWork->animBlendFrames);
    }
}

/// Applies animation requests and advances the eight animated model parts.
///
/// Slots 1..8 share animId and the signed low byte of animRate (sixteenths of a
/// frame); slot 0 is retained. A blend seeks even when the clip repeats, and clears
/// animBlendFrames only after changing clips. A reset cuts to the requested clip.
/// Either request becomes PLAYING with animFrames zero; later PLAYING calls
/// increment that 16-bit counter. Every call ticks the eight slots, including an
/// idle request value of zero. Requires initialized nine-slot/pose storage, a
/// nine-part model, a valid loaded animation-bank entry, and the playback API's
/// scratch-stack and track-data requirements.
static void _madChaserTickAnim(Task* task)
{
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            resetSlot;
    s32            playSlot;

    work = task->work;
    // Apply requests before ticking so a blend captures the preceding pose.
    if (work->animRequest == MAD_CHASER_ANIM_REQUEST_BLEND) {
        requestWork = work;
        if (requestWork->appliedAnim == requestWork->animId) {
            _madChaserBlendAnimSlots(requestWork);
        } else {
            _madChaserBlendAnimSlots(requestWork);
            requestWork->animBlendFrames = 0;
        }
        requestWork->appliedAnim = requestWork->animId;
        work->animRequest        = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames         = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_RESET) {
        requestWork = work;
        for (resetSlot = 1; resetSlot < ARRAY_SIZE(requestWork->slots); resetSlot++) {
            animationResetSlot(&requestWork->anim, resetSlot, requestWork->animId);
            requestWork->slots[resetSlot].rate = requestWork->animRate;
        }
        requestWork->appliedAnim = requestWork->animId;
        work->animRequest        = MAD_CHASER_ANIM_REQUEST_PLAYING;
        work->animFrames         = 0;
    } else if (work->animRequest == MAD_CHASER_ANIM_REQUEST_PLAYING) {
        work->animFrames++;
    }
    // A newly applied request also advances its first pose on this call.
    for (playSlot = 1; playSlot < ARRAY_SIZE(work->slots); playSlot++) {
        work->slots[playSlot].rate = work->animRate;
        animationTickSlot(&work->anim, playSlot);
    }
}
