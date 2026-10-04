/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. Inline helpers the fragments use. */

/// Serves the animation request (`animRequest`: a blend goes to `animClip`, a
/// restart cuts to it, playing counts frames in `animFrame`) and ticks body
/// slots 1-17 at `animStep`.
static __inline__ void stalkerZebraIvoryTickAnimInline(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;
    s32                    i;

    if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrame = 0;
        } else {
            work->animFrame = stalkerZebraIvoryScaleFrame(arg0, work->animFrame);
        }
        stalkerZebraIvoryBlendClip(arg0);
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING;
    } else if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART) {
        stalkerZebraIvoryRestartClip(arg0);
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING;
        work->animFrame   = 0;
    } else if (work->animRequest == STALKER_ZEBRA_IVORY_ANIM_REQUEST_PLAYING) {
        work->animFrame++;
    }
    i = 1;
    do {
        work->slots[i].rate = work->animStep;
        animationTickSlot(&work->anim, i);
        i++;
    } while (i < 0x12);
}

/// Wraps the actor's `pitch`, `yaw` and `roll` to 12 bits and rebuilds the
/// model root's rotation from them: an identity matrix taken off the scratch
/// stack is turned by roll, pitch and then yaw, and its 3x3 copied into the
/// root coordinate, whose translation is left alone.
static __inline__ void stalkerZebraIvoryApplyRotationInline(Task* arg0)
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
