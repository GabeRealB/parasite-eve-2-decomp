/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Applies the current hit flinch to body part 3 and eases its pitch and yaw to zero.
///
/// Angles use 4096 units per turn and decay by 32 per call, snapping inside that
/// distance. Run after animation has rebuilt the part's pose; the enclosing frame
/// handler marks its composition dirty. Clears hitTiltActive once both axes settle.
/// The caller supplies live body work; the temporary matrix is released on return.
static void _golemPawnRookDecayHitTilt(Task* actor)
{
    enum { GOLEM_PAWN_ROOK_HIT_TILT_DECAY = 32 };
    GolemPawnRookWork* work;
    GfxCoord*          bodyCoords;
    MATRIX*            tiltMatrix;
    s32                pitch;
    s32                yaw;
    s32                absolutePitch;
    s32                nextPitch;
    s32                absoluteYaw;
    s32                nextYaw;
    s32                tiltRemaining;

    /// Steps one signed flinch angle toward zero and records any remaining tilt.
    ///
    /// angleField is a writable s16 angle, in 4096 units per turn. value,
    /// magnitude, next and remaining are distinct s32 lvalues. decay is a
    /// positive signed-word angular step whose addition of 1 must not overflow;
    /// callers use 32. Magnitudes <= decay snap to zero; otherwise the updated
    /// angle retains its sign and remaining is set to 1. remaining is never
    /// cleared; callers initialize it to 0 before processing both axes.
    /// magnitude/next are written only on their respective nonzero/decay branches.
    /// Arguments must be side-effect-free and can be evaluated repeatedly.
    /// Captures no locals; use the compound statement only as a standalone
    /// invocation. It is undefined below.
#define GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(angleField, value, magnitude, next, remaining, decay) \
    {                                                                                         \
        (value) = (angleField);                                                               \
        if ((value) != 0) {                                                                   \
            (magnitude) = __builtin_abs((value));                                             \
            if ((magnitude) < (decay) + 1) {                                                  \
                (angleField) = 0;                                                             \
            } else {                                                                          \
                (next) = (value) - (decay);                                                   \
                if ((value) <= 0) {                                                           \
                    (next) = (value) + (decay);                                               \
                }                                                                             \
                (angleField) = (next);                                                        \
                (remaining)  = 1;                                                             \
            }                                                                                 \
        }                                                                                     \
    }

    tiltMatrix    = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    tiltRemaining = 0;
    work          = actor->work;
    bodyCoords    = actor->extra.tmd->coords;
    RotMatrix(&work->hitTilt, tiltMatrix);
    gte_MulMatrix0(&bodyCoords[3].coord, tiltMatrix, &bodyCoords[3].coord);
    GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(work->hitTilt.vx, pitch, absolutePitch, nextPitch, tiltRemaining, GOLEM_PAWN_ROOK_HIT_TILT_DECAY);
    GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(work->hitTilt.vy, yaw, absoluteYaw, nextYaw, tiltRemaining, GOLEM_PAWN_ROOK_HIT_TILT_DECAY);
    if (tiltRemaining == 0) {
        work->hitTiltActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);

#undef GOLEM_PAWN_ROOK_DECAY_TILT_AXIS
}
