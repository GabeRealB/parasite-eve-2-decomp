/* Part of the Glutton library; see glutton.h. */

/// Stores the encounter's spinner-release word without normalization.
///
/// Waiting spinners advance for value 1; chasing spinners abort for value 0.
/// This accessor has no callers in either carrier.
static void _gluttonSetSpinnersReleased(s16 releaseState)
{
    gGluttonSpinnersReleased = releaseState;
}
