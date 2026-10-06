/* Part of the library; see mad_chaser.h. Inline helpers the fragments use. */

/// Moves the task to `state` with a fresh state machine.
static __inline__ void madChaserEnterState(Task* arg0, s32 state)
{
    MadChaserWork* w = (MadChaserWork*)arg0->work;

    arg0->state = state;
    w->state    = 0;
    w->subState = 0;
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

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`depth`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void madChaserCalcPush(Task* arg0, GfxCoord* coord, WorldCollisionContact* rec, SVECTOR* out)
{
    SVECTOR   pos;
    VECTOR    d;
    VECTOR    n;
    GfxCoord* c2;
    s32       t;
    s32       pen;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    c2     = arg0->extra.tmd->coords;
    d.vx   = pos.vx - rec->point.vx;
    d.vy   = 0;
    d.vz   = pos.vz - rec->point.vz;
    pen    = SquareRoot0(d.vx * d.vx + d.vz * d.vz);
    pen    = rec->distance - pen;
    if (pen <= 0) {
        t = 0;
    } else {
        t = pen;
    }
    pen  = t;
    d.vx = c2->workm.t[0] - rec->point.vx;
    d.vy = c2->workm.t[1] - rec->point.vy;
    d.vz = c2->workm.t[2] - rec->point.vz;
    VectorNormal(&d, &n);
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Moves the state machine to `state` at sub-state 0, reloading the work
/// block through the task as the original does.
static __inline__ void madChaserSetState(Task* arg0, s32 state)
{
    MadChaserWork* w = (MadChaserWork*)arg0->work;

    w->state    = state;
    w->subState = 0;
}

/// Inlined copy of `madChaserTakeHitRequest`: while `hitTaken` is 1,
/// consumes the request in `hitReaction` (1..5 jump to states 6, 7, 8, 7, 9)
/// and returns 1; otherwise returns 0.
static __inline__ s32 madChaserTakeRequest(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->hitTaken == 1) {
        switch ((s16)(work->hitReaction - 1)) {
            case MAD_CHASER_HIT_REACTION_LIGHT - 1:
                madChaserSetState(arg0, 6);
                break;
            case MAD_CHASER_HIT_REACTION_HEAVY - 1:
                madChaserSetState(arg0, 7);
                break;
            case MAD_CHASER_HIT_REACTION_STATUS - 1:
                madChaserSetState(arg0, 8);
                break;
            case MAD_CHASER_HIT_REACTION_BLAST - 1:
                madChaserSetState(arg0, 7);
                break;
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN - 1:
                madChaserSetState(arg0, 9);
                break;
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

static __inline__ s32 madChaserIsHit(Task* arg0)
{
    MadChaserWork* w = (MadChaserWork*)arg0->work;

    if ((w->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (w->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// `madChaserSetState` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `madChaserEmergeAtSpot` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void madChaserSetStateS16(Task* arg0, s16 state)
{
    MadChaserWork* w = (MadChaserWork*)arg0->work;

    w->state    = state;
    w->subState = 0;
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
            madChaserEnterState(arg0, 3);
            w2           = (MadChaserWork*)arg0->work;
            w2->state    = 10;
            w2->subState = 0;
            return 1;
        }
    } else if ((work->command & MAD_CHASER_COMMAND_KIND_MASK) == MAD_CHASER_COMMAND_VANISH) {
        work->command = 0;
        madChaserEnterState(arg0, 7);
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

    work->rotation.vx           &= 0xFFF;
    work->rotation.vy           &= 0xFFF;
    work->rotation.vz           &= 0xFFF;
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
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
