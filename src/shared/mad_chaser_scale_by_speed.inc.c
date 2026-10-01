/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Scales `arg1` by the animation speed `field_41C`, in 1/16 units.
s32 madChaserScaleBySpeed(Task* arg0, s16 arg1)
{
    return (s32)((((Actor341700Work*)arg0->work)->field_41C * arg1) << 0xC) >> 0x10;
}
