/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Scales `arg1` by the animation speed `animRate`, in 1/16 units.
s32 madChaserScaleBySpeed(Task* arg0, s16 arg1)
{
    return (s32)((((MadChaserWork*)arg0->work)->animRate * arg1) << 0xC) >> 0x10;
}
