/* Part of the Glutton library; see glutton.h. */

/// Returns the encounter's spinner-release word unchanged.
///
/// Waiting spinners advance for value 1; chasing spinners abort for value 0.
/// This accessor has no callers in either carrier.
static s16 _gluttonGetSpinnersReleased(void)
{
    return gGluttonSpinnersReleased;
}
