/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Applies a changed GOLEM animation request or advances the active animation.
///
/// `task` owns an initialized nineteen-part rig. Requested clip IDs 1..21
/// select live entries in the carrier's animation and blend-duration tables;
/// animation entry 0 is empty. A changed ID
/// restarts slots 1..18 with the selected duration in frames and clears
/// `animFrame`; an unchanged ID increments that signed halfword and ticks the
/// same slots once. Root slot 0 is left alone.
static void _golemKnightBishopTickAnim(Task* task)
{
    _golemKnightBishopTickAnimInline(task);
}
