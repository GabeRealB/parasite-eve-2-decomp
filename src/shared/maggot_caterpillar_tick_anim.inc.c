/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Restarts changed animation requests or advances the seven driven model parts.
///
/// Requires initialized work, model coordinates and animation slots 1..7,
/// plus a clip and blend-table entry for the requested `animId`. A restart
/// resets the frame counter and blends from record zero; an unchanged request
/// increments the counter and advances each slot once. Slot zero is untouched.
static void _maggotCaterpillarTickAnim(Task* actor)
{
    _maggotCaterpillarTickAnimInline(actor);
}
