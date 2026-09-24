#include "common.h"

#include "main/gameflow.h"
#include "main/task.h"

/// Fade-out task, entry 2 of the room's task table: subtracts a full-screen
/// grey that grows by 0x20 a frame, counted in `Task::killCountdown`, and asks
/// for its own release once the screen is black. The streamed-scene tasks
/// wait on that release before warping the player.
void func_acropolis_observatory_8017E0D4(Task* arg0)
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

/// Fade-in task, entry 3 of the room's task table: the reverse of
/// `func_acropolis_observatory_8017E0D4`, subtracting a grey that shrinks by
/// 0x20 a frame from black, then killing itself.
void func_acropolis_observatory_8017E134(Task* arg0)
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
