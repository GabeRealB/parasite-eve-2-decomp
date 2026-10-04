/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Applies the decaying `hitTilt` to part 3 of the model: the tilt's
/// rotation matrix is multiplied into that part's matrix, then X and Y each
/// step 0x20 toward zero, snapping to zero within 0x20. Once both have
/// settled, `hitTiltActive` is cleared.
void golemPawnRookDecayHitTilt(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          coord;
    MATRIX*            matrix;
    s32                angleX;
    s32                angleY;
    s32                absX;
    s32                nextX;
    s32                absY;
    s32                nextY;
    s32                active;

    matrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->hitTilt, matrix);
    gte_MulMatrix0(&coord[3].coord, matrix, &coord[3].coord);
    angleX = work->hitTilt.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->hitTilt.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->hitTilt.vx = nextX;
            active           = 1;
        }
    }
    angleY = work->hitTilt.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->hitTilt.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->hitTilt.vy = nextY;
            active           = 1;
        }
    }
    if (active == 0) {
        work->hitTiltActive = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}
