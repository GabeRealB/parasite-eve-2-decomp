/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Returns to walking when the backward-leap clip ends without a pending reaction.
///
/// Pending reactions take priority and may reset the running behavior.
/// Otherwise a slot boundary, loop jump or settled end selects walk and clears
/// `subState`. Does not advance animation; the running update does that first.
static void _stalkerZebraIvoryWaitClip(Task* task)
{
    if ((s16)_stalkerZebraIvoryApplyPendingReaction(task) == 0 && (s16)_stalkerZebraIvoryClipDone(task) != 0) {
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_WALK);
    }
}
