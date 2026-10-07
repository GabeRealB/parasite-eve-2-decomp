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

/// Colours `enemy` from `coord`'s world position through a 0x10-byte
/// `VECTOR` taken off the scratch stack. Inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void madChaserUpdateColor(void* enemy, GfxCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    block->vz                    = coord->workm.t[2];
    worldCoordUpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
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

/// Message 0x2C00 (see `command`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `busy` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `subState` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 madChaserTakeHit(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    MadChaserWork* w2;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_PULL) {
        if (work->busy == 0) {
            work->command = 0;
            _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_COMBAT);
            w2           = (MadChaserWork*)arg0->work;
            w2->state    = 10;
            w2->subState = 0;
            return 1;
        }
    } else if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_VANISH) {
        work->command = 0;
        _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_VANISH);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll in `rotation` to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off the scratch stack, copying the 3x3 into the root coordinate.
static __inline__ void madChaserUpdateRotation(Task* arg0)
{
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    MATRIX*        m     = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    GfxCoord*      coord = arg0->extra.tmd->coords;
    MATRIX*        dst;

    work->rotation.vx &= 0xFFF;
    work->rotation.vy &= 0xFFF;
    work->rotation.vz &= 0xFFF;
    gfxSetRotIdentity(m);
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->rotation.vz, m);
    RotMatrixX(work->rotation.vx, m);
    RotMatrixY(work->rotation.vy, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_STACK_RELEASE_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

/// Message 0x2C00 with low nibble 3 (see `command`) consumes the message and
/// moves the task to state 7 with a fresh state machine; returns 1 when it did,
/// so the caller skips this frame's state handler. The `s16` result is what
/// keeps the `move` between the flag and its test, and the reload through a
/// second local is what puts it in `$v1`.
static __inline__ s16 madChaserTakeHitNibble3(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            hit  = 0;
    MadChaserWork* w2;

    if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_VANISH) {
        hit           = 1;
        work->command = 0;
        arg0->state   = 7;
        w2            = (MadChaserWork*)arg0->work;
        w2->state     = 0;
        w2->subState  = 0;
    }
    return hit;
}
