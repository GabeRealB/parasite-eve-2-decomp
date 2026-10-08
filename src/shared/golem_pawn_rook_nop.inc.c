/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Leaves the body unchanged for behavior slots unavailable in this carrier.
///
/// Used for sword or launcher handlers the other weapon lacks and for the Pawn's
/// absent Silence scream. The callback argument is unused.
static void _golemPawnRookNopState(Task* task)
{
}
