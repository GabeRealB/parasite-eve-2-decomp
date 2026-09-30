/* Part of the library; see hopping_enemy.h. Inline helpers the fragments use. */

/// Moves the task to `state` with a fresh state machine.
static __inline__ void hopperEnterState(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Colours `enemy` from `coord`'s world position through a 0x10-byte
/// `VECTOR` taken off the scratch stack. Inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void hopperUpdateColor(void* enemy, GfxCoord* coord)
{
    VECTOR* block = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);

    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    block->vz                    = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Push-out of the model from contact record `rec`: how far `coord` sits
/// inside the record's radius (`depth`), along the direction from the
/// record's centre to the root part, carried into grid space.
///
/// `rec` must stay an inline argument: `integrate.c` expands it with
/// `EXPAND_SUM`, giving `(i * 0x18 + work) + 0x2EC` rather than a loop giv.
static __inline__ void hopperCalcPush(Task* arg0, GfxCoord* coord, WorldCollisionContact* rec, SVECTOR* out)
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
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &n, &d);
    out->vx = (pen * d.vx) >> 12;
    out->vy = 0;
    out->vz = (pen * d.vz) >> 12;
}

/// Moves the state machine to `state` at sub-state 0, reloading the work
/// block through the task as the original does.
static __inline__ void hopperSetState(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Inlined copy of `hopperTakeHitRequest`: while `field_41E` is 1,
/// consumes the request in `field_448` (1..5 jump to states 6, 7, 8, 7, 9)
/// and returns 1; otherwise returns 0.
static __inline__ s32 hopperTakeRequest(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0:
                hopperSetState(arg0, 6);
                break;
            case 1:
                hopperSetState(arg0, 7);
                break;
            case 2:
                hopperSetState(arg0, 8);
                break;
            case 3:
                hopperSetState(arg0, 7);
                break;
            case 4:
                hopperSetState(arg0, 9);
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

static __inline__ s32 hopperIsHit(Task* arg0)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    if ((w->flags_EC.half & 1) || (w->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// `hopperSetState` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `hopperEmergeAtSpot` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void hopperSetStateS16(Task* arg0, s16 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}
