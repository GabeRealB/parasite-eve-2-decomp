/* Part of the Dryfield driveway library; see dryfield_driveway.h. */

/// Script callback: stores its argument in the session's `viewDirty` flag.
void drivewaySetViewDirty(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}
