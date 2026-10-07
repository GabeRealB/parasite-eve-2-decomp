/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Applies a pending body-animation request and advances body tracks once.
///
/// Out-of-line entry for `_stalkerZebraIvoryTickAnimInline`, with the same
/// initialized-rig, clip, rate and storage requirements.
static void _stalkerZebraIvoryTickAnim(Task* task)
{
    _stalkerZebraIvoryTickAnimInline(task);
}
