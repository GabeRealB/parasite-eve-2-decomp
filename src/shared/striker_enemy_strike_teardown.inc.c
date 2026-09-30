/* Part of the striker enemy library; see striker_enemy.h. */

/// Teardown state of the beam child: each frame it clears the root
/// coordinate's `composeStamp` and counts `killCountdown` up, and on the twelfth frame
/// unlinks the beam's collision object and kills the task.
void strikerStrikeTeardown(Task* task)
{
    StrikerChildWork* child;
    TmdObject*        tmd;
    u16               countdown;

    child                     = (StrikerChildWork*)task->work;
    tmd                       = task->extra.tmd;
    tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    countdown                 = task->killCountdown + 1;
    task->killCountdown       = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj(&child->obj);
        taskKill(task);
    }
}
