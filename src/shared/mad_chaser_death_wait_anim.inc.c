/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifdef MAD_CHASER_DEATH_WAIT_ANIM_HANDLER
/// Ticks the settle animation and advances command death on a boundary status.
///
/// Requires live task-owned `MadChaserWork` with initialized nine-slot animation
/// and pose storage, a nine-part model and valid loaded clip tracks. Ticks slots
/// 1..8, then tests slot 1 for a boundary, control jump or held boundary; a jump
/// can advance the state even while a looping clip continues. Called at behavior
/// state 2, advances the 16-bit state to 3 without consuming the slot status or
/// releasing resources. Playback rate is in sixteenths of a normal-rate frame.
/// Storage and clip data must stay live through the call; the playback API's
/// scratch-stack and alignment requirements apply.
static void MAD_CHASER_DEATH_WAIT_ANIM_HANDLER(Task* task)
#else
void madChaserDeathWaitAnim(Task* task)
#endif
{
    enum { MAD_CHASER_DEATH_STATUS_SLOT = 1 };
    MadChaserWork* stateWork;
    MadChaserWork* animationWork;
    s32            hasBoundaryStatus;

    stateWork = task->work;
    _madChaserTickAnim(task);
    animationWork = task->work;
    if ((animationWork->slots[MAD_CHASER_DEATH_STATUS_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (animationWork->slots[MAD_CHASER_DEATH_STATUS_SLOT].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        stateWork->state++;
    }
}
