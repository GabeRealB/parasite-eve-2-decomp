/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_AIM`: watches the player and pounces
/// (`MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE`) once they are within
/// `MAGGOT_CATERPILLAR_POUNCE_RANGE` and less than 0x80 off its facing.
void maggotCaterpillarAimState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s16                    angle;
    s32                    magnitude;
    s16                    wrapped;
    s16                    difference;
    s32                    distance;
    s32                    dx;
    s32                    dz;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    coord                                                                     = arg0->extra.tmd->coords;
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = arg0->work;
    work->yaw                                                                 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
    scratchEnd[-1].vx                                                         = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                                                                 = 0;
    dz                                                                        = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                                                                 = dz;
    dx                                                                        = scratchEnd[-1].vx;
    distance                                                                  = SquareRoot0((dx * dx) + (dz * dz));
    angle                                                                     = work->yaw - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & 0xFFF);
    magnitude                                                                 = __builtin_abs((s32)angle);
    if (magnitude < 0x800) {
        difference = magnitude;
    } else {
        if (angle > 0) {
            wrapped = 0x1000 - angle;
        } else {
            wrapped = angle + 0x1000;
        }
        difference = wrapped;
    }
    if ((distance < MAGGOT_CATERPILLAR_POUNCE_RANGE) && (difference < 0x80)) {
        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
        work->step      = 0;
        work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}
