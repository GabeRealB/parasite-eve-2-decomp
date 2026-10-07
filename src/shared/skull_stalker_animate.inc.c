/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Applies the requested animation or advances the two animated model parts.
///
/// This out-of-line entry shares its playback operation with the death state.
/// Requires initialized work and the live three-part TMD model.
static void _skullStalkerAnimate(Task* task)
{
    _skullStalkerTickAnimation(task);
}
