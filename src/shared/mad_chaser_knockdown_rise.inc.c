/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts recovery once the knockdown animation reports a boundary, jump or hold.
///
/// Saved stance 1 recovers upright with clip 7 and a 50-frame blend; all other
/// values recover low with clip 1 and a 30-frame blend. Both play at normal rate
/// and advance the sub-state. Requires initialized animation storage and the
/// stance saved by `_madChaserKnockdownStart`; busy and task state are retained.
static void _madChaserKnockdownRise(Task* task)
{
    enum {
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_STANCE       = 1,
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_CLIP         = 7,
        MAD_CHASER_KNOCKDOWN_RISE_LOW_CLIP             = 1,
        MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_BLEND_FRAMES = 50,
        MAD_CHASER_KNOCKDOWN_RISE_LOW_BLEND_FRAMES     = 30,
    };
    MadChaserWork* work;
    MadChaserWork* lowWork;
    MadChaserWork* uprightWork;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        if (work->stateScratch == MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_STANCE) {
            uprightWork                  = task->work;
            uprightWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_BLEND_FRAMES;
            uprightWork->animRate        = ANIMATION_RATE_ONE;
            uprightWork->animId          = MAD_CHASER_KNOCKDOWN_RISE_UPRIGHT_CLIP;
            uprightWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            lowWork                  = task->work;
            lowWork->animBlendFrames = MAD_CHASER_KNOCKDOWN_RISE_LOW_BLEND_FRAMES;
            lowWork->animRate        = ANIMATION_RATE_ONE;
            lowWork->animId          = MAD_CHASER_KNOCKDOWN_RISE_LOW_CLIP;
            lowWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
        work->subState = work->subState + 1;
    }
}
