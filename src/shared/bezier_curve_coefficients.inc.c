/* Part of the bezier curve library; see bezier_curve.h. */

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`)
/// into the coefficients of its polynomial, highest order first: `t^3` in
/// `vx`, `t^2` in `vy`, `t` in `vz` and the constant term in `pad`.
static void bezierCurveCoefficients(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}
