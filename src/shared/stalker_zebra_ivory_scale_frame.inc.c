/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Converts frame count `arg1` to the clip's frame at step `animStep` (0 when
/// the step is 0).
s16 stalkerZebraIvoryScaleFrame(Task* arg0, s16 arg1)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    if (work->animStep == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->animStep << 12) >> 16;
}
