/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Approach-cycle state (entry 1 of the package's behaviour table). State 0 drains the
/// `field_6DA` budget by `field_69C` (0 while `field_698` is under the
/// animation's start frame in `gLungerAnimBlendFrames`, 0x14 once past it) and runs
/// the proximity check `lungerCheckProximity` every frame; when the budget runs
/// out it switches to animation 4 and state 1. State 1 waits for `field_698`
/// to reach 0x60, then either falls back to animation 2 (budget left) or
/// turns about: animation 3, state 2, a fresh budget of 1000 per unit of the
/// placement record's `variant`, and `field_6A2` / `field_6A4` set to the
/// current yaw and its opposite. State 2 turns at `field_69E` = 0x3B until
/// `field_698` reaches 0x23, then returns to animation 2 and state 0. A set
/// `field_6B2` or `Gp_StateF0.field_29` overrides everything with animation 2, entry 2
/// and the shared state-F0 slot.
void lungerApproachState(Task* arg0)
{
    GpEnemy*         spawn;
    Actor105600Work* work;
    GfxCoord*        self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;

    self  = arg0->extra.tmd->coords;
    work  = arg0->work;
    spawn = arg0->spawnArg2.pointer;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= gLungerAnimBlendFrames[work->field_694]) {
                delta = 0x14;
            }
            work->field_69C  = delta;
            work->field_69E  = 0;
            work->field_6DA -= work->field_69C;
            if (work->field_6DA <= 0) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
                work->field_69C = 0;
            }
            lungerCheckProximity(arg0);
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x60) {
                if (work->field_6DA <= 0) {
                    param           = spawn->place->variant;
                    work->field_694 = 3;
                    work->field_6A8 = 2;
                    work->field_6DA = param * 1000;
                    ang             = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->field_6A2 = ang;
                    work->field_6A4 = (ang + 0x800) & 0xFFF;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (Gp_StateF0.field_29 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
