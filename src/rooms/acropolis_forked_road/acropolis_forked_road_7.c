#include "common.h"

#include "main/gameflow.h"
#include "main/task.h"

/// An eight-frame screen fade: draws the fade overlay (mode 2) at the level
/// held in the task's `killCountdown`, which rises by 0x20 a frame, and asks
/// for the task to be killed once it passes 0xFF.
void func_acropolis_forked_road_8017E1C0(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}

/// The same eight-frame fade run the other way: the overlay level is the
/// complement of the rising counter, so it falls from 0xFF by 0x20 a frame,
/// and the task kills itself once the counter passes 0xFF.
void func_acropolis_forked_road_8017E220(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}
