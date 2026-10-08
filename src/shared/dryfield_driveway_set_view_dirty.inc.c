/* Part of the Dryfield driveway library; see dryfield_driveway.h. */

/// Sets the deferred reload request for the live save's view.
///
/// The event-script callback consumes a signed halfword: zero clears the
/// request and any nonzero value requests a reload. The script passes one.
static void _drivewaySetViewDirty(s16 viewDirty)
{
    gGameSession->viewDirty = viewDirty;
}
