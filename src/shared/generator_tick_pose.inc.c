/* Part of the Generator library; see generator.h. */

/// Applies the body's animation-set request or advances its current playback.
///
/// Requires initialized GeneratorWork; changed requests are GENERATOR_ANIM_*
/// values 1..3. Initial equal zero requests retain the idle slots started at
/// spawn. A changed request resets animFrames and blends slots 1..9 from frame
/// zero over the set's blend duration; unchanged requests tick those slots and
/// advance the signed halfword frame counter. Slot 0 remains untouched.
static void _generatorUpdateAnimation(Task* task)
{
    _generatorUpdateAnimationInline(task);
}
