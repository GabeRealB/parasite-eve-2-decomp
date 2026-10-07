/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Scales a normal-rate movement distance by the task's animation rate.
///
/// Rate is in sixteenths of a frame. The result is a signed distance in the same
/// parent-coordinate units as distanceAtNormalRate, rounded down. The shifts keep
/// only the signed low 20 bits of the product before division by 16; inputs whose
/// product fits that range scale without wrap. Requires a live Mad Chaser work block.
static s32 _madChaserScaleByAnimRate(Task* task, s16 distanceAtNormalRate)
{
    enum {
        MAD_CHASER_ANIM_RATE_FRACTION_BITS = 4,
    };
    MadChaserWork* work = task->work;

    return (s32)((work->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_ANIM_RATE_FRACTION_BITS)) >> 16;
}
