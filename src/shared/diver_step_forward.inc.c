/* Part of the Diver library; see diver.h. */

/// Steps the model root along an explicit heading in its parent's XZ plane.
///
/// `distance` is a signed displacement in parent-coordinate units, normally
/// world units beneath the view; a negative value steps backward. `yaw` uses
/// 4096 units per turn (zero along +Z, quarter turn along +X). Requires a live
/// model root. Preserves Y and rotation, marks the composition cache dirty and
/// leaves composition to the caller. Products retain signed 32-bit arithmetic.
static void _diverStepForward(Task* task, s16 distance, s16 yaw)
{
    enum { DIVER_STEP_TRIG_LEFT_SHIFT       = 4,
           DIVER_STEP_PRODUCT_FRACTION_BITS = 16 };

    task->extra.tmd->coords->coord.t[0]  += ((rsin(yaw) << DIVER_STEP_TRIG_LEFT_SHIFT) * distance) >> DIVER_STEP_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(yaw) << DIVER_STEP_TRIG_LEFT_SHIFT) * distance) >> DIVER_STEP_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
