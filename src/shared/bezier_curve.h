/* Cubic Bezier power-basis conversion and reverse sampling.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. Both functions are static: every file that sweeps a curve carries
 * its own copy, as actor_503500 does in two of its files.
 */

#ifndef SRC_SHARED_BEZIER_CURVE_H
#define SRC_SHARED_BEZIER_CURVE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Signed halfword power-basis terms for one coordinate of a cubic Bezier.
typedef struct {
    s16 cubic;     // Coefficient of t^3
    s16 quadratic; // Coefficient of t^2
    s16 linear;    // Coefficient of t
    s16 constant;  // Coordinate at t = 0
} _BezierCurveAxisCoefficients;

STATIC_ASSERT_SIZEOF(_BezierCurveAxisCoefficients, 8);

enum {
    BEZIER_CURVE_PARAMETER_FRACTION_BITS = 16,                                             // Binary fractional places in the sampling parameter
    BEZIER_CURVE_PARAMETER_MAX           = (1 << BEZIER_CURVE_PARAMETER_FRACTION_BITS) - 1 // Greatest parameter below t = 1
};

static void _bezierCurveEvaluate(const SVECTOR controlPoints[3], const SVECTOR* endPoint, s32 stepCount, s32 stepIndex, long outXyz[3]);
static void _bezierCurveCoefficients(s32 startValue, s32 firstControlValue, s32 secondControlValue, s32 endValue, _BezierCurveAxisCoefficients* coefficients);

#endif /* SRC_SHARED_BEZIER_CURVE_H */
