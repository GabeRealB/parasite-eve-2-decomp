/* Part of the library; see mad_chaser.h. Inline helpers the fragments use. */

/// Enters a task state at behavior zero and sub-state zero.
///
/// taskState must select a supported entry of this form's task table (0..5 for
/// the ordinary form, 0..9 for the hidden form). The work must be live; animation
/// requests and frame counters are retained.
static __inline__ void _madChaserEnterTaskState(Task* task, s32 taskState)
{
    MadChaserWork* work = task->work;

    task->state    = taskState;
    work->state    = 0;
    work->subState = 0;
}

/// Updates the enemy's lighting and colour from a part's cached translation.
///
/// Requires a live enemy/model with writable lighting matrices and a readable
/// sampleCoord. Copies cached signed 32-bit XYZ in game units directly to the
/// lighting query, which interprets them as world coordinates; no composition
/// or coordinate conversion occurs here. Borrows both inputs and reserves one
/// VECTOR on an initialized, word-aligned scratch stack, in addition to the
/// query's nested reservations. The fourth word is unused. Releases the sample
/// before returning; the query changes GTE state.
static __inline__ void _madChaserUpdateColor(Enemy* enemy, const GfxCoord* sampleCoord)
{
    VECTOR* samplePosition = SCRATCH_STACK_CURSOR(VECTOR) - 1;

    samplePosition->vx           = sampleCoord->workm.t[0];
    samplePosition->vy           = sampleCoord->workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = samplePosition;
    samplePosition->vz           = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, samplePosition, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Computes the Mad Chaser's horizontal push from one sphere contact.
///
/// Measures overlap from overlapCoord's composed X/Z, narrowed to signed
/// halfwords, against `contact->distance` (summed sphere radii). The direction
/// uses the live model root's full XYZ separation from the contact centre.
/// Both inputs must be in the same composed view frame; the grid view basis's
/// transpose carries the normalized direction into room axes. Negative depth
/// produces zero push. X/Z are narrowed to s16 after Q12 scaling; Y is zero
/// and pad is untouched. Output must provide writable XYZ.
///
/// Requires live model storage, a sphere contact, and an initialized active
/// grid view matrix. Inputs must fit SDK normalization and signed 32-bit
/// squares/products. Borrows all pointers for the call and uses SDK/GTE math;
/// it does not refresh the composed coordinates or reserve scratch storage.
static __inline__ void _madChaserCalcContactPushback(const Task* task, const GfxCoord* overlapCoord, const WorldCollisionContact* contact, SVECTOR* pushDelta)
{
    enum { MAD_CHASER_PUSH_DIRECTION_FRACTION_BITS = 12 };
    SVECTOR         overlapPosition;
    VECTOR          separation;
    VECTOR          normalizedDirection;
    const GfxCoord* root;
    s32             penetrationDepth;

    // Measure X/Z overlap before including height in the push direction.
    overlapPosition.vx = overlapCoord->workm.t[0];
    overlapPosition.vy = overlapCoord->workm.t[1];
    overlapPosition.vz = overlapCoord->workm.t[2];
    root               = task->extra.tmd->coords;
    separation.vx      = overlapPosition.vx - contact->point.vx;
    separation.vy      = 0;
    separation.vz      = overlapPosition.vz - contact->point.vz;
    penetrationDepth   = SquareRoot0(separation.vx * separation.vx + separation.vz * separation.vz);
    penetrationDepth   = contact->distance - penetrationDepth;
    penetrationDepth   = (penetrationDepth <= 0) ? 0 : penetrationDepth;
    // Use the live root for direction, then remove the composed view basis.
    separation.vx = root->workm.t[0] - contact->point.vx;
    separation.vy = root->workm.t[1] - contact->point.vy;
    separation.vz = root->workm.t[2] - contact->point.vz;
    VectorNormal(&separation, &normalizedDirection);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &normalizedDirection, &separation);
    pushDelta->vx = (penetrationDepth * separation.vx) >> MAD_CHASER_PUSH_DIRECTION_FRACTION_BITS;
    pushDelta->vy = 0;
    pushDelta->vz = (penetrationDepth * separation.vz) >> MAD_CHASER_PUSH_DIRECTION_FRACTION_BITS;
}

/// Selects a behavior in the current task state and resets its sub-state.
///
/// behaviorState is narrowed to 16 bits and must index the active behavior table.
/// Requires live Mad Chaser work; task state, animation and counters are retained.
static __inline__ void _madChaserSetBehaviorState(Task* task, s32 behaviorState)
{
    MadChaserWork* work = task->work;

    work->state    = behaviorState;
    work->subState = 0;
}

/// Consumes the pending hit reaction when this frame's hit latch equals one.
///
/// Reactions 1..5 select light recoil, heavy recoil, status hold, heavy recoil
/// and knockdown, resetting the behavior sub-state. Other values only clear the
/// reaction. Returns 1 for hitTaken == 1, even for no/unsupported reaction, and
/// 0 otherwise; the hit latch and task state are retained. Requires live work.
static __inline__ s32 _madChaserTakeHitReaction(Task* task)
{
    MadChaserWork* work = task->work;

    if (work->hitTaken == 1) {
        switch ((s16)(work->hitReaction - 1)) {
            case MAD_CHASER_HIT_REACTION_LIGHT - 1:
                _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_RECOIL_LIGHT);
                break;
            case MAD_CHASER_HIT_REACTION_HEAVY - 1:
                _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_RECOIL_HEAVY);
                break;
            case MAD_CHASER_HIT_REACTION_STATUS - 1:
                _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_STATUS_HOLD);
                break;
            case MAD_CHASER_HIT_REACTION_BLAST - 1:
                _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_RECOIL_HEAVY);
                break;
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN - 1:
                _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_KNOCKDOWN);
                break;
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

/// Returns an s32 boolean for slot 1's animation boundary, jump or held pose.
///
/// Reads the latest animation status without consuming it; a looping clip may
/// report a jump while continuing to play. Requires initialized Mad Chaser work.
/// This inline interface retains the recoil handlers' inlined status test.
static __inline__ s32 _madChaserAnimHasBoundaryStatusInline(Task* task)
{
    MadChaserWork* work = task->work;

    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Selects a behavior from a signed halfword and resets its sub-state.
///
/// behaviorState must be nonnegative and index the active behavior table. The
/// signed-halfword interface retains the entry selectors used by emerge and alert
/// transitions. Requires live Mad Chaser work; task state, animation and counters
/// are retained.
static __inline__ void _madChaserSetBehaviorStateS16(Task* task, s16 behaviorState)
{
    MadChaserWork* work = task->work;

    work->state    = behaviorState;
    work->subState = 0;
}

/// Consumes a pending pull or vanish command before a lurk or combat frame.
///
/// Requires live Mad Chaser work; vanish needs the extended task table. Pull
/// waits until busy is zero, then enters combat's pull behavior with a fresh sub-state.
/// Vanish enters its task state regardless of busy. Either transition clears
/// the entire command halfword and returns s16 1 so the caller skips the old
/// behavior handler. Otherwise returns 0 and leaves the command intact.
static __inline__ s16 _madChaserTakePullOrVanishCommand(Task* task)
{
    MadChaserWork* work = task->work;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_PULL) {
        if (work->busy == 0) {
            work->command = 0;
            _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_PULL);
            return 1;
        }
    } else if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_VANISH) {
        work->command = 0;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_VANISH);
        return 1;
    }
    return 0;
}

/// Installs a scratch-built rotation and releases its MATRIX reservation.
///
/// rootMatrix is live, writable and disjoint from rotationMatrix, which must
/// be the current scratch block. Copies only the nine basis halfwords, retaining
/// translation and alignment bytes. Release precedes the last halfword load;
/// there is no call or reservation between them. The caller dirties composition.
static __inline__ void _madChaserApplyScratchRotation(MATRIX* rootMatrix, const MATRIX* rotationMatrix)
{
    rootMatrix->m[0][0] = rotationMatrix->m[0][0];
    rootMatrix->m[0][1] = rotationMatrix->m[0][1];
    rootMatrix->m[0][2] = rotationMatrix->m[0][2];
    rootMatrix->m[1][0] = rotationMatrix->m[1][0];
    rootMatrix->m[1][1] = rotationMatrix->m[1][1];
    rootMatrix->m[1][2] = rotationMatrix->m[1][2];
    rootMatrix->m[2][0] = rotationMatrix->m[2][0];
    rootMatrix->m[2][1] = rotationMatrix->m[2][1];
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
    rootMatrix->m[2][2] = rotationMatrix->m[2][2];
}

/// Rebuilds the root rotation from the work block's pitch, heading and roll.
///
/// Requires live work/model storage and one MATRIX of free aligned scratch
/// space. Wraps each angle to 0..4095 units per turn and builds Z, then X, then
/// Y rotation at Q12 unit scale. Copies only the nine basis halfwords, retaining
/// translation and alignment bytes; the caller dirties the root composition.
/// Releases scratch before the last coefficient load/store, with no intervening
/// call or reservation, so that load still reads the just-built matrix.
static __inline__ void _madChaserUpdateRotation(Task* task)
{
    MadChaserWork* work           = task->work;
    MATRIX*        rotationMatrix = SCRATCH_STACK_CURSOR(MATRIX) - 1;
    GfxCoord*      root           = task->extra.tmd->coords;
    MATRIX*        rootMatrix;

    work->rotation.vx &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->rotation.vy &= ACTOR_TRANSFORM_ANGLE_MASK;
    work->rotation.vz &= ACTOR_TRANSFORM_ANGLE_MASK;
    gfxSetRotIdentity(rotationMatrix);
    SCRATCH_STACK_CURSOR(MATRIX) = rotationMatrix;
    RotMatrixZ(work->rotation.vz, rotationMatrix);
    RotMatrixX(work->rotation.vx, rotationMatrix);
    RotMatrixY(work->rotation.vy, rotationMatrix);
    rootMatrix = &root->coord;
    _madChaserApplyScratchRotation(rootMatrix, rotationMatrix);
}

/// Consumes a pending vanish command during emergence.
///
/// Requires live Mad Chaser work and the extended task table. Vanish clears
/// the entire command halfword and enters its task state with behavior and
/// sub-state zero, regardless of busy. Returns s16 1 for that transition so
/// the caller skips the old behavior handler; returns 0 for every other command
/// without consuming it. Animation requests and counters are retained.
static __inline__ s16 _madChaserTakeVanishCommand(Task* task)
{
    MadChaserWork* work     = task->work;
    s16            consumed = 0;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_VANISH) {
        consumed      = 1;
        work->command = 0;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_VANISH);
    }
    return consumed;
}
