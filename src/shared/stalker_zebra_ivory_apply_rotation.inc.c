/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Wraps the actor's pitch, yaw and roll (`pitch`, `yaw`, `roll`)
/// to 12 bits and rebuilds the model root's rotation from them: an identity
/// matrix taken off the scratch stack is turned by roll, pitch and then yaw,
/// and its 3x3 copied into the root coordinate, whose translation is left
/// alone.
void stalkerZebraIvoryApplyRotation(Task* arg0)
{
    StalkerZebraIvoryWork* work  = (StalkerZebraIvoryWork*)arg0->work;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    MATRIX*                m;
    MATRIX*                dst;

    work->pitch                 &= 0xFFF;
    work->yaw                   &= 0xFFF;
    work->roll                  &= 0xFFF;
    m                            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->roll, m);
    RotMatrixX(work->pitch, m);
    RotMatrixY(work->yaw, m);
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
