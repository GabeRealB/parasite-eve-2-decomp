#include "common.h"
#include "main/task.h"

/// Entry 3 of the room's task table: draws `Fade_DrawOverlay` at the grey
/// level `killCountdown`, which rises by 0x20 a frame; at 0x100 the task asks
/// to be killed with `Task_RequestKill`, which the streamed-scene task that
/// spawned it polls for.
void func_acropolis_promenade_8017DF74(Task* arg0)
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

/// Entry 4 of the room's task table: the reverse ramp of entry 3, drawing the
/// overlay at `~killCountdown`, and killing itself outright at the end.
void func_acropolis_promenade_8017DFD4(Task* arg0)
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
