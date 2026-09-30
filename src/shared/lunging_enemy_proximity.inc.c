/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Proximity check of the approach states. Measures the player's horizontal
/// distance from the root coordinate through a 0x10-byte scratch stack
/// block: under 0x5DC one of `Gp_StateF0.prefix.bytes.field_2`'s bit groups raises `field_6B2`;
/// past it the other two (the second only within 0xBB8) put the enemy into
/// animation 4 and state 1.
void lungerCheckProximity(Task* arg0)
{
    Actor105600Work* work;
    GfxCoord*        self;
    s32              dx;
    s32              distance;
    s32              dz;
    s32              trigger;
    VECTOR*          head;
    VECTOR*          delta;

    self                         = arg0->extra.tmd->coords;
    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = head - 1;
    head[-1].vx                  = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
    delta->vy                    = 0;
    dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2];
    delta->vz                    = dz;
    dx                           = head[-1].vx;
    trigger                      = 0;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    distance                     = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x5DC) {
        if (Gp_StateF0.prefix.bytes.field_2 & 0x17) {
            work->field_6B2 = 1;
        }
    } else {
        if (Gp_StateF0.prefix.bytes.field_2 & 5) {
            trigger = 1;
        }
        if ((Gp_StateF0.prefix.bytes.field_2 & 0x12) && (distance < 0xBB8)) {
            trigger = 1;
        }
        if (trigger != 0) {
            work->field_694 = 4;
            work->field_69C = 0;
            work->field_69E = 0;
            work->field_6AE = 0;
            work->field_6A8 = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
