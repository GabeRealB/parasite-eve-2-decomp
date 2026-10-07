/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Queues a clip restart at a signed rate measured in sixteenths of a frame per tick.
///
/// The next animation update applies the request; this call leaves slots alone.
/// `clipIndex` must select a loaded set supporting body tracks 1..17. The rate
/// is stored as s16 and narrows to a signed byte when applied; 16 is normal speed.
static void _stalkerZebraIvoryRequestClipRestart(Task* task, s16 clipIndex, s16 rate)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    work->animStep    = rate;
    work->animClip    = clipIndex;
    work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
}
