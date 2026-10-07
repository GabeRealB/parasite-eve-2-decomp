/* Part of the backdrop crossfade library; see backdrop_crossfade.h. */

/// Fade the room back out eight levels a frame, driving both backdrop redraws
/// with complementary shades, and advance the task's state once the level
/// bottoms out. `Task::killCountdown` holds the level.
void crossfadeOutState(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    _crossfadeDrawBackdrop(task->killCountdown);
    _crossfadeDrawLive(CROSSFADE_SHADE_UNITY - task->killCountdown);
}
