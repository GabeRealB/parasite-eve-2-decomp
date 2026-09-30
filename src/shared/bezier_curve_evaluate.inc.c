/* Part of the bezier curve library; see bezier_curve.h. */

/// Evaluates a cubic Bezier segment at step `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`, so step 0 is the `p3` end. Writes the X/Y/Z result to `out`
/// as ((a*t >> 16 + b)*t >> 16 + c)*t >> 16 + d per axis. Does nothing when
/// `len` is 0.
static void bezierCurveEvaluate(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        bezierCurveCoefficients(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        bezierCurveCoefficients(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        bezierCurveCoefficients(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}
