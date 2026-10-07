/* Part of the bezier curve library; see bezier_curve.h. */

/// Converts four scalar Bezier control values to signed halfword polynomial terms.
///
/// `coefficients` is caller-owned writable storage. Arithmetic uses signed
/// 32-bit inputs; storing each term retains its low 16 bits, including the
/// constant term. The stored terms describe a*t^3 + b*t^2 + c*t + d in the
/// input coordinate units, with `startValue` at t = 0 and `endValue` at t = 1
/// before narrowing.
static void _bezierCurveCoefficients(s32 startValue, s32 firstControlValue, s32 secondControlValue, s32 endValue, _BezierCurveAxisCoefficients* coefficients)
{
    coefficients->cubic     = -startValue + (firstControlValue - secondControlValue) * 3 + endValue;
    coefficients->quadratic = (startValue + secondControlValue) * 3 - firstControlValue * 6;
    coefficients->linear    = (-startValue + firstControlValue) * 3;
    coefficients->constant  = startValue;
}
