/* Part of the patrol walker library; see patrol_walker.h. */

/// Turns the 16.16 collision delta from func_800E0C10 into a whole-unit step
/// rounded away from zero, and adds a 0x10 fall unless field_6B pins it. Y is
/// applied in bands (+8 hop above 0x20, -0x20 drop below -0x20, otherwise the
/// plain step), and moving records whether it moved in XZ.
void patrolApplyGroundStep(OverlayWalker* work)
{
    u8*                       head;
    OverlayWalkerMoveScratch* s;
    s32                       valx;
    s32                       valy;
    s32                       valz;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       y;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x18;
    s                        = (OverlayWalkerMoveScratch*)(head - 0x18);
    if (func_800E0C10(work->recs, &s->delta, work->field_56, NULL) != 0) {
        dx         = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.halves.integer;
        dz         = s->delta.vz.halves.integer;
        s->move.vx = dx;
        s->move.vz = dz;
        valx       = ((OverlayWalkerMoveScratch*)(head - 0x18))->delta.vx.word;
        if ((valx & 0xFFFF) != 0) {
            if (valx > 0) {
                s->move.vx++;
            } else {
                s->move.vx--;
            }
        }
        valz = s->delta.vz.word;
        if ((valz & 0xFFFF) != 0) {
            if (valz > 0) {
                s->move.vz++;
            } else {
                s->move.vz--;
            }
        }
        if (work->field_6B == 0) {
            dy         = s->delta.vy.halves.integer;
            valy       = s->delta.vy.word;
            s->move.vy = s->move.vy + dy;
            if ((valy & 0xFFFF) != 0) {
                if (valy > 0) {
                    s->move.vy++;
                } else {
                    s->move.vy--;
                }
            }
        } else {
            s->move.vy = 0;
        }
    } else {
        s->move.vx = 0;
        s->move.vy = 0;
        s->move.vz = 0;
    }
    if (work->field_6B == 0) {
        s->move.vy += 0x10;
    }
    work->moveDelta          = s->move;
    work->coord->coord.t[0] += s->move.vx;
    if (s->move.vy >= 0x21) {
        work->coord->coord.t[1] += 8;
    }
    if (s->move.vy < -0x20) {
        work->coord->coord.t[1] -= 0x20;
    }
    y = s->move.vy;
    if (ABS(y) < 0x20) {
        work->coord->coord.t[1] += y;
    }
    work->coord->coord.t[2] += s->move.vz;
    if (work->coord->coord.t[0] != 0 || work->coord->coord.t[2] != 0) {
        work->moving = 1;
    } else {
        work->moving = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
