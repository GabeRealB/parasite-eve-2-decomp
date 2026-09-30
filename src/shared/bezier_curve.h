/* Cubic Bezier evaluation for actors and rooms that sweep objects along
 * curves: a helper that turns four control values into power-basis
 * coefficients, and an evaluator that samples a 3D curve at step `pos` of
 * `len` by Horner's rule in 16-bit fixed point.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. Both functions are static: every file that sweeps a curve carries
 * its own copy, as actor_503500 does in two of its files.
 */

#ifndef SRC_SHARED_BEZIER_CURVE_H
#define SRC_SHARED_BEZIER_CURVE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

static void bezierCurveEvaluate(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
static void bezierCurveCoefficients(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

#endif /* SRC_SHARED_BEZIER_CURVE_H */
