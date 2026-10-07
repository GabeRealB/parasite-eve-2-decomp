/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Ticks the death settle pose and advances when slot 1 reports a boundary.
///
/// Requires initialized nine-slot work/pose storage, a nine-part live model and
/// valid loaded clip tracks in death behavior 2. Ticks slots 1..8, then tests
/// slot 1 for a boundary, control jump or held boundary. A jump may advance even
/// while a clip loops. Advances the u16 behavior to 3 without consuming status
/// or releasing resources. animRate is in sixteenths of a frame; animation API
/// scratch/alignment requirements apply. A carrier may bind the static command
/// death instance through MAD_CHASER_DEATH_WAIT_ANIM_HANDLER; the default is the
/// static ordinary-death instance. All storage remains live through the call.
#ifdef MAD_CHASER_DEATH_WAIT_ANIM_HANDLER
static void MAD_CHASER_DEATH_WAIT_ANIM_HANDLER(Task* task)
#else
static void _madChaserDeathWaitAnim(Task* task)
#endif
{
    MadChaserWork* stateWork;

    stateWork = task->work;
    _madChaserTickAnim(task);
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        stateWork->state++;
    }
}
