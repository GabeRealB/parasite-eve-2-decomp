/* Part of the bezier curve library; see bezier_curve.h. */

/// Evaluates one coordinate, rounding down after each fixed-point Horner product.
///
/// Borrows four signed-halfword power-basis terms in coordinate units.
/// `parameterQ16` has 16 fractional bits; the reverse sampler supplies 0..65535.
/// Each signed 32-bit product must fit before its arithmetic right shift.
/// Returns the coordinate in whole input units without final halfword narrowing;
/// intermediate truncation can make it differ from a single final rounding.
static inline s32 _bezierCurveEvaluateAxis(const _BezierCurveAxisCoefficients* coefficients, s32 parameterQ16)
{
    s32 value;

    value = ((coefficients->cubic * parameterQ16) >> BEZIER_CURVE_PARAMETER_FRACTION_BITS) + coefficients->quadratic;
    value = ((value * parameterQ16) >> BEZIER_CURVE_PARAMETER_FRACTION_BITS) + coefficients->linear;
    return ((value * parameterQ16) >> BEZIER_CURVE_PARAMETER_FRACTION_BITS) + coefficients->constant;
}

/// Samples a cubic Bezier backward from its fourth control point.
///
/// `controlPoints` supplies the first three points; `endPoint` supplies the
/// fourth and may be `&controlPoints[3]`. All points use the same coordinate
/// frame and units. `stepCount` is a subdivision count and `stepIndex` is in
/// [0, stepCount]. For positive counts the parameter is
/// ((stepCount - stepIndex) * 65535) / stepCount with 16 fractional bits:
/// index 0 is just below t = 1, and index stepCount is exactly t = 0.
/// Coefficients narrow to signed halfwords before evaluation; each Horner
/// stage uses signed 32-bit multiplication and an arithmetic right shift.
///
/// Writes three consecutive signed 32-bit SDK `long` coordinates (X, Y, Z)
/// to caller-owned `outXyz`, also accepting the XYZ prefix of a `VECTOR`.
/// A zero `stepCount` accesses neither points nor output. Inputs must keep
/// the parameter scaling and polynomial arithmetic within signed 32-bit range.
static void _bezierCurveEvaluate(const SVECTOR controlPoints[3], const SVECTOR* endPoint, s32 stepCount, s32 stepIndex, long outXyz[3])
{
    _BezierCurveAxisCoefficients coefficients[3];
    const SVECTOR*               firstControlPoint;
    const SVECTOR*               secondControlPoint;
    s32                          parameterQ16;
    s32                          axis;
    long*                        coordinateOut;

    if (stepCount != 0) {
        parameterQ16       = ((stepCount - stepIndex) * BEZIER_CURVE_PARAMETER_MAX) / stepCount;
        firstControlPoint  = &controlPoints[1];
        secondControlPoint = &controlPoints[2];
        _bezierCurveCoefficients(controlPoints->vx, firstControlPoint->vx, secondControlPoint->vx, endPoint->vx, &coefficients[0]);
        _bezierCurveCoefficients(controlPoints->vy, firstControlPoint->vy, secondControlPoint->vy, endPoint->vy, &coefficients[1]);
        _bezierCurveCoefficients(controlPoints->vz, firstControlPoint->vz, secondControlPoint->vz, endPoint->vz, &coefficients[2]);

        // Round down at each fixed-point Horner stage, preserving halfword terms.
        coordinateOut = outXyz;
        for (axis = 0; axis < ARRAY_SIZE(coefficients); axis++) {
            *coordinateOut++ = _bezierCurveEvaluateAxis(&coefficients[axis], parameterQ16);
        }
    }
}
