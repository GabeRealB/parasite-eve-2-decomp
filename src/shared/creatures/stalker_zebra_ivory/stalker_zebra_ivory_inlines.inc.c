/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. Inline helpers the fragments use. */

/// Applies a pending body-animation request and advances tracks 1..17 once.
///
/// Requires a live initialized rig and valid loaded clips for all body tracks.
/// Blend/restart requests become PLAYING; a changed clip resets the counter.
/// Repeating
/// a blend clip converts that counter through `_stalkerZebraIvoryFrameToTicks`.
/// PLAYING increments it as an s16. Every request state ticks slots, including
/// the default zero state; rates narrow to signed bytes (16 is normal speed).
static inline void _stalkerZebraIvoryTickAnimInline(Task* task)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;
    s32                    slotIndex;

    if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrame = 0;
        } else {
            work->animFrame = _stalkerZebraIvoryFrameToTicks(task, work->animFrame);
        }
        _stalkerZebraIvoryBlendClip(task);
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART) {
        _stalkerZebraIvoryRestartClip(task);
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING;
        work->animFrame   = 0;
    } else if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING) {
        work->animFrame++;
    }
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animStep;
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Rebuilds the root's rotation from wrapped roll, pitch and yaw.
///
/// `actorTask` is a stable Task pointer read twice, with this carrier's live
/// StalkerZebraIvoryWork and model root. Angles narrow in place to 0..4095.
/// Composes Z, X, then Y at Q12 unit scale and copies only the nine rotation
/// coefficients, preserving translation and composition stamps. Requires an
/// initialized, word-aligned scratch stack with sizeof(MATRIX) free bytes;
/// releases the temporary before completion and clobbers GTE state.
/// Declares work, rootCoord, rotation and rootRotation in its own scope;
/// actorTask must not use those names. Captures no locals; expands to one
/// statement shared by the inline and out-of-line entries.
#define STALKER_ZEBRA_IVORY_REBUILD_ROOT_ROTATION(actorTask)               \
    (void)({                                                               \
        StalkerZebraIvoryWork* work      = (actorTask)->work;              \
        GfxCoord*              rootCoord = (actorTask)->extra.tmd->coords; \
        MATRIX*                rotation;                                   \
        MATRIX*                rootRotation;                               \
                                                                           \
        work->pitch &= ACTOR_TRANSFORM_ANGLE_MASK;                         \
        work->yaw   &= ACTOR_TRANSFORM_ANGLE_MASK;                         \
        work->roll  &= ACTOR_TRANSFORM_ANGLE_MASK;                         \
        rotation     = SCRATCH_STACK_CURSOR(MATRIX) - 1;                   \
        gfxSetRotIdentity(rotation);                                       \
        SCRATCH_STACK_CURSOR(MATRIX) = rotation;                           \
        RotMatrixZ(work->roll, rotation);                                  \
        RotMatrixX(work->pitch, rotation);                                 \
        RotMatrixY(work->yaw, rotation);                                   \
        rootRotation          = &rootCoord->coord;                         \
        rootRotation->m[0][0] = rotation->m[0][0];                         \
        rootRotation->m[0][1] = rotation->m[0][1];                         \
        rootRotation->m[0][2] = rotation->m[0][2];                         \
        rootRotation->m[1][0] = rotation->m[1][0];                         \
        rootRotation->m[1][1] = rotation->m[1][1];                         \
        rootRotation->m[1][2] = rotation->m[1][2];                         \
        rootRotation->m[2][0] = rotation->m[2][0];                         \
        rootRotation->m[2][1] = rotation->m[2][1];                         \
        rootRotation->m[2][2] = rotation->m[2][2];                         \
        SCRATCH_STACK_RELEASE_BLOCK(MATRIX);                               \
    })

/// Rebuilds root rotation from pitch, yaw and roll in 4096ths of a turn.
///
/// Wraps the stored angles, composes roll then pitch then yaw, and copies only
/// the nine rotation coefficients. Preserves translation and cache stamps:
/// callers invalidate/recompose the root when needed. Requires an initialized
/// scratch stack with sizeof(MATRIX) free bytes; no reservation survives the call.
static inline void _stalkerZebraIvoryApplyRotationInline(Task* task)
{
    STALKER_ZEBRA_IVORY_REBUILD_ROOT_ROTATION(task);
}

/// Running behavior indices selected by the shared completion handlers.
enum {
    STALKER_ZEBRA_IVORY_STATE_WALK          = 2,
    STALKER_ZEBRA_IVORY_STATE_GRAB          = 8,
    STALKER_ZEBRA_IVORY_STATE_CRAWL_ON_BACK = 10,
    STALKER_ZEBRA_IVORY_STATE_CEILING_DROP  = 13
};

/// Starts a selected Stalker behavior at its first sub-state.
///
/// `state` must index the carrier's running-behavior table. Reads the live
/// task work pointer at selection time, including after calls that can change
/// it. Preserves animation, timers and pending reactions; only the signed
/// halfword state and sub-state cursor change.
static inline void _stalkerZebraIvorySelectState(Task* task, s16 state)
{
    StalkerZebraIvoryWork* stateWork = task->work;

    stateWork->state    = state;
    stateWork->subState = 0;
}

/// Chooses a fresh 30..157-update duration for the on-back crawl.
///
/// Advances the shared 32-bit LCG once with unsigned wraparound and uses seven
/// bits of its high halfword. Borrows writable work; no animation or state changes.
static inline void _stalkerZebraIvorySeedCrawlCountdown(StalkerZebraIvoryWork* work)
{
    enum { STALKER_ZEBRA_IVORY_CRAWL_BASE_TICKS        = 30,
           STALKER_ZEBRA_IVORY_CRAWL_SPREAD_MASK       = 127,
           STALKER_ZEBRA_IVORY_CRAWL_RANDOM_HIGH_SHIFT = 16 };
    u32 randomState;

    randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState = randomState;
    work->countdown = ((randomState >> STALKER_ZEBRA_IVORY_CRAWL_RANDOM_HIGH_SHIFT) & STALKER_ZEBRA_IVORY_CRAWL_SPREAD_MASK) + STALKER_ZEBRA_IVORY_CRAWL_BASE_TICKS;
}
